#include "Combat/SFCrowdControlComponent.h"

#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

bool USFCrowdControlSettings::Validate(FString& Error) const
{
    const bool bOk = DrWindowMs >= 5000 && DrWindowMs <= 60000 && DrWindowMs % 1000 == 0 &&
        ReductionCapBp >= 0 && ReductionCapBp <= 6000 && ReductionCapBp % 100 == 0 &&
        SnareCapBp >= 3000 && SnareCapBp <= 9000 && SnareCapBp % 500 == 0;
    if (!bOk) Error = TEXT("Crowd-control settings outside their published bounds or steps");
    return bOk;
}

int32 SFCrowdControlRules::DrStepPercent(int32 PriorApplications)
{
    return PriorApplications <= 0 ? 100 : PriorApplications == 1 ? 50 : PriorApplications == 2 ? 25 : 0;
}

USFCrowdControlComponent::USFCrowdControlComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void USFCrowdControlComponent::BeginPlay()
{
    Super::BeginPlay();
    ApplyMovement();  // caches the unmodified walk speed before any status lands
}

void USFCrowdControlComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USFCrowdControlComponent, SpeedMultBp);
    DOREPLIFETIME(USFCrowdControlComponent, Flags);
}

void USFCrowdControlComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (GetOwner()->HasAuthority()) Advance(DeltaTime);
}

int32 USFCrowdControlComponent::Apply(ESFCrowdControl Type, int32 StrengthBp, int32 DurationMs, int32 ReductionBp)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || DurationMs <= 0) return 0;
    const USFCrowdControlSettings* Settings = GetDefault<USFCrowdControlSettings>();
    int64 Ms = DurationMs;
    if (Type != ESFCrowdControl::Snare)  // snare is not diminished and keeps its authored time
    {
        FDr& Dr = Type == ESFCrowdControl::Root ? RootDr : StunDr;
        if ((Clock - Dr.LastApplied) * 1000.0 > Settings->DrWindowMs) Dr.Count = 0;
        const int32 Step = SFCrowdControlRules::DrStepPercent(Dr.Count);
        if (Step == 0) return 0;  // immune; an immune hit does not extend the window
        Dr.Count++;
        Dr.LastApplied = Clock;
        const int32 Reduction = FMath::Clamp(ReductionBp, 0, Settings->ReductionCapBp);
        Ms = Ms * Step / 100 * (10000 - Reduction) / 10000;
        if (Ms <= 0) return 0;
    }
    const double End = Clock + Ms / 1000.0;
    const int32 Strength = Type == ESFCrowdControl::Snare ? FMath::Clamp(StrengthBp, 0, Settings->SnareCapBp) : 0;
    FInstance* Same = Type == ESFCrowdControl::Snare ? nullptr : Instances.FindByPredicate([Type](const FInstance& I) { return I.Type == Type; });
    if (Same) Same->EndSeconds = FMath::Max(Same->EndSeconds, End);  // root and stun extend, never stack
    else
    {
        if (Instances.Num() >= SFCrowdControlRules::MaxInstances)  // bounded: drop the soonest to expire
        {
            int32 Soonest = 0;
            for (int32 I = 1; I < Instances.Num(); ++I) if (Instances[I].EndSeconds < Instances[Soonest].EndSeconds) Soonest = I;
            Instances.RemoveAtSwap(Soonest);
        }
        Instances.Add({Type, Strength, End});
    }
    Recompute();
    return (int32)Ms;
}

void USFCrowdControlComponent::Advance(float DeltaSeconds)
{
    Clock += DeltaSeconds;
    if (!Instances.IsEmpty()) Recompute();
}

void USFCrowdControlComponent::ClearRoot()
{
    if (!GetOwner()->HasAuthority()) return;
    Instances.RemoveAll([](const FInstance& Instance) { return Instance.Type == ESFCrowdControl::Root; });
    Recompute();
}

void USFCrowdControlComponent::Recompute()
{
    Instances.RemoveAll([this](const FInstance& I) { return I.EndSeconds <= Clock; });
    uint8 NewFlags = 0;
    int32 Snare = 0;
    for (const FInstance& I : Instances)
    {
        if (I.Type == ESFCrowdControl::Root) NewFlags |= RootFlag;
        else if (I.Type == ESFCrowdControl::Stun) NewFlags |= StunFlag;
        else Snare = FMath::Max(Snare, I.StrengthBp);
    }
    const uint16 NewMult = NewFlags ? 0 : (uint16)(10000 - Snare);
    if (NewFlags == Flags && NewMult == SpeedMultBp) return;
    Flags = NewFlags;
    SpeedMultBp = NewMult;
    ApplyMovement();
}

void USFCrowdControlComponent::OnRep_State()
{
    ApplyMovement();
}

void USFCrowdControlComponent::ApplyMovement()
{
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
    if (!Movement) return;
    if (BaseWalkSpeed < 0.f) BaseWalkSpeed = Movement->MaxWalkSpeed;
    Movement->MaxWalkSpeed = BaseWalkSpeed * SpeedMultBp / 10000.f;
}

int32 USFCrowdControlComponent::ApplyInRadius(UWorld* World, const FVector& Point, float RadiusCm, const AActor* Ignore, ESFCrowdControl Type, int32 StrengthBp, int32 DurationMs)
{
    TArray<FOverlapResult> Hits;
    World->OverlapMultiByObjectType(Hits, Point, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(RadiusCm));
    TSet<AActor*> Seen;
    int32 Affected = 0;
    for (const FOverlapResult& Hit : Hits)
    {
        AActor* Actor = Hit.GetActor();
        if (!Actor || Actor == Ignore || Seen.Contains(Actor)) continue;
        Seen.Add(Actor);
        if (USFCrowdControlComponent* Cc = Actor->FindComponentByClass<USFCrowdControlComponent>())
        {
            if (Cc->Apply(Type, StrengthBp, DurationMs) > 0) ++Affected;
        }
    }
    return Affected;
}
