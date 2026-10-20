#include "Creatures/SFVoidreignCharacter.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    const FName RiderSeatBone(TEXT("RiderSeat"));
    const FName ProgressParam(TEXT("Progress"));
    constexpr float TrailInterval = 0.12f;
    constexpr float HornForward = 200.f;  // horn tip ahead of the capsule centre, cm
    constexpr float HornUp = 95.f;
}

ASFVoidreignCharacter::ASFVoidreignCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    RiderMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RiderMesh"));
    RiderMesh->SetupAttachment(GetMesh(), RiderSeatBone);
    RiderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASFVoidreignCharacter::BeginPlay()
{
    Super::BeginPlay();
    RiderMesh->SetVisibility(bShowRider);
    if (RiderLocomotion) RiderMesh->PlayAnimation(RiderLocomotion, true);
}

float ASFVoidreignCharacter::ClipLength(const UAnimSequence* Clip) const
{
    return Clip ? Clip->GetPlayLength() : 0.5f;
}

void ASFVoidreignCharacter::PlayMount(UAnimSequence* Clip, bool bLoop)
{
    if (Clip) GetMesh()->PlayAnimation(Clip, bLoop);  // temporarily replaces ABP locomotion
}

void ASFVoidreignCharacter::PlayRider(UAnimSequence* Clip)
{
    if (Clip && bShowRider) RiderMesh->PlayAnimation(Clip, false);
}

void ASFVoidreignCharacter::PlayBasicAttack()
{
    if (Phase != EPhase::None) return;
    GetCharacterMovement()->DisableMovement();
    PlayMount(MountAttack, false);
    PlayRider(RiderAttack);
    EnterPhase(EPhase::Attack);
}

void ASFVoidreignCharacter::PlayRiftRecall()
{
    if (Phase != EPhase::None) return;
    GetCharacterMovement()->DisableMovement();
    Anchor = GetActorTransform();
    const FVector Ground = GetActorLocation() - FVector(0, 0, GetSimpleCollisionHalfHeight());
    SpawnVfx(VfxRiftOpen, Ground + FVector(0, 0, 2), 3.f, 0.9f, false);
    SpawnVfx(VfxRiftAnchor, Ground + FVector(0, 0, 3), 2.f, 0.f, true);
    AnchorVfx = Vfx.Num() > 0 ? Vfx.Last().Component : nullptr;
    PlayMount(RiftWindup, false);
    PlayRider(RiderBrace);
    EnterPhase(EPhase::Windup);
}

void ASFVoidreignCharacter::EnterPhase(EPhase NewPhase)
{
    Phase = NewPhase;
    PhaseTime = 0.f;
    bHitSpawned = false;
}

void ASFVoidreignCharacter::EndAction()
{
    Phase = EPhase::None;
    GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);  // back to ABP locomotion
    if (RiderLocomotion && bShowRider) RiderMesh->PlayAnimation(RiderLocomotion, true);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ASFVoidreignCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    TickVfx(DeltaSeconds);

    if (Phase == EPhase::None)
    {
        if (UAnimSingleNodeInstance* Rider = RiderMesh->GetSingleNodeInstance())
        {
            if (Rider->GetAnimationAsset() == RiderLocomotion) Rider->SetBlendSpacePosition(FVector(GetVelocity().Size2D(), 0.f, 0.f));
        }
        if (DemoAbilityInterval > 0.f && (DemoTimer += DeltaSeconds) >= DemoAbilityInterval)
        {
            DemoTimer = 0.f;
            bDemoNextIsRift ? PlayRiftRecall() : PlayBasicAttack();
            bDemoNextIsRift = !bDemoNextIsRift;
        }
        return;
    }

    PhaseTime += DeltaSeconds;
    const FVector Horn = GetActorLocation() + GetActorForwardVector() * HornForward + FVector(0, 0, HornUp);
    const FVector Ground = GetActorLocation() - FVector(0, 0, GetSimpleCollisionHalfHeight());
    switch (Phase)
    {
    case EPhase::Attack:
        if (!bHitSpawned && PhaseTime >= 0.5f * ClipLength(MountAttack))  // clip event 'hit' at frame 18 of 36
        {
            bHitSpawned = true;
            SpawnVfx(VfxSpark, Horn, 0.8f, 0.25f, false, true);
        }
        if (PhaseTime >= ClipLength(MountAttack)) EndAction();
        break;
    case EPhase::Windup:
        if (PhaseTime >= ClipLength(RiftWindup))
        {
            ChargeTravelled = 0.f;
            TrailTimer = 0.f;
            PlayMount(RiftChargeRun, true);
            EnterPhase(EPhase::Charge);
        }
        break;
    case EPhase::Charge:
    {
        const float Step = FMath::Min(ChargeSpeed * DeltaSeconds, ChargeDistance - ChargeTravelled);
        FHitResult Hit;
        AddActorWorldOffset(GetActorForwardVector() * Step, true, &Hit);  // swept: stops at obstacles
        ChargeTravelled += Step;
        if ((TrailTimer += DeltaSeconds) >= TrailInterval)
        {
            TrailTimer = 0.f;
            SpawnVfx(VfxRiftTrail, Ground + FVector(0, 0, 2), 1.4f, 0.5f, false);
        }
        if (Hit.bBlockingHit || ChargeTravelled >= ChargeDistance)
        {
            PlayMount(RiftImpact, false);
            SpawnVfx(VfxSpark, Horn, 1.2f, 0.3f, false, true);
            EnterPhase(EPhase::Impact);
        }
        break;
    }
    case EPhase::Impact:
        if (PhaseTime >= ClipLength(RiftImpact)) EnterPhase(EPhase::Hold);
        break;
    case EPhase::Hold:
        if (PhaseTime >= RecallDelay)
        {
            PlayMount(RecallEnter, false);
            PlayRider(RiderRecallHold);
            EnterPhase(EPhase::RecallEnter);
        }
        break;
    case EPhase::RecallEnter:
        if (PhaseTime >= ClipLength(RecallEnter))
        {
            SpawnVfx(VfxRiftCollapse, Ground + FVector(0, 0, 3), 2.4f, 0.6f, false);
            TeleportTo(Anchor.GetLocation(), Anchor.Rotator(), false, true);
            SpawnVfx(VfxRiftCollapse, Anchor.GetLocation() - FVector(0, 0, GetSimpleCollisionHalfHeight() - 3), 2.4f, 0.6f, false);
            for (FSFVoidreignVfx& Effect : Vfx)
            {
                if (Effect.Component == AnchorVfx) { Effect.bPersistent = false; Effect.Duration = Effect.Age + 0.4f; }
            }
            AnchorVfx = nullptr;
            PlayMount(RecallExit, false);
            EnterPhase(EPhase::RecallExit);
        }
        break;
    case EPhase::RecallExit:
        if (PhaseTime >= ClipLength(RecallExit)) EndAction();
        break;
    default:
        break;
    }
}

void ASFVoidreignCharacter::SpawnVfx(UMaterialInterface* Material, const FVector& Location, float SizeMetres, float Duration, bool bPersistent, bool bUpright)
{
    if (!Material || !VfxPlane) return;
    UStaticMeshComponent* Plane = NewObject<UStaticMeshComponent>(this);
    Plane->SetStaticMesh(VfxPlane);
    Plane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Plane->SetCastShadow(false);
    Plane->SetWorldLocationAndRotation(Location, bUpright ? FRotator(90.f, GetActorRotation().Yaw, 0.f) : FRotator::ZeroRotator);
    Plane->SetWorldScale3D(FVector(SizeMetres));  // engine plane is 1 m square
    Plane->RegisterComponent();
    FSFVoidreignVfx& Effect = Vfx.AddDefaulted_GetRef();
    Effect.Component = Plane;
    Effect.Material = UMaterialInstanceDynamic::Create(Material, this);
    Effect.Duration = FMath::Max(Duration, 0.05f);
    Effect.bPersistent = bPersistent;
    Plane->SetMaterial(0, Effect.Material);
    Effect.Material->SetScalarParameterValue(ProgressParam, 0.f);
}

void ASFVoidreignCharacter::TickVfx(float DeltaSeconds)
{
    for (int32 Index = Vfx.Num() - 1; Index >= 0; --Index)
    {
        FSFVoidreignVfx& Effect = Vfx[Index];
        Effect.Age += DeltaSeconds;
        const float Progress = Effect.bPersistent
            ? 0.7f + 0.3f * FMath::Sin(Effect.Age * 4.f)
            : FMath::Sin(PI * FMath::Clamp(Effect.Age / Effect.Duration, 0.f, 1.f));
        if (Effect.Material) Effect.Material->SetScalarParameterValue(ProgressParam, Progress);
        if (!Effect.bPersistent && Effect.Age >= Effect.Duration)
        {
            if (Effect.Component) Effect.Component->DestroyComponent();
            Vfx.RemoveAtSwap(Index);
        }
    }
}
