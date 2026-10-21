#include "Characters/SFE1Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Framework/SFE1Settings.h"
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Framework/SFPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "Townsfolk/SFTownsfolk.h"
#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionSandboxController.h"

ASFE1Character::ASFE1Character()
{
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    FString Error;
    if (!Settings->Validate(Error))
    {
        UE_LOG(LogTemp, Fatal, TEXT("Invalid E1 release configuration: %s"), *Error);
    }

    bReplicates = true;
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, Settings->TurnRateDegreesPerSecond, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = Settings->RunSpeedMetresPerSecond * 100.0f;
    GetCharacterMovement()->JumpZVelocity = FMath::Sqrt(2.0f * Settings->GravityMetresPerSecondSquared * Settings->JumpHeightMetres) * 100.0f;
    GetCharacterMovement()->MaxStepHeight = Settings->StepHeightMetres * 100.0f;
    GetCharacterMovement()->AirControl = 0.0f;

    CrowdControl = CreateDefaultSubobject<USFCrowdControlComponent>(TEXT("CrowdControl"));
    // Rigged body on the UE4 mannequin skeleton the NPC pipeline also fits. Locomotion comes from the AnimBP reading
    // replicated CharacterMovement velocity and falling state, so remote proxies animate with no extra replication.
    GetMesh()->SetRelativeLocationAndRotation(FVector(0, 0, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()), FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Skinned body: always invalidate its virtual shadow map pages, so no stale shadow pieces stay behind it.
    GetMesh()->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Always;
    if (!IsRunningDedicatedServer())
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
        static ConstructorHelpers::FClassFinder<UAnimInstance> Locomotion(TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP"));
        GetMesh()->SetSkeletalMeshAsset(Body.Object);
        GetMesh()->SetAnimInstanceClass(Locomotion.Class);
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = Settings->CameraBoomLengthCm;
    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->SetRelativeLocation(FVector(0, 0, Settings->CameraPivotHeightCm));
    CameraBoom->bEnableCameraLag = Settings->CameraLagSpeed > 0.0f;
    CameraBoom->CameraLagSpeed = Settings->CameraLagSpeed;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
}

void ASFE1Character::BeginPlay()
{
    Super::BeginPlay();
    const float WorldGravity = GetWorld()->GetGravityZ();
    if (!FMath::IsFinite(WorldGravity) || WorldGravity >= -UE_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Fatal, TEXT("E1 fixture requires finite downward world gravity"));
    }
    GetCharacterMovement()->GravityScale = GetDefault<USFE1Settings>()->GravityMetresPerSecondSquared * 100.0f / -WorldGravity;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetNetMode() == NM_Client && FParse::Param(FCommandLine::Get(), TEXT("CombatPilotScenario")))
    { GetWorldTimerManager().SetTimer(CombatScenarioTimer, this, &ASFE1Character::TickCombatScenario, 1.0f, true); }
#endif
}

void ASFE1Character::TickCombatScenario()
{
    if (!IsLocallyControlled() || CombatScenarioAttackCount >= 3) { return; }
    ASFPilotBoar* Nearest = nullptr; double Distance = 300;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    {
        const double D = FVector::Dist2D(GetActorLocation(), It->GetActorLocation());
        if (It->Health() > 0 && D < Distance) { Nearest = *It; Distance = D; }
    }
    if (!Nearest || !Controller || PilotHealth <= 0) { return; }
    const FVector ToBoar = (Nearest->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    // The server judges facing from the control rotation replicated with movement, so aim on one tick and swing once it has arrived.
    if (FVector::DotProduct(Controller->GetControlRotation().Vector().GetSafeNormal2D(), ToBoar) < 0.9)
    { Controller->SetControlRotation(ToBoar.Rotation()); return; }
    PilotAttack();
    ++CombatScenarioAttackCount;
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_CLIENT_ATTACK Index=%d"), CombatScenarioAttackCount);
}

namespace SFE1CameraControls
{
    // Order matches the IA_* names below; tools/Create-PlayerInputAssets.py creates the assets.
    enum EAction { IA_MoveForward, IA_MoveBack, IA_StrafeRight, IA_StrafeLeft, IA_TurnRight, IA_TurnLeft, IA_Look, IA_LookStick, IA_Zoom,
                   IA_MouseLook, IA_Orbit, IA_Jump, IA_Autorun, IA_Walk, IA_WalkToggle, IA_Interact, IA_Count };
    const TCHAR* const ActionNames[IA_Count] = { TEXT("IA_MoveForward"), TEXT("IA_MoveBack"), TEXT("IA_StrafeRight"),
        TEXT("IA_StrafeLeft"), TEXT("IA_TurnRight"), TEXT("IA_TurnLeft"), TEXT("IA_Look"), TEXT("IA_LookStick"), TEXT("IA_Zoom"),
        TEXT("IA_MouseLook"), TEXT("IA_Orbit"), TEXT("IA_Jump"), TEXT("IA_Autorun"), TEXT("IA_Walk"), TEXT("IA_WalkToggle"),
        TEXT("IA_Interact") };
}

bool ASFE1Character::UsesCameraControls() const
{
    // Every player controller; IMC_Build is layered on while build mode is active (TickCameraControls).
    return Cast<ASFPlayerController>(Controller) != nullptr;
}

void ASFE1Character::ApplyControlScheme()
{
    // Runs on the server and the owning client so both turn the pawn the same way; facing is the replicated
    // control yaw (ServerMove carries it), the camera orbits on its own local rotation.
    const bool bCameraControls = UsesCameraControls();
    bUseControllerRotationYaw = bCameraControls;
    GetCharacterMovement()->bOrientRotationToMovement = !bCameraControls;
    CameraBoom->bUsePawnControlRotation = !bCameraControls;
    CameraBoom->SetUsingAbsoluteRotation(bCameraControls);
}

void ASFE1Character::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    ApplyControlScheme();
}

void ASFE1Character::PawnClientRestart()
{
    Super::PawnClientRestart();
    ApplyControlScheme();
    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!UsesCameraControls() || !PC || !PC->GetLocalPlayer()) { return; }
    if (!PlayerMapping)
    {
        PlayerMapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Frontier/Input/IMC_Player.IMC_Player"));
        BuildMapping = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Frontier/Input/IMC_Build.IMC_Build"));
        for (const TCHAR* Name : SFE1CameraControls::ActionNames)
        { PlayerActions.Add(LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("/Game/Frontier/Input/%s.%s"), Name, Name))); }
    }
    UEnhancedInputLocalPlayerSubsystem* Input = PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    if (!PlayerMapping || !BuildMapping || PlayerActions.Contains(nullptr) || !Input)
    {
        UE_LOG(LogTemp, Error, TEXT("CTL-01 player input assets missing; run tools/Create-PlayerInputAssets.py"));
        return;
    }
    Input->AddMappingContext(PlayerMapping, 0);
    Input->RemoveMappingContext(BuildMapping);  // TickCameraControls adds it back while build mode is active
    bBuildMappingActive = false;
    const USFE1Settings* Settings = GetDefault<USFE1Settings>();
    CameraRotation = FRotator(-15.0f, GetActorRotation().Yaw, 0.0f);
    PC->SetControlRotation(FRotator(0.0f, CameraRotation.Yaw, 0.0f));
    LastControlYaw = CameraRotation.Yaw;
    LastControlPitch = 0.0f;
    DesiredArmCm = Settings->CameraBoomLengthCm;
    SetMouseCaptured(false);
}

void ASFE1Character::SetMouseCaptured(bool bCaptured)
{
    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC) { return; }
    bMouseCaptured = bCaptured;
    PC->bShowMouseCursor = !bCaptured;
    if (bCaptured)
    { FInputModeGameOnly Mode; Mode.SetConsumeCaptureMouseDown(false); PC->SetInputMode(Mode); }
    else
    { FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); PC->SetInputMode(Mode); }
}

void ASFE1Character::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsLocallyControlled() && PlayerMapping && UsesCameraControls()) { TickCameraControls(DeltaSeconds); }
}

void ASFE1Character::TickCameraControls(float Dt)
{
    using namespace SFE1CameraControls;
    APlayerController* PC = Cast<APlayerController>(Controller);
    UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    UEnhancedPlayerInput* In = Subsystem ? Subsystem->GetPlayerInput() : nullptr;
    if (!In) { return; }
    const USFE1Settings* S = GetDefault<USFE1Settings>();
    const auto Value = [&](EAction A) { return In->GetActionValue(PlayerActions[A]); };
    const auto Axis = [&](EAction A) { return Value(A).Get<float>(); };
    uint32 Held = 0;
    for (EAction A : { IA_MouseLook, IA_Orbit, IA_Jump, IA_Autorun, IA_Walk, IA_WalkToggle, IA_Interact }) { if (Value(A).Get<bool>()) { Held |= 1u << A; } }
    const auto IsHeld = [Held](EAction A) { return (Held & (1u << A)) != 0; };
    const uint32 HeldBefore = HeldLastTick;
    HeldLastTick = Held;
    const auto Pressed = [&](EAction A) { return IsHeld(A) && !(HeldBefore & (1u << A)); };
    const auto Released = [&](EAction A) { return !IsHeld(A) && (HeldBefore & (1u << A)) != 0; };

    // Build mode (GM-01): toggled with B on the player controller; the standalone sandbox fixture is always in it.
    // IMC_Build carries the build actions at a higher priority and consumes their keys from IMC_Player while it lasts.
    const ASFPlayerController* Player = Cast<ASFPlayerController>(PC);
    const bool bBuildMode = PC->IsA<ASFConstructionSandboxController>() || (Player && Player->IsBuildModeActive());
    if (bBuildMode != bBuildMappingActive)
    {
        bBuildMappingActive = bBuildMode;
        if (bBuildMode) { Subsystem->AddMappingContext(BuildMapping, 1); } else { Subsystem->RemoveMappingContext(BuildMapping); }
    }

    // A SetControlRotation from elsewhere (combat aim, construction camera aim, review scripts) moves the camera to it.
    FRotator Control = PC->GetControlRotation();
    if (!FMath::IsNearlyEqual(FRotator::NormalizeAxis(Control.Yaw - LastControlYaw), 0.0f, 0.01f)
        || !FMath::IsNearlyEqual(FRotator::NormalizeAxis(Control.Pitch - LastControlPitch), 0.0f, 0.01f))
    {
        CameraRotation.Yaw = Control.Yaw;
        CameraRotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Control.Pitch), S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
    }

    // LMB/RMB: a press becomes a camera drag once the cursor moves or the press outlasts a click; then the cursor is
    // hidden and locked, and it comes back where it was when the last drag ends. A short still press is a click.
    const double Now = GetWorld()->GetRealTimeSeconds();
    float CursorX = 0, CursorY = 0;
    PC->GetMousePosition(CursorX, CursorY);
    bool bClickOrbit = false, bClickMouseLook = false;
    const auto UpdateButton = [&](FSFMouseButtonState& B, EAction A, bool& bClicked)
    {
        if (Pressed(A)) { B = FSFMouseButtonState{ true, false, Now, FVector2D(CursorX, CursorY) }; }
        if (B.bDown && !B.bDragging && (Now - B.DownAt > S->ClickMaxSeconds
            || FVector2D::Distance(B.DownCursor, FVector2D(CursorX, CursorY)) > S->ClickDragThresholdPixels))
        { B.bDragging = true; }
        if (Released(A)) { bClicked = B.bDown && !B.bDragging; B.bDown = B.bDragging = false; }
    };
    const FVector2D RestoreCursor = OrbitButton.bDragging ? OrbitButton.DownCursor : MouseLookButton.DownCursor;
    UpdateButton(OrbitButton, IA_Orbit, bClickOrbit);
    UpdateButton(MouseLookButton, IA_MouseLook, bClickMouseLook);
    const bool bDragging = OrbitButton.bDragging || MouseLookButton.bDragging;
    if (bDragging != bMouseCaptured)
    {
        SetMouseCaptured(bDragging);
        if (!bDragging) { PC->SetMouseLocation(FMath::RoundToInt(RestoreCursor.X), FMath::RoundToInt(RestoreCursor.Y)); }
    }
    if (bClickOrbit) { PilotAttack(); }
    if (bClickMouseLook) { Interact(); }

    // Camera rotation: mouse while dragging, right stick always (mouselook semantics).
    const FVector2D Stick = Value(IA_LookStick).Get<FVector2D>();
    FVector2D LookDelta = Stick * S->GamepadLookDegreesPerSecond * Dt;
    if (bDragging) { LookDelta += Value(IA_Look).Get<FVector2D>() * S->MouseLookDegreesPerCount; }
    CameraRotation.Yaw += LookDelta.X;
    CameraRotation.Pitch = FMath::Clamp(CameraRotation.Pitch + LookDelta.Y * (S->bInvertLookY ? -1.0f : 1.0f), S->CameraPitchMinDegrees, S->CameraPitchMaxDegrees);
    const bool bMouseLook = MouseLookButton.bDragging || !Stick.IsNearlyZero(0.05f);

    // Facing: RMB or the stick snaps the character to the camera; otherwise A/D turn it (and the camera behind it,
    // unless LMB is orbiting) or strafe when the setting says so.
    const float TurnAxis = Axis(IA_TurnRight) - Axis(IA_TurnLeft);
    const bool bTurnKeysStrafe = bMouseLook || S->bStrafeWithAD;
    if (bMouseLook) { Control.Yaw = CameraRotation.Yaw; }
    else if (!bTurnKeysStrafe && TurnAxis != 0.0f)
    {
        const float Turn = TurnAxis * S->TurnRateDegreesPerSecond * Dt;
        Control.Yaw += Turn;
        if (!OrbitButton.bDragging) { CameraRotation.Yaw += Turn; }
    }

    // Movement in the facing frame. Walking scales the analog input, which CharacterMovement replicates.
    float Forward = Axis(IA_MoveForward) - Axis(IA_MoveBack);
    const float Strafe = Axis(IA_StrafeRight) - Axis(IA_StrafeLeft) + (bTurnKeysStrafe ? TurnAxis : 0.0f);
    if (Pressed(IA_Autorun)) { bAutorun = !bAutorun; }
    if (Forward != 0.0f) { bAutorun = false; }
    if (bAutorun || (OrbitButton.bDown && MouseLookButton.bDown)) { Forward = 1.0f; }
    if (Pressed(IA_WalkToggle)) { bWalkToggled = !bWalkToggled; }
    const FRotator Facing(0.0f, Control.Yaw, 0.0f);
    FVector Move = FRotationMatrix(Facing).GetUnitAxis(EAxis::X) * Forward + FRotationMatrix(Facing).GetUnitAxis(EAxis::Y) * Strafe;
    Move = Move.GetClampedToMaxSize(1.0f) * ((IsHeld(IA_Walk) != bWalkToggled) ? S->WalkInputScale : 1.0f);
    if (!Move.IsNearlyZero()) { AddMovementInput(Move, 1.0f); }
    if (Pressed(IA_Jump)) { ACharacter::Jump(); }
    if (Released(IA_Jump)) { StopJumping(); }
    if (Pressed(IA_Interact)) { Interact(); }

    // Optional follow: swing behind the character while it moves and the player is not steering the camera.
    if (S->bCameraFollowWhileMoving && !bDragging && !bMouseLook && GetVelocity().Size2D() > 10.0f)
    { CameraRotation.Yaw = FMath::FInterpTo(CameraRotation.Yaw, CameraRotation.Yaw + FRotator::NormalizeAxis(Control.Yaw - CameraRotation.Yaw), Dt, S->CameraFollowInterpSpeed); }

    // Zoom: wheel steps the target length; in past the minimum is first person (owner mesh hidden), out leaves it.
    const float Wheel = Axis(IA_Zoom);
    if (Wheel > 0.0f && !bFirstPerson)
    {
        DesiredArmCm -= Wheel * S->CameraZoomStepCm;
        if (DesiredArmCm < S->CameraZoomMinCm) { bFirstPerson = true; DesiredArmCm = S->CameraZoomMinCm; }
    }
    else if (Wheel < 0.0f)
    {
        if (bFirstPerson) { bFirstPerson = false; }
        else { DesiredArmCm = FMath::Min(DesiredArmCm - Wheel * S->CameraZoomStepCm, S->CameraZoomMaxCm); }
    }
    CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, bFirstPerson ? 0.0f : DesiredArmCm, Dt, S->CameraZoomInterpSpeed);
    const bool bHideBody = bFirstPerson && CameraBoom->TargetArmLength < S->CameraZoomMinCm * 0.5f;
    if (GetMesh()->bOwnerNoSee != bHideBody) { GetMesh()->SetOwnerNoSee(bHideBody); }
    CameraBoom->bEnableCameraLag = S->CameraLagSpeed > 0.0f && !bFirstPerson;

    CameraRotation.Yaw = FRotator::NormalizeAxis(CameraRotation.Yaw);
    CameraBoom->SetWorldRotation(FRotator(CameraRotation.Pitch, CameraRotation.Yaw, 0.0f));
    Control.Yaw = FRotator::NormalizeAxis(Control.Yaw);
    Control.Pitch = CameraRotation.Pitch;
    PC->SetControlRotation(Control);
    LastControlYaw = Control.Yaw;
    LastControlPitch = Control.Pitch;
}

void ASFE1Character::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    if (UsesCameraControls()) { return; }  // CTL-01 input is polled from IMC_Player in TickCameraControls
    Input->BindAxis(TEXT("MoveForward"), this, &ASFE1Character::MoveForward);
    Input->BindAxis(TEXT("MoveRight"), this, &ASFE1Character::MoveRight);
    Input->BindAxis(TEXT("LookRight"), this, &ASFE1Character::LookRight);
    Input->BindAxis(TEXT("LookUp"), this, &ASFE1Character::LookUp);
    Input->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &ASFE1Character::Interact);
    Input->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASFE1Character::PilotAttack);
}

void ASFE1Character::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFE1Character, PilotHealth);
}

void ASFE1Character::CombatSetHealth(int32 Value)
{
    if (HasAuthority()) { PilotHealth = Value; ForceNetUpdate(); }
}

void ASFE1Character::PilotAttack()
{
    if (!IsLocallyControlled()) { return; }
    AActor* Selected = Cast<ASFPlayerController>(Controller) ? Cast<ASFPlayerController>(Controller)->GetSelectedCombatTarget() : nullptr;
    if (ASFTownsfolkNpc* Npc = Cast<ASFTownsfolkNpc>(Selected))
    {
        if (USFTownsfolkHandoffComponent* Link = Controller->FindComponentByClass<USFTownsfolkHandoffComponent>())
        { Link->ServerAttackSelectedNpc(Npc); }
        return;
    }
    if (Selected) { if (PilotHealth > 0) { ServerPilotAttack(Selected); } return; }
    // A townsfolk resident in reach takes the swing (hand-off); otherwise it goes to the combat pilot.
    if (USFTownsfolkSubsystem* Town = GetWorld()->GetSubsystem<USFTownsfolkSubsystem>(); Town && Town->LocalAttack(this)) { return; }
    if (PilotHealth > 0) { ServerPilotAttack(nullptr); }
}

void ASFE1Character::ServerPilotAttack_Implementation(AActor* SelectedCombatTarget)
{
    if (auto* Mode = Cast<ASFGameMode>(GetWorld()->GetAuthGameMode()))
    { Mode->TryPlayerAttack(this, SelectedCombatTarget); }
}

void ASFE1Character::Interact()
{
    if (ASFPlayerController* PlayerController = Cast<ASFPlayerController>(Controller))
    {
        PlayerController->InteractWithSelectedTarget();
    }
}

void ASFE1Character::MoveForward(float Value)
{
    if (Controller && Value != 0.0f)
    {
        const FRotator Yaw(0, Controller->GetControlRotation().Yaw, 0);
        AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Value);
    }
}

void ASFE1Character::MoveRight(float Value)
{
    if (Controller && Value != 0.0f)
    {
        const FRotator Yaw(0, Controller->GetControlRotation().Yaw, 0);
        AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Value);
    }
}

void ASFE1Character::LookRight(float Value)
{
    AddControllerYawInput(Value * GetDefault<USFE1Settings>()->LookSensitivity);
}

void ASFE1Character::LookUp(float Value)
{
    AddControllerPitchInput(Value * GetDefault<USFE1Settings>()->LookSensitivity);
}
