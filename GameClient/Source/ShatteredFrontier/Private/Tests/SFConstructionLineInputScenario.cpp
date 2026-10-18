#include "SFConstructionLineInputScenario.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Characters/SFE1Character.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Tickable.h"

namespace
{
class FLineInputScenario final : public FTickableGameObject
{
public:
    explicit FLineInputScenario(UWorld* InWorld) : World(InWorld), Started(FPlatformTime::Seconds()) {}
    virtual UWorld* GetTickableGameObjectWorld() const override { return World.Get(); }
    virtual bool IsTickable() const override { return !bFinished && World.IsValid(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FLineInputScenario,STATGROUP_Tickables); }
    virtual void Tick(float DeltaSeconds) override
    {
        if (bFinished || !World.IsValid()) { return; }
        if (FPlatformTime::Seconds()-Started > 50) { Fail(TEXT("Finite line-input timeout")); return; }
        if (World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>())
        { Fail(TEXT("Standalone sandbox lost")); return; }
        if (!Controller.IsValid() || !Character.IsValid() || !State.IsValid())
        {
            Controller = Cast<ASFConstructionSandboxController>(World->GetFirstPlayerController());
            if (!Controller.IsValid() || !Controller->IsLocalController() || !Controller->PlayerInput) { return; }
            Character = Cast<ASFE1Character>(Controller->GetPawn());
            for (TActorIterator<ASFConstructionSandboxState> It(World.Get()); It; ++It) { State = *It; break; }
            if (!Character.IsValid() || !State.IsValid()) { return; }
        }
        if (Controller->GetPawn() != Character.Get()) { Fail(TEXT("Owning character changed")); return; }
        if (bAiming)
        {
            FVector Camera; FRotator View; Controller->GetPlayerViewPoint(Camera,View);
            Controller->SetControlRotation((AimTarget-Camera).Rotation());
        }
        if (GFrameCounter < NextFrame) { return; }
        if (!ReleaseKeys.IsEmpty())
        {
            for (const FKey& Key : ReleaseKeys) { Input(Key,IE_Released); }
            ReleaseKeys.Reset(); NextFrame = GFrameCounter+5; return;
        }
        if (bAwaitAim)
        {
            // Camera boom/view caches settle over ordinary frames. Corroborate
            // the actual ray, not just the requested control rotation.
            FVector Camera; FRotator View; Controller->GetPlayerViewPoint(Camera,View);
            const FVector Delta = AimTarget-Camera;
            const double Along = FVector::DotProduct(Delta,View.Vector());
            const double Miss = (Delta-View.Vector()*Along).Size();
            if (Along > 0 && Miss < .2) { ++AimStableFrames; } else { AimStableFrames = 0; }
            if (AimStableFrames < 4 || FPlatformTime::Seconds()-AimStarted < .25)
            {
                if (FPlatformTime::Seconds()-AimStarted > 3) { LogAim(); Fail(TEXT("Actual view ray failed bounded aim settling")); }
                return;
            }
            bAwaitAim = false; LogAim();
        }
        switch (Step)
        {
        case 0:
            if (!Check(Counts(0,0),TEXT("Fixture must start empty"))) { return; }
            ReferenceActors = LivePreviews(); Character->GetCharacterMovement()->DisableMovement();
            Setup(FVector(475,225,110),FVector(425,425,0)); Press({EKeys::L}); break;
        case 1:
            if (!Check(Controller->IsLineMode() && !Controller->HasLineSeed(),TEXT("L did not enter empty line mode"))) { return; }
            Mark(TEXT("LINE_MODE_ENTERED")); Press({EKeys::LeftMouseButton}); break;
        case 2:
            if (!Check(Controller->HasLineSeed() && Counts(0,0) && LivePreviews() == ReferenceActors,TEXT("First click mutated authority or omitted seed"))) { return; }
            Mark(TEXT("SEED_READ_ONLY")); Aim(FVector(525,425,0)); Advance(); break;
        case 3:
            if (!WaitForGhost({8,8,0},{10,8,0},3,true)) { return; }
            if (!Check(Counts(0,0) && LivePreviews() == ReferenceActors,TEXT("Full ground ghost mutated authority"))) { return; }
            Mark(TEXT("FULL_GHOST_THREE")); Press({EKeys::LeftMouseButton}); break;
        case 4:
            if (!Check(Counts(3,1) && !Controller->HasLineSeed() && Physics({8,8,0}) && Physics({9,8,0}) && Physics({10,8,0}),TEXT("Ground line missing one atomic revision/canonical collision"))) { return; }
            Mark(TEXT("GROUND_LINE_ATOMIC")); Setup(FVector(675,225,110),FVector(725,425,0)); Advance(); break;
        case 5: Press({EKeys::LeftMouseButton}); break;
        case 6: Aim(FVector(625,425,0)); Advance(); break;
        case 7:
            if (!WaitForGhost({12,8,0},{14,8,0},3,true)) { return; }
            Press({EKeys::LeftMouseButton}); break;
        case 8:
            if (!Check(Counts(6,2) && !Controller->HasLineSeed() && Physics({12,8,0}) && Physics({13,8,0}) && Physics({14,8,0}),TEXT("Reverse line missing atomic canonical members"))) { return; }
            Mark(TEXT("REVERSE_LINE_ATOMIC")); Setup(FVector(475,225,110),FVector(375,425,0)); Advance(); break;
        case 9: Press({EKeys::LeftMouseButton}); break;
        case 10: Aim(FVector(575,425,0)); Advance(); break;
        case 11:
            if (!WaitForGhost({7,8,0},{11,8,0},5,false)) { return; }
            Press({EKeys::LeftMouseButton}); break;
        case 12:
            if (!Check(Counts(6,2) && Controller->HasLineSeed() && Controller->GetLastConstructionResult().Contains(TEXT("occupied")),TEXT("Occupied line was partial or lost its seed"))) { return; }
            Mark(TEXT("OCCUPIED_LINE_DENIED")); Press({EKeys::Escape}); break;
        case 13: Setup(FVector(425,875,110),FVector(375,625,0)); Advance(); break;
        case 14: Press({EKeys::LeftMouseButton}); break;
        case 15: Aim(FVector(425,675,0)); Advance(); break;
        case 16:
            if (!Check(Controller->GetLineGhostCells().IsEmpty() && !Controller->IsLinePreviewAllowed(),TEXT("Diagonal preview enumerated cells"))) { return; }
            Press({EKeys::LeftMouseButton}); break;
        case 17:
            if (!Check(Counts(6,2) && Controller->HasLineSeed(),TEXT("Diagonal line mutated authority or dropped seed"))) { return; }
            Mark(TEXT("DIAGONAL_DENIED")); Press({EKeys::Escape}); break;
        case 18: Aim(FVector(225,625,0)); Advance(); break;
        case 19: Press({EKeys::LeftMouseButton}); break;
        case 20: Aim(FVector(625,625,0)); Advance(); break;
        case 21:
            if (!Check(Controller->GetLineGhostCells().IsEmpty() && !Controller->IsLinePreviewAllowed(),TEXT("Over-cap preview enumerated cells"))) { return; }
            Press({EKeys::LeftMouseButton}); break;
        case 22:
            if (!Check(Counts(6,2) && Controller->HasLineSeed(),TEXT("Over-cap line mutated authority or dropped seed"))) { return; }
            Mark(TEXT("CAP_DENIED")); Press({EKeys::Escape}); break;
        case 23: Press({EKeys::LeftMouseButton}); break;
        case 24:
            if (!Check(Controller->HasLineSeed(),TEXT("Selection-reset seed missing"))) { return; }
            Press({EKeys::Two,EKeys::LeftMouseButton}); break;
        case 25:
            if (!Check(!Controller->HasLineSeed() && Counts(6,2),TEXT("Shape selection reused sampled endpoint"))) { return; }
            Mark(TEXT("SELECTION_CLEARS_SEED")); Press({EKeys::LeftMouseButton}); break;
        case 26: Press({EKeys::R,EKeys::LeftMouseButton}); break;
        case 27:
            if (!Check(!Controller->HasLineSeed() && Counts(6,2),TEXT("Rotation reused sampled endpoint"))) { return; }
            Mark(TEXT("ROTATION_CLEARS_SEED")); Press({EKeys::LeftMouseButton}); break;
        case 28: Press({EKeys::L,EKeys::LeftMouseButton}); break;
        case 29:
            if (!Check(!Controller->IsLineMode() && !Controller->HasLineSeed() && Counts(6,2),TEXT("Mode change reused sampled endpoint"))) { return; }
            Mark(TEXT("MODE_CLEARS_SEED")); Press({EKeys::L}); break;
        case 30: Press({EKeys::LeftMouseButton}); break;
        case 31: Press({EKeys::B,EKeys::LeftMouseButton}); break;
        case 32:
            if (!Check(!Controller->IsBuildingEnabled() && !Controller->HasLineSeed() && Counts(6,2),TEXT("Pause reused line seed"))) { return; }
            Mark(TEXT("PAUSE_CLEARS_SEED")); Press({EKeys::B}); break;
        case 33: Press({EKeys::LeftMouseButton}); break;
        case 34: Press({EKeys::Escape,EKeys::LeftMouseButton}); break;
        case 35:
            if (!Check(!Controller->HasLineSeed() && Counts(6,2),TEXT("Cancel reused sampled endpoint"))) { return; }
            Mark(TEXT("CANCEL_CLEARS_SEED")); Press({EKeys::LeftMouseButton}); break;
        case 36: Press({EKeys::Four,EKeys::LeftMouseButton}); break;
        case 37:
            if (!Check(Controller->IsDoorSelected() && !Controller->IsLineMode() && !Controller->HasLineSeed() && Counts(6,2),TEXT("Door selection retained structural line or stale placement"))) { return; }
            Mark(TEXT("DOOR_RETURNS_SINGLE")); Press({EKeys::One}); break;
        case 38: Setup(FVector(225,875,110),FVector(225,625,0)); Advance(); break;
        case 39: Press({EKeys::LeftMouseButton}); break;
        case 40:
            if (!Check(Counts(7,3) && Physics({4,12,0}),TEXT("Single placement regression"))) { return; }
            Mark(TEXT("SINGLE_PLACEMENT_RETAINED")); Aim(FVector(225,625,25)); Advance(); break;
        case 41: Press({EKeys::RightMouseButton}); break;
        case 42:
            if (!Check(Counts(6,4) && !OwnedProjection({4,12,0}),TEXT("Targeted single removal regression"))) { return; }
            Mark(TEXT("SINGLE_REMOVAL_RETAINED")); Aim(FVector(225,625,0)); Press({EKeys::L}); break;
        case 43: Press({EKeys::LeftMouseButton}); break;
        case 44:
            if (!Check(Controller->HasLineSeed(),TEXT("Revision-change seed missing"))) { return; }
            Setup(FVector(525,225,110),FVector(525,400,25)); Advance(); break;
        case 45: Press({EKeys::RightMouseButton}); break;
        case 46:
            if (!Check(Counts(5,5) && !Controller->HasLineSeed() && !OwnedProjection({10,8,0}),TEXT("Demolition failed to invalidate line seed"))) { return; }
            Mark(TEXT("DEMOLITION_CANCELLED_SEED"));
            bAiming = false; SetupForWalking(); Advance(); break;
        case 47:
            if (!Character->GetCharacterMovement()->IsMovingOnGround()) { return; }
            WalkStart = Character->GetActorLocation(); WalkingAt = World->GetTimeSeconds(); Input(EKeys::W,IE_Pressed); bWalking = true; Advance(); break;
        case 48:
            if (World->GetTimeSeconds()-WalkingAt < .3) { return; }
            Input(EKeys::W,IE_Released); bWalking = false;
            if (!Check(FVector::Dist2D(WalkStart,Character->GetActorLocation()) > 20 && Counts(5,5),TEXT("Inherited character movement did not process W"))) { return; }
            Mark(TEXT("INHERITED_MOVEMENT_RETAINED"));
            if (!Check(Physics({8,8,0}) && Physics({9,8,0}) && Physics({12,8,0}) && Physics({13,8,0}) && Physics({14,8,0}),TEXT("Final canonical/simple projections changed"))) { return; }
            bFinished = true;
            UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_LINE_INPUT_SUCCESS Steps=18 Members=5 Revision=5 AutomatedInput=1 SimpleCollision=1"));
            FPlatformMisc::RequestExitWithStatus(false,0); break;
        }
    }
private:
    void Input(FKey Key,EInputEvent Event)
    { Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,Event,Event == IE_Pressed ? 1.f : 0.f,1,FInputDeviceId::CreateFromInternalId(0))); }
    void Press(std::initializer_list<FKey> Keys)
    { for (const auto& Key : Keys) { Input(Key,IE_Pressed); ReleaseKeys.Add(Key); } ++Step; NextFrame = GFrameCounter+1; }
    void Advance() { ++Step; NextFrame = GFrameCounter+5; }
    void Aim(FVector Point)
    {
        bAiming = true; bAwaitAim = true; AimStableFrames = 0; AimStarted = FPlatformTime::Seconds();
        AimTarget = Point; Controller->SetControlRotation((Point-Character->GetActorLocation()).Rotation());
    }
    void Setup(FVector Position,FVector Target) { Character->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics); Aim(Target); }
    void SetupForWalking()
    { Character->SetActorLocation(FVector(-500,-500,110),false,nullptr,ETeleportType::TeleportPhysics); Controller->SetControlRotation(FRotator::ZeroRotator); Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking); }
    bool Counts(int32 Members,uint64 Revision) const { return State->GetMemberCount() == Members && State->GetDoorCount() == 0 && State->GetRevision() == Revision; }
    bool Ghost(FIntVector First,FIntVector Last,int32 Count,bool bAllowed) const
    {
        const auto& Cells = Controller->GetLineGhostCells();
        return Cells.Num() == Count && Cells[0] == First && Cells.Last() == Last && Controller->IsLinePreviewAllowed() == bAllowed;
    }
    bool WaitForGhost(FIntVector First,FIntVector Last,int32 Count,bool bAllowed)
    {
        if (Ghost(First,Last,Count,bAllowed)) { return true; }
        if (FPlatformTime::Seconds()-AimStarted > 3)
        { LogAim(); Fail(TEXT("Settled full ghost differs from exact expected cells/status")); }
        return false;
    }
    void LogAim() const
    {
        FVector Camera; FRotator View; Controller->GetPlayerViewPoint(Camera,View);
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(LineInputAimEvidence),false,Character.Get());
        const bool bHit = World->LineTraceSingleByChannel(Hit,Camera,Camera+View.Vector()*State->GetReachCentimetres(),ECC_Visibility,Query);
        FString Cells;
        for (const auto& Cell : Controller->GetLineGhostCells()) { Cells += TEXT("[")+Cell.ToString()+TEXT("]"); }
        UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_LINE_INPUT_AIM Step=%d Camera=%s Direction=%s Target=%s Hit=%d HitActor=%s Impact=%s Normal=%s Seed=%d Ghost=%s Preview=%s"),
            Step,*Camera.ToString(),*View.Vector().ToString(),*AimTarget.ToString(),bHit,*GetNameSafe(Hit.GetActor()),*Hit.ImpactPoint.ToString(),
            *Hit.ImpactNormal.ToString(),Controller->HasLineSeed(),*Cells,*Controller->GetLinePreviewReason());
    }
    ASFConstructionSandboxPreview* OwnedProjection(FIntVector Cell) const
    { for (TActorIterator<ASFConstructionSandboxPreview> It(World.Get()); It; ++It)
        { FIntVector Found; if (State->FindCell(*It,Found) && Found == Cell) { return *It; } } return nullptr; }
    bool Physics(FIntVector Cell) const
    {
        auto* Projection = OwnedProjection(Cell);
        FSFConstructionSandboxSnapshot Snapshot; FString Reason;
        if (!Projection || !State->ExportSnapshot(Controller.Get(),State->GetRevision(),Snapshot,Reason)) { return false; }
        const auto* Record = Snapshot.Cells.FindByPredicate([&](const auto& Entry) { return Entry.Cell == Cell; });
        if (!Record || Record->Shape != 0 || !Projection->IsProjectionIntact(Record->Shape,Record->Rotation,
            Projection->GetGeneration(),ASFConstructionSandboxState::CellCorner(Cell))) { return false; }
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(LineInputSimple),false,Character.Get());
        const FVector Corner = ASFConstructionSandboxState::CellCorner(Cell);
        return World->LineTraceSingleByChannel(Hit,Corner+FVector(25,-20,25),Corner+FVector(25,70,25),ECC_Visibility,Query) && Hit.GetActor() == Projection;
    }
    int32 LivePreviews() const
    { int32 Count = 0; for (TActorIterator<ASFConstructionSandboxPreview> It(World.Get()); It; ++It) { if (!It->IsActorBeingDestroyed()) { ++Count; } } return Count; }
    void Mark(const TCHAR* Name) const
    { UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_LINE_INPUT_STEP Name=%s Members=%d Doors=%d Revision=%llu LineMode=%d Seed=%d GhostCells=%d WorldSeconds=%.3f"),
        Name,State->GetMemberCount(),State->GetDoorCount(),State->GetRevision(),Controller->IsLineMode(),Controller->HasLineSeed(),Controller->GetLineGhostCells().Num(),World->GetTimeSeconds()); }
    bool Check(bool bCondition,const TCHAR* Reason) { if (!bCondition) { Fail(Reason); } return bCondition; }
    void Fail(const TCHAR* Reason)
    {
        if (Controller.IsValid() && Character.IsValid() && State.IsValid()) { LogAim(); }
        bFinished = true;
        if (Controller.IsValid())
        { for (const auto& Key : ReleaseKeys) { Input(Key,IE_Released); } if (bWalking) { Input(EKeys::W,IE_Released); } }
        UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_LINE_INPUT_FAILED Step=%d Reason=%s AuthorityReason=%s PreviewReason=%s"),Step,Reason,
            Controller.IsValid() ? *Controller->GetLastConstructionResult() : TEXT("unavailable"),Controller.IsValid() ? *Controller->GetLinePreviewReason() : TEXT("unavailable"));
        FPlatformMisc::RequestExitWithStatus(false,1);
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ASFConstructionSandboxController> Controller;
    TWeakObjectPtr<ASFE1Character> Character;
    TWeakObjectPtr<ASFConstructionSandboxState> State;
    double Started, WalkingAt = 0, AimStarted = 0;
    uint64 NextFrame = 0;
    int32 Step = 0, ReferenceActors = 0, AimStableFrames = 0;
    bool bFinished = false, bAiming = false, bWalking = false, bAwaitAim = false;
    FVector AimTarget = FVector::ZeroVector, WalkStart = FVector::ZeroVector;
    TArray<FKey> ReleaseKeys;
};
TUniquePtr<FLineInputScenario> LineScenario;
}
void StartConstructionLineInputScenario(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(),TEXT("ConstructionLineInputScenario")) || !World
        || World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>() || LineScenario) { return; }
    for (const TCHAR* Flag : {TEXT("ConstructionInputScenario"),TEXT("ConstructionDoorInputScenario"),TEXT("ConstructionSaveRestartWriter"),TEXT("ConstructionSaveRestartReader")})
    { if (FParse::Param(FCommandLine::Get(),Flag))
        { UE_LOG(LogTemp,Error,TEXT("CONSTRUCTION_LINE_INPUT_FAILED Reason=ConflictingFiniteScenarios")); FPlatformMisc::RequestExitWithStatus(false,1); return; } }
    UE_LOG(LogTemp,Display,TEXT("CONSTRUCTION_LINE_INPUT_STARTED Standalone=1 DevelopmentOnly=1"));
    LineScenario = MakeUnique<FLineInputScenario>(World);
}
#endif
