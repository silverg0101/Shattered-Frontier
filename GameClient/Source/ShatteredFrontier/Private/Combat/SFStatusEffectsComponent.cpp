#include "Combat/SFStatusEffectsComponent.h"
#include "Combat/SFCombatPilot.h"

#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

bool USFStatusEffectsSettings::Validate(FString& Error) const
{
    const auto ValidDuration = [](float Value) { return FMath::IsFinite(Value) && Value >= 1.f && Value <= 30.f; };
    const auto ValidTicks = [](float Duration, int32 Ticks)
    { return Ticks >= 1 && Ticks <= 12 && Duration / Ticks >= .1f; };
    if (FMath::IsFinite(DotRefreshCap) && DotRefreshCap >= 1.f && DotRefreshCap <= 1.5f &&
        ValidDuration(RendDurationSeconds) && ValidTicks(RendDurationSeconds, RendTicks) &&
        ValidDuration(SerpentDurationSeconds) && ValidTicks(SerpentDurationSeconds, SerpentTicks) &&
        ValidDuration(WeakPoisonDurationSeconds) && ValidTicks(WeakPoisonDurationSeconds, WeakPoisonTicks) &&
        WeakPoisonMaxStacks >= 1 && WeakPoisonMaxStacks <= 3 &&
        FMath::IsFinite(WeakPoisonProcChance) && WeakPoisonProcChance >= 0.f && WeakPoisonProcChance <= 1.f &&
        FMath::IsFinite(WeakPoisonIcdSeconds) && WeakPoisonIcdSeconds >= .1f && WeakPoisonIcdSeconds <= 5.f &&
        FMath::IsFinite(PoisonBladeDurationSeconds) && PoisonBladeDurationSeconds >= 1.f && PoisonBladeDurationSeconds <= 60.f &&
        ValidDuration(EntangleDurationSeconds) && ValidDuration(FrostSlowDurationSeconds) &&
        ValidDuration(CripplingSlowDurationSeconds) &&
        FMath::IsFinite(FrostSlowMultiplier) && FrostSlowMultiplier >= .3f && FrostSlowMultiplier <= 1.f &&
        FMath::IsFinite(CripplingSlowMultiplier) && CripplingSlowMultiplier >= .3f && CripplingSlowMultiplier <= 1.f) return true;
    Error = TEXT("Status balance values exceed their published bounds or tick period is below 0.1 s");
    return false;
}

USFStatusEffectsComponent::USFStatusEffectsComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void USFStatusEffectsComponent::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!GetDefault<USFStatusEffectsSettings>()->Validate(Error))
        UE_LOG(LogTemp, Fatal, TEXT("Invalid combat status configuration: %s"), *Error);
    Clock = GetWorld()->GetTimeSeconds();
}

void USFStatusEffectsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USFStatusEffectsComponent, DisplayEffects);
}

void USFStatusEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (GetOwner() && GetOwner()->HasAuthority()) Advance(DeltaTime);
}

bool USFStatusEffectsComponent::Apply(FName EffectId, ESFStatusKind Kind, ESFDispelType DispelType, AActor* Source,
    float DurationSeconds, float TickSeconds, int32 MaxStacks, bool bStack, TFunction<void()> Tick)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() ||
        GetOwner()->ActorHasTag(ASFCombatNpc::OutOfPlaceNpcTag) || EffectId.IsNone() || !IsValid(Source) ||
        !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.f || DurationSeconds > 60.f ||
        !FMath::IsFinite(TickSeconds) || TickSeconds < 0.f || TickSeconds > DurationSeconds ||
        (TickSeconds > 0.f && (TickSeconds < .1f || DurationSeconds / TickSeconds > 60.f)) ||
        MaxStacks < 1 || MaxStacks > 8 || (TickSeconds > 0.f && !Tick)) return false;
    const double Now = Clock;
    Instances.RemoveAll([Now](const FInstance& Instance) { return Instance.ExpiresAt <= Now; });
    FInstance* Existing = Instances.FindByPredicate([EffectId, Source](const FInstance& Instance)
    { return Instance.EffectId == EffectId && Instance.Source.Get() == Source; });
    if (Existing)
    {
        const float Cap = FMath::Clamp(GetDefault<USFStatusEffectsSettings>()->DotRefreshCap, 1.f, 1.5f);
        Existing->ExpiresAt = TickSeconds > 0.f
            ? Now + FMath::Min(Existing->ExpiresAt - Now + DurationSeconds, double(DurationSeconds * Cap))
            : FMath::Max(Existing->ExpiresAt, Now + DurationSeconds);
        Existing->Stacks = bStack ? FMath::Min(Existing->Stacks + 1, MaxStacks) : 1;
        Existing->Tick = MoveTemp(Tick);
        Existing->ApplicationOrder = ++NextApplicationOrder;
    }
    else
    {
        if (Instances.Num() >= 32) return false;
        FInstance& Instance = Instances.AddDefaulted_GetRef();
        Instance.EffectId = EffectId;
        Instance.Kind = Kind;
        Instance.DispelType = DispelType;
        Instance.Source = Source;
        Instance.ExpiresAt = Now + DurationSeconds;
        Instance.NextTickAt = TickSeconds > 0 ? Now + TickSeconds : 0;
        Instance.DurationSeconds = DurationSeconds;
        Instance.TickSeconds = TickSeconds;
        Instance.ApplicationOrder = ++NextApplicationOrder;
        Instance.Tick = MoveTemp(Tick);
    }
    RefreshDisplay();
    return true;
}

void USFStatusEffectsComponent::Advance(float DeltaSeconds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || DeltaSeconds < 0.f) return;
    Clock += DeltaSeconds;
    const double Through = Clock;
    bAdvancing = true;
    for (FInstance& Instance : Instances)
    {
        if (Instance.TickSeconds <= 0.f || !Instance.Tick) continue;
        while (Instance.NextTickAt <= Through && Instance.NextTickAt <= Instance.ExpiresAt)
        {
            Instance.NextTickAt += Instance.TickSeconds;
            for (int32 Stack = 0; Stack < Instance.Stacks && !bClearPending; ++Stack) Instance.Tick();
            if (bClearPending) break;
        }
        if (bClearPending) break;
    }
    bAdvancing = false;
    if (bClearPending)
    {
        bClearPending = false;
        Instances.Reset();
        RefreshDisplay();
        return;
    }
    const int32 Before = Instances.Num();
    Instances.RemoveAll([Through](const FInstance& Instance) { return Instance.ExpiresAt <= Through; });
    if (Instances.Num() != Before) RefreshDisplay();
}

bool USFStatusEffectsComponent::PurifyNewest()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (Instances.RemoveAll([this](const FInstance& Instance) { return Instance.ExpiresAt <= Clock; }) > 0)
        RefreshDisplay();
    int32 Newest = INDEX_NONE;
    for (int32 Index = 0; Index < Instances.Num(); ++Index)
    {
        const ESFDispelType Type = Instances[Index].DispelType;
        if ((Type == ESFDispelType::Magic || Type == ESFDispelType::Poison || Type == ESFDispelType::Disease) &&
            (Newest == INDEX_NONE || Instances[Index].ApplicationOrder > Instances[Newest].ApplicationOrder))
            Newest = Index;
    }
    if (Newest == INDEX_NONE) return false;
    Instances.RemoveAt(Newest);
    RefreshDisplay();
    return true;
}

void USFStatusEffectsComponent::ClearAll()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Instances.IsEmpty()) return;
    if (bAdvancing) { bClearPending = true; return; }
    Instances.Reset();
    RefreshDisplay();
}

int32 USFStatusEffectsComponent::GetStacks(FName EffectId, const AActor* Source) const
{
    const FInstance* Instance = Instances.FindByPredicate([EffectId, Source](const FInstance& Item)
    { return Item.EffectId == EffectId && Item.Source.Get() == Source; });
    return Instance && Instance->ExpiresAt > Clock ? Instance->Stacks : 0;
}

void USFStatusEffectsComponent::RefreshDisplay()
{
    DisplayEffects.Reset(Instances.Num());
    for (const FInstance& Instance : Instances)
    {
        FSFStatusDisplay& View = DisplayEffects.AddDefaulted_GetRef();
        View.EffectId = Instance.EffectId;
        View.Kind = Instance.Kind;
        View.DispelType = Instance.DispelType;
        View.Stacks = Instance.Stacks;
        View.DurationSeconds = Instance.DurationSeconds;
        View.ExpiresAtServerSeconds = Instance.ExpiresAt;
        View.ApplicationOrder = Instance.ApplicationOrder;
    }
    GetOwner()->ForceNetUpdate();
}

void USFStatusEffectsComponent::OnRep_DisplayEffects()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsAbilityNetcheck")) ||
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsStatusNetcheck")))
    {
        bool bPoison = false, bCurse = false;
        for (const FSFStatusDisplay& View : DisplayEffects)
        {
            bPoison |= View.EffectId == TEXT("netcheck/poison");
            bCurse |= View.EffectId == TEXT("netcheck/curse");
        }
        if (bPoison || bCurse)
            UE_LOG(LogTemp, Display, TEXT("STATUS_NETCHECK_CLIENT_VIEW Poison=%d Curse=%d"), bPoison, bCurse);
    }
#endif
}
