#include "SFConstructionDoorInputScenario.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Characters/SFE1Character.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tickable.h"

namespace
{
class FDoorInputScenario final : public FTickableGameObject
{
public:
    explicit FDoorInputScenario(UWorld* InWorld) : World(InWorld), Started(FPlatformTime::Seconds()) {}
    virtual UWorld* GetTickableGameObjectWorld() const override { return World.Get(); }
    virtual bool IsTickable() const override { return !bFinished && World.IsValid(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FDoorInputScenario,STATGROUP_Tickables); }
    virtual void Tick(float DeltaSeconds) override
    {
        if (bFinished || !World.IsValid()) { return; }
        if (FPlatformTime::Seconds()-Started > 55) { Fail(TEXT("Finite input/walking timeout")); return; }
        if (World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>()) { Fail(TEXT("Standalone sandbox lost")); return; }
        if (Step == 0 && (!Controller.IsValid() || !Character.IsValid() || !State.IsValid()))
        {
            Controller = Cast<ASFConstructionSandboxController>(World->GetFirstPlayerController());
            if (!Controller.IsValid() || !Controller->PlayerInput || !Controller->IsLocalController()) { return; }
            Character = Cast<ASFE1Character>(Controller->GetPawn());
            if (!Character.IsValid()) { return; }
            for (TActorIterator<ASFConstructionSandboxState> It(World.Get()); It; ++It) { State = *It; break; }
            if (!State.IsValid()) { return; }
        }
        if (!Controller.IsValid() || !Character.IsValid() || !State.IsValid() || Controller->GetPawn() != Character.Get())
        { Fail(TEXT("Owning character lost")); return; }
        if (bMeasuredWalking)
        {
            const FVector Position = Character->GetActorLocation();
            PeakWalkZ = FMath::Max(PeakWalkZ,Position.Z);
            if (Step >= 25) { OpenTraversalPeakZ = FMath::Max(OpenTraversalPeakZ,Position.Z); }
            if (!Character->GetCharacterMovement()->IsMovingOnGround()) { bGroundedThroughout = false; }
            if (FMath::Abs(Position.X-452.5) > 3) { Fail(TEXT("Measured movement left actual door corridor")); return; }
        }
        if (bAiming)
        {
            FVector Camera; FRotator View;
            Controller->GetPlayerViewPoint(Camera,View);
            Controller->SetControlRotation((AimTarget-Camera).Rotation());
        }
        if (GFrameCounter < NextFrame) { return; }
        if (!ReleaseKeys.IsEmpty())
        {
            for (const auto& Key : ReleaseKeys) { Input(Key,IE_Released); }
            ReleaseKeys.Reset(); NextFrame = GFrameCounter+4; return;
        }
        const double Now = World->GetTimeSeconds();
        switch (Step)
        {
        case 0:
            if (!Check(State->GetMemberCount() == 0 && State->GetDoorCount() == 0,TEXT("Expected empty authority state"))) { return; }
            BaseAttempts = Controller->GetInteractionInputAttemptCount();
            Character->GetCharacterMovement()->DisableMovement();
            SetupPosition(FVector(225,425,110)); Aim(FVector(425,425,0));
            Mark(TEXT("DETERMINISTIC_SETUP")); Advance(); break;
        case 1: Press({EKeys::Four,EKeys::LeftMouseButton}); break;
        case 2:
            if (!Check(Controller->IsDoorSelected() && State->GetDoorCount() == 0,TEXT("Door selection reused structural ghost"))) { return; }
            Mark(TEXT("DOOR_SELECTION_STALE_DENIED")); Press({EKeys::R,EKeys::LeftMouseButton}); break;
        case 3:
            if (!Check(Controller->GetDoorYawQuarterTurns() == 1 && State->GetDoorCount() == 0,TEXT("Door yaw reused sampled placement"))) { return; }
            Mark(TEXT("DOOR_YAW_STALE_DENIED")); Press({EKeys::R}); break;
        case 4:
            if (!Check(Controller->GetDoorYawQuarterTurns() == 2,TEXT("Second door yaw missing"))) { return; }
            Press({EKeys::R}); break;
        case 5:
            if (!Check(Controller->GetDoorYawQuarterTurns() == 3,TEXT("Third door yaw missing"))) { return; }
            Press({EKeys::R}); break;
        case 6:
            if (!Check(Controller->GetDoorYawQuarterTurns() == 0,TEXT("Door yaw did not wrap at four"))) { return; }
            Mark(TEXT("FOUR_YAWS_CONFIRMED")); Press({EKeys::LeftMouseButton}); break;
        case 7:
            if (!Check(DoorMatches(false),TEXT("Sampled door placement missing"))) { return; }
            Mark(TEXT("DOOR_PLACED")); Aim(FVector(402.5,425,100)); Advance(); break;
        case 8: Press({EKeys::RightMouseButton}); break;
        case 9:
            if (!Check(State->GetDoorCount() == 0,TEXT("Targeted RMB did not remove actual door"))) { return; }
            Mark(TEXT("TARGETED_DOOR_REMOVED")); Aim(FVector(425,425,0)); Advance(); break;
        case 10: Press({EKeys::LeftMouseButton}); break;
        case 11:
            if (!Check(DoorMatches(false),TEXT("Door replacement missing"))) { return; }
            Press({EKeys::One,EKeys::LeftMouseButton}); break;
        case 12:
            if (!Check(!Controller->IsDoorSelected() && State->GetMemberCount() == 0 && State->GetDoorCount() == 1,TEXT("Structural selection reused door ghost"))) { return; }
            Mark(TEXT("STRUCTURAL_SELECTION_STALE_DENIED")); Press({EKeys::Four,EKeys::LeftMouseButton}); break;
        case 13:
            if (!Check(Controller->IsDoorSelected() && State->GetDoorCount() == 1,TEXT("Repeated door selection reused structural sample"))) { return; }
            Aim(FVector(402.5,425,100)); Press({EKeys::B}); break;
        case 14:
            if (!Check(!Controller->IsBuildingEnabled(),TEXT("Build pause input failed"))) { return; }
            Press({EKeys::E}); break;
        case 15:
            if (!Check(DoorMatches(true) && NoBaseDoorDispatch(),TEXT("Build-off E did not exclusively open door"))) { return; }
            Mark(TEXT("BUILD_OFF_DOOR_OPENED")); Press({EKeys::E}); break;
        case 16:
            if (!Check(DoorMatches(false) && NoBaseDoorDispatch(),TEXT("Build-off E did not exclusively close door"))) { return; }
            Mark(TEXT("DOOR_CLOSED_WITHOUT_PROBE"));
            // Stand in front with the capsule crossing the motion reservation,
            // while its body remains clear of the closed leaf. This keeps the
            // third-person boom outside the frame during deterministic setup.
            SetupPosition(FVector(452.5,380,110)); Aim(FVector(450,424.5,100));
            RevisionBeforeBlocked = State->GetRevision(); SettlingAt = Now; AimConfirmedFrames = 0; Advance(); break;
        case 17:
            if (Now-SettlingAt < .2) { return; }
            if (!ViewTargetsOwnedDoor())
            {
                AimConfirmedFrames = 0;
                if (Now-SettlingAt > 2) { LogAim(); Fail(TEXT("Blocked-character setup could not establish actual owned-door view trace")); }
                return;
            }
            if (++AimConfirmedFrames < 3) { return; }
            LogAim(); Press({EKeys::E}); break;
        case 18:
            if (!Check(DoorMatches(false) && NoBaseDoorDispatch() && State->GetRevision() == RevisionBeforeBlocked
                && Controller->GetLastConstructionResult().Contains(TEXT("blocked")),TEXT("Character motion reservation did not deny door E"))) { return; }
            Mark(TEXT("CHARACTER_MOTION_BLOCKED"));
            bAiming = false;
            SetupPosition(FVector(452.5,260,100)); Controller->SetControlRotation(FRotator(0,90,0));
            Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            SettlingAt = Now; Advance(); break;
        case 19:
            if (Now-SettlingAt < .3 || !Character->GetCharacterMovement()->IsMovingOnGround()) { return; }
            if (!Check(FMath::IsNearlyEqual(Character->GetCapsuleComponent()->GetScaledCapsuleRadius(),34.f)
                && FMath::IsNearlyEqual(Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),88.f),TEXT("Inherited capsule is not validated 34/88 profile"))) { return; }
            if (!Check(OnSyntheticFloor(),TEXT("Measured walking must start on actual synthetic floor"))) { return; }
            WalkStart = Character->GetActorLocation(); PeakWalkZ = WalkStart.Z;
            bMeasuredWalking = true; bGroundedThroughout = true; WalkStartedAt = Now; PhaseStartedAt = Now;
            Mark(TEXT("MEASURED_WALK_STARTED")); Hold(EKeys::W); Advance(); break;
        case 20:
            if (Now-PhaseStartedAt < .9) { return; }
            Release(EKeys::W); Advance(); break;
        case 21:
        {
            const FVector P = Character->GetActorLocation();
            if (!Check(P.Y > 365 && P.Y < 395 && Character->GetCharacterMovement()->IsMovingOnGround() && DoorMatches(false),TEXT("Actual walking did not stop at closed leaf"))) { return; }
            Mark(TEXT("CLOSED_DOOR_BLOCKED_WALK")); Hold(EKeys::S); Advance(); break;
        }
        case 22:
            if (Character->GetActorLocation().Y > 265) { return; }
            Release(EKeys::S); Advance(); break;
        case 23:
            if (Character->GetVelocity().Size() > 1) { return; }
            if (!Check(Character->GetActorLocation().Y < 300 && Character->GetCharacterMovement()->IsMovingOnGround(),TEXT("Input retreat did not clear swing"))) { return; }
            if (!Check(ViewTargetsOwnedDoor(),TEXT("Retreated actual view trace missed owned corridor door"))) { return; }
            Mark(TEXT("INPUT_RETREAT_CLEARED_SWING")); Press({EKeys::E}); break;
        case 24:
            if (!Check(DoorMatches(true) && NoBaseDoorDispatch(),TEXT("Retreated E failed to open actual corridor"))) { return; }
            OpenTraversalPeakZ = Character->GetActorLocation().Z;
            Mark(TEXT("WALKING_DOOR_OPENED")); Hold(EKeys::W); Advance(); break;
        case 25:
            if (Character->GetActorLocation().Y < 610) { return; }
            Release(EKeys::W); Advance(); break;
        case 26:
            if (Character->GetVelocity().Size() > 1) { return; }
            WalkEnd = Character->GetActorLocation();
            if (!Check(WalkEnd.Y > 600 && FMath::Abs(WalkEnd.Z-WalkStart.Z) < 1 && OpenTraversalPeakZ > WalkStart.Z+3
                && Character->GetCharacterMovement()->IsMovingOnGround() && bGroundedThroughout && OnSyntheticFloor() && DoorMatches(true),TEXT("Measured walk did not cross raised sill grounded"))) { return; }
            bMeasuredWalking = false;
            Mark(TEXT("GROUNDED_SILL_TRAVERSAL"));
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_DOOR_WALK Start=%s End=%s OpenTraversalPeakZ=%.3f WorldSeconds=%.3f Teleports=0 GroundedThroughout=1 CapsuleRadius=34 CapsuleHalfHeight=88"),
                *WalkStart.ToString(),*WalkEnd.ToString(),OpenTraversalPeakZ,Now-WalkStartedAt);
            for (TActorIterator<ASFE1InteractionTarget> It(World.Get()); It; ++It) { Probe = *It; break; }
            if (!Check(Probe.IsValid(),TEXT("Original E1 probe unavailable"))) { return; }
            Character->GetCharacterMovement()->DisableMovement();
            SetupPosition(Probe->GetActorLocation()+FVector(-150,0,40)); Aim(Probe->GetInteractionAnchor());
            SettlingAt = Now; Advance(); break;
        case 27:
            if (Now-SettlingAt < .3) { return; }
            if (!Check(NoBaseDoorDispatch(),TEXT("Door E leaked an inherited probe dispatch"))) { return; }
            Press({EKeys::E}); break;
        case 28:
            if (!Check(Controller->GetInteractionInputAttemptCount() == BaseAttempts+1 && DoorMatches(true),TEXT("Probe fallback was missing or dispatched more than once"))) { return; }
            Mark(TEXT("PROBE_FALLBACK_EXACTLY_ONCE"));
            bFinished = true;
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_DOOR_INPUT_SUCCESS Steps=16 Doors=1 BaseEntryDelta=1 AutomatedInput=1 ActualCharacterWalking=1 Rendered=0"));
            FPlatformMisc::RequestExitWithStatus(false,0); break;
        }
    }
private:
    void Input(FKey Key, EInputEvent Event)
    { Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,Event,Event == IE_Pressed ? 1.f : 0.f,1,FInputDeviceId::CreateFromInternalId(0))); }
    void Press(std::initializer_list<FKey> Keys)
    { for (const auto& Key : Keys) { Input(Key,IE_Pressed); ReleaseKeys.Add(Key); } ++Step; NextFrame = GFrameCounter+1; }
    void Hold(FKey Key) { Input(Key,IE_Pressed); HeldKeys.AddUnique(Key); }
    void Release(FKey Key) { Input(Key,IE_Released); HeldKeys.Remove(Key); }
    void Advance() { ++Step; NextFrame = GFrameCounter+4; }
    void Aim(FVector Point) { bAiming = true; AimTarget = Point; Controller->SetControlRotation((Point-Character->GetActorLocation()).Rotation()); }
    void SetupPosition(FVector Point)
    {
        check(!bMeasuredWalking);
        Character->SetActorLocation(Point,false,nullptr,ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_DOOR_INPUT_SETUP Step=%d Position=%s MeasuredWalking=0"),Step,*Point.ToString());
    }
    bool DoorMatches(bool bOpen) const
    {
        const auto* Door = State->FindDoor(FIntVector(8,8,0));
        return State->GetDoorCount() == 1 && Door && Door->YawQuarterTurns == 0 && Door->bOpen == bOpen
            && Door->Projection.IsValid() && Door->Projection->IsProjectionIntact();
    }
    bool NoBaseDoorDispatch() const { return Controller->GetInteractionInputAttemptCount() == BaseAttempts; }
    bool ViewTargetsOwnedDoor() const
    {
        FVector Eyes; FRotator View;
        Controller->GetPlayerViewPoint(Eyes,View);
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorInputAimCorroboration),false,Character.Get());
        FIntVector Origin;
        if (!World->LineTraceSingleByChannel(Hit,Eyes,Eyes+View.Vector()*State->GetReachCentimetres(),ECC_Visibility,Query)
            || !State->FindDoorOrigin(Hit.GetActor(),Origin) || Origin != FIntVector(8,8,0)) { return false; }
        const auto* Door = State->FindDoor(Origin);
        return Door && Door->Projection.IsValid() && Door->Projection->IsProjectionIntact();
    }
    void LogAim() const
    {
        FVector Eyes; FRotator View;
        Controller->GetPlayerViewPoint(Eyes,View);
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(DoorInputAimEvidence),false,Character.Get());
        World->LineTraceSingleByChannel(Hit,Eyes,Eyes+View.Vector()*State->GetReachCentimetres(),ECC_Visibility,Query);
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_DOOR_INPUT_AIM Step=%d Camera=%s Direction=%s Hit=%s OwnedDoor=%d ConsecutiveFrames=%d"),
            Step,*Eyes.ToString(),*View.Vector().ToString(),*GetNameSafe(Hit.GetActor()),ViewTargetsOwnedDoor(),AimConfirmedFrames);
    }
    bool OnSyntheticFloor() const
    {
        const AActor* Floor = Character->GetCharacterMovement()->CurrentFloor.HitResult.GetActor();
        return IsValid(Floor) && Floor->ActorHasTag(TEXT("SFConstructionSandboxFoundationV1"));
    }
    void Mark(const TCHAR* Name) const
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_DOOR_INPUT_STEP Name=%s Doors=%d Members=%d Revision=%llu BaseEntries=%llu WorldSeconds=%.3f"),Name,State->GetDoorCount(),State->GetMemberCount(),State->GetRevision(),Controller->GetInteractionInputAttemptCount()-BaseAttempts,World->GetTimeSeconds()); }
    bool Check(bool Condition,const TCHAR* Reason) { if (!Condition) { Fail(Reason); } return Condition; }
    void Fail(const TCHAR* Reason)
    {
        bFinished = true;
        if (Controller.IsValid())
        {
            for (const auto& Key : HeldKeys) { Input(Key,IE_Released); }
            for (const auto& Key : ReleaseKeys) { Input(Key,IE_Released); }
        }
        UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_DOOR_INPUT_FAILED Step=%d Reason=%s AuthorityReason=%s Pawn=%s"),Step,Reason,
            Controller.IsValid() ? *Controller->GetLastConstructionResult() : TEXT("unavailable"),Character.IsValid() ? *Character->GetActorLocation().ToString() : TEXT("unavailable"));
        FPlatformMisc::RequestExitWithStatus(false,1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ASFConstructionSandboxController> Controller;
    TWeakObjectPtr<ASFE1Character> Character;
    TWeakObjectPtr<ASFConstructionSandboxState> State;
    TWeakObjectPtr<ASFE1InteractionTarget> Probe;
    double Started, SettlingAt = 0, WalkStartedAt = 0, PhaseStartedAt = 0, PeakWalkZ = 0, OpenTraversalPeakZ = 0;
    FVector AimTarget = FVector::ZeroVector, WalkStart = FVector::ZeroVector, WalkEnd = FVector::ZeroVector;
    uint64 NextFrame = 0, BaseAttempts = 0, RevisionBeforeBlocked = 0;
    int32 Step = 0;
    int32 AimConfirmedFrames = 0;
    bool bFinished = false, bAiming = false, bMeasuredWalking = false, bGroundedThroughout = true;
    TArray<FKey> HeldKeys, ReleaseKeys;
};
TUniquePtr<FDoorInputScenario> DoorScenario;
}

void StartConstructionDoorInputScenario(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("ConstructionDoorInputScenario"))) { return; }
    if (!World || World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>() || DoorScenario) { return; }
    if (FParse::Param(FCommandLine::Get(),TEXT("ConstructionInputScenario")))
    { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_DOOR_INPUT_FAILED Reason=ConflictingFiniteScenarios")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_DOOR_INPUT_STARTED Standalone=1 DevelopmentOnly=1"));
    DoorScenario = MakeUnique<FDoorInputScenario>(World);
}
#endif
