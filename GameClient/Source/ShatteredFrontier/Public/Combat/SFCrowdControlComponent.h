#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "SFCrowdControlComponent.generated.h"

class UCharacterMovementComponent;

/** Crowd-control tuning (`tune.combat.dr_window_s`, `cc_reduction_cap`, World `D_CAP`; Combat Part 3, Crowd control). */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Crowd Control"))
class SHATTEREDFRONTIER_API USFCrowdControlSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Crowd Control", meta=(DisplayName="Diminishing returns window", ClampMin="5000", ClampMax="60000", Delta="1000", Units="ms"))
    int32 DrWindowMs = 18000;
    UPROPERTY(Config, EditAnywhere, Category="Crowd Control", meta=(DisplayName="Duration reduction cap (bp)", ClampMin="0", ClampMax="6000", Delta="100"))
    int32 ReductionCapBp = 4000;
    UPROPERTY(Config, EditAnywhere, Category="Crowd Control", meta=(DisplayName="Snare cap (bp)", ClampMin="3000", ClampMax="9000", Delta="500"))
    int32 SnareCapBp = 7000;
    bool Validate(FString& Error) const;
};

enum class ESFCrowdControl : uint8 { Snare, Root, Stun };

namespace SFCrowdControlRules
{
    /** Release-bound (not tuning): live instances kept per component; at the limit the soonest to expire is dropped. */
    constexpr int32 MaxInstances = 8;
    /** DR_step(n): 1.0, 0.5, 0.25, then immune, for the n-th (0-based) application inside the DR window. */
    SHATTEREDFRONTIER_API int32 DrStepPercent(int32 PriorApplications);
}

/**
 * Server-authoritative timed crowd-control statuses (snare, root, stun). The server owns the instances and the clock;
 * only the derived state replicates (speed multiplier + root/stun flags), and every machine scales
 * CharacterMovement::MaxWalkSpeed from it, so the owner predicts with the same speed the server simulates.
 * Stacking: snares do not stack, the strongest live snare applies (capped); root and stun extend to the later end.
 * Root and stun are diminished per category (18 s window); snare is not.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class SHATTEREDFRONTIER_API USFCrowdControlComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFCrowdControlComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Server only. StrengthBp is the speed reduction of a snare (ignored for root/stun); ReductionBp is the target's
     *  duration reduction (Willpower + items, capped). Returns the applied duration in ms, 0 when immune or refused. */
    int32 Apply(ESFCrowdControl Type, int32 StrengthBp, int32 DurationMs, int32 ReductionBp = 0);
    /** Advances the component clock and expires instances (the tick calls this; tests step it directly). */
    void Advance(float DeltaSeconds);

    bool IsRooted() const { return (Flags & RootFlag) != 0; }
    bool IsStunned() const { return (Flags & StunFlag) != 0; }
    /** Stun or root: cannot start movement abilities such as Rift Recall. */
    bool IsImmobilised() const { return Flags != 0; }
    /** Current movement multiplier in basis points (10000 = unaffected). */
    int32 GetSpeedMultBp() const { return SpeedMultBp; }

    /** Server: applies the status to every character with this component within RadiusCm of Point (not Ignore). */
    static int32 ApplyInRadius(UWorld* World, const FVector& Point, float RadiusCm, const AActor* Ignore, ESFCrowdControl Type, int32 StrengthBp, int32 DurationMs);

private:
    static constexpr uint8 RootFlag = 1;
    static constexpr uint8 StunFlag = 2;
    struct FInstance { ESFCrowdControl Type; int32 StrengthBp; double EndSeconds; };
    struct FDr { double LastApplied = -1e9; int32 Count = 0; };

    UFUNCTION() void OnRep_State();
    void Recompute();
    void ApplyMovement();

    TArray<FInstance> Instances;  // server only
    FDr RootDr, StunDr;
    double Clock = 0.0;
    float BaseWalkSpeed = -1.f;

    UPROPERTY(ReplicatedUsing=OnRep_State) uint16 SpeedMultBp = 10000;
    UPROPERTY(ReplicatedUsing=OnRep_State) uint8 Flags = 0;
};
