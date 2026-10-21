#include "Creatures/SFVoidreignCharacter.h"

#include "Combat/SFCrowdControlComponent.h"
#include "Creatures/SFCompanionMilestones.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"

namespace
{
    const FName RiderSeatBone(TEXT("RiderSeat"));
    const FName MountSlot(TEXT("DefaultSlot"));
    const FName ProgressParam(TEXT("Progress"));
    constexpr float TrailInterval = 0.12f;
    constexpr float HornForward = 200.f;  // horn tip ahead of the capsule centre, cm
    constexpr float HornUp = 95.f;
}

bool USFVoidreignSettings::Validate(FString& Error) const
{
    const bool bOk = RiftRecallWindowMs >= 1500 && RiftRecallWindowMs <= 4000 && RiftRecallWindowMs % 250 == 0 &&
        FMath::IsWithinInclusive(RiftAnchorMaxMetres, 8.f, 15.f) && FMath::IsNearlyEqual(FMath::Fmod(RiftAnchorMaxMetres, 0.5f), 0.f) &&
        RiftRiderLockoutMs >= 250 && RiftRiderLockoutMs <= 1000 && RiftRiderLockoutMs % 50 == 0 &&
        RiftCollapseDelayMs >= 500 && RiftCollapseDelayMs <= 1500 && RiftCollapseDelayMs % 100 == 0 &&
        RiftCollapseSnareBp >= 1500 && RiftCollapseSnareBp <= 4000 && RiftCollapseSnareBp % 100 == 0 &&
        RiftCollapseSnareMs >= 1000 && RiftCollapseSnareMs <= 3000 && RiftCollapseSnareMs % 250 == 0 &&
        LongShadowWindowMs >= 3000 && LongShadowWindowMs <= 5000 && LongShadowWindowMs % 250 == 0 &&
        LongShadowHoldMs >= 1000 && LongShadowHoldMs <= 3000 && LongShadowHoldMs % 250 == 0;
    if (!bOk) Error = TEXT("Rift Recall / level-70 option settings outside their published bounds or steps");
    return bOk;
}

ESFRiftRecallReject SFRiftRecallRules::Validate(bool bHasAnchor, double Now, double WindowEnd, double DistanceCm,
    double AnchorMaxCm, bool bLineOfSight, double PathRatio, bool bIncapacitated)
{
    if (!bHasAnchor) return ESFRiftRecallReject::NoAnchor;
    if (Now > WindowEnd) return ESFRiftRecallReject::WindowClosed;
    if (bIncapacitated) return ESFRiftRecallReject::Incapacitated;
    if (DistanceCm > AnchorMaxCm) return ESFRiftRecallReject::TooFar;
    if (!bLineOfSight) return ESFRiftRecallReject::NoLineOfSight;
    if (PathRatio <= 0.0 || PathRatio > MaxPathRatio) return ESFRiftRecallReject::PathBlocked;
    return ESFRiftRecallReject::None;
}

ASFVoidreignCharacter::ASFVoidreignCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    CrowdControl = CreateDefaultSubobject<USFCrowdControlComponent>(TEXT("CrowdControl"));
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

void ASFVoidreignCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFVoidreignCharacter, Phase);
    DOREPLIFETIME(ASFVoidreignCharacter, AnchorLocation);
}

void ASFVoidreignCharacter::PlayMount(UAnimSequence* Clip, bool bLoop)
{
    // A dynamic montage in ABP_Voidreign's DefaultSlot blends over the locomotion graph; the next montage replaces it.
    UAnimInstance* Anim = GetMesh()->GetAnimInstance();
    if (Clip && Anim) Anim->PlaySlotAnimationAsDynamicMontage(Clip, MountSlot, 0.12f, 0.2f, 1.f, bLoop ? 1000 : 1);
}

void ASFVoidreignCharacter::PlayRider(UAnimSequence* Clip)
{
    if (Clip && bShowRider) RiderMesh->PlayAnimation(Clip, false);
}

void ASFVoidreignCharacter::PlayBasicAttack()
{
    if (!HasAuthority() || Phase != ESFVoidreignPhase::None) return;
    GetCharacterMovement()->DisableMovement();
    EnterPhase(ESFVoidreignPhase::Attack);
}

void ASFVoidreignCharacter::PlayRiftRecall()
{
    if (!HasAuthority() || Phase != ESFVoidreignPhase::None) return;
    const USFVoidreignSettings* Settings = GetDefault<USFVoidreignSettings>();
    const bool bLongShadow = RiftOption == SFCompanionMilestones::LongShadow;
    WindowSeconds = (bLongShadow ? Settings->LongShadowWindowMs : Settings->RiftRecallWindowMs) / 1000.f;
    HoldMaxSeconds = bLongShadow ? Settings->LongShadowHoldMs / 1000.f : 0.f;
    HeldSeconds = 0.f;
    bHolding = false;
    bCollapseArmed = RiftOption == SFCompanionMilestones::RiftCollapse;
    CollapseDelaySeconds = Settings->RiftCollapseDelayMs / 1000.f;
    CollapseSnareBp = Settings->RiftCollapseSnareBp;
    CollapseSnareMs = Settings->RiftCollapseSnareMs;
    AnchorMaxCm = Settings->RiftAnchorMaxMetres * 100.f;
    LastReject = ESFRiftRecallReject::None;
    GetCharacterMovement()->DisableMovement();
    Anchor = GetActorTransform();
    AnchorLocation = Anchor.GetLocation();
    EnterPhase(ESFVoidreignPhase::Windup);
}

void ASFVoidreignCharacter::SetRiftOption(FName Option)
{
    if (!HasAuthority()) return;
    RiftOption = (Option == SFCompanionMilestones::LongShadow || Option == SFCompanionMilestones::RiftCollapse) ? Option : NAME_None;
}

void ASFVoidreignCharacter::HoldRiftAnchor(bool bHold)
{
    if (HasAuthority()) ServerHoldRiftAnchor_Implementation(bHold);
    else ServerHoldRiftAnchor(bHold);
}

void ASFVoidreignCharacter::ServerHoldRiftAnchor_Implementation(bool bHold)
{
    // Only Long Shadow has a hold budget, only while the window is open; the budget is spent once per charge.
    bHolding = bHold && Phase == ESFVoidreignPhase::Hold && HeldSeconds < HoldMaxSeconds;
}

void ASFVoidreignCharacter::MulticastRiftCollapse_Implementation(FVector_NetQuantize Point)
{
    SpawnVfx(VfxRiftCollapse, FVector(Point) - FVector(0, 0, GetSimpleCollisionHalfHeight() - 3.f), 3.f, 0.8f, false);
}

void ASFVoidreignCharacter::RequestRiftRecall()
{
    if (HasAuthority()) TryRiftRecall();
    else ServerRiftRecall();
}

void ASFVoidreignCharacter::ServerRiftRecall_Implementation()
{
    TryRiftRecall();
}

bool ASFVoidreignCharacter::IsRiderLockedOut() const
{
    return LocalTime < RiderLockedUntil;
}

void ASFVoidreignCharacter::TryRiftRecall()
{
    const FVector Here = GetActorLocation(), There = Anchor.GetLocation();
    FCollisionQueryParams Params(TEXT("RiftRecall"), false, this);
    const bool bLineOfSight = !GetWorld()->LineTraceTestByChannel(Here, There, ECC_Visibility, Params);
    // ponytail: straight capsule sweep only. A clear sweep is a 1.0 path; a blocked one is refused because the
    // 1.5x detour cannot be proven without navigation data. Use a nav path once mount maps carry navmesh.
    const UCapsuleComponent* Capsule = GetCapsuleComponent();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() * 0.9f, Capsule->GetScaledCapsuleHalfHeight() * 0.9f);
    const bool bClear = !GetWorld()->SweepTestByChannel(Here, There, FQuat::Identity, ECC_Pawn, Shape, Params);
    // ponytail: only stun/root are crowd control so far; add fear/incapacitate to IsImmobilised when those categories exist.
    LastReject = SFRiftRecallRules::Validate(Phase == ESFVoidreignPhase::Hold, PhaseTime - HeldSeconds, WindowSeconds, FVector::Dist(Here, There),
        AnchorMaxCm, bLineOfSight, bClear ? 1.0 : 0.0, CrowdControl->IsImmobilised());
    if (LastReject != ESFRiftRecallReject::None) return;  // anchor kept until the window expires
    RiderLockoutSeconds = GetDefault<USFVoidreignSettings>()->RiftRiderLockoutMs / 1000.f;
    RiderLockedUntil = TNumericLimits<double>::Max();  // until arrival plus the lockout
    bHolding = false;
    if (bCollapseArmed)  // the rift at the impact point implodes later; the snare itself is the CC system's (OnRiftCollapse)
    {
        bCollapsePending = true;
        CollapseTimer = CollapseDelaySeconds;
        CollapsePoint = Here;
    }
    EnterPhase(ESFVoidreignPhase::RecallEnter);
}

void ASFVoidreignCharacter::EnterPhase(ESFVoidreignPhase NewPhase)
{
    Phase = NewPhase;
    BeginPhasePresentation();
}

void ASFVoidreignCharacter::OnRep_Phase()
{
    BeginPhasePresentation();
}

void ASFVoidreignCharacter::ReleaseAnchorVfx()
{
    for (FSFVoidreignVfx& Effect : Vfx)
    {
        if (AnchorVfx && Effect.Component == AnchorVfx) { Effect.bPersistent = false; Effect.Duration = Effect.Age + 0.4f; }
    }
    AnchorVfx = nullptr;
}

void ASFVoidreignCharacter::BeginPhasePresentation()
{
    PhaseTime = 0.f;
    bHitSpawned = false;
    const FVector Ground = AnchorLocation - FVector(0, 0, GetSimpleCollisionHalfHeight());
    const FVector Horn = GetActorLocation() + GetActorForwardVector() * HornForward + FVector(0, 0, HornUp);
    // The anchor lives from Windup through RecallEnter; a client that missed Windup still gets it from a later phase.
    const bool bAnchored = Phase >= ESFVoidreignPhase::Windup && Phase <= ESFVoidreignPhase::RecallEnter;
    if (bAnchored && !AnchorVfx) AnchorVfx = SpawnVfx(VfxRiftAnchor, Ground + FVector(0, 0, 3), 2.f, 0.f, true);
    switch (Phase)
    {
    case ESFVoidreignPhase::None:
        ReleaseAnchorVfx();  // window expired without a recall
        if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.2f);
        if (RiderLocomotion && bShowRider) RiderMesh->PlayAnimation(RiderLocomotion, true);
        break;
    case ESFVoidreignPhase::Attack:
        PlayMount(MountAttack, false);
        PlayRider(RiderAttack);
        break;
    case ESFVoidreignPhase::Windup:
        SpawnVfx(VfxRiftOpen, Ground + FVector(0, 0, 2), 3.f, 0.9f, false);
        PlayMount(RiftWindup, false);
        PlayRider(RiderBrace);
        break;
    case ESFVoidreignPhase::Charge:
        TrailTimer = 0.f;
        PlayMount(RiftChargeRun, true);
        break;
    case ESFVoidreignPhase::Impact:
        PlayMount(RiftImpact, false);
        SpawnVfx(VfxSpark, Horn, 1.2f, 0.3f, false, true);
        break;
    case ESFVoidreignPhase::RecallEnter:
        PlayMount(RecallEnter, false);
        PlayRider(RiderRecallHold);
        break;
    case ESFVoidreignPhase::RecallExit:
        SpawnVfx(VfxRiftCollapse, Ground + FVector(0, 0, 3), 2.4f, 0.6f, false);  // arrival; the departure one is timed in Tick
        ReleaseAnchorVfx();
        PlayMount(RecallExit, false);
        break;
    default:
        break;
    }
}

void ASFVoidreignCharacter::EndAction()
{
    EnterPhase(ESFVoidreignPhase::None);
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ASFVoidreignCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    LocalTime += DeltaSeconds;
    TickVfx(DeltaSeconds);

    if (bCollapsePending && HasAuthority() && (CollapseTimer -= DeltaSeconds) <= 0.f)
    {
        bCollapsePending = false;
        MulticastRiftCollapse(CollapsePoint);
        USFCrowdControlComponent::ApplyInRadius(GetWorld(), CollapsePoint, SFRiftRecallRules::CollapseRadiusCm, this, ESFCrowdControl::Snare, CollapseSnareBp, CollapseSnareMs);
        OnRiftCollapse.Broadcast(CollapsePoint, SFRiftRecallRules::CollapseRadiusCm, CollapseSnareBp, CollapseSnareMs);
    }

    if (Phase == ESFVoidreignPhase::None)
    {
        if (UAnimSingleNodeInstance* Rider = RiderMesh->GetSingleNodeInstance())
        {
            if (Rider->GetAnimationAsset() == RiderLocomotion) Rider->SetBlendSpacePosition(FVector(GetVelocity().Size2D(), 0.f, 0.f));
        }
        if (HasAuthority() && DemoAbilityInterval > 0.f && (DemoTimer += DeltaSeconds) >= DemoAbilityInterval)
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

    // Timed effects, identical on the server and every client.
    if (Phase == ESFVoidreignPhase::Attack && !bHitSpawned && PhaseTime >= 0.5f * ClipLength(MountAttack))  // clip event hit at frame 18 of 36
    {
        bHitSpawned = true;
        SpawnVfx(VfxSpark, Horn, 0.8f, 0.25f, false, true);
    }
    else if (Phase == ESFVoidreignPhase::Charge && (TrailTimer += DeltaSeconds) >= TrailInterval)
    {
        TrailTimer = 0.f;
        SpawnVfx(VfxRiftTrail, Ground + FVector(0, 0, 2), 1.4f, 0.5f, false);
    }
    else if (Phase == ESFVoidreignPhase::RecallEnter && !bHitSpawned && PhaseTime >= ClipLength(RecallEnter))
    {
        bHitSpawned = true;  // departure collapse, just before the server teleports
        SpawnVfx(VfxRiftCollapse, Ground + FVector(0, 0, 3), 2.4f, 0.6f, false);
    }

    if (!HasAuthority()) return;  // clients follow the replicated phase and location

    switch (Phase)
    {
    case ESFVoidreignPhase::Attack:
        if (PhaseTime >= ClipLength(MountAttack)) EndAction();
        break;
    case ESFVoidreignPhase::Windup:
        if (PhaseTime >= ClipLength(RiftWindup))
        {
            ChargeTravelled = 0.f;
            EnterPhase(ESFVoidreignPhase::Charge);
        }
        break;
    case ESFVoidreignPhase::Charge:
    {
        const float Step = FMath::Min(ChargeSpeed * DeltaSeconds, ChargeDistance - ChargeTravelled);
        FHitResult Hit;
        AddActorWorldOffset(GetActorForwardVector() * Step, true, &Hit);  // swept: stops at obstacles
        ChargeTravelled += Step;
        if (Hit.bBlockingHit || ChargeTravelled >= ChargeDistance) EnterPhase(ESFVoidreignPhase::Impact);
        break;
    }
    case ESFVoidreignPhase::Impact:
        if (PhaseTime >= ClipLength(RiftImpact)) EnterPhase(ESFVoidreignPhase::Hold);
        break;
    case ESFVoidreignPhase::Hold:  // return window: the recall is the player's re-press (RequestRiftRecall)
        if (bHolding && (HeldSeconds += DeltaSeconds) >= HoldMaxSeconds) { HeldSeconds = HoldMaxSeconds; bHolding = false; }
        if (DemoAbilityInterval > 0.f && PhaseTime >= RecallDelay) TryRiftRecall();
        if (Phase == ESFVoidreignPhase::Hold && PhaseTime - HeldSeconds > WindowSeconds) EndAction();  // no recall: the anchor is lost
        break;
    case ESFVoidreignPhase::RecallEnter:
        if (PhaseTime >= ClipLength(RecallEnter))
        {
            TeleportTo(Anchor.GetLocation(), Anchor.Rotator(), false, true);
            RiderLockedUntil = LocalTime + RiderLockoutSeconds;
            EnterPhase(ESFVoidreignPhase::RecallExit);
        }
        break;
    case ESFVoidreignPhase::RecallExit:
        if (PhaseTime >= ClipLength(RecallExit)) EndAction();
        break;
    default:
        break;
    }
}

UStaticMeshComponent* ASFVoidreignCharacter::SpawnVfx(UMaterialInterface* Material, const FVector& Location, float SizeMetres, float Duration, bool bPersistent, bool bUpright)
{
    if (!Material || !VfxPlane) return nullptr;
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
    return Plane;
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
