#include "Townsfolk/SFTownsfolk.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "MassCrowdFragments.h"
#include "MassCrowdVisualizationTrait.h"
#include "MassLODTrait.h"
#include "MassSpawnerSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "ZoneGraphData.h"
#include "ZoneGraphDelegates.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneShapeActor.h"
#include "ZoneShapeComponent.h"

namespace
{
    const TCHAR* ZoneMap=TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
    constexpr float CellCm=200;  // sidestep neighbour grid
    constexpr float WalkClipCmPerSecond=150;  // ponytail: ThirdPersonWalk's stride speed, judged by eye

    FVector Metres(const TArray<TSharedPtr<FJsonValue>>& At) { return FVector(At[0]->AsNumber()*100, At[1]->AsNumber()*100, 0); }

    float Hash01(uint32 A, uint32 B) { return float(HashCombine(A, B) % 10000)/10000.0f; }

    FIntPoint Cell(const FVector& P) { return FIntPoint(FMath::FloorToInt32(P.X/CellCm),FMath::FloorToInt32(P.Y/CellCm)); }

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
        && StartHour>=0 && StartHour<24 && WalkSpeedMetresPerSecond>=0.5f && WalkSpeedMetresPerSecond<=3
        && SidestepMaxMetres>=0 && SidestepMaxMetres<=2;
    bool bLooksOk=true;
    for (const FSFTownsfolkLook& L:Looks)
    { bLooksOk=bLooksOk && !L.Archetypes.IsEmpty() && !L.Mesh.IsNull() && !L.Walk.IsNull() && !L.Idle.IsNull()
        && FMath::IsFinite(L.YawOffset) && FMath::Abs(L.YawOffset)<=180 && L.WalkClipCmPerSecond>=50 && L.WalkClipCmPerSecond<=400; }
    OutError=(bOk && bLooksOk) ? FString() : TEXT("Townsfolk prototype settings outside declared bounds.");
    return bOk && bLooksOk;
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
    BuildContext.AddFragment<FMassActorFragment>();  // the visualization trait requires it; residents never spawn actors
    BuildContext.AddTag<FMassCrowdTag>();  // MassCrowd's LOD and visualization processors are the registered ones; they select by this tag
}

ASFTownsfolkBody::ASFTownsfolkBody()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    Mesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetRelativeRotation(FRotator(0,-90,0));  // the mannequin faces +Y
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Mesh->bEnableUpdateRateOptimizations=true;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkClip(TEXT("/Game/Mannequin/Animations/ThirdPersonWalk.ThirdPersonWalk"));
    static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleClip(TEXT("/Game/Mannequin/Animations/ThirdPersonIdle.ThirdPersonIdle"));
    Mesh->SetSkeletalMeshAsset(Manny.Object); Walk=WalkClip.Object; Idle=IdleClip.Object; DefaultMesh=Manny.Object;
}

void ASFTownsfolkBody::Pose(const FVector& At, float Yaw, bool bWalking, float WalkSpeedCm, bool bIndoors, const FSFTownsfolkResolvedLook* Look)
{
    SetActorHiddenInGame(bIndoors);
    if (bIndoors) { return; }
    if (Look!=Applied)
    {   // First pose, or a recycled body now showing another archetype: swap the mesh and its clips together (the skeletons differ).
        Mesh->SetSkeletalMeshAsset(Look ? Look->Mesh : DefaultMesh.Get());
        Mesh->SetRelativeRotation(FRotator(0,Look ? Look->YawOffset : -90.0f,0));  // the mannequin faces +Y
        Applied=Look; Playing=-1;
    }
    UAnimSequence* WalkClip=Look ? Look->Walk : Walk.Get(); UAnimSequence* IdleClip=Look ? Look->Idle : Idle.Get();
    const float WalkRate=WalkSpeedCm/(Look ? Look->WalkClipCmPerSecond : WalkClipCmPerSecond);
    SetActorTransform(FTransform(FRotator(0,Yaw,0),At));  // full transform: an actor spawned for an indoor resident got its zero scale
    if (Playing!=int8(bWalking)) { Mesh->PlayAnimation(bWalking ? WalkClip : IdleClip,true); Playing=int8(bWalking); }
    Mesh->SetPlayRate(bWalking ? WalkRate : 1.0f);
}

USFTownsfolkProcessor::USFTownsfolkProcessor()
    : EntityQuery(*this)
{
    ExecutionFlags=int32(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Client);
    bRequiresGameThreadExecution=true;  // ZoneGraph queries and the subsystem's per-resident arrays
}

void USFTownsfolkProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FSFTownsfolkResidentFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
}

void USFTownsfolkProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    USFTownsfolkSubsystem* Town=Context.GetWorld() ? Context.GetWorld()->GetSubsystem<USFTownsfolkSubsystem>() : nullptr;
    if (!Town || !Town->IsActive()) { return; }
    const double Started=FPlatformTime::Seconds();
    int32 Day=0; const float Hour=Town->GameHour(Day);
    const float Dt=Context.GetDeltaTimeSeconds(), Step=Town->WalkSpeedCm()*Dt;
    Town->BeginFrame();
    EntityQuery.ForEachEntityChunk(Context,[Town,Hour,Day,Step,Dt](FMassExecutionContext& Context)
    {
        const TArrayView<FTransformFragment> Transforms=Context.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FSFTownsfolkResidentFragment> Walkers=Context.GetMutableFragmentView<FSFTownsfolkResidentFragment>();
        const TArrayView<FMassActorFragment> Bodies=Context.GetMutableFragmentView<FMassActorFragment>();
        for (FMassExecutionContext::FEntityIterator It=Context.CreateEntityIterator(); It; ++It)
        {
            FSFTownsfolkResidentFragment& W=Walkers[It]; FTransform& T=Transforms[It].GetMutableTransform();
            const FSFTownsfolkResident& R=Town->Resident(W.Resident); TArray<FVector>& Route=Town->Route(W.Resident);
            int32 Market=INDEX_NONE; const ESFTownsfolkGoal Goal=R.GoalAt(Hour,Day,Market);
            if (Goal!=W.Goal || Market!=W.Market)
            {
                const FVector To=Goal==ESFTownsfolkGoal::Work ? R.Work : Goal==ESFTownsfolkGoal::Market ? R.Markets[Market] : R.Home;
                if (W.Goal!=ESFTownsfolkGoal::None || Goal!=ESFTownsfolkGoal::Home) { W.bInside=false; }  // a new world starts with everyone home
                Town->PlanRoute(W.OnRoute,To,Route); W.Next=0; W.Goal=Goal; W.Market=Market;
                if (Goal!=ESFTownsfolkGoal::Home)
                {   // Stand somewhere around the stall or workplace, 1-3 m out, not all on its one point.
                    const float Angle=2*PI*Hash01(R.Seed,uint32(Goal)*31+Market), Out=100+200*Hash01(R.Seed,uint32(Goal)*37+Market);
                    Route.Last()+=FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Out;
                }
            }
            // Walk the route: horizontal step toward the next point, height between the points.
            float Left=Step;
            while (Left>0 && W.Next<Route.Num())
            {
                const FVector At=W.OnRoute, Target=Route[W.Next]; const FVector Flat(Target.X-At.X,Target.Y-At.Y,0);
                const float D=Flat.Size();
                if (D<=Left) { W.OnRoute=Target; Left-=D; ++W.Next; continue; }
                W.OnRoute=At+Flat/D*Left+FVector(0,0,(Target.Z-At.Z)*Left/D); W.Forward=Flat/D;
                Left=0;
            }
            if (W.Next>=Route.Num() && W.Goal==ESFTownsfolkGoal::Home) { W.bInside=true; }  // indoors at home
            // Shown beside the route by the sidestep; indoors as a zero-size instance, so nobody joins or leaves the ISM.
            W.Side=W.bInside ? 0 : Town->Sidestep(W.Resident,W.Side,Dt);
            const FVector Shown=W.OnRoute+FVector(-W.Forward.Y,W.Forward.X,0)*W.Side;
            T.SetLocation(Shown); T.SetRotation(FRotator(0,W.Forward.Rotation().Yaw,0).Quaternion());
            T.SetScale3D(FVector(W.bInside ? 0 : 1));
            Town->Note(W.Resident,Shown,W.Forward,!W.bInside);
            if (ASFTownsfolkBody* Body=Cast<ASFTownsfolkBody>(Bodies[It].GetMutable()))
            { Body->Pose(Shown,W.Forward.Rotation().Yaw,W.Next<Route.Num(),Town->WalkSpeedCm(),W.bInside,Town->LookFor(R.Archetype)); }
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
    TArray<TSharedPtr<FJsonObject>> Pool;
    for (const auto& V:Census->GetArrayField(TEXT("residents")))
    { if (Settings->District.IsEmpty() || V->AsObject()->GetStringField(TEXT("district"))==Settings->District) { Pool.Add(V->AsObject()); } }
    const int32 Count=FMath::Min(Pool.Num(),Settings->ResidentCap);
    for (int32 K=0; K<Count; ++K)
    {
        const TSharedPtr<FJsonObject> O=Pool[int32(int64(K)*Pool.Num()/Count)];  // evenly through the census: every district gets its share
        FSFTownsfolkResident R; R.Id=O->GetStringField(TEXT("id")); R.Seed=GetTypeHash(R.Id); R.Archetype=FName(*O->GetStringField(TEXT("archetype")));
        R.Home=Homes.FindRef(O->GetStringField(TEXT("household")));
        const TSharedPtr<FJsonObject> Hours=O->GetObjectField(TEXT("schedule"));
        R.Wake=Hours->GetNumberField(TEXT("wake")); R.Sleep=Hours->GetNumberField(TEXT("sleep")); R.ErrandChance=Hours->GetNumberField(TEXT("errand_chance"));
        FString Work;
        if (O->TryGetStringField(TEXT("workplace"),Work) && Works.Contains(Work) && Hours->HasField(TEXT("work_start")))
        { R.bHasWork=true; R.Work=Works[Work]; R.WorkStart=Hours->GetNumberField(TEXT("work_start")); R.WorkEnd=Hours->GetNumberField(TEXT("work_end")); }
        for (const auto& M:O->GetArrayField(TEXT("markets"))) { if (const FVector* At=Markets.Find(M->AsString())) { R.Markets.Add(*At); } }
        Residents.Add(R);
    }
    if (Residents.IsEmpty()) { OutError=FString::Printf(TEXT("No census residents in '%s'"),*Settings->District); return false; }
    return true;
}

bool USFTownsfolkSubsystem::LoadLooks(FString& OutError)
{
    for (const FSFTownsfolkLook& L:GetDefault<USFTownsfolkSettings>()->Looks)
    {
        FSFTownsfolkResolvedLook Look;
        Look.Mesh=Cast<USkeletalMesh>(L.Mesh.TryLoad()); Look.Walk=Cast<UAnimSequence>(L.Walk.TryLoad()); Look.Idle=Cast<UAnimSequence>(L.Idle.TryLoad());
        Look.YawOffset=L.YawOffset; Look.WalkClipCmPerSecond=L.WalkClipCmPerSecond;
        if (!Look.Mesh || !Look.Walk || !Look.Idle) { OutError=FString::Printf(TEXT("Townsfolk look assets missing: %s"),*L.Mesh.ToString()); return false; }
        LookAssets.Add(Look.Mesh); LookAssets.Add(Look.Walk); LookAssets.Add(Look.Idle);
        for (const FName& A:L.Archetypes) { Looks.Add(A,Look); }
    }
    return true;
}

bool USFTownsfolkSubsystem::PlanRoute(const FVector& From, const FVector& To, TArray<FVector>& Out) const
{
    // Shortest lane path: along lanes (outgoing links) or across the street (adjacent links), so dead ends and
    // one-way lane directions never strand anyone. ponytail: Dijkstra over the town's lanes per request; cache per
    // destination if goal changes ever bunch up into frame spikes.
    Out.Reset(); ++Routes_Planned;
    const UZoneGraphSubsystem* ZG=GetWorld()->GetSubsystem<UZoneGraphSubsystem>();
    const FVector Ext(25000,25000,6000); FZoneGraphLaneLocation A,B; float D=0;  // places off the footway network join it at its nearest point
    const auto Fail=[&](const TCHAR* Why)
    {
        if (Routes_Failed++<12) { UE_LOG(LogTemp,Warning,TEXT("TOWNSFOLK_ROUTE_FAILED %s from %s to %s (lanes %d -> %d)"),Why,*From.ToCompactString(),*To.ToCompactString(),A.LaneHandle.Index,B.LaneHandle.Index); }
        Out.Reset(); Out.Add(FVector(To.X,To.Y,From.Z)); return false;
    };
    if (!ZG || !ZG->FindNearestLane(FBox(From-Ext,From+Ext),FZoneGraphTagFilter(),A,D)
        || !ZG->FindNearestLane(FBox(To-Ext,To+Ext),FZoneGraphTagFilter(),B,D)) { return Fail(TEXT("no-lane")); }
    if (A.LaneHandle.DataHandle!=B.LaneHandle.DataHandle) { return Fail(TEXT("other-graph")); }
    const FZoneGraphStorage& S=ZG->GetZoneGraphData(A.LaneHandle.DataHandle)->GetStorage();
    const int32 N=S.Lanes.Num(); TArray<double> Cost; Cost.Init(TNumericLimits<double>::Max(),N);
    TArray<int32> Prev; Prev.Init(INDEX_NONE,N); TArray<bool> Done; Done.Init(false,N);
    const auto Length=[&S](int32 L){ return FVector::Dist(S.LanePoints[S.Lanes[L].PointsBegin],S.LanePoints[S.Lanes[L].PointsEnd-1]); };
    struct FOpen { double Cost; int32 Lane; bool operator<(const FOpen& O) const { return Cost<O.Cost; } };
    TArray<FOpen> Open; Open.HeapPush({0.0,A.LaneHandle.Index}); Cost[A.LaneHandle.Index]=0;
    while (!Open.IsEmpty())
    {
        FOpen O; Open.HeapPop(O,EAllowShrinking::No);
        const int32 U=O.Lane;
        if (Done[U]) { continue; }
        Done[U]=true;
        if (U==B.LaneHandle.Index) { break; }
        for (int32 K=S.Lanes[U].LinksBegin; K<S.Lanes[U].LinksEnd; ++K)
        {
            const FZoneLaneLinkData& Link=S.LaneLinks[K];
            const double Step=Link.Type==EZoneLaneLinkType::Outgoing ? Length(U) : Link.Type==EZoneLaneLinkType::Adjacent ? 300.0 : -1;
            if (Step>=0 && Cost[U]+Step<Cost[Link.DestLaneIndex])
            { Cost[Link.DestLaneIndex]=Cost[U]+Step; Prev[Link.DestLaneIndex]=U; Open.HeapPush({Cost[Link.DestLaneIndex],Link.DestLaneIndex}); }
        }
    }
    TArray<int32> Lanes;
    for (int32 L=B.LaneHandle.Index; L!=INDEX_NONE; L=Prev[L]) { Lanes.Insert(L,0); }
    if (Lanes[0]!=A.LaneHandle.Index) { return Fail(TEXT("unreachable")); }
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

void USFTownsfolkSubsystem::BeginFrame()
{
    Cells.Reset();
    for (int32 I=0; I<Residents.Num(); ++I) { if (Outdoors[I]) { Cells.FindOrAdd(Cell(Shown[I])).Add(I); } }
    for (const TPair<FIntPoint,TArray<int32>>& C:Cells) { for (const int32 I:C.Value) { for (int32 X=-1; X<=1; ++X) { for (int32 Y=-1; Y<=1; ++Y)
    { if (const TArray<int32>* In=Cells.Find(C.Key+FIntPoint(X,Y))) { for (const int32 J:*In) { ClosePairs+=J>I && FVector::DistSquared2D(Shown[I],Shown[J])<60.0*60.0; } } } } } }
    const APlayerController* PC=GetWorld()->GetFirstPlayerController();
    Player=PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector(TNumericLimits<float>::Max());
}

float USFTownsfolkSubsystem::Sidestep(int32 I, float Side, float DeltaSeconds) const
{
    // Steer the offset, never the route: push away from anyone within 1.2 m (the player within 2 m), keep right of
    // anyone coming the other way, and drift back onto the footway once clear. Reads last frame's positions.
    const FVector P=Shown[I], F=Facing[I], R(-F.Y,F.X,0);
    float Push=0;
    const auto Away=[&](const FVector& Q, const FVector& QF, float Radius, float Weight, float Tie)
    {
        const FVector D=(P-Q)*FVector(1,1,0); const float Dist=D.Size();
        if (Dist<Radius) { Push+=Weight*(1-Dist/Radius)*(Dist>1 ? float(FVector::DotProduct(D/Dist,R)) : Tie); }  // on one spot: the pair split
        const float Ahead=-FVector::DotProduct(D,F);
        if (Ahead>0 && Ahead<CellCm && FMath::Abs(FVector::DotProduct(D,R))<100 && FVector::DotProduct(F,QF)<-0.5f) { Push+=0.5f*Weight; }
    };
    const FIntPoint C=Cell(P);
    for (int32 X=-1; X<=1; ++X) { for (int32 Y=-1; Y<=1; ++Y)
    { if (const TArray<int32>* In=Cells.Find(C+FIntPoint(X,Y))) { for (const int32 J:*In) { if (J!=I) { Away(Shown[J],Facing[J],120,1,I<J ? 1.0f : -1.0f); } } } } }
    Away(Player,FVector::ZeroVector,200,2,1);
    const float Rate=WalkSpeedCmPerSecond*DeltaSeconds;
    Side=FMath::Abs(Push)>0.01f ? Side+FMath::Clamp(Push,-1.0f,1.0f)*0.6f*Rate : FMath::FInterpConstantTo(Side,0,1,0.3f*Rate);
    return FMath::Clamp(Side,-SidestepMaxCm,SidestepMaxCm);
}

void USFTownsfolkSubsystem::Note(int32 Index, const FVector& At, const FVector& Forward, bool bOutdoors)
{
    Shown[Index]=At; Facing[Index]=Forward; Outdoors[Index]=bOutdoors;
}

TArray<FVector> USFTownsfolkSubsystem::OutdoorPositions() const
{
    TArray<FVector> Out;
    for (int32 I=0; I<Residents.Num(); ++I) { if (Outdoors[I]) { Out.Add(Shown[I]); } }
    return Out;
}

void USFTownsfolkSubsystem::NoteFrame(double Milliseconds)
{
    FrameMs+=Milliseconds; ++Frames;
    int32 Day=0; const float Hour=GameHour(Day); const double Now=Day*24.0+Hour;
    if (LastReport>=0 && Now-LastReport<1.0) { return; }
    LastReport=FMath::FloorToDouble(Now);
    // Once per game hour: where everyone is meant to be, who is outdoors, close passes, and the processor's cost.
    int32 Counts[4]={0,0,0,0};
    for (const FSFTownsfolkResident& R:Residents) { int32 M; ++Counts[int32(R.GoalAt(Hour,Day,M))]; }
    const TArray<FVector> Out=OutdoorPositions();
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HOUR Day=%d Hour=%.1f Home=%d Work=%d Market=%d Outdoors=%d (one at %s) ClosePairFrames=%lld Routes=%d RouteFailures=%d ProcessorMs=%.3f"),
        Day,Hour,Counts[1],Counts[2],Counts[3],Out.Num(),Out.IsEmpty() ? TEXT("-") : *Out[0].ToCompactString(),ClosePairs,Routes_Planned,Routes_Failed,Frames ? FrameMs/Frames : 0.0);
    FrameMs=0; Frames=0; ClosePairs=0;
}

void USFTownsfolkSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
#if !UE_BUILD_SHIPPING
    if (!FParse::Param(FCommandLine::Get(),TEXT("TownsfolkPrototype")) || World.GetNetMode()==NM_DedicatedServer
        || World.GetOutermost()->GetName()!=ZoneMap) { return; }
    const USFTownsfolkSettings* Settings=GetDefault<USFTownsfolkSettings>();
    FString Error;
    if (!Settings->Validate(Error) || !LoadCensus(Error) || !LoadLooks(Error)) { UE_LOG(LogTemp,Error,TEXT("TOWNSFOLK_DISABLED %s"),*Error); return; }
    StartHour=Settings->StartHour; StartSeconds=World.GetTimeSeconds();
    SecondsPerGameHour=Settings->GameDayMinutes*60.0f/24.0f;
    WalkSpeedCmPerSecond=Settings->WalkSpeedMetresPerSecond*100.0f*FMath::Max(1.0f,60.0f/Settings->GameDayMinutes);
    SidestepMaxCm=Settings->SidestepMaxMetres*100.0f;

    // Bodies: MassRepresentation spawns a mannequin actor (ASFTownsfolkBody) for each resident within 150 m, nearest
    // first up to 120 actors; past that nobody is drawn (a 1.8 m figure is about 10 px there).
    Config=NewObject<UMassEntityConfigAsset>(this);
    UMassCrowdVisualizationTrait* Look=NewObject<UMassCrowdVisualizationTrait>(Config);  // AddTrait(Class) is editor-only
    Config->GetMutableConfig().AddTrait(*NewObject<USFTownsfolkTrait>(Config));
    Config->GetMutableConfig().AddTrait(*NewObject<UMassLODCollectorTrait>(Config));
    Config->GetMutableConfig().AddTrait(*Look);
    Look->HighResTemplateActor=Look->LowResTemplateActor=ASFTownsfolkBody::StaticClass();
    const EMassRepresentationType Shows[EMassLOD::Max]={EMassRepresentationType::HighResSpawnedActor,EMassRepresentationType::LowResSpawnedActor,
        EMassRepresentationType::None,EMassRepresentationType::None};
    const float From[EMassLOD::Max]={0,4000,15000,15000}; const int32 Most[EMassLOD::Max]={40,80,MAX_int32,MAX_int32};
    for (int32 L=0; L<EMassLOD::Max; ++L)
    {
        Look->Params.LODRepresentation[L]=Shows[L];
        Look->LODParams.BaseLODDistance[L]=Look->LODParams.VisibleLODDistance[L]=From[L]; Look->LODParams.LODMaxCount[L]=Most[L];
    }

    TArray<FMassEntityHandle> Entities; Routes.SetNum(Residents.Num());
    Shown.Init(FVector::ZeroVector,Residents.Num()); Facing.Init(FVector::ForwardVector,Residents.Num()); Outdoors.Init(false,Residents.Num());
    World.GetSubsystem<UMassSpawnerSubsystem>()->SpawnEntities(Config->GetOrCreateEntityTemplate(World),Residents.Num(),Entities);
    FMassEntityManager& Manager=World.GetSubsystem<UMassEntitySubsystem>()->GetMutableEntityManager();
    for (int32 I=0; I<Entities.Num(); ++I)
    {
        TArray<FVector> Route; PlanRoute(Residents[I].Home,Residents[I].Home,Route);  // start on the footway by home
        FTransform& T=Manager.GetFragmentDataChecked<FTransformFragment>(Entities[I]).GetMutableTransform();
        T.SetLocation(Route.Last()); T.SetScale3D(FVector::ZeroVector);
        FSFTownsfolkResidentFragment& W=Manager.GetFragmentDataChecked<FSFTownsfolkResidentFragment>(Entities[I]);
        W.Resident=I; W.OnRoute=Route.Last(); Shown[I]=Route.Last();
    }
    bActive=true;
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_READY Residents=%d District=%s DayMinutes=%.1f"),Residents.Num(),
        Settings->District.IsEmpty() ? TEXT("(whole town)") : *Settings->District,Settings->GameDayMinutes);
#endif
}

AActor* USFTownsfolkLaneLibrary::SpawnFootway(UObject* WorldContext, const TArray<FVector>& Points, bool bPolygon, FName Label, FVector Centre)
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
        {   // Every polygon point is an arm; its forward points into the junction, back along the street it joins (a
            // centroid of the arms would skew bends and T-junctions past ZoneGraph's snap angle and split the network).
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

TArray<FVector> USFTownsfolkLaneLibrary::OutdoorResidents(UObject* WorldContext)
{
    const UWorld* World=WorldContext ? WorldContext->GetWorld() : nullptr;
    const USFTownsfolkSubsystem* Town=World ? World->GetSubsystem<USFTownsfolkSubsystem>() : nullptr;
    return Town && Town->IsActive() ? Town->OutdoorPositions() : TArray<FVector>();
}
