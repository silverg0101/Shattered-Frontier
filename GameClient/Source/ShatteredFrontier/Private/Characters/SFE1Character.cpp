#include "Characters/SFE1Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Framework/SFE1Settings.h"
#include "Interaction/SFE1PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

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

    FixtureBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureBody"));
    FixtureBody->SetupAttachment(GetCapsuleComponent());
    FixtureBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Fixed graybox geometry, not a character/stat or runtime tuning profile.
    FixtureBody->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.5f));
    if (!IsRunningDedicatedServer())
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
        FixtureBody->SetStaticMesh(Cube.Object);
    }

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = Settings->CameraBoomLengthCm;
    CameraBoom->bUsePawnControlRotation = true;
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
}

void ASFE1Character::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    Input->BindAxis(TEXT("MoveForward"), this, &ASFE1Character::MoveForward);
    Input->BindAxis(TEXT("MoveRight"), this, &ASFE1Character::MoveRight);
    Input->BindAxis(TEXT("LookRight"), this, &ASFE1Character::LookRight);
    Input->BindAxis(TEXT("LookUp"), this, &ASFE1Character::LookUp);
    Input->BindAction(TEXT("Jump"), IE_Pressed, this, &ACharacter::Jump);
    Input->BindAction(TEXT("Jump"), IE_Released, this, &ACharacter::StopJumping);
    Input->BindAction(TEXT("Interact"), IE_Pressed, this, &ASFE1Character::Interact);
}

void ASFE1Character::Interact()
{
    if (ASFE1PlayerController* PlayerController = Cast<ASFE1PlayerController>(Controller))
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
