#include "Gathering/SFGatheringPilot.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Gathering/SFHarvestFallingTree.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Inventory/SFInventory.h"

struct FSFHarvestTreeFallNetworkState
{
    double Started=0,StableSince=0,RestSince=0,ReplayedAt=0;
    FVector LastPosition=FVector::ZeroVector;
    uint32 Generation=0;
    int32 Completed=0;
    bool bInitialized=false,bToolRequested=false,bWaiting=false,bReplayed=false,bDone=false;
};

void USFGatheringPilotComponent::TickFellingNetworkScenario()
{
    const bool bLateJoin=FParse::Param(FCommandLine::Get(),TEXT("HarvestTreeFellingLateJoin"));
    auto* PC=Cast<APlayerController>(GetOwner());
    if (!FParse::Param(FCommandLine::Get(),TEXT("HarvestTreeFellingNetworkScenario")) || GetNetMode()!=NM_Client
        || !PC || !PC->IsLocalController() || !bEnabled || !bRoleAssigned || !PC->GetPawn()) { return; }
    if (!FellingNetworkState) { FellingNetworkState=MakeShared<FSFHarvestTreeFallNetworkState>(); }
    auto& Test=*FellingNetworkState; if (Test.bDone) { return; }
    const double Now=GetWorld()->GetTimeSeconds(); const bool bHarvester=bPrimaryRole&&!bLateJoin;
    const auto Fail=[&](const TCHAR* Reason)
    {
        Test.bDone=true;
        UE_LOG(LogTemp,Error,TEXT("HARVEST_TREE_FALL_FAILED Reason=%s Primary=%d LateJoin=%d Completed=%d Phase=%d Result=%d"),
            Reason,bHarvester,bLateJoin,Test.Completed,int32(Feedback.Phase),int32(Feedback.Result));
        FPlatformMisc::RequestExitWithStatus(false,1);
    };
    if (Test.Started==0) { Test.Started=Now; }
    if (Now-Test.Started>110) { Fail(TEXT("ScenarioDeadline")); return; }
    ASFGatheringPilotNode* Wood=nullptr;
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld());It;++It)
    { if (It->Kind==ESFGatheringPilotKind::Oak) { Wood=*It; break; } }
    auto* Inventory=PC->FindComponentByClass<USFInventoryComponent>();
    if (!Wood || Wood->GetState().Capacity==0 || !Inventory) { return; }
    const auto Quantity=[&]() { return SFInventoryTransactions::GetItemQuantity(Inventory,
        TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog")))); };
    const auto& State=Wood->GetState();
    if (!Test.bInitialized)
    {
        Test.bInitialized=true; Test.Generation=State.Generation;
        if (!Wood->HarvestVisuals || !Wood->HarvestVisuals->IsReady()) { Fail(TEXT("ActualOakSourceUnavailable")); return; }
        if (bHarvester && (State.Capacity!=3 || State.Charges!=3 || State.Revision!=0 || State.Cuts.Num()!=0))
        { Fail(TEXT("ExpectedFreshThreeChargeNode")); return; }
    }
    if (State.Generation!=Test.Generation) { Fail(TEXT("UnexpectedRenewal")); return; }
    if (!bHarvester && (Quantity()!=0 || Skills.PracticeCount!=0 || Skills.GainCount!=0))
    { Fail(TEXT("ObserverReceivedPrivateGrant")); return; }
    // Owner-only progression must remain absent from every foreign endpoint visible locally.
    int32 InventoryCount=0;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<USFInventoryComponent*> Bags; It->GetComponents(Bags); InventoryCount+=Bags.Num();
        TArray<USFGatheringPilotComponent*> Endpoints; It->GetComponents(Endpoints);
        for (const auto* Endpoint:Endpoints)
        {
            if (Endpoint==this) { continue; }
            if (Endpoint->Skills.PracticeCount!=0 || Endpoint->Skills.GainCount!=0)
            { Fail(TEXT("ForeignProgressionVisible")); return; }
            for (const int32 Value:Endpoint->Skills.Values)
            { if (Value!=0) { Fail(TEXT("ForeignSkillVisible")); return; } }
        }
    }
    if (InventoryCount!=1) { Fail(TEXT("ForeignInventoryVisible")); return; }
    if (bHarvester && Test.Completed<3)
    {
        if (!HasRequiredTool(ESFGatheringPilotKind::Oak))
        { if (!Test.bToolRequested) { Test.bToolRequested=TryEquipTool(true); } return; }
        if (Test.bWaiting)
        {
            if (Feedback.Phase==ESFGatheringPilotPhase::Rejected || Feedback.Phase==ESFGatheringPilotPhase::Canceled)
            { Fail(TEXT("RealHarvestRejected")); return; }
            if (Feedback.Phase!=ESFGatheringPilotPhase::Committed || State.Cuts.Num()!=Test.Completed+1
                || Quantity()!=Test.Completed+1 || Skills.PracticeCount!=Test.Completed+1) { return; }
            ++Test.Completed; Test.bWaiting=false; return;
        }
        const auto* Character=Cast<ACharacter>(PC->GetPawn()); const FVector Position=PC->GetPawn()->GetActorLocation();
        if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetVelocity().IsNearlyZero(1)
            || FVector::DistSquared(Position,Test.LastPosition)>1)
        { Test.StableSince=0; Test.LastPosition=Position; return; }
        if (Test.StableSince==0) { Test.StableSince=Now; }
        if (Now-Test.StableSince<1) { return; }
        const FVector Eye=PC->GetPawn()->GetPawnViewLocation();
        const FVector Aim=Wood->GetActorLocation();
        FVector Contact,Normal;
        if (!Wood->HarvestVisuals->TraceSurface(Eye,(Aim-Eye).GetSafeNormal(),Contact,Normal)
            || !InteractFromRay(Eye,(Contact-Eye).GetSafeNormal()) || !bWaitingHadSurface)
        { Fail(TEXT("ActualOakSurfaceInteraction")); return; }
        Test.bWaiting=true; return;
    }
    if (State.Charges!=0 || State.Cuts.Num()!=3 || State.Revision!=3) { return; }
    if (bHarvester && !Test.bReplayed)
    {
        ServerGatheringActionAtSurface(Wood,WaitingRevision,Feedback.Operation,WaitingSurface);
        Test.bReplayed=true; Test.ReplayedAt=Now; return;
    }
    if (bHarvester && Now-Test.ReplayedAt<1) { return; }
    auto* Tree=State.FallingTree.Get();
    if (!IsValid(Tree) || !Tree->IsVisualSourceReady() || !Tree->MatchesResource(Wood,State.Generation)
        || !Wood->HarvestVisuals->IsTreeDetachedTo(Tree)) { return; }
    int32 MatchingFalls=0;
    for (TActorIterator<ASFHarvestFallingTree> It(GetWorld());It;++It)
    { if (It->MatchesResource(Wood,State.Generation)) { ++MatchingFalls; } }
    if (MatchingFalls!=1) { Fail(TEXT("DuplicateFallActor")); return; }
    if (!Tree->IsResting()) { return; }
    auto* Body=Tree->GetFallBody(); if (!Body) { Fail(TEXT("MissingFallRoot")); return; }
    const FTransform Actual=Body->GetComponentTransform(),Expected=Tree->GetAuthoritativeRestingTransform();
    const double PositionError=FVector::Dist(Actual.GetLocation(),Expected.GetLocation());
    const double AngleError=FMath::RadiansToDegrees(Actual.GetRotation().AngularDistance(Expected.GetRotation()));
    if (Body->IsSimulatingPhysics() || PositionError>2 || AngleError>0.5)
    { Test.RestSince=0; return; }
    if (Test.RestSince==0) { Test.RestSince=Now; }
    if (Now-Test.RestSince<1) { return; }
    int32 HistoricalFragments=0;
    if (bLateJoin)
    {
        TArray<UStaticMeshComponent*> Parts; Wood->GetComponents(Parts);
        for (const auto* Part:Parts)
        {
            if (Part==Wood->Mesh || Part->GetName().StartsWith(TEXT("HarvestOakCrown"))) { continue; }
            if (Part->GetStaticMesh() && Part->IsVisible()) { ++HistoricalFragments; }
        }
        if (HistoricalFragments!=0) { Fail(TEXT("LateJoinHistoricalFragments")); return; }
    }
    const int64 ExpectedPractice=bHarvester?3:0;
    if (Quantity()!=ExpectedPractice || Skills.PracticeCount!=ExpectedPractice || Skills.GainCount>ExpectedPractice
        || Skills.Get(ESFGatheringSkill::Mining)!=0 || Skills.Get(ESFGatheringSkill::Logging)!=Skills.GainCount || bKeyConflict)
    { Fail(TEXT("FinalPrivateOrReplayInvariant")); return; }
    Test.bDone=true;
    UE_LOG(LogTemp,Display,TEXT("HARVEST_TREE_FALL_CLIENT_DONE Primary=%d LateJoin=%d Wood=%lld Practice=%lld Cuts=3 Charges=0 Revision=3 Generation=%u MatchingFalls=1 Resting=1 Physics=0 PositionErrorCm=%.4f AngleErrorDeg=%.4f Private=1 ReplayUnchanged=%d HistoricalFragments=%d"),
        bHarvester,bLateJoin,Quantity(),Skills.PracticeCount,State.Generation,PositionError,AngleError,bHarvester,HistoricalFragments);
    FPlatformMisc::RequestExitWithStatus(false,0);
}
#endif
