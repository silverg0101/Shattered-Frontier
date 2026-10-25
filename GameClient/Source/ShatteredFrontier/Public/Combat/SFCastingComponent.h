#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "Combat/SFCastRules.h"
#include "SFCastingComponent.generated.h"

class ASFCastingTarget;

/** Release-authored combat timing, adopted when a character enters the zone. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Combat Casting"))
class SHATTEREDFRONTIER_API USFCastingSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Global Cooldown", meta=(ClampMin="0.75", ClampMax="3.0", Units="s")) float GcdBaseSeconds = 1.5f;
    UPROPERTY(Config, EditAnywhere, Category="Global Cooldown", meta=(ClampMin="0.75", ClampMax="2.0", Units="s")) float GcdShortSeconds = 1.0f;
    UPROPERTY(Config, EditAnywhere, Category="Global Cooldown", meta=(ClampMin="0.5", ClampMax="1.5", Units="s")) float GcdMinSeconds = 0.75f;
    UPROPERTY(Config, EditAnywhere, Category="Haste", meta=(ClampMin="0", ClampMax="0.75")) float HasteMax = 0.75f;
    UPROPERTY(Config, EditAnywhere, Category="Casting", meta=(ClampMin="0", ClampMax="2", Units="m")) float MoveToleranceMetres = 0.25f;
    UPROPERTY(Config, EditAnywhere, Category="Casting", meta=(ClampMin="0", ClampMax="1", Units="s")) float PushbackSeconds = 0.25f;
    UPROPERTY(Config, EditAnywhere, Category="Casting", meta=(ClampMin="0", ClampMax="5")) int32 PushbackMax = 2;
    UPROPERTY(Config, EditAnywhere, Category="Casting", meta=(ClampMin="0", ClampMax="1000", Units="ms")) int32 QueueWindowMs = 400;
    UPROPERTY(Config, EditAnywhere, Category="Casting", meta=(ClampMin="0", ClampMax="100", Units="%")) int32 CancelRefundPercent = 50;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="0.1", ClampMax="10", Units="s")) float TrainingCastSeconds = 1.5f;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="0.1", ClampMax="10", Units="s")) float TrainingChannelSeconds = 2.0f;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="1", ClampMax="20")) int32 TrainingChannelTicks = 4;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="0", ClampMax="30", Units="s")) float TrainingCooldownSeconds = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="1", ClampMax="100")) int32 TrainingDamagePerTick = 5;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="0", ClampMax="100")) int32 TrainingResourceCost = 10;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="10", ClampMax="1000")) int32 TrainingResourceMax = 100;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="10", ClampMax="10000")) int32 TrainingTargetHealth = 500;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="1", ClampMax="20", Units="m")) float TrainingRangeMetres = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="0", ClampMax="0.75")) float TrainingHasteFraction = 0.0f;
    UPROPERTY(Config, EditAnywhere, Category="Training Abilities", meta=(ClampMin="0", ClampMax="10", Units="s")) float TrainingInterruptLockoutSeconds = 3.0f;
    bool Validate(FString& Error) const;
    SFCastRules::FTuning Tuning() const;
};

UENUM(BlueprintType)
enum class ESFCastPhase : uint8 { Idle, Casting, Channeling };

USTRUCT(BlueprintType)
struct FSFCastView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ESFCastPhase Phase = ESFCastPhase::Idle;
    UPROPERTY(BlueprintReadOnly) uint8 AbilityId = 0;
    UPROPERTY(BlueprintReadOnly) float StartedAt = 0;
    UPROPERTY(BlueprintReadOnly) float EndsAt = 0;
    UPROPERTY(BlueprintReadOnly) float GcdEndsAt = 0;
    UPROPERTY(BlueprintReadOnly) int32 TicksDone = 0;
    UPROPERTY(BlueprintReadOnly) bool bInterrupted = false;
};

/** Lives on ASFE1Character. Only the server changes timing or target health. */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SHATTEREDFRONTIER_API USFCastingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFCastingComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    /** Training ability 1: cast; 2: channel. Sandbox only, through the owning client. */
    UFUNCTION(BlueprintCallable, Category="Combat") void UseTrainingAbility(uint8 AbilityId);
    UFUNCTION(BlueprintCallable, Category="Combat") void CancelCast();
    /** Sandbox training interrupt. The server selects and validates the nearby caster. */
    UFUNCTION(BlueprintCallable, Category="Combat") void RequestTrainingInterrupt();
    /** Rank-one published interrupt: pommel (Stamina) or counterspell (Mana). */
    UFUNCTION(BlueprintCallable, Category="Combat") void RequestPublishedInterrupt(bool bCounterspell, AActor* SelectedTarget);
    /** Server interrupt; school lockout is the training ability's authored lockout. */
    void Interrupt();
    /** Called by the existing combat health path after positive damage. */
    void NotifyDamage();
    UFUNCTION(BlueprintPure, Category="Combat") float GetCastProgress() const;
    UFUNCTION(BlueprintPure, Category="Combat") FSFCastView GetCastView() const { return View; }
    UFUNCTION(BlueprintPure, Category="Combat") int32 GetTrainingResource() const { return Resource; }
    bool IsCurrentCastInterruptible() const { return bCurrentCastInterruptible; }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bool BeginPublishedInterruptFixtureCast(bool bInterruptible = true);
#endif
private:
    UFUNCTION(Server, Reliable) void ServerUseTrainingAbility(uint8 AbilityId);
    UFUNCTION(Server, Reliable) void ServerCancelCast();
    UFUNCTION(Server, Reliable) void ServerRequestTrainingInterrupt();
    UFUNCTION(Server, Reliable) void ServerRequestPublishedInterrupt(bool bCounterspell, AActor* SelectedTarget);
    UFUNCTION(Server, Reliable) void ServerSetInterruptScenarioPosition(bool bCaster, bool bNear);
    void TryUse(uint8 AbilityId);
    void SyncView();
    void ResolveTick(uint8 AbilityId);
    SFCastRules::FAbility Definition(uint8 AbilityId) const;
    bool InSandbox() const;
    double Now() const;

    UPROPERTY(Replicated) FSFCastView View;
    UPROPERTY(Replicated) int32 Resource = 100;
    SFCastRules::FState State;
    SFCastRules::FTuning Tune;
    float CastSeconds = 1.5f, ChannelSeconds = 2.f, CooldownSeconds = 3.f, LockoutSeconds = 3.f;
    int32 ChannelTicks = 4, DamagePerTick = 5, ResourceCost = 10, ResourceMax = 100, CancelRefundPercent = 50;
    float RangeMetres = 5.f, HasteFraction = 0.f;
    bool bCurrentCastInterruptible = true;
    double NextPommelAt = 0.0, NextCounterspellAt = 0.0;
    FVector StartLocation = FVector::ZeroVector;
    TWeakObjectPtr<ASFCastingTarget> Target;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    int32 ScenarioStep = 0;
    double ScenarioStartedAt = 0;
    double ScenarioCastRequestedAt = 0;
    bool bScenarioSawCastView = false;
    int32 InterruptScenarioStep = 0;
    double InterruptScenarioAt = 0;
#endif
};

/** Simple sandbox-only damage target; no Greenlands spawn or production content dependency. */
UCLASS()
class SHATTEREDFRONTIER_API ASFCastingTarget : public AActor
{
    GENERATED_BODY()
public:
    ASFCastingTarget();
    void ReceiveTrainingDamage(int32 Damage);
    UFUNCTION(BlueprintPure, Category="Combat") int32 GetTrainingHealth() const { return Health; }
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Body;
    UPROPERTY(Replicated) int32 Health = 500;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
