#include "Townsfolk/SFTownsfolk.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "MassSpawnerSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ZoneGraphData.h"
#include "ZoneGraphDelegates.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"

namespace
{
    const TCHAR* ZoneMap=TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");

    FVector Metres(const TArray<TSharedPtr<FJsonValue>>& At) { return FVector(At[0]->AsNumber()*100, At[1]->AsNumber()*100, 0); }

    float Hash01(uint32 A, uint32 B) { return float(HashCombine(A, B) % 10000)/10000.0f; }

    /** Index of the lane segment nearest P (points Begin..End-1 of the lane). */
    int32 NearestSegment(const FZoneGraphStorage& S, int32 Lane, const FVector& P)
    {
        const FZoneLaneData& L=S.Lanes[Lane]; int32 Best=L.PointsBegin; double BestD=TNumericLimits<double>::Max();
        for (int32 I=L.PointsBegin; I<L.PointsEnd-1; ++I)
        {
            const double D=FMath::PointDistToSegmentSquared(P,S.LanePoints[I],S.LanePoints[I+1]);
            if (D<BestD) { BestD=D; Best=I; }
        }
        return Best;
    }
}

bool USFTownsfolkSettings::Validate(FString& OutError) const
{
    const bool bOk=ResidentCap>=1 && ResidentCap<=300 && FMath::IsFinite(GameDayMinutes) && GameDayMinutes>=1 && GameDayMinutes<=240
        && StartHour>=0 && StartHour<24 && WalkSpeedMetresPerSecond>=0.5f && WalkSpeedMetresPerSecond<=3 && !District.IsEmpty();
    OutError=bOk ? FString() : TEXT("Townsfolk prototype settings outside declared bounds.");
    return bOk;
}

ESFTownsfolkGoal FSFTownsfolkResident::GoalAt(float Hour, int32 Day, int32& OutMarket) const
{
    // One market errand on some days: after work for workers, mid-morning for everyone else (doc section 4 day blocks).
    OutMarket=INDEX_NONE;
    if (Hour<Wake || Hour>=Sleep) { return ESFTownsfolkGoal::Home; }
    if (bHasWork && Hour>=WorkStart && Hour<WorkEnd) { return ESFTownsfolkGoal::Work; }
    const float ErrandAt=bHasWork ? WorkEnd : Wake+1.0f+3.0f*Hash01(Seed,Day*7+1);
    if (!Markets.IsEmpty() && Hash01(Seed,Day)<ErrandChance && Hour>=ErrandAt && Hour<ErrandAt+1.0f)
    { OutMarket=int32(HashCombine(Seed,Day)%uint32(Markets.Num())); return ESFTownsfolkGoal::Market; }
    return ESFTownsfolkGoal::Home;
}

void USFTownsfolkTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
    BuildContext.AddFragment<FTransformFragment>();
    BuildContext.AddFragment<FSFTownsfolkResidentFragment>();
}

USFTownsfolkProcessor::USFTownsfolkProcessor()
    : EntityQuery(*this)
{
    ExecutionFlags=int32(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Client);
    bRequiresGameThreadExecution=true;  // ZoneGraph queries and the placeholder instance updates
}

void USFTownsfolkProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FSFTownsfolkResidentFragment>(EMassFragmentAccess::ReadWrite);
}

void USFTownsfolkProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    USFTownsfolkSubsystem* Town=Context.GetWorld() ? Context.GetWorld()->GetSubsystem<USFTownsfolkSubsystem>() : nullptr;
    if (!Town || !Town->IsActive()) { return; }
    const double Started=FPlatformTime::Seconds();
    int32 Day=0; const float Hour=Town->GameHour(Day);
    const float Step=Town->WalkSpeedCm()*Context.GetDeltaTimeSeconds();
    EntityQuery.ForEachEntityChunk(Context,[Town,Hour,Day,Step](FMassExecutionContext& Context)
    {
        const TArrayView<FTransformFragment> Transforms=Context.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FSFTownsfolkResidentFragment> Walkers=Context.GetMutableFragmentView<FSFTownsfolkResidentFragment>();
        for (FMassExecutionContext::FEntityIterator It=Context.CreateEntityIterator(); It; ++It)
        {
            FSFTownsfolkResidentFragment& W=Walkers[It]; FTransform& T=Transforms[It].GetMutableTransform();
            const FSFTownsfolkResident& R=Town->Resident(W.Resident); TArray<FVector>& Route=Town->Route(W.Resident);
            int32 Market=INDEX_NONE; const ESFTownsfolkGoal Goal=R.GoalAt(Hour,Day,Market);
            if (Goal!=W.Goal || Market!=W.Market)
            {
                const FVector To=Goal==ESFTownsfolkGoal::Work ? R.Work : Goal==ESFTownsfolkGoal::Market ? R.Markets[Market] : R.Home;
                if (W.Goal!=ESFTownsfolkGoal::None || Goal!=ESFTownsfolkGoal::Home) { W.bInside=false; }  // a new world starts with everyone home
                Town->PlanRoute(T.GetLocation(),To,Route); W.Next=0; W.Goal=Goal; W.Market=Market;
            }
            // Walk the route: horizontal step toward the next point, height between the points.
            float Left=Step;
            while (Left>0 && W.Next<Route.Num())
            {
                const FVector At=T.GetLocation(), Target=Route[W.Next]; const FVector Flat(Target.X-At.X,Target.Y-At.Y,0);
                const float D=Flat.Size();
                if (D<=Left) { T.SetLocation(Target); Left-=D; ++W.Next; continue; }
                T.SetLocation(FVector(At.X,At.Y,At.Z)+Flat/D*Left+FVector(0,0,(Target.Z-At.Z)*Left/D));
                T.SetRotation(FRotator(0,Flat.Rotation().Yaw,0).Quaternion());
                Left=0;
            }
            if (W.Next>=Route.Num() && W.Goal==ESFTownsfolkGoal::Home) { W.bInside=true; }  // indoors at home: hidden
            Town->Show(W.Resident,T,!W.bInside);
        }
    });
    Town->NoteFrame((FPlatformTime::Seconds()-Started)*1000.0);
}

bool USFTownsfolkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World=Cast<UWorld>(Outer);
    return World && (World->WorldType==EWorldType::Game || World->WorldType==EWorldType::PIE);
}

float USFTownsfolkSubsystem::GameHour(int32& OutDay) const
{
    const double Hours=StartHour+(GetWorld()->GetTimeSeconds()-StartSeconds)/SecondsPerGameHour;
    OutDay=int32(Hours/24.0);
    return float(FMath::Fmod(Hours,24.0));
}

bool USFTownsfolkSubsystem::LoadCensus(FString& OutError)
{
    const USFTownsfolkSettings* Settings=GetDefault<USFTownsfolkSettings>();
    const FString Path=FPaths::Combine(FPaths::ProjectDir(),TEXT("ArtSource/Greenlands/silverbrook-census.json"));
    FString Text; TSharedPtr<FJsonObject> Census;
    if (!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Census) || !Census.IsValid())
    { OutError=FString::Printf(TEXT("Census missing or unreadable: %s"),*Path); return false; }
    TMap<FString,FVector> Homes, Works, Markets;
    for (const auto& V:Census->GetArrayField(TEXT("households"))) { Homes.Add(V->AsObject()->GetStringField(TEXT("id")),Metres(V->AsObject()->GetArrayField(TEXT("at")))); }
    for (const auto& V:Census->GetArrayField(TEXT("workplaces"))) { Works.Add(V->AsObject()->GetStringField(TEXT("id")),Metres(V->AsObject()->GetArrayField(TEXT("at")))); }
    for (const auto& V:Census->GetArrayField(TEXT("markets"))) { Markets.Add(V->AsObject()->GetStringField(TEXT("label")),Metres(V->AsObject()->GetArrayField(TEXT("at")))); }
    for (const auto& V:Census->GetArrayField(TEXT("residents")))
    {
        const TSharedPtr<FJsonObject> O=V->AsObject();
        if (O->GetStringField(TEXT("district"))!=Settings->District || Residents.Num()>=Settings->ResidentCap) { continue; }
        FSFTownsfolkResident R; R.Id=O->GetStringField(TEXT("id")); R.Seed=GetTypeHash(R.Id);
        R.Home=Homes.FindRef(O->GetStringField(TEXT("household")));
        const TSharedPtr<FJsonObject> Hours=O->GetObjectField(TEXT("schedule"));
        R.Wake=Hours->GetNumberField(TEXT("wake")); R.Sleep=Hours->GetNumberField(TEXT("sleep")); R.ErrandChance=Hours->GetNumberField(TEXT("errand_chance"));
        FString Work;
        if (O->TryGetStringField(TEXT("workplace"),Work) && Works.Contains(Work) && Hours->HasField(TEXT("work_start")))
        { R.bHasWork=true; R.Work=Works[Work]; R.WorkStart=Hours->GetNumberField(TEXT("work_start")); R.WorkEnd=Hours->GetNumberField(TEXT("work_end")); }
        for (const auto& M:O->GetArrayField(TEXT("markets"))) { if (const FVector* At=Markets.Find(M->AsString())) { R.Markets.Add(*At); } }
        Residents.Add(R);
    }
    if (Residents.IsEmpty()) { OutError=FString::Printf(TEXT("No census residents in %s"),*Settings->District); return false; }
    return true;
}

bool USFTownsfolkSubsystem::PlanRoute(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
    // Shortest lane path: along lanes (outgoing links) or across the street (adjacent links), so dead ends and
    // one-way lane directions never strand anyone. ponytail: Dijkstra over every lane per request; fine for 50
    // residents on a few hundred lanes, cache per destination when the town grows.
    Out.Reset(); ++Routes_Planned;
    const UZoneGraphSubsystem* ZG=GetWorld()->GetSubsystem<UZoneGraphSubsystem>();
    const FVector Ext(25000,25000,6000); FZoneGraphLaneLocation A,B; float D=0;  // places off the footway network join it at its nearest point
    if (!ZG || !ZG->FindNearestLane(FBox(From-Ext,From+Ext),FZoneGraphTagFilter(),A,D)
        || !ZG->FindNearestLane(FBox(To-Ext,To+Ext),FZoneGraphTagFilter(),B,D) || A.LaneHandle.DataHandle!=B.LaneHandle.DataHandle)
    { Out.Add(FVector(To.X,To.Y,From.Z)); ++Routes_Failed; return false; }
    const FZoneGraphStorage& S=ZG->GetZoneGraphData(A.LaneHandle.DataHandle)->GetStorage();
    const int32 N=S.Lanes.Num(); TArray<double> Cost; Cost.Init(TNumericLimits<double>::Max(),N);
    TArray<int32> Prev; Prev.Init(INDEX_NONE,N); TArray<bool> Done; Done.Init(false,N);
    const auto Length=[&S](int32 L){ return FVector::Dist(S.LanePoints[S.Lanes[L].PointsBegin],S.LanePoints[S.Lanes[L].PointsEnd-1]); };
    Cost[A.LaneHandle.Index]=0;
    for (;;)
    {
        int32 U=INDEX_NONE;
        for (int32 I=0; I<N; ++I) { if (!Done[I] && Cost[I]<TNumericLimits<double>::Max() && (U==INDEX_NONE || Cost[I]<Cost[U])) { U=I; } }
        if (U==INDEX_NONE || U==B.LaneHandle.Index) { break; }
        Done[U]=true;
        for (int32 K=S.Lanes[U].LinksBegin; K<S.Lanes[U].LinksEnd; ++K)
        {
            const FZoneLaneLinkData& Link=S.LaneLinks[K];
            const double Step=Link.Type==EZoneLaneLinkType::Outgoing ? Length(U) : Link.Type==EZoneLaneLinkType::Adjacent ? 300.0 : -1;
            if (Step>=0 && Cost[U]+Step<Cost[Link.DestLaneIndex]) { Cost[Link.DestLaneIndex]=Cost[U]+Step; Prev[Link.DestLaneIndex]=U; }
        }
    }
    TArray<int32> Lanes;
    for (int32 L=B.LaneHandle.Index; L!=INDEX_NONE; L=Prev[L]) { Lanes.Insert(L,0); }
    if (Lanes[0]!=A.LaneHandle.Index) { Out.Add(FVector(To.X,To.Y,From.Z)); ++Routes_Failed; return false; }
    // Points: on each lane from where we join it (its start after an outgoing link, else the nearest point) to where we leave it.
    FVector At=A.Position; Out.Add(At);
    for (int32 I=0; I<Lanes.Num(); ++I)
    {
        const int32 L=Lanes[I]; const FZoneLaneData& Lane=S.Lanes[L];
        bool bFromStart=false;
        if (I>0) { for (int32 K=S.Lanes[Lanes[I-1]].LinksBegin; K<S.Lanes[Lanes[I-1]].LinksEnd; ++K)
        { bFromStart|=S.LaneLinks[K].DestLaneIndex==L && S.LaneLinks[K].Type==EZoneLaneLinkType::Outgoing; } }
        const int32 First=bFromStart ? Lane.PointsBegin : NearestSegment(S,L,At)+1;
        const bool bLast=I==Lanes.Num()-1;
        bool bLeaveAtEnd=false;
        if (!bLast) { for (int32 K=Lane.LinksBegin; K<Lane.LinksEnd; ++K)
        { bLeaveAtEnd|=S.LaneLinks[K].DestLaneIndex==Lanes[I+1] && S.LaneLinks[K].Type==EZoneLaneLinkType::Outgoing; } }
        const int32 Last=bLast ? NearestSegment(S,L,B.Position) : bLeaveAtEnd ? Lane.PointsEnd-1 : NearestSegment(S,L,At);
        for (int32 P=First; P<=Last; ++P) { Out.Add(S.LanePoints[P]); }
        At=Out.Last();
    }
    Out.Add(B.Position);
    // Then up to 4 m off the footway toward the door, at footway height.
    const FVector Off=(To-B.Position)*FVector(1,1,0);
    Out.Add(B.Position+Off.GetClampedToMaxSize(400.0));
    return true;
}

void USFTownsfolkSubsystem::Show(int32 Index, const FTransform& Transform, bool bVisible)
{
    // Placeholder body: a 0.45 x 1.75 m cylinder standing on the point; residents indoors are hidden.
    AStaticMeshActor* Body=Bodies[Index];
    Body->SetActorHiddenInGame(!bVisible);
    if (bVisible) { Body->SetActorLocationAndRotation(Transform.GetLocation()+FVector(0,0,87.5),Transform.GetRotation()); Sample=VisibleCounting ? Sample : Body->GetActorLocation(); }
    VisibleCounting+=bVisible;
}

void USFTownsfolkSubsystem::NoteFrame(double Milliseconds)
{
    VisibleNow=VisibleCounting; VisibleCounting=0;
    FrameMs+=Milliseconds; ++Frames;
    int32 Day=0; const float Hour=GameHour(Day); const double Now=Day*24.0+Hour;
    if (LastReport>=0 && Now-LastReport<1.0) { return; }
    LastReport=FMath::FloorToDouble(Now);
    // Once per game hour: where everyone is meant to be, and the processor's average cost.
    int32 Counts[4]={0,0,0,0};
    for (const FSFTownsfolkResident& R:Residents) { int32 M; ++Counts[int32(R.GoalAt(Hour,Day,M))]; }
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HOUR Day=%d Hour=%.1f Home=%d Work=%d Market=%d Outdoors=%d (one at %s) Routes=%d RouteFailures=%d ProcessorMs=%.3f"),
        Day,Hour,Counts[1],Counts[2],Counts[3],VisibleNow,*Sample.ToCompactString(),Routes_Planned,Routes_Failed,Frames ? FrameMs/Frames : 0.0);
    FrameMs=0; Frames=0;
}

void USFTownsfolkSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("TownsfolkPrototype")) || World.GetNetMode()==NM_DedicatedServer
        || World.GetOutermost()->GetName()!=ZoneMap) { return; }
    const USFTownsfolkSettings* Settings=GetDefault<USFTownsfolkSettings>();
    FString Error;
    if (!Settings->Validate(Error) || !LoadCensus(Error)) { UE_LOG(LogTemp,Error,TEXT("TOWNSFOLK_DISABLED %s"),*Error); return; }
    StartHour=Settings->StartHour; StartSeconds=World.GetTimeSeconds();
    SecondsPerGameHour=Settings->GameDayMinutes*60.0f/24.0f;
    WalkSpeedCmPerSecond=Settings->WalkSpeedMetresPerSecond*100.0f*FMath::Max(1.0f,60.0f/Settings->GameDayMinutes);

    UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Config=NewObject<UMassEntityConfigAsset>(this); Config->AddTrait(USFTownsfolkTrait::StaticClass());
    TArray<FMassEntityHandle> Entities; Routes.SetNum(Residents.Num());
    World.GetSubsystem<UMassSpawnerSubsystem>()->SpawnEntities(Config->GetOrCreateEntityTemplate(World),Residents.Num(),Entities);
    FMassEntityManager& Manager=World.GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
    for (int32 I=0; I<Entities.Num(); ++I)
    {
        TArray<FVector> Route; PlanRoute(Residents[I].Home,Residents[I].Home,Route);  // start on the footway by home
        Manager.GetFragmentDataChecked<FTransformFragment>(Entities[I]).GetMutableTransform().SetLocation(Route.Last());
        Manager.GetFragmentDataChecked<FSFTownsfolkResidentFragment>(Entities[I]).Resident=I;
        AStaticMeshActor* Body=World.SpawnActor<AStaticMeshActor>(Route.Last(),FRotator::ZeroRotator);
        Body->SetMobility(EComponentMobility::Movable); Body->GetStaticMeshComponent()->SetStaticMesh(Cylinder);
        Body->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Body->SetActorScale3D(FVector(0.45,0.45,1.75)); Body->SetActorHiddenInGame(true); Bodies.Add(Body);
    }
    bActive=true;
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_READY Residents=%d District=%s DayMinutes=%.1f"),Residents.Num(),*Settings->District,Settings->GameDayMinutes);
#endif
}

AActor* USFTownsfolkLaneLibrary::SpawnFootway(UObject* WorldContext, const TArray<FVector>& Points, bool bPolygon, FName Label)
{
    UWorld* World=WorldContext ? WorldContext->GetWorld() : nullptr;
    const FZoneLaneProfile* Profile=GetDefault<UZoneGraphSettings>()->GetLaneProfiles().FindByPredicate([](const FZoneLaneProfile& P){ return P.Name==TEXT("Footway"); });
    if (!World || !Profile || Points.Num()<2) { return nullptr; }
    TArray<FVector> Ground;
    for (const FVector& P:Points)
    {
        FHitResult Hit; FVector G=P;
        FCollisionQueryParams Params(TEXT("TownsfolkFootwayGround"),true);  // complex: the zone terrain has complex collision only
        if (P.Z!=0) { Ground.Add(P); continue; }  // a height given by the caller (the zone builder) wins
        if (World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,50000),FVector(P.X,P.Y,-50000),ECC_Visibility,Params)) { G.Z=Hit.ImpactPoint.Z+10; }
        else { UE_LOG(LogTemp,Warning,TEXT("TOWNSFOLK_FOOTWAY_NO_GROUND %s"),*P.ToCompactString()); }
        Ground.Add(G);
    }
    FVector Centre=FVector::ZeroVector; for (const FVector& G:Ground) { Centre+=G; } Centre/=Ground.Num();
    AZoneShape* Shape=World->SpawnActor<AZoneShape>();  // at the origin: shape points are then world positions
    UZoneShapeComponent* Comp=Shape ? Shape->FindComponentByClass<UZoneShapeComponent>() : nullptr;
    if (!Comp) { return nullptr; }
    Comp->SetShapeType(bPolygon ? FZoneShapeType::Polygon : FZoneShapeType::Spline);
    Comp->SetCommonLaneProfile(FZoneLaneProfileRef(*Profile));
    TArray<FZoneShapePoint>& Out=Comp->GetMutablePoints(); Out.Reset();
    for (int32 I=0; I<Ground.Num(); ++I)
    {
        FZoneShapePoint P(Ground[I]);
        if (bPolygon)
        {   // Every polygon point is an arm; its forward points into the junction.
            P.Type=FZoneShapePointType::LaneProfile;
            P.SetRotationFromForwardAndUp((Centre-Ground[I]).GetSafeNormal2D(),FVector::UpVector);
        }
        else { P.Type=FZoneShapePointType::AutoBezier; }
        Out.Add(P);
    }
    Comp->UpdateShape();
#if WITH_EDITOR
    Shape->SetActorLabel(Label.ToString()); Shape->SetFolderPath(TEXT("Townsfolk/Lanes"));
#endif
    return Shape;
}

int32 USFTownsfolkLaneLibrary::RebuildZoneGraph(UObject* WorldContext)
{
    int32 Lanes=0;
#if WITH_EDITOR
    UWorld* World=WorldContext ? WorldContext->GetWorld() : nullptr;
    UZoneGraphSubsystem* ZG=World ? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
    if (!ZG) { return 0; }
    UE::ZoneGraphDelegates::OnZoneGraphRequestRebuild.Broadcast();  // the editor's Build ZoneGraph command: a forced, synchronous rebuild
    for (const FRegisteredZoneGraphData& Data:ZG->GetRegisteredZoneGraphData())
    { if (Data.ZoneGraphData) { Lanes+=Data.ZoneGraphData->GetStorage().Lanes.Num(); } }
#endif
    return Lanes;
}
