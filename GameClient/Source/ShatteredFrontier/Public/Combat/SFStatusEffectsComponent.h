#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "SFStatusEffectsComponent.generated.h"

/** Effect taxonomy from Combat Part 3. Physical bleed is deliberately not a Purify target. */
UENUM(BlueprintType)
enum class ESFDispelType : uint8 { None, Magic, Curse, Poison, Disease, Enrage };

/** Gameplay taxonomy is separate from what a particular dispel can remove. */
UENUM(BlueprintType)
enum class ESFStatusKind : uint8 { Physical, Bleed, Magic, Curse, Poison, Disease, Enrage };

USTRUCT(BlueprintType)
struct FSFStatusDisplay
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName EffectId;
    UPROPERTY(BlueprintReadOnly) ESFStatusKind Kind = ESFStatusKind::Physical;
    UPROPERTY(BlueprintReadOnly) ESFDispelType DispelType = ESFDispelType::None;
    UPROPERTY(BlueprintReadOnly) int32 Stacks = 0;
    UPROPERTY(BlueprintReadOnly) float DurationSeconds = 0;
    /** Server world time; clients can use GameState's synchronized server time for countdowns. */
    UPROPERTY(BlueprintReadOnly) double ExpiresAtServerSeconds = 0;
    UPROPERTY(BlueprintReadOnly) int32 ApplicationOrder = 0;
};

/** Realm balance, adopted at server restart. The formula and eligible dispel types remain release-bound. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Combat Status Effects"))
class SHATTEREDFRONTIER_API USFStatusEffectsSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="DoT refresh duration cap", ClampMin="1.0", ClampMax="1.5", Units="x", ToolTip="Maximum refreshed duration as a multiple of the authored duration; realm balance, server restart."))
    float DotRefreshCap = 1.3f;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Rend duration", ClampMin="1", ClampMax="30", Units="s", ToolTip="Realm balance; server operator; server restart."))
    float RendDurationSeconds = 12.f;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Rend ticks", ClampMin="1", ClampMax="12", ToolTip="Realm balance; server operator; server restart."))
    int32 RendTicks = 6;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Serpent Sting duration", ClampMin="1", ClampMax="30", Units="s", ToolTip="Realm balance; server operator; server restart."))
    float SerpentDurationSeconds = 15.f;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Serpent Sting ticks", ClampMin="1", ClampMax="12", ToolTip="Realm balance; server operator; server restart."))
    int32 SerpentTicks = 5;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Weak Poison duration", ClampMin="1", ClampMax="30", Units="s", ToolTip="Realm balance; server operator; server restart."))
    float WeakPoisonDurationSeconds = 6.f;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Weak Poison ticks", ClampMin="1", ClampMax="12", ToolTip="Realm balance; server operator; server restart. Three ticks preserve the current Greenlands behavior pending owner cadence."))
    int32 WeakPoisonTicks = 3;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Weak Poison stack limit", ClampMin="1", ClampMax="3", ToolTip="Realm balance; server operator; server restart."))
    int32 WeakPoisonMaxStacks = 3;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Weak Poison proc chance", ClampMin="0", ClampMax="1", Units="x", ToolTip="Chance on an eligible hit; realm balance; server operator; server restart."))
    float WeakPoisonProcChance = .30f;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Weak Poison internal cooldown", ClampMin="0.1", ClampMax="5", Units="s", ToolTip="Realm balance; server operator; server restart."))
    float WeakPoisonIcdSeconds = 1.f;
    UPROPERTY(Config, EditAnywhere, Category="Damage Over Time", meta=(DisplayName="Poisoned Blade buff duration", ClampMin="1", ClampMax="60", Units="s", ToolTip="Realm balance; server operator; server restart."))
    float PoisonBladeDurationSeconds = 30.f;
    UPROPERTY(Config, EditAnywhere, Category="Debuffs", meta=(DisplayName="Entangle root duration", ClampMin="1", ClampMax="30", Units="s", ToolTip="Realm balance; server operator; server restart. Diminishing returns still apply."))
    float EntangleDurationSeconds = 5.f;
    UPROPERTY(Config, EditAnywhere, Category="Debuffs", meta=(DisplayName="Frost Shard slow duration", ClampMin="1", ClampMax="30", Units="s", ToolTip="Realm balance; server operator; server restart."))
    float FrostSlowDurationSeconds = 4.f;
    UPROPERTY(Config, EditAnywhere, Category="Debuffs", meta=(DisplayName="Frost Shard movement multiplier", ClampMin="0.3", ClampMax="1", Units="x", ToolTip="1 means no slow; realm balance; server operator; server restart."))
    float FrostSlowMultiplier = .70f;
    UPROPERTY(Config, EditAnywhere, Category="Debuffs", meta=(DisplayName="Crippling Shot slow duration", ClampMin="1", ClampMax="30", Units="s", ToolTip="Realm balance; server operator; server restart."))
    float CripplingSlowDurationSeconds = 6.f;
    UPROPERTY(Config, EditAnywhere, Category="Debuffs", meta=(DisplayName="Crippling Shot movement multiplier", ClampMin="0.3", ClampMax="1", Units="x", ToolTip="1 means no slow; realm balance; server operator; server restart."))
    float CripplingSlowMultiplier = .60f;
    bool Validate(FString& Error) const;
};

/** Server-owned status clock and tick callbacks; only immutable display snapshots replicate. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SHATTEREDFRONTIER_API USFStatusEffectsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFStatusEffectsComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Server only. Stack=true adds one stack up to MaxStacks and refreshes a shared duration.
     *  Tick is called once per active stack at each period; no client can supply a callback. */
    bool Apply(FName EffectId, ESFStatusKind Kind, ESFDispelType DispelType, AActor* Source, float DurationSeconds,
        float TickSeconds, int32 MaxStacks, bool bStack, TFunction<void()> Tick = {});
    /** Purify removes one eligible effect, newest application first. Server only. */
    bool PurifyNewest();
    void ClearAll();
    void Advance(float DeltaSeconds);
    UFUNCTION(BlueprintPure, Category="Combat|Status")
    TArray<FSFStatusDisplay> GetDisplayEffects() const { return DisplayEffects; }
    int32 GetStacks(FName EffectId, const AActor* Source) const;

private:
    UFUNCTION() void OnRep_DisplayEffects();
    struct FInstance
    {
        FName EffectId;
        ESFStatusKind Kind = ESFStatusKind::Physical;
        ESFDispelType DispelType = ESFDispelType::None;
        TWeakObjectPtr<AActor> Source;
        double ExpiresAt = 0;
        double NextTickAt = 0;
        float DurationSeconds = 0;
        float TickSeconds = 0;
        int32 Stacks = 1;
        int32 ApplicationOrder = 0;
        TFunction<void()> Tick;
    };
    void RefreshDisplay();
    TArray<FInstance> Instances;
    UPROPERTY(ReplicatedUsing=OnRep_DisplayEffects) TArray<FSFStatusDisplay> DisplayEffects;
    int32 NextApplicationOrder = 0;
    double Clock = 0;
    bool bAdvancing = false;
    bool bClearPending = false;
};
