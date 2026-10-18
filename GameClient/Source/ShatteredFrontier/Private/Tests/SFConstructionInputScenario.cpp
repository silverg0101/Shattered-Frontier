#include "SFConstructionInputScenario.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionSandboxController.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Construction/SFConstructionGeometry.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProceduralMeshComponent.h"
#include "Tickable.h"

namespace
{
class FConstructionInputScenario final : public FTickableGameObject
{
public:
    explicit FConstructionInputScenario(UWorld* InWorld) : World(InWorld), Started(FPlatformTime::Seconds()) {}
    virtual UWorld* GetTickableGameObjectWorld() const override { return World.Get(); }
    virtual bool IsTickable() const override { return !bFinished && World.IsValid(); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FConstructionInputScenario, STATGROUP_Tickables); }
    virtual void Tick(float DeltaSeconds) override
    {
        UWorld* W = World.Get();
        if (!W || bFinished) { return; }
        if (FPlatformTime::Seconds() - Started > 45) { Fail(TEXT("Finite scenario timeout")); return; }
        if (W->GetNetMode() != NM_Standalone || !W->GetAuthGameMode<ASFConstructionSandboxGameMode>())
        { Fail(TEXT("Sandbox authority lost")); return; }
        if (Step == 0 && (!Controller.IsValid() || !State.IsValid()
            || !Controller->GetPawn() || !Controller->PlayerInput))
        {
            Controller = Cast<ASFConstructionSandboxController>(W->GetFirstPlayerController());
            if (!Controller.IsValid() || !Controller->GetPawn() || !Controller->PlayerInput) { return; }
            for (TActorIterator<ASFConstructionSandboxState> It(W); It; ++It) { State = *It; break; }
            if (!State.IsValid()) { return; }
            if (!Controller->IsLocalController() || !Controller->HasAuthority()) { Fail(TEXT("No owning local authority")); return; }
        }
        if (!State.IsValid() || !Controller.IsValid() || !Controller->GetPawn()) { Fail(TEXT("Fixture ownership lost")); return; }
        if (bAiming)
        {
            FVector Camera; FRotator View;
            Controller->GetPlayerViewPoint(Camera, View);
            Controller->SetControlRotation((AimTarget - Camera).Rotation());
        }
        if (GFrameCounter < NextFrame) { return; }
        if (!ReleaseKeys.IsEmpty())
        {
            for (const FKey& Key : ReleaseKeys) { Input(Key, IE_Released); }
            ReleaseKeys.Reset();
            NextFrame = GFrameCounter + 4;
            return;
        }
        if (bAwaitAim)
        {
            FVector Camera; FRotator View;
            Controller->GetPlayerViewPoint(Camera, View);
            const FVector Delta = AimTarget - Camera;
            const double Along = FVector::DotProduct(Delta, View.Vector());
            const double Miss = (Delta - View.Vector() * Along).Size();
            if (Along > 0.0 && Miss < 0.2) { ++AimStableFrames; } else { AimStableFrames = 0; }
            const double WorldElapsed = W->GetTimeSeconds() - AimWorldStartedAt;
            if (AimStableFrames < 4 || WorldElapsed < 0.25)
            {
                if (FPlatformTime::Seconds() - AimRealStartedAt > 3.0)
                {
                    LogAimDiagnostics(TEXT("Aim did not settle within three seconds"));
                    Fail(TEXT("Actual view ray did not settle before input"));
                }
                return;
            }
            bAwaitAim = false;
            LogAimDiagnostics(TEXT("Aim settled"));
        }
        if (bHasPendingPress)
        {
            const TArray<FKey> Keys = MoveTemp(PendingPressKeys);
            bHasPendingPress = false;
            DispatchPress(Keys);
            return;
        }
        if (bPendingDemolition)
        {
            bPendingDemolition = false;
            Press({EKeys::LeftMouseButton});
            return;
        }
        switch (Step)
        {
        case 0:
        {
            if (!Check(State->GetMemberCount() == 0, TEXT("Expected empty editable state"))) { return; }
            auto* Character = Cast<ACharacter>(Controller->GetPawn());
            if (!Check(Character != nullptr, TEXT("Expected actual construction character"))) { return; }
            Character->GetCharacterMovement()->DisableMovement();
            Character->SetActorLocation(FVector(225,425,110), false, nullptr, ETeleportType::TeleportPhysics);
            Aim(FVector(425,425,0));
            Mark(TEXT("OWNING_PLAYER_READY")); Advance(); break;
        }
        case 1: Press({EKeys::B}); break;
        case 2: Press({EKeys::LeftMouseButton}); break;
        case 3:
            if (!Check(State->GetMemberCount() == 0, TEXT("Disabled click edited construction"))) { return; }
            Mark(TEXT("DISABLED_CLICK_DENIED")); Press({EKeys::B}); break;
        case 4: Press({EKeys::Three, EKeys::LeftMouseButton}); break;
        case 5:
            if (!Check(State->GetMemberCount() == 0, TEXT("Shape change committed stale ghost"))) { return; }
            Mark(TEXT("SHAPE_STALE_GHOST_DENIED")); Press({EKeys::R, EKeys::LeftMouseButton}); break;
        case 6:
            if (!Check(State->GetMemberCount() == 0, TEXT("Rotation committed stale ghost"))) { return; }
            Mark(TEXT("ROTATION_STALE_GHOST_DENIED")); Press({EKeys::LeftMouseButton}); break;
        case 7:
            if (!Check(State->GetMemberCount() == 1 && Matches(FIntVector(8,8,0), 2, 0), TEXT("Input slope/orientation identity mismatch"))) { return; }
            Mark(TEXT("SLOPE_ROTATION_PLACED")); Aim(FVector(400,425,25)); Advance(); break;
        case 8: Press({EKeys::Delete}); break;
        case 9:
            if (!Check(State->GetMemberCount() == 0, TEXT("Safe input removal did not remove slope"))) { return; }
            Mark(TEXT("SAFE_REMOVAL_APPLIED")); Aim(FVector(425,425,0)); Press({EKeys::Two, EKeys::LeftMouseButton}); break;
        case 10:
            if (!Check(State->GetMemberCount() == 0, TEXT("Half slab change committed stale ghost"))) { return; }
            Mark(TEXT("SLAB_STALE_GHOST_DENIED")); Press({EKeys::LeftMouseButton}); break;
        case 11:
            if (!Check(State->GetMemberCount() == 1 && Matches(FIntVector(8,8,0), 1, 0), TEXT("Input half slab identity mismatch"))) { return; }
            Mark(TEXT("HALF_SLAB_PLACED")); Aim(FVector(400,425,12)); Advance(); break;
        case 12: Press({EKeys::Delete}); break;
        case 13:
            if (!Check(State->GetMemberCount() == 0, TEXT("Input half slab removal failed"))) { return; }
            Mark(TEXT("HALF_SLAB_REMOVED")); Aim(FVector(425,425,0)); Press({EKeys::One}); break;
        case 14: Press({EKeys::LeftMouseButton}); break;
        case 15:
            if (!Check(State->GetMemberCount() == 1 && Matches(FIntVector(8,8,0), 0, 0), TEXT("Input cube placement missing"))) { return; }
            Mark(TEXT("CUBE_PLACED")); Aim(FVector(425,425,50)); Advance(); break;
        case 16: Press({EKeys::LeftMouseButton}); break;
        case 17:
            if (!Check(State->GetMemberCount() == 2 && Matches(FIntVector(8,8,1), 0, 0), TEXT("Supported input stack missing"))) { return; }
            Mark(TEXT("SUPPORTED_STACK_PLACED")); Aim(FVector(400,425,25)); Advance(); break;
        case 18: Press({EKeys::Delete}); break;
        case 19:
            if (!Check(State->GetMemberCount() == 2, TEXT("Unconfirmed support loss edited members"))) { return; }
            Mark(TEXT("REMOVAL_REQUIRES_CONFIRMATION")); Press({EKeys::Escape}); break;
        case 20: Press({EKeys::Enter}); break;
        case 21:
            if (!Check(State->GetMemberCount() == 2, TEXT("Cancelled removal was confirmed"))) { return; }
            Mark(TEXT("CANCELLED_CONFIRMATION_DENIED")); Press({EKeys::Delete}); break;
        case 22: ConfirmationSentAt = W->GetTimeSeconds(); Press({EKeys::Enter}); break;
        case 23:
            if (!Check(State->GetMemberCount() == 1 && Matches(FIntVector(8,8,1), 0, 0)
                && State->GetInstabilitySummary().StartsWith(TEXT("UNSTABLE: 1")), TEXT("Confirmed support loss did not enter grace"))) { return; }
            GraceObservedAt = W->GetTimeSeconds();
            Mark(TEXT("SUPPORT_LOSS_GRACE_STARTED")); Advance(); break;
        case 24:
            if (W->GetTimeSeconds() - GraceObservedAt < 9.0)
            {
                if (!Check(State->GetMemberCount() == 1, TEXT("Collapse occurred before ten-second world grace"))) { return; }
                return;
            }
            Mark(TEXT("WORLD_TIME_GRACE_PRESERVED")); Advance(); break;
        case 25:
            if (State->GetMemberCount() != 0) { return; }
            if (!Check(W->GetTimeSeconds() - ConfirmationSentAt >= 10.0, TEXT("World grace elapsed too early"))) { return; }
            Mark(TEXT("WORLD_TIME_COLLAPSE_APPLIED"));
            bFinished = true;
            UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_INPUT_SUCCESS Steps=16 Members=0 WorldGraceSeconds=%.3f AutomatedEngineInput=1 PhysicalKeyboard=0 Rendered=0"), W->GetTimeSeconds() - ConfirmationSentAt);
            FPlatformMisc::RequestExitWithStatus(false, 0); break;
        }
    }
private:
    void Input(const FKey& Key, EInputEvent Event)
    {
        // The ordinary player-input stack processes these next frame. Never call
        // controller actions, State::Apply, or ProcessInputStack directly.
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key, Event, Event == IE_Pressed ? 1.f : 0.f,
            1, FInputDeviceId::CreateFromInternalId(0)));
    }
    void Press(std::initializer_list<FKey> Keys)
    {
        if (bAwaitAim)
        {
            PendingPressKeys.Reset();
            for (const FKey& Key : Keys) { PendingPressKeys.Add(Key); }
            bHasPendingPress = true;
            return;
        }
        TArray<FKey> Copy;
        for (const FKey& Key : Keys) { Copy.Add(Key); }
        DispatchPress(Copy);
    }
    void DispatchPress(const TArray<FKey>& Keys)
    {
        if (Keys.Num() == 1 && Keys[0] == EKeys::Delete)
        {
            bPendingDemolition = true;
            if (!Controller->IsDemolitionMode())
            {
                Input(EKeys::X, IE_Pressed);
                ReleaseKeys.Add(EKeys::X);
                NextFrame = GFrameCounter + 1;
            }
            else { NextFrame = GFrameCounter + 2; }
            return;
        }
        for (const FKey& Key : Keys) { Input(Key, IE_Pressed); ReleaseKeys.Add(Key); }
        ++Step; NextFrame = GFrameCounter + 1;
    }
    void Advance() { ++Step; NextFrame = GFrameCounter + 4; }
    void Aim(const FVector& Target)
    {
        AimTarget = Target; bAiming = true;
        bAwaitAim = true; AimStableFrames = 0;
        AimWorldStartedAt = World->GetTimeSeconds(); AimRealStartedAt = FPlatformTime::Seconds();
        Controller->SetControlRotation((Target - Controller->GetPawn()->GetActorLocation()).Rotation());
    }
    bool Matches(const FIntVector& Cell, int32 Shape, int32 Rotation) const
    {
        FSFConstructionSolid Expected;
        if (!SFConstructionGeometry::Build(Shape, Rotation, Expected)) { return false; }
        for (TActorIterator<ASFConstructionSandboxPreview> It(World.Get()); It; ++It)
        {
            FIntVector Actual;
            if (!State->FindCell(*It, Actual) || Actual != Cell) { continue; }
            auto* Mesh = const_cast<UProceduralMeshComponent*>(It->GetMesh());
            const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
            if (!Body || Body->AggGeom.ConvexElems.Num() != 1 || It->GetPayloadHash() != Expected.PayloadSha256) { return false; }
            const auto& Vertices = Body->AggGeom.ConvexElems[0].VertexData;
            if (Vertices.Num() != Expected.Vertices.Num()) { return false; }
            for (const FVector& V : Expected.Vertices) { if (!Vertices.Contains(V)) { return false; } }
            return true;
        }
        return false;
    }
    bool Check(bool bCondition, const TCHAR* Reason) { if (!bCondition) { Fail(Reason); } return bCondition; }
    void Mark(const TCHAR* Name) const
    {
        UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_INPUT_STEP Name=%s Count=%d Revision=%llu WorldSeconds=%.3f"),
            Name, State->GetMemberCount(), State->GetRevision(), World->GetTimeSeconds());
    }
    void Fail(const TCHAR* Reason)
    {
        LogAimDiagnostics(TEXT("Scenario failure"));
        bFinished = true;
        UE_LOG(LogTemp, Error, TEXT("CONSTRUCTION_INPUT_FAILED Step=%d Reason=%s Members=%d Revision=%llu LastAuthorityReason=%s"),
            Step, Reason, State.IsValid() ? State->GetMemberCount() : -1, State.IsValid() ? State->GetRevision() : 0,
            Controller.IsValid() ? *Controller->GetLastConstructionResult() : TEXT("unavailable"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
    void LogAimDiagnostics(const TCHAR* Context) const
    {
        if (!World.IsValid() || !Controller.IsValid() || !Controller->GetPawn()) { return; }
        FVector Camera; FRotator View;
        Controller->GetPlayerViewPoint(Camera, View);
        const FVector Direction = View.Vector();
        const FVector Delta = AimTarget - Camera;
        const double Along = FVector::DotProduct(Delta, Direction);
        const double Miss = (Delta - Direction * Along).Size();
        FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ConstructionInputAimDiagnostic), false, Controller->GetPawn());
        const float Reach = State.IsValid() ? State->GetReachCentimetres() : 0.0f;
        const bool bHit = Reach > 0.0f && World->LineTraceSingleByChannel(Hit, Camera, Camera + Direction * Reach, ECC_Visibility, Query);
        UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_INPUT_AIM Context=%s Step=%d Camera=%s Direction=%s Target=%s Along=%.3f Miss=%.3f Hit=%d HitActor=%s HitPoint=%s HitNormal=%s LastAuthorityReason=%s"),
            Context, Step, *Camera.ToString(), *Direction.ToString(), *AimTarget.ToString(), Along, Miss, bHit,
            *GetNameSafe(Hit.GetActor()), *Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString(),
            Controller.IsValid() ? *Controller->GetLastConstructionResult() : TEXT("unavailable"));
    }
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ASFConstructionSandboxController> Controller;
    TWeakObjectPtr<ASFConstructionSandboxState> State;
    double Started;
    double GraceObservedAt = 0;
    double ConfirmationSentAt = 0;
    double AimWorldStartedAt = 0, AimRealStartedAt = 0;
    uint64 NextFrame = 0;
    int32 Step = 0, AimStableFrames = 0;
    bool bFinished = false;
    bool bAiming = false, bAwaitAim = false, bPendingDemolition = false, bHasPendingPress = false;
    FVector AimTarget = FVector::ZeroVector;
    TArray<FKey> PendingPressKeys;
    TArray<FKey> ReleaseKeys;
};
TUniquePtr<FConstructionInputScenario> Scenario;
}

void StartConstructionInputScenario(UWorld* World)
{
    if (!FParse::Param(FCommandLine::Get(), TEXT("ConstructionInputScenario"))) { return; }
    if (!World || World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>()) { return; }
    if (Scenario) { return; }
    UE_LOG(LogTemp, Display, TEXT("CONSTRUCTION_INPUT_STARTED Standalone=1 DevelopmentOnly=1"));
    Scenario = MakeUnique<FConstructionInputScenario>(World);
}
#endif
