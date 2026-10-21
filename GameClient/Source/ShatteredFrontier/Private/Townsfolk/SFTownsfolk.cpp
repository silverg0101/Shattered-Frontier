#include "Townsfolk/SFTownsfolk.h"
#include "Algo/Sort.h"
#include "Animation/AnimSequence.h"
#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
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
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Net/UnrealNetwork.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectSubsystem.h"
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

    /** P on the ground (complex collision: the zone terrain has no simple collision), raised by Up cm. */
    FVector OnGround(const UWorld* World, const FVector& P, float Up, const AActor* Ignore=nullptr)
    {
        FHitResult Hit; FCollisionQueryParams Params(TEXT("TownsfolkGround"),true,Ignore);
        const float From=P.Z!=0 ? P.Z+300 : 50000;  // census points have no height: from high up
        return World->LineTraceSingleByChannel(Hit,FVector(P.X,P.Y,From),FVector(P.X,P.Y,-50000),ECC_Visibility,Params) ? Hit.ImpactPoint+FVector(0,0,Up) : P;
    }

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
        && SidestepMaxMetres>=0 && SidestepMaxMetres<=2 && SidestepRadiusMetres>=0.5f && SidestepRadiusMetres<=2
        && SidestepSpeedShare>=0.1f && SidestepSpeedShare<=2 && DoorDwellSeconds>=0.2f && DoorDwellSeconds<=10
        && WorkIndoorShare>=0 && WorkIndoorShare<=1 && BenchShare>=0 && BenchShare<=1 && BenchSearchMetres>=5 && BenchSearchMetres<=100
        && MarketOpenHour>=0 && MarketOpenHour<=24 && MarketCloseHour>=0 && MarketCloseHour<=24
        && DangerCellMetres>=10 && DangerCellMetres<=200 && DangerSeconds>=5 && DangerSeconds<=1800;
    bool bLooksOk=true;
    for (const FSFTownsfolkLook& L:Looks)
    { bLooksOk=bLooksOk && !L.Archetypes.IsEmpty() && !L.Mesh.IsNull() && !L.Walk.IsNull() && !L.Idle.IsNull()
        && FMath::IsFinite(L.YawOffset) && FMath::Abs(L.YawOffset)<=180 && L.WalkClipCmPerSecond>=50 && L.WalkClipCmPerSecond<=400; }
    OutError=(bOk && bLooksOk) ? FString() : TEXT("Townsfolk prototype settings outside declared bounds.");
    return bOk && bLooksOk;
}

ESFTownsfolkGoal FSFTownsfolkResident::GoalAt(float Hour, int32 Day, int32& OutMarket, uint32 OpenMarkets) const
{
    // One market errand on some days: after work for workers, mid-morning for everyone else (doc section 4 day blocks).
    OutMarket=INDEX_NONE;
    if (Hour<Wake || Hour>=Sleep) { return ESFTownsfolkGoal::Home; }
    if (bHasWork && Hour>=WorkStart && Hour<WorkEnd) { return ESFTownsfolkGoal::Work; }
    const float ErrandAt=bHasWork ? WorkEnd : Wake+1.0f+3.0f*Hash01(Seed,Day*7+1);
    if (!Markets.IsEmpty() && Hash01(Seed,Day)<ErrandChance && Hour>=ErrandAt && Hour<ErrandAt+1.0f)
    {
        OutMarket=int32(HashCombine(Seed,Day)%uint32(Markets.Num()));
        if (OpenMarkets>>(MarketBits[OutMarket]&31)&1) { return ESFTownsfolkGoal::Market; }
        OutMarket=INDEX_NONE;  // closed: no errand today
    }
    return ESFTownsfolkGoal::Home;
}

ASFTownState::ASFTownState()
{
    bReplicates=true; bAlwaysRelevant=true; SetNetUpdateFrequency(2.0f);  // changes force an update; the clock never sends
    PrimaryActorTick.bCanEverTick=true; PrimaryActorTick.TickInterval=0.25f;
}

void ASFTownState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFTownState,StartHour); DOREPLIFETIME(ASFTownState,StartServerSeconds); DOREPLIFETIME(ASFTownState,SecondsPerGameHour);
    DOREPLIFETIME(ASFTownState,DangerCellCm); DOREPLIFETIME(ASFTownState,OpenMarkets); DOREPLIFETIME(ASFTownState,Danger);
}

void ASFTownState::BeginPlay()
{
    Super::BeginPlay();
    SetActorTickEnabled(HasAuthority());  // clients only read
    if (USFTownsfolkSubsystem* Town=GetWorld()->GetSubsystem<USFTownsfolkSubsystem>()) { Town->SetState(this); }
}

void ASFTownState::Start(const USFTownsfolkSettings& Settings)
{
    StartHour=Settings.StartHour; StartServerSeconds=ServerSeconds(); SecondsPerGameHour=Settings.GameDayMinutes*60.0f/24.0f;
    DangerCellCm=Settings.DangerCellMetres*100.0f; OpenHour=Settings.MarketOpenHour; CloseHour=Settings.MarketCloseHour; DangerSeconds=Settings.DangerSeconds;
    Tick(0);
}

double ASFTownState::ServerSeconds() const
{
    const AGameStateBase* GS=GetWorld()->GetGameState();
    return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

float ASFTownState::Hour(int32& OutDay) const
{
    const double Hours=StartHour+(ServerSeconds()-StartServerSeconds)/SecondsPerGameHour;
    OutDay=int32(Hours/24.0);
    return float(FMath::Fmod(Hours,24.0));
}

void ASFTownState::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    int32 Day=0; const float H=Hour(Day);
    const bool bOpen=OpenHour<=CloseHour ? H>=OpenHour && H<CloseHour : H>=OpenHour || H<CloseHour;  // a close past midnight wraps
    const int32 Open=bOpen ? -1 : 0;
    const double Now=ServerSeconds();
    const int32 Expired=Danger.RemoveAll([Now](const FSFTownDangerCell& C){ return C.Until<=Now; });
    if (Open!=OpenMarkets || Expired)
    {
        OpenMarkets=Open; ForceNetUpdate();
        UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_STATE_CHANGE Utc=%.3f Day=%d Hour=%.3f OpenMarkets=%d Danger=%d"),FDateTime::UtcNow().ToUnixTimestampDecimal(),Day,H,OpenMarkets,Danger.Num());
    }
}

bool ASFTownState::IsDanger(const FVector& At) const
{
    if (Danger.IsEmpty()) { return false; }
    const FIntPoint C(FMath::FloorToInt32(At.X/DangerCellCm),FMath::FloorToInt32(At.Y/DangerCellCm)); const double Now=ServerSeconds();
    return Danger.ContainsByPredicate([&](const FSFTownDangerCell& D){ return D.Cell==C && D.Until>Now; });
}

int32 ASFTownState::DangerCount() const
{
    const double Now=ServerSeconds(); int32 N=0;
    for (const FSFTownDangerCell& D:Danger) { N+=D.Until>Now; }
    return N;
}

void ASFTownState::MarkDanger(const FVector& At)
{
    check(HasAuthority());
    const FIntPoint C(FMath::FloorToInt32(At.X/DangerCellCm),FMath::FloorToInt32(At.Y/DangerCellCm)); const double Until=ServerSeconds()+DangerSeconds;
    if (FSFTownDangerCell* D=Danger.FindByPredicate([&C](const FSFTownDangerCell& E){ return E.Cell==C; })) { D->Until=Until; }
    else { FSFTownDangerCell Added; Added.Cell=C; Added.Until=Until; Danger.Add(Added); }
    ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_STATE_CHANGE Utc=%.3f Danger=%d Cell=%d,%d"),FDateTime::UtcNow().ToUnixTimestampDecimal(),Danger.Num(),C.X,C.Y);
}

bool USFClientSmartObjectSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // The engine's subsystem refuses NM_Client; this one takes only that case, so a world never has both.
    const UWorld* World=Cast<UWorld>(Outer);
    return World && World->IsNetMode(NM_Client) && UWorldSubsystem::ShouldCreateSubsystem(Outer);
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
    if (!Town || !Town->IsActive() || !Town->HasState()) { return; }
    const double Started=FPlatformTime::Seconds();
    int32 Day=0; const float Hour=Town->GameHour(Day);
    const float Dt=Context.GetDeltaTimeSeconds(), Step=Town->WalkSpeedCm()*Dt;
    Town->BeginFrame(Dt);
    EntityQuery.ForEachEntityChunk(Context,[Town,Hour,Day,Step,Dt](FMassExecutionContext& Context)
    {
        const TArrayView<FTransformFragment> Transforms=Context.GetMutableFragmentView<FTransformFragment>();
        const TArrayView<FSFTownsfolkResidentFragment> Walkers=Context.GetMutableFragmentView<FSFTownsfolkResidentFragment>();
        const TArrayView<FMassActorFragment> Bodies=Context.GetMutableFragmentView<FMassActorFragment>();
        for (FMassExecutionContext::FEntityIterator It=Context.CreateEntityIterator(); It; ++It)
        {
            FSFTownsfolkResidentFragment& W=Walkers[It]; FTransform& T=Transforms[It].GetMutableTransform();
            const FSFTownsfolkResident& R=Town->Resident(W.Resident); TArray<FVector>& Route=Town->Route(W.Resident);
            if (Town->IsHandedOff(W.Resident))
            {   // The server's actor stands in for this resident: hide the Mass copy and let go of its slots.
                if (W.Phase!=0) { Town->ReleaseDoor(W.Resident); W.Phase=0; }
                Town->ReleaseStand(W.Resident);
                T.SetScale3D(FVector::ZeroVector); Town->Note(W.Resident,W.OnRoute,W.Forward,false);
                if (ASFTownsfolkBody* Body=Cast<ASFTownsfolkBody>(Bodies[It].GetMutable())) { Body->Pose(W.OnRoute,0,false,0,true,nullptr); }
                continue;
            }
            FVector ResumeAt;
            if (Town->TakeResume(W.Resident,ResumeAt))
            {   // Released: the schedule picks up from where the server's actor stood (no jump back to the old spot).
                W.OnRoute=ResumeAt; W.bInside=false; W.Phase=0; W.bEnter=false; W.bFace=false; W.Side=0;
                W.Goal=ESFTownsfolkGoal::None; W.Market=INDEX_NONE;  // forces a new route below
            }
            int32 Market=INDEX_NONE; const ESFTownsfolkGoal Goal=Town->GoalFor(W.Resident,Hour,Day,Market);
            if (Goal!=W.Goal || Market!=W.Market)
            {
                Town->ReleaseStand(W.Resident);  // up from the bench, away from the stall
                if (W.Phase==3) { Town->ReleaseDoor(W.Resident); W.Phase=0; }  // changed course at the door
                FVector To; bool bExact=false, bScatter=false;
                Town->PlanStop(W.Resident,Goal,Market,Day,W,To,bExact,bScatter);
                if (W.Goal!=ESFTownsfolkGoal::None || Goal!=ESFTownsfolkGoal::Home)  // a new world starts with everyone home
                {   // Leaving a building: wait inside for its door's slot when it has one.
                    if (W.bInside && W.Phase==0 && Town->DoorFor(W.Resident,W.InsideGoal)) { W.Phase=1; } else if (W.Phase!=1) { W.bInside=false; }
                }
                Town->PlanRoute(W.OnRoute,To,Route,bExact); W.Next=0; W.Goal=Goal; W.Market=Market;
                if (bScatter)
                {   // Stand somewhere around the stall or workplace, 1-3 m out, not all on its one point.
                    const float Angle=2*PI*Hash01(R.Seed,uint32(Goal)*31+Market), Out=100+200*Hash01(R.Seed,uint32(Goal)*37+Market);
                    Route.Last()+=FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Out;
                }
            }
            if (W.Phase==1)
            {   // Inside, waiting to come out: take a slot of the door, appear in front of it, hold it for the dwell.
                FVector At; float Yaw; const FSFTownsfolkSpot* Door=Town->DoorFor(W.Resident,W.InsideGoal);
                if (!Door) { W.Phase=0; W.bInside=false; }
                else if (Town->ClaimDoor(W.Resident,*Door,At,Yaw))
                { W.OnRoute=FVector(At.X,At.Y,W.OnRoute.Z); W.Forward=FRotator(0,Yaw+180,0).Vector(); W.bInside=false; W.Phase=2; W.Wait=Town->DoorDwell(); }
            }
            if (W.Phase==2 || W.Phase==3)
            {
                W.Wait-=Dt;
                if (W.Wait<=0)
                {
                    Town->ReleaseDoor(W.Resident);
                    if (W.Phase==3) { W.bInside=true; W.InsideGoal=W.Goal; W.bEnter=false; }
                    W.Phase=0;
                }
            }
            // Walk the route: horizontal step toward the next point, height between the points.
            float Left=W.Phase==0 ? Step : 0;
            while (Left>0 && W.Next<Route.Num())
            {
                const FVector At=W.OnRoute, Target=Route[W.Next]; const FVector Flat(Target.X-At.X,Target.Y-At.Y,0);
                const float D=Flat.Size();
                if (D<=Left) { W.OnRoute=Target; Left-=D; ++W.Next; continue; }
                W.OnRoute=At+Flat/D*Left+FVector(0,0,(Target.Z-At.Z)*Left/D); W.Forward=Flat/D;
                Left=0;
            }
            const bool bArrived=W.Next>=Route.Num();
            if (bArrived && W.Phase==0 && W.Goal==ESFTownsfolkGoal::Home && !W.bEnter) { W.bInside=true; }  // indoors at home, no door to go through
            if (bArrived && W.Phase==0 && W.bEnter && !W.bInside)
            {   // At the door: take a slot, face the door, go in when the dwell ends.
                FVector At; float Yaw; const FSFTownsfolkSpot* Door=Town->DoorFor(W.Resident,W.Goal);
                if (!Door) { W.bEnter=false; }
                else if (Town->ClaimDoor(W.Resident,*Door,At,Yaw))
                { W.OnRoute=FVector(At.X,At.Y,W.OnRoute.Z); W.Forward=FRotator(0,Yaw,0).Vector(); W.Phase=3; W.Wait=Town->DoorDwell(); }
            }
            if (bArrived && W.Phase==0 && W.bFace) { W.Forward=FRotator(0,W.FaceYaw,0).Vector(); }  // facing the stall, or seated on the bench's front
            // Shown beside the route by the sidestep; indoors as a zero-size instance, so nobody joins or leaves the ISM.
            // At a door, stall or bench they hold their slot.
            const bool bHolding=W.Phase!=0 || (bArrived && W.bFace);
            W.Side=W.bInside ? 0 : bHolding ? FMath::FInterpConstantTo(W.Side,0,1,Town->WalkSpeedCm()*Dt) : Town->Sidestep(W.Resident,W.Side,Dt);
            const FVector Shown=W.OnRoute+FVector(-W.Forward.Y,W.Forward.X,0)*W.Side;
            T.SetLocation(Shown); T.SetRotation(FRotator(0,W.Forward.Rotation().Yaw,0).Quaternion());
            T.SetScale3D(FVector(W.bInside ? 0 : 1));
            Town->Note(W.Resident,Shown,W.Forward,!W.bInside);
            if (ASFTownsfolkBody* Body=Cast<ASFTownsfolkBody>(Bodies[It].GetMutable()))
            { Body->Pose(Shown,W.Forward.Rotation().Yaw,W.Phase==0 && !bArrived,Town->WalkSpeedCm(),W.bInside,Town->LookFor(R.Archetype)); }
        }
    });
    Town->NoteFrame((FPlatformTime::Seconds()-Started)*1000.0);
}

bool USFTownsfolkSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World=Cast<UWorld>(Outer);
    return World && (World->WorldType==EWorldType::Game || World->WorldType==EWorldType::PIE);
}

ESFTownsfolkGoal USFTownsfolkSubsystem::GoalFor(int32 Resident, float Hour, int32 Day, int32& OutMarket) const
{
    const FSFTownsfolkResident& R=Residents[Resident];
    const ESFTownsfolkGoal Goal=R.GoalAt(Hour,Day,OutMarket,State->OpenMask());
    const FVector& To=Goal==ESFTownsfolkGoal::Work ? R.Work : Goal==ESFTownsfolkGoal::Market ? R.Markets[OutMarket] : R.Home;
    if (Goal!=ESFTownsfolkGoal::Home && State->IsDanger(To)) { OutMarket=INDEX_NONE; return ESFTownsfolkGoal::Home; }  // ponytail: by destination only; fleeing from where they stand comes with section 5.4 reactions
    return Goal;
}

bool USFTownsfolkSubsystem::LoadCensus(FString& OutError)
{
    const USFTownsfolkSettings* Settings=GetDefault<USFTownsfolkSettings>();
    const FString Path=FPaths::Combine(FPaths::ProjectDir(),TEXT("ArtSource/Greenlands/silverbrook-census.json"));
    FString Text; TSharedPtr<FJsonObject> Census;
    if (!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Census) || !Census.IsValid())
    { OutError=FString::Printf(TEXT("Census missing or unreadable: %s"),*Path); return false; }
    TMap<FString,FVector> Homes, Works, Markets; TMap<FString,FString> WorkLabels; TMap<FString,int32> MarketBit;
    for (const auto& V:Census->GetArrayField(TEXT("households"))) { Homes.Add(V->AsObject()->GetStringField(TEXT("id")),Metres(V->AsObject()->GetArrayField(TEXT("at")))); }
    for (const auto& V:Census->GetArrayField(TEXT("workplaces")))
    { Works.Add(V->AsObject()->GetStringField(TEXT("id")),Metres(V->AsObject()->GetArrayField(TEXT("at")))); WorkLabels.Add(V->AsObject()->GetStringField(TEXT("id")),V->AsObject()->GetStringField(TEXT("label"))); }
    for (const auto& V:Census->GetArrayField(TEXT("markets")))
    { MarketBit.Add(V->AsObject()->GetStringField(TEXT("label")),MarketBit.Num()); Markets.Add(V->AsObject()->GetStringField(TEXT("label")),Metres(V->AsObject()->GetArrayField(TEXT("at")))); }
    TArray<TSharedPtr<FJsonObject>> Pool;
    for (const auto& V:Census->GetArrayField(TEXT("residents")))
    { if (Settings->District.IsEmpty() || V->AsObject()->GetStringField(TEXT("district"))==Settings->District) { Pool.Add(V->AsObject()); } }
    const int32 Count=FMath::Min(Pool.Num(),Settings->ResidentCap);
    for (int32 K=0; K<Count; ++K)
    {
        const TSharedPtr<FJsonObject> O=Pool[int32(int64(K)*Pool.Num()/Count)];  // evenly through the census: every district gets its share
        FSFTownsfolkResident R; R.Id=O->GetStringField(TEXT("id")); R.Seed=GetTypeHash(R.Id); R.Archetype=FName(*O->GetStringField(TEXT("archetype")));
        R.Home=Homes.FindRef(O->GetStringField(TEXT("household"))); R.HomeLabel=O->GetStringField(TEXT("home"));
        const TSharedPtr<FJsonObject> Hours=O->GetObjectField(TEXT("schedule"));
        R.Wake=Hours->GetNumberField(TEXT("wake")); R.Sleep=Hours->GetNumberField(TEXT("sleep")); R.ErrandChance=Hours->GetNumberField(TEXT("errand_chance"));
        FString Work;
        if (O->TryGetStringField(TEXT("workplace"),Work) && Works.Contains(Work) && Hours->HasField(TEXT("work_start")))
        { R.bHasWork=true; R.Work=Works[Work]; R.WorkLabel=WorkLabels.FindRef(Work); R.WorkStart=Hours->GetNumberField(TEXT("work_start")); R.WorkEnd=Hours->GetNumberField(TEXT("work_end")); }
        for (const auto& M:O->GetArrayField(TEXT("markets"))) { if (const FVector* At=Markets.Find(M->AsString())) { R.Markets.Add(*At); R.MarketLabels.Add(M->AsString()); R.MarketBits.Add(MarketBit[M->AsString()]); } }
        Ids.Add(R.Id,Residents.Num()); Residents.Add(R);
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

bool USFTownsfolkSubsystem::PlanRoute(const FVector& From, const FVector& To, TArray<FVector>& Out, bool bExact) const
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
    if (Lanes[0]!=A.LaneHandle.Index)
    {
        if (Routes_Failed<=2)
        {   // Why: how much of the network the start reaches, and how near its lane ends come to lanes it does reach.
            int32 Reached=0; for (int32 L=0; L<N; ++L) { Reached+=Done[L] || Cost[L]<TNumericLimits<double>::Max(); }
            const FZoneLaneData& LA=S.Lanes[A.LaneHandle.Index];
            double Gap=TNumericLimits<double>::Max(); int32 Near=INDEX_NONE; FVector Ends[2]={S.LanePoints[LA.PointsBegin],S.LanePoints[LA.PointsEnd-1]};
            for (int32 L=0; L<N; ++L) { if (Cost[L]<TNumericLimits<double>::Max()) { for (const FVector& E:Ends) { for (const int32 P:{S.Lanes[L].PointsBegin,S.Lanes[L].PointsEnd-1})
            { const double Dg=FVector::Dist(E,S.LanePoints[P]); if (Dg<Gap) { Gap=Dg; Near=L; } } } } }
            UE_LOG(LogTemp,Warning,TEXT("TOWNSFOLK_ROUTE_DIAG lane %d (%s to %s, %d links) reaches %d of %d lanes; nearest reached lane %d is %.0f cm from its ends"),
                A.LaneHandle.Index,*Ends[0].ToCompactString(),*Ends[1].ToCompactString(),LA.LinksEnd-LA.LinksBegin,Reached,N,Near,Gap);
        }
        return Fail(TEXT("unreachable"));
    }
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
    // Then off the footway toward the door (up to 4 m, or all the way to a Smart Object), at footway height.
    const FVector Off=(To-B.Position)*FVector(1,1,0);
    Out.Add(B.Position+(bExact ? Off : Off.GetClampedToMaxSize(400.0)));
    return true;
}

void USFTownsfolkSubsystem::BeginFrame(float DeltaSeconds)
{
    LastDelta=DeltaSeconds; EnsureSpots();
    Cells.Reset();
    for (int32 I=0; I<Residents.Num(); ++I) { if (Outdoors[I]) { Cells.FindOrAdd(Cell(Shown[I])).Add(I); } }
    for (const TPair<FIntPoint,TArray<int32>>& C:Cells) { for (const int32 I:C.Value) { for (int32 X=-1; X<=1; ++X) { for (int32 Y=-1; Y<=1; ++Y)
    { if (const TArray<int32>* In=Cells.Find(C.Key+FIntPoint(X,Y))) { for (const int32 J:*In) { const bool bClose=J>I && FVector::DistSquared2D(Shown[I],Shown[J])<60.0*60.0; ClosePairs+=bClose; ClosePairSeconds+=bClose ? DeltaSeconds : 0.0; } } } } } }
    const APlayerController* PC=GetWorld()->GetFirstPlayerController();
    Player=PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : FVector(TNumericLimits<float>::Max());
    TickScenario();
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
    { if (const TArray<int32>* In=Cells.Find(C+FIntPoint(X,Y))) { for (const int32 J:*In) { if (J!=I) { Away(Shown[J],Facing[J],SidestepRadiusCm,1,I<J ? 1.0f : -1.0f); } } } } }
    Away(Player,FVector::ZeroVector,200,2,1);
    const float Rate=WalkSpeedCm()*DeltaSeconds;
    Side=FMath::Abs(Push)>0.01f ? Side+FMath::Clamp(Push,-1.0f,1.0f)*SidestepSpeedShare*Rate : FMath::FInterpConstantTo(Side,0,1,0.3f*Rate);
    return FMath::Clamp(Side,-SidestepMaxCm,SidestepMaxCm);
}

void USFTownsfolkSubsystem::Note(int32 Index, const FVector& At, const FVector& Forward, bool bOutdoors)
{
    Still[Index]=FVector::DistSquared(At,Shown[Index])<1.0 ? Still[Index]+1 : 0;
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
    const double Real=FPlatformTime::Seconds();
    if (Real-LastSample>=1.0)
    {   // Once a real second, stamped with the machine's clock, so runs on one machine can compare their hours and markets.
        LastSample=Real;
        UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_STATE Net=%d Utc=%.3f Day=%d Hour=%.4f OpenMarkets=%d Danger=%d"),int32(GetWorld()->GetNetMode()),
            FDateTime::UtcNow().ToUnixTimestampDecimal(),Day,Hour,int32(State->OpenMask()),State->DangerCount());
        for (const TPair<int32,TWeakObjectPtr<ASFTownsfolkNpc>>& P:Npcs)
        {   // MassOutdoors must stay 0: the resident is never shown twice.
            if (const ASFTownsfolkNpc* N=P.Value.Get())
            { UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_VIEW Id=%s Serial=%d Health=%d Hidden=%d MassOutdoors=%d At=%s"),*N->Id(),N->Serial(),N->Health(),N->IsHidden(),Outdoors[P.Key],*N->GetActorLocation().ToCompactString()); }
        }
        if (LastReturned!=INDEX_NONE && Real-LastReturnedAt<15)
        { UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_AFTER Id=%s Outdoors=%d HandedOff=%d At=%s"),*Residents[LastReturned].Id,Outdoors[LastReturned],IsHandedOff(LastReturned),*Shown[LastReturned].ToCompactString()); }
    }
    if (LastReport>=0 && Now-LastReport<1.0) { return; }
    LastReport=FMath::FloorToDouble(Now);
    // Once per game hour: where everyone is meant to be, who is outdoors, close passes, and the processor's cost.
    int32 Counts[4]={0,0,0,0};
    for (int32 I=0; I<Residents.Num(); ++I) { int32 M; ++Counts[int32(GoalFor(I,Hour,Day,M))]; }
    const TArray<FVector> Out=OutdoorPositions();
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HOUR Day=%d Hour=%.1f Home=%d Work=%d Market=%d Outdoors=%d (one at %s) ClosePairFrames=%lld ClosePairSeconds=%.1f Routes=%d RouteFailures=%d ProcessorMs=%.3f DoorClaims=%d StallClaims=%d BenchClaims=%d ClaimFailures=%d"),
        Day,Hour,Counts[1],Counts[2],Counts[3],Out.Num(),Out.IsEmpty() ? TEXT("-") : *Out[0].ToCompactString(),ClosePairs,ClosePairSeconds,Routes_Planned,Routes_Failed,Frames ? FrameMs/Frames : 0.0,
        DoorClaims,StallClaims,BenchClaims,ClaimFailures);
    FrameMs=0; Frames=0; ClosePairs=0; ClosePairSeconds=0; DoorClaims=StallClaims=BenchClaims=ClaimFailures=0;
}

USmartObjectDefinition* USFTownsfolkSubsystem::MakeDefinition(const TArray<FVector>& Offsets, float YawDegrees)
{
    // Slots only; residents are not driven by behavior definitions, so the definition is never Validate()d.
    USmartObjectDefinition* Def=NewObject<USmartObjectDefinition>(this);
    for (const FVector& O:Offsets)
    { FSmartObjectSlotDefinition& Slot=Def->DebugAddSlot(); Slot.Offset=FVector3f(O); Slot.Rotation=FRotator3f(0,YawDegrees,0); }
    SpotDefinitions.Add(Def);
    return Def;
}

void USFTownsfolkSubsystem::EnsureSpots()
{
    if (bSpotsTried) { return; }
    USmartObjectSubsystem* SO=GetWorld()->GetSubsystem<USmartObjectSubsystem>();
    if (!SO) { return; }
    bSpotsTried=true;
    FString Text; TSharedPtr<FJsonObject> Root;
    const FString Path=FPaths::Combine(FPaths::ProjectDir(),TEXT("ArtSource/Greenlands/silverbrook-smartobjects.json"));
    if (!FFileHelper::LoadFileToString(Text,*Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid())
    { UE_LOG(LogTemp,Warning,TEXT("TOWNSFOLK_SPOTS none: %s missing or unreadable"),*Path); return; }
    // Slot offsets are in the object's frame, X along the way it faces (cm). Door and stall slots look back at the object; a bench's look out.
    USmartObjectDefinition* Defs[3]={ MakeDefinition({FVector(120,-45,0),FVector(120,45,0)},180), MakeDefinition({FVector(120,-120,0),FVector(120,0,0),FVector(120,120,0)},180),
        MakeDefinition({FVector(0,-45,0),FVector(0,45,0)},0) };
    for (const auto& V:Root->GetArrayField(TEXT("objects")))
    {
        const TSharedPtr<FJsonObject> O=V->AsObject(); const FString Kind=O->GetStringField(TEXT("kind"));
        FSFTownsfolkSpot Spot; Spot.At=Metres(O->GetArrayField(TEXT("at"))); Spot.Facing=O->GetNumberField(TEXT("facing"));
        const int32 K=Kind==TEXT("door") ? 0 : Kind==TEXT("stall") ? 1 : 2;
        // Z is 0: residents keep the footway height and use only a slot's X and Y.
        Spot.Handle=SO->CreateSmartObject(*Defs[K],FTransform(FRotator(0,Spot.Facing,0),Spot.At),FConstStructView());
        if (!Spot.Handle.IsValid()) { continue; }
        if (K==0) { Doors.Add(O->GetStringField(TEXT("owner")),Spot); } else if (K==1) { Stalls.Add(O->GetStringField(TEXT("owner")),Spot); } else { Benches.Add(Spot); }
    }
    bSpots=!Doors.IsEmpty();
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_SPOTS Doors=%d Stalls=%d Benches=%d"),Doors.Num(),Stalls.Num(),Benches.Num());
}

const FSFTownsfolkSpot* USFTownsfolkSubsystem::DoorFor(int32 Resident, ESFTownsfolkGoal Building) const
{
    const FSFTownsfolkResident& R=Residents[Resident];
    return Building==ESFTownsfolkGoal::Home ? Doors.Find(R.HomeLabel) : Building==ESFTownsfolkGoal::Work ? Doors.Find(R.WorkLabel) : nullptr;
}

bool USFTownsfolkSubsystem::Claim(int32 Resident, ESFTownsfolkSpot Kind, const FSFTownsfolkSpot& Spot, FVector& OutAt, float& OutYaw)
{
    USmartObjectSubsystem* SO=GetWorld()->GetSubsystem<USmartObjectSubsystem>();
    FSmartObjectClaimHandle& Hold=Kind==ESFTownsfolkSpot::Door ? DoorHolds[Resident] : StandHolds[Resident];
    if (!SO) { return false; }
    if (Hold.IsValid()) { SO->MarkSlotAsFree(Hold); Hold=FSmartObjectClaimHandle::InvalidHandle; }
    TArray<FSmartObjectSlotHandle> Slots; SO->GetAllSlots(Spot.Handle,Slots);
    const int32 Start=Slots.IsEmpty() ? 0 : int32(Hash01(Residents[Resident].Seed,uint32(Resident)*7+uint32(Kind))*Slots.Num())%Slots.Num();  // not everyone takes slot 0
    for (int32 K=0; K<Slots.Num(); ++K)
    {
        const FSmartObjectSlotHandle& Slot=Slots[(Start+K)%Slots.Num()];
        if (!SO->CanBeClaimed(Slot)) { continue; }
        const FSmartObjectClaimHandle Claimed=SO->MarkSlotAsClaimed(Slot,ESmartObjectClaimPriority::Normal);
        FTransform At;
        if (Claimed.IsValid() && SO->GetSlotTransform(Claimed,At))
        {
            Hold=Claimed; OutAt=At.GetLocation(); OutYaw=At.Rotator().Yaw;
            (Kind==ESFTownsfolkSpot::Door ? DoorClaims : Kind==ESFTownsfolkSpot::Stall ? StallClaims : BenchClaims)++;
            return true;
        }
    }
    return false;
}

bool USFTownsfolkSubsystem::ClaimDoor(int32 Resident, const FSFTownsfolkSpot& Door, FVector& OutAt, float& OutYaw)
{
    return Claim(Resident,ESFTownsfolkSpot::Door,Door,OutAt,OutYaw);
}

void USFTownsfolkSubsystem::ReleaseDoor(int32 Resident)
{
    USmartObjectSubsystem* SO=GetWorld()->GetSubsystem<USmartObjectSubsystem>();
    if (SO && DoorHolds[Resident].IsValid()) { SO->MarkSlotAsFree(DoorHolds[Resident]); }
    DoorHolds[Resident]=FSmartObjectClaimHandle::InvalidHandle;
}

void USFTownsfolkSubsystem::ReleaseStand(int32 Resident)
{
    USmartObjectSubsystem* SO=GetWorld()->GetSubsystem<USmartObjectSubsystem>();
    if (SO && StandHolds[Resident].IsValid()) { SO->MarkSlotAsFree(StandHolds[Resident]); }
    StandHolds[Resident]=FSmartObjectClaimHandle::InvalidHandle;
}

void USFTownsfolkSubsystem::PlanStop(int32 Resident, ESFTownsfolkGoal Goal, int32 Market, int32 Day, FSFTownsfolkResidentFragment& W, FVector& OutTo, bool& bOutExact, bool& bOutScatter)
{
    // Where this goal sends a resident, by seeded choices: a door to go in through (home always, work for some), a free
    // stall slot, or a bench near the market. Anything not found keeps the census point, scattered 1-3 m around it.
    const FSFTownsfolkResident& R=Residents[Resident];
    OutTo=Goal==ESFTownsfolkGoal::Work ? R.Work : Goal==ESFTownsfolkGoal::Market ? R.Markets[Market] : R.Home;
    bOutExact=false; bOutScatter=Goal!=ESFTownsfolkGoal::Home; W.bEnter=false; W.bFace=false;
    if (!bSpots) { return; }
    if (Goal==ESFTownsfolkGoal::Home || (Goal==ESFTownsfolkGoal::Work && Hash01(R.Seed,Day*13+5)<WorkIndoorShare))
    {
        if (const FSFTownsfolkSpot* Door=DoorFor(Resident,Goal))
        { OutTo=Door->At+FRotator(0,Door->Facing,0).Vector()*120; bOutExact=true; bOutScatter=false; W.bEnter=true; }
        return;
    }
    if (Goal!=ESFTownsfolkGoal::Market) { return; }
    FVector At; float Yaw;
    if (Hash01(R.Seed,Day*17+Market*3+7)<BenchShare)
    {   // Benches within reach of the market, nearest first; the first with a free slot.
        TArray<const FSFTownsfolkSpot*> Near;
        for (const FSFTownsfolkSpot& B:Benches) { if (FVector::Dist2D(B.At,OutTo)<=BenchSearchCm) { Near.Add(&B); } }
        Algo::Sort(Near,[&OutTo](const FSFTownsfolkSpot* A, const FSFTownsfolkSpot* B){ return FVector::DistSquared2D(A->At,OutTo)<FVector::DistSquared2D(B->At,OutTo); });
        for (const FSFTownsfolkSpot* B:Near)
        { if (Claim(Resident,ESFTownsfolkSpot::Bench,*B,At,Yaw)) { OutTo=At; W.bFace=true; W.FaceYaw=Yaw; bOutExact=true; bOutScatter=false; return; } }
    }
    if (const FSFTownsfolkSpot* Stall=Stalls.Find(R.MarketLabels[Market]))
    {
        if (Claim(Resident,ESFTownsfolkSpot::Stall,*Stall,At,Yaw)) { OutTo=At; W.bFace=true; W.FaceYaw=Yaw; bOutExact=true; bOutScatter=false; return; }
        ++ClaimFailures;  // every slot taken: stand around it as before
    }
}

void USFTownsfolkSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    if (World.GetOutermost()->GetName()!=ZoneMap) { return; }
    const USFTownsfolkSettings* Settings=GetDefault<USFTownsfolkSettings>();
    FString Error;
    if (!Settings->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("TOWNSFOLK_DISABLED %s"),*Error); return; }
    if (World.GetNetMode()!=NM_Client)
    {   // The authority owns the town state; clients receive it by replication.
        ASFTownState* Town=World.SpawnActor<ASFTownState>();
        Town->Start(*Settings);
#if !UE_BUILD_SHIPPING
        float X=0, Y=0;  // development check: -TownsfolkDangerX= -TownsfolkDangerY= (metres) mark one danger cell at start
        if (FParse::Value(FCommandLine::Get(),TEXT("TownsfolkDangerX="),X) && FParse::Value(FCommandLine::Get(),TEXT("TownsfolkDangerY="),Y)) { Town->MarkDanger(FVector(X,Y,0)*100); }
        // Hand-offs check requests against the census schedule; a listen or standalone prototype loads it below.
        if (World.GetNetMode()==NM_DedicatedServer)
        {
            bCensus=LoadCensus(Error);
            if (!bCensus) { UE_LOG(LogTemp,Error,TEXT("TOWNSFOLK_HANDOFF_DISABLED %s"),*Error); }
        }
#endif
        PostLoginHandle=FGameModeEvents::GameModePostLoginEvent.AddUObject(this,&USFTownsfolkSubsystem::OnPostLogin);
    }
#if !UE_BUILD_SHIPPING
    if (!Settings->bEnabled || World.GetNetMode()==NM_DedicatedServer) { return; }
    if (!LoadCensus(Error) || !LoadLooks(Error)) { UE_LOG(LogTemp,Error,TEXT("TOWNSFOLK_DISABLED %s"),*Error); return; }
    bCensus=true; bScenario=FParse::Param(FCommandLine::Get(),TEXT("TownsfolkHandoffScenario"));
    WalkSpeedMetres=Settings->WalkSpeedMetresPerSecond;
    SidestepMaxCm=Settings->SidestepMaxMetres*100.0f; SidestepRadiusCm=Settings->SidestepRadiusMetres*100.0f; SidestepSpeedShare=Settings->SidestepSpeedShare;
    DoorDwellSeconds=Settings->DoorDwellSeconds; WorkIndoorShare=Settings->WorkIndoorShare; BenchShare=Settings->BenchShare; BenchSearchCm=Settings->BenchSearchMetres*100.0f;

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
    DoorHolds.Init(FSmartObjectClaimHandle::InvalidHandle,Residents.Num()); StandHolds.Init(FSmartObjectClaimHandle::InvalidHandle,Residents.Num());
    Shown.Init(FVector::ZeroVector,Residents.Num()); Facing.Init(FVector::ForwardVector,Residents.Num()); Outdoors.Init(false,Residents.Num()); Still.Init(0,Residents.Num());
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
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_READY Residents=%d District=%s Net=%d SmartObjects=%s"),Residents.Num(),
        Settings->District.IsEmpty() ? TEXT("(whole town)") : *Settings->District,int32(World.GetNetMode()),
        World.GetSubsystem<USmartObjectSubsystem>() ? *World.GetSubsystem<USmartObjectSubsystem>()->GetClass()->GetName() : TEXT("none"));
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

int32 USFTownsfolkLaneLibrary::ReachableLanes(UObject* WorldContext, FVector At, int32& OutTotal)
{
    OutTotal=0;
    const UWorld* World=WorldContext ? WorldContext->GetWorld() : nullptr;
    const UZoneGraphSubsystem* ZG=World ? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
    FZoneGraphLaneLocation Start; float D=0; const FVector Ext(2500,2500,2500);
    if (!ZG || !ZG->FindNearestLane(FBox(At-Ext,At+Ext),FZoneGraphTagFilter(),Start,D)) { return 0; }
    const FZoneGraphStorage& S=ZG->GetZoneGraphData(Start.LaneHandle.DataHandle)->GetStorage();
    OutTotal=S.Lanes.Num();
    TArray<bool> Seen; Seen.Init(false,OutTotal); TArray<int32> Todo{Start.LaneHandle.Index}; Seen[Start.LaneHandle.Index]=true; int32 Count=0;
    while (!Todo.IsEmpty())
    {
        const int32 U=Todo.Pop(); ++Count;
        for (int32 K=S.Lanes[U].LinksBegin; K<S.Lanes[U].LinksEnd; ++K)
        { const int32 V=S.LaneLinks[K].DestLaneIndex; if (!Seen[V]) { Seen[V]=true; Todo.Add(V); } }
    }
    return Count;
}

TArray<FVector> USFTownsfolkLaneLibrary::OutdoorResidents(UObject* WorldContext)
{
    const UWorld* World=WorldContext ? WorldContext->GetWorld() : nullptr;
    const USFTownsfolkSubsystem* Town=World ? World->GetSubsystem<USFTownsfolkSubsystem>() : nullptr;
    return Town && Town->IsActive() ? Town->OutdoorPositions() : TArray<FVector>();
}

void USFTownsfolkSubsystem::Deinitialize()
{
    FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
    Super::Deinitialize();
}

ASFTownsfolkNpc::ASFTownsfolkNpc()
{
    bReplicates=true; SetReplicateMovement(true);
    bAlwaysRelevant=true;  // ponytail: every client in the zone sees it; net culling when one town spans several cells
    PrimaryActorTick.bCanEverTick=true;
    GetCapsuleComponent()->InitCapsuleSize(35,90);
    GetMesh()->SetRelativeLocationAndRotation(FVector(0,0,-90),FRotator(0,-90,0));  // the mannequin faces +Y
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    CrowdControl=CreateDefaultSubobject<USFCrowdControlComponent>(TEXT("CrowdControl"));
    if (!IsRunningDedicatedServer())
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> Manny(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
        static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkClip(TEXT("/Game/Mannequin/Animations/ThirdPersonWalk.ThirdPersonWalk"));
        static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleClip(TEXT("/Game/Mannequin/Animations/ThirdPersonIdle.ThirdPersonIdle"));
        GetMesh()->SetSkeletalMeshAsset(Manny.Object); Walk=WalkClip.Object; Idle=IdleClip.Object;
    }
}

void ASFTownsfolkNpc::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFTownsfolkNpc,ResidentId); DOREPLIFETIME(ASFTownsfolkNpc,Archetype);
    DOREPLIFETIME(ASFTownsfolkNpc,HandoffSerial); DOREPLIFETIME(ASFTownsfolkNpc,CurrentHealth);
}

void ASFTownsfolkNpc::Init(const FString& Id, FName InArchetype, int32 Serial, int32 Health, bool bQuest)
{
    ResidentId=Id; Archetype=InArchetype; HandoffSerial=Serial; CurrentHealth=Health; bQuestHeld=bQuest;
    LastActiveSeconds=GetWorld()->GetTimeSeconds();
}

void ASFTownsfolkNpc::BeginPlay()
{
    Super::BeginPlay();
    LastAt=GetActorLocation();
    if (USFTownsfolkSubsystem* Town=GetWorld()->GetSubsystem<USFTownsfolkSubsystem>()) { Town->NoteNpc(this,true); }
}

void ASFTownsfolkNpc::EndPlay(const EEndPlayReason::Type Reason)
{
    if (USFTownsfolkSubsystem* Town=GetWorld()->GetSubsystem<USFTownsfolkSubsystem>()) { Town->NoteNpc(this,false); }
    Super::EndPlay(Reason);
}

void ASFTownsfolkNpc::ReceiveHit(const AActor* Attacker, int32 Damage)
{
    if (!HasAuthority() || CurrentHealth<=0 || !IsValid(Attacker) || Damage<=0) { return; }
    const USFTownsfolkSettings* S=GetDefault<USFTownsfolkSettings>();
    CurrentHealth=SFCombatPilotRules::RemainingHealth(CurrentHealth,Damage); ForceNetUpdate();
    LastActiveSeconds=GetWorld()->GetTimeSeconds(); FleeFrom=Attacker->GetActorLocation(); FleeUntil=LastActiveSeconds+S->FleeSeconds;
    USFTownsfolkSubsystem* Town=GetWorld()->GetSubsystem<USFTownsfolkSubsystem>();
    if (ASFTownState* State=Town ? Town->TownState() : nullptr)
    {   // Combat in town: residents keep away from this cell (section 5.4).
        State->MarkDanger(GetActorLocation());
        if (CurrentHealth==0) { State->Hour(DeathDay); }
    }
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_HIT Id=%s Serial=%d Health=%d"),*ResidentId,HandoffSerial,CurrentHealth);
    if (CurrentHealth==0)
    {   // ponytail: gone until the next dawn, then back on its schedule from here; respawning at home comes with section 5.5.
        SetActorHiddenInGame(true); SetActorEnableCollision(false);
    }
}

void ASFTownsfolkNpc::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    USFTownsfolkSubsystem* Town=GetWorld()->GetSubsystem<USFTownsfolkSubsystem>();
    if (HasAuthority())
    {
        const double Now=GetWorld()->GetTimeSeconds();
        if (CurrentHealth>0 && Now<FleeUntil && !CrowdControl->IsImmobilised())
        {   // Run straight away from the attacker, slowed by any snare.
            const FVector Away=(GetActorLocation()-FleeFrom).GetSafeNormal2D();
            const float Speed=GetDefault<USFTownsfolkSettings>()->FleeSpeedMetresPerSecond*100.0f*CrowdControl->GetSpeedMultBp()/10000.0f;
            const FVector To=OnGround(GetWorld(),GetActorLocation()+Away*Speed*DeltaSeconds,GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),this);
            SetActorLocation(To); SetActorRotation(Away.Rotation());  // ponytail: no sweep (a slope or the attacker stopped it dead); runs through walls until it has a path
        }
        if (Town && Town->ReleaseDue(*this))
        {
            UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_RELEASE Id=%s Serial=%d Health=%d At=%s"),*ResidentId,HandoffSerial,CurrentHealth,*GetActorLocation().ToCompactString());
            Destroy();
        }
        return;
    }
    // Clients: the resident's look when the prototype has one; walk or stand by how far it moved this frame.
    if (!bLookApplied && Town)
    {
        if (const FSFTownsfolkResolvedLook* L=Town->LookFor(Archetype))
        { GetMesh()->SetSkeletalMeshAsset(L->Mesh); GetMesh()->SetRelativeRotation(FRotator(0,L->YawOffset,0)); Walk=L->Walk; Idle=L->Idle; }
        bLookApplied=true;
    }
    const bool bWalking=FVector::DistSquared2D(GetActorLocation(),LastAt)>FMath::Square(20.0f*DeltaSeconds);
    LastAt=GetActorLocation();
    if (Playing!=int8(bWalking)) { GetMesh()->PlayAnimation(bWalking ? Walk.Get() : Idle.Get(),true); Playing=int8(bWalking); }
}

USFTownsfolkHandoffComponent::USFTownsfolkHandoffComponent()
{
    SetIsReplicatedByDefault(true);
}

void USFTownsfolkHandoffComponent::ServerAttackResident_Implementation(const FString& ResidentId, FVector_NetQuantize SeenAt)
{
    USFTownsfolkSubsystem* Town=GetWorld()->GetSubsystem<USFTownsfolkSubsystem>();
    if (Town && ResidentId.Len()<=64) { Town->HandleAttackRequest(Cast<APlayerController>(GetOwner()),ResidentId,SeenAt); }
}

void USFTownsfolkHandoffComponent::ServerAttackNpc_Implementation(ASFTownsfolkNpc* Npc)
{
    if (USFTownsfolkSubsystem* Town=GetWorld()->GetSubsystem<USFTownsfolkSubsystem>()) { Town->HandleNpcAttack(Cast<APlayerController>(GetOwner()),Npc); }
}

void USFTownsfolkSubsystem::OnPostLogin(AGameModeBase* Mode, APlayerController* PC)
{
    if (!PC || PC->GetWorld()!=GetWorld() || PC->FindComponentByClass<USFTownsfolkHandoffComponent>()) { return; }
    USFTownsfolkHandoffComponent* Link=NewObject<USFTownsfolkHandoffComponent>(PC,TEXT("TownsfolkHandoff"));
    Link->RegisterComponent();
}

FVector USFTownsfolkSubsystem::StopFor(int32 Resident, ESFTownsfolkGoal Goal, int32 Market) const
{
    const FSFTownsfolkResident& R=Residents[Resident];
    return Goal==ESFTownsfolkGoal::Work ? R.Work : Goal==ESFTownsfolkGoal::Market && Market!=INDEX_NONE ? R.Markets[Market] : R.Home;
}

bool USFTownsfolkSubsystem::OnSchedule(int32 Resident, const FVector& Seen) const
{
    // Clients and the server share the census, the clock and the seeds, but a client's exact steps (doors, slots,
    // sidesteps) are its own. So the check is a corridor: the footway route from the stop held before to the current
    // stop, within the tolerance, or a bench near the market for an errand.
    int32 Day=0; const float Hour=State->Hour(Day);
    int32 Market=INDEX_NONE; const ESFTownsfolkGoal Goal=GoalFor(Resident,Hour,Day,Market);
    int32 PrevMarket=Market; ESFTownsfolkGoal Prev=Goal;
    for (int32 K=1; K<=96 && Prev==Goal && PrevMarket==Market; ++K)  // back a quarter hour at a time, up to a day
    {
        float Back=Hour-0.25f*K; int32 BackDay=Day;
        if (Back<0) { Back+=24; --BackDay; }
        Prev=GoalFor(Resident,Back,BackDay,PrevMarket);
    }
    const FVector To=StopFor(Resident,Goal,Market), Flat=Seen*FVector(1,1,0);
    TArray<FVector> Path; PlanRoute(StopFor(Resident,Prev,PrevMarket),To,Path);
    double Best=FVector::Dist2D(Seen,To);
    for (int32 I=0; I+1<Path.Num(); ++I) { Best=FMath::Min(Best,FMath::Sqrt(FMath::PointDistToSegmentSquared(Flat,Path[I]*FVector(1,1,0),Path[I+1]*FVector(1,1,0)))); }
    if (Goal==ESFTownsfolkGoal::Market) { Best=FMath::Min(Best,FMath::Max(0.0,FVector::Dist2D(Seen,To)-BenchSearchCm)); }
    return Best<=GetDefault<USFTownsfolkSettings>()->HandoffToleranceMetres*100.0;
}

ASFTownsfolkNpc* USFTownsfolkSubsystem::SpawnNpc(int32 Resident, const FVector& At, bool bQuest)
{
    const FSFTownsfolkResident& R=Residents[Resident];
    const FTransform Where(FRotator::ZeroRotator,At+FVector(0,0,90));
    ASFTownsfolkNpc* Npc=GetWorld()->SpawnActorDeferred<ASFTownsfolkNpc>(ASFTownsfolkNpc::StaticClass(),Where,nullptr,nullptr,
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
    if (!Npc) { return nullptr; }
    Npc->Init(R.Id,R.Archetype,++HandoffSerial,GetDefault<USFTownsfolkSettings>()->ResidentHealth,bQuest);
    Npc->FinishSpawning(Where);
    return Npc;
}

void USFTownsfolkSubsystem::HandleAttackRequest(APlayerController* PC, const FString& ResidentId, const FVector& SeenAt)
{
    const USFTownsfolkSettings* S=GetDefault<USFTownsfolkSettings>(); const USFCombatPilotSettings* Combat=GetDefault<USFCombatPilotSettings>();
    APawn* Pawn=PC ? PC->GetPawn() : nullptr; const int32* Found=Ids.Find(ResidentId); const int32 I=Found ? *Found : INDEX_NONE;
    const TCHAR* Why=nullptr;
    const double Now=GetWorld()->GetTimeSeconds();
    if (!bCensus || !State) { Why=TEXT("no-census"); }
    else if (!Pawn) { Why=TEXT("no-pawn"); }
    else if (I==INDEX_NONE) { Why=TEXT("unknown"); }
    else if (const TWeakObjectPtr<ASFTownsfolkNpc>* Held=Npcs.Find(I); Held && Held->IsValid()) { HandleNpcAttack(PC,Held->Get()); return; }
    else if (Npcs.Num()>=S->MaxHandoffs) { Why=TEXT("cap"); }
    else if (Now<NextSwing.FindRef(PC)) { Why=TEXT("cooldown"); }
    else if (FVector::Dist2D(Pawn->GetActorLocation(),SeenAt)>Combat->MeleeRangeMetres*100.0+100.0) { Why=TEXT("reach"); }
    else if (!OnSchedule(I,SeenAt)) { Why=TEXT("schedule"); }
    if (Why)
    {   // Security: a request that fails the check is dropped and logged (doc section 5.3).
        UE_LOG(LogTemp,Warning,TEXT("TOWNSFOLK_HANDOFF_DENIED Id=%s Reason=%s Seen=%s"),*ResidentId,Why,*SeenAt.ToCompactString());
        return;
    }
    NextSwing.Add(PC,Now+Combat->PlayerMeleeCooldownSeconds);
    ASFTownsfolkNpc* Npc=SpawnNpc(I,SeenAt,false);
    if (!Npc) { UE_LOG(LogTemp,Warning,TEXT("TOWNSFOLK_HANDOFF_DENIED Id=%s Reason=spawn"),*ResidentId); return; }
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF Id=%s Serial=%d Reason=attack At=%s"),*ResidentId,Npc->Serial(),*SeenAt.ToCompactString());
    Npc->ReceiveHit(Pawn,Combat->PlayerMeleeDamage);
}

void USFTownsfolkSubsystem::HandleNpcAttack(APlayerController* PC, ASFTownsfolkNpc* Npc)
{
    APawn* Pawn=PC ? PC->GetPawn() : nullptr;
    if (!Pawn || !IsValid(Npc) || Npc->Health()<=0) { return; }
    const USFCombatPilotSettings* Combat=GetDefault<USFCombatPilotSettings>(); const double Now=GetWorld()->GetTimeSeconds();
    const FVector D=Npc->GetActorLocation()-Pawn->GetActorLocation();
    if (!SFCombatPilotRules::CanMelee(D.Size2D(),FVector::DotProduct(PC->GetControlRotation().Vector().GetSafeNormal2D(),D.GetSafeNormal2D()),
        Now,NextSwing.FindRef(PC),Combat->MeleeRangeMetres*100.0)) { return; }
    NextSwing.Add(PC,Now+Combat->PlayerMeleeCooldownSeconds);
    Npc->ReceiveHit(Pawn,Combat->PlayerMeleeDamage);
}

ASFTownsfolkNpc* USFTownsfolkSubsystem::RequestQuestHandoff(FName Archetype, const FVector& Near)
{
    if (!bCensus || !State) { return nullptr; }
    int32 Day=0; const float Hour=State->Hour(Day);
    int32 Best=INDEX_NONE, BestMarket=INDEX_NONE; ESFTownsfolkGoal BestGoal=ESFTownsfolkGoal::Home; double BestD=TNumericLimits<double>::Max();
    for (int32 I=0; I<Residents.Num(); ++I)
    {
        if (Residents[I].Archetype!=Archetype || IsHandedOff(I)) { continue; }
        int32 Market=INDEX_NONE; const ESFTownsfolkGoal Goal=GoalFor(I,Hour,Day,Market);
        const double D=FVector::Dist2D(StopFor(I,Goal,Market),Near);
        if (D<BestD) { BestD=D; Best=I; BestMarket=Market; BestGoal=Goal; }
    }
    if (Best==INDEX_NONE || Npcs.Num()>=GetDefault<USFTownsfolkSettings>()->MaxHandoffs)
    { UE_LOG(LogTemp,Warning,TEXT("TOWNSFOLK_HANDOFF_DENIED Archetype=%s Reason=none-free"),*Archetype.ToString()); return nullptr; }
    // At its scheduled stop: clients whose copy is still walking there see it appear at the stop.
    ASFTownsfolkNpc* Npc=SpawnNpc(Best,OnGround(GetWorld(),StopFor(Best,BestGoal,BestMarket),0),true);
    if (Npc) { UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF Id=%s Serial=%d Reason=quest Archetype=%s"),*Npc->Id(),Npc->Serial(),*Archetype.ToString()); }
    return Npc;
}

void USFTownsfolkSubsystem::ReleaseQuestHandoff(ASFTownsfolkNpc* Npc)
{
    // The quest is done with it; it returns once no player is near (ReleaseDue).
    if (IsValid(Npc)) { Npc->bQuestHeld=false; Npc->LastActiveSeconds=-1e9; }
}

void USFTownsfolkSubsystem::NoteNpc(ASFTownsfolkNpc* Npc, bool bPresent)
{
    const int32* Found=Ids.Find(Npc->Id()); const int32 I=Found ? *Found : INDEX_NONE;
    if (I==INDEX_NONE) { return; }  // no census here (a client without the prototype): nothing to hide
    if (bPresent)
    {
        Npcs.Add(I,Npc);
        UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_SEEN Net=%d Id=%s Serial=%d At=%s"),int32(GetWorld()->GetNetMode()),*Npc->Id(),Npc->Serial(),*Npc->GetActorLocation().ToCompactString());
        return;
    }
    Npcs.Remove(I);
    if (!bActive) { return; }
    Resume.Add(I,Npc->GetActorLocation()-FVector(0,0,Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-10));
    LastReturned=I; LastReturnedAt=FPlatformTime::Seconds();
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_RETURN Net=%d Id=%s Serial=%d At=%s"),int32(GetWorld()->GetNetMode()),*Npc->Id(),Npc->Serial(),*Npc->GetActorLocation().ToCompactString());
}

bool USFTownsfolkSubsystem::ReleaseDue(const ASFTownsfolkNpc& Npc) const
{
    if (Npc.bQuestHeld || !State) { return false; }
    if (Npc.Health()<=0)
    {   // Dead: back at the next dawn (doc section 5.5).
        int32 Day=0; const float Hour=State->Hour(Day);
        return Day>Npc.DeathDay && Hour>=6.0f;
    }
    const USFTownsfolkSettings* S=GetDefault<USFTownsfolkSettings>();
    if (GetWorld()->GetTimeSeconds()-Npc.LastActiveSeconds<S->HandoffQuietSeconds) { return false; }
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APawn* Pawn=It->Get() ? It->Get()->GetPawn() : nullptr;
        if (Pawn && FVector::Dist2D(Pawn->GetActorLocation(),Npc.GetActorLocation())<S->HandoffReleaseMetres*100.0f) { return false; }
    }
    return true;
}

bool USFTownsfolkSubsystem::LocalAttack(ASFE1Character* Attacker)
{
    APlayerController* PC=IsValid(Attacker) && Attacker->IsLocallyControlled() ? Cast<APlayerController>(Attacker->GetController()) : nullptr;
    USFTownsfolkHandoffComponent* Link=PC ? PC->FindComponentByClass<USFTownsfolkHandoffComponent>() : nullptr;
    if (!Link) { return false; }
    const double Range=GetDefault<USFCombatPilotSettings>()->MeleeRangeMetres*100.0;
    const FVector From=Attacker->GetActorLocation(), Aim=PC->GetControlRotation().Vector().GetSafeNormal2D();
    for (const TPair<int32,TWeakObjectPtr<ASFTownsfolkNpc>>& P:Npcs)
    {   // Someone already handed off and in reach takes the swing.
        ASFTownsfolkNpc* N=P.Value.Get();
        if (!N || N->Health()<=0) { continue; }
        const FVector D=N->GetActorLocation()-From;
        if (SFCombatPilotRules::CanMelee(D.Size2D(),FVector::DotProduct(Aim,D.GetSafeNormal2D()),0,0,Range)) { Link->ServerAttackNpc(N); return true; }
    }
    if (!bActive) { return false; }
    int32 Best=INDEX_NONE; double BestD=Range;
    for (int32 I=0; I<Residents.Num(); ++I)
    {
        if (!Outdoors[I] || IsHandedOff(I)) { continue; }
        const FVector D=Shown[I]-From; const double Dist=D.Size2D();
        if (Dist<=BestD && SFCombatPilotRules::CanMelee(Dist,FVector::DotProduct(Aim,D.GetSafeNormal2D()),0,0,Range)) { Best=I; BestD=Dist; }
    }
    if (Best==INDEX_NONE) { return false; }
    Link->ServerAttackResident(Residents[Best].Id,Shown[Best]);
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_REQUEST Id=%s Seen=%s"),*Residents[Best].Id,*Shown[Best].ToCompactString());
    return true;
}

void USFTownsfolkSubsystem::TickScenario()
{
    // Development check (-TownsfolkHandoffScenario, one client): walk the player to a resident standing still and
    // swing through the same LocalAttack the attack key uses.
    if (!bScenario || bScenarioSent) { return; }
    APlayerController* PC=GetWorld()->GetFirstPlayerController();
    ASFE1Character* Me=PC ? Cast<ASFE1Character>(PC->GetPawn()) : nullptr;
    if (!Me || !PC->FindComponentByClass<USFTownsfolkHandoffComponent>()) { return; }
    const double Now=FPlatformTime::Seconds();
    if (ScenarioStart<0) { ScenarioStart=Now; }
    if (Now-ScenarioStart<5) { return; }  // let the crowd leave home first
    if (Now-ScenarioStart>90) { UE_LOG(LogTemp,Error,TEXT("TOWNSFOLK_HANDOFF_SCENARIO_FAILED no resident reached")); bScenarioSent=true; return; }
    if (ScenarioTarget==INDEX_NONE || !Outdoors[ScenarioTarget])
    {   // A worker standing at work with two game hours of shift left: still there when the player arrives.
        ScenarioTarget=INDEX_NONE; double BestD=TNumericLimits<double>::Max();
        int32 Day=0; const float Hour=GameHour(Day);
        for (int32 I=0; I<Residents.Num(); ++I)
        {
            int32 M=INDEX_NONE; const double D=FVector::Dist2D(Shown[I],Me->GetActorLocation());
            if (Outdoors[I] && Still[I]>=30 && D<BestD && GoalFor(I,Hour,Day,M)==ESFTownsfolkGoal::Work && Residents[I].WorkEnd-Hour>=2) { BestD=D; ScenarioTarget=I; }
        }
        if (ScenarioTarget==INDEX_NONE) { return; }
        UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_SCENARIO_TARGET Id=%s Distance=%.0f"),*Residents[ScenarioTarget].Id,BestD);
    }
    const FVector D=(Shown[ScenarioTarget]-Me->GetActorLocation())*FVector(1,1,0);
    PC->SetControlRotation(D.Rotation());
    if (D.Size()>150) { Me->AddMovementInput(D.GetSafeNormal(),1.0f); return; }
    bScenarioSent=LocalAttack(Me);
    UE_LOG(LogTemp,Display,TEXT("TOWNSFOLK_HANDOFF_SCENARIO_ATTACK Id=%s Sent=%d"),*Residents[ScenarioTarget].Id,bScenarioSent);
}
