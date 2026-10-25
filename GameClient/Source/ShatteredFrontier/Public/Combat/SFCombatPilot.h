#pragma once

#include "Engine/DeveloperSettings.h"
#include "Construction/SFConstructionNetwork.h"
#include "GameFramework/Character.h"
#include "SFCombatPilot.generated.h"

class ASFE1Character;
class UStaticMeshComponent;
class UTextRenderComponent;

namespace SFCombatPilotRules
{
    SHATTEREDFRONTIER_API bool CanMelee(double DistanceCm, double FacingDot, double Now,
        double NextAllowed, double RangeCm);
    SHATTEREDFRONTIER_API int32 RemainingHealth(int32 Current, int32 Damage);
    /** AI NPC Behavior §8.2: returning creatures travel at 1.5 times pursuit speed. */
    constexpr double EvadeReturnSpeedCmPerSecond(double ChaseMetresPerSecond)
    { return ChaseMetresPerSecond * 150.0; }
    constexpr bool UnreachableEvade(double UnreachableSeconds, double LastDamageSeconds)
    { return UnreachableSeconds >= 10.0 && LastDamageSeconds >= 10.0; }
    SHATTEREDFRONTIER_API bool LeashAbuseSignal(const TArray<double>& EvadeTimes, double Now);
    SHATTEREDFRONTIER_API double AggroRadiusMetres(double BaseRadius, int32 NpcLevel,
        int32 CharacterLevel, double MinimumRadius, double MaximumBonus);
    SHATTEREDFRONTIER_API bool DetectBySight(double DistanceMetres, double FacingDot,
        bool bHasLineOfSight, double AggroRadius, double ProximityRadius, double FieldOfViewDegrees);
    constexpr bool CanAssist(double EngagedSeconds, double DistanceMetres, double DelaySeconds, double RadiusMetres)
    { return EngagedSeconds >= DelaySeconds && DistanceMetres <= RadiusMetres; }
}

/** Server-owned, per-creature threat. IDs are stable character IDs, never client-supplied damage. */
class SHATTEREDFRONTIER_API FSFBoarThreatTable
{
public:
    void Add(const FString& CharacterId, double Amount, int32 MaxEntries);
    void Remove(const FString& CharacterId);
    void Clear() { Entries.Empty(); }
    bool Contains(const FString& CharacterId) const;
    FString Select(const FString& CurrentId, const TMap<FString, double>& ReachableDistancesCm,
        double MeleeThreshold, double RangedThreshold) const;
private:
    struct FEntry { FString Id; double Threat = 0; };
    TArray<FEntry> Entries;
};

/** AI NPC Behavior §9.1: damage attackers stay in one brain for 60 s after reset. */
class SHATTEREDFRONTIER_API FSFRecentAttackerMemory
{
public:
    static constexpr double RetentionSeconds = 60.0;
    void RecordReset(const TSet<FString>& Attackers, double Now);
    bool Contains(const FString& CharacterId, double Now) const;
private:
    TMap<FString, double> ExpiresAt;
};

UENUM()
enum class ESFPlayerRespawnPointRule : uint8 { NearestToDeath, FirstAuthored };

// Release-authored defaults. Both switches are adopted at the next zone start.
// The command-line flag admits this development pilot; it is not a tuning override.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Silverbrook Combat Pilot"))
class SHATTEREDFRONTIER_API USFCombatPilotSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Combat", meta=(DisplayName="Combat available (realm)", ToolTip="Master combat availability. Suppresses new offensive actions at next zone start."))
    bool bCombatAvailable = true;
    UPROPERTY(Config, EditAnywhere, Category="Combat", meta=(DisplayName="Hostile spawning (Silverbrook)", ToolTip="Hostile NPC spawning in Silverbrook at next zone start."))
    bool bSilverbrookHostileSpawning = true;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="20", ClampMax="500"))
    int32 PlayerHealth = 80;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0", ClampMax="50", Units="m/s", DisplayName="Fall damage speed threshold", ToolTip="Impact speed before fall damage starts; adopted at next zone start."))
    float FallSpeedThresholdMps = 12.0f;
    // Shown on the vitals stamina bar until a stamina stat is bound (the bar draws full).
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="10", ClampMax="500", DisplayName="Player stamina (display)"))
    int32 PlayerStamina = 150;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="10", ClampMax="500"))
    int32 BoarHealth = 60;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1", ClampMax="100"))
    int32 PlayerMeleeDamage = 20;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1", ClampMax="100"))
    int32 BoarMeleeDamage = 14;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.3", ClampMax="5.0", Units="s"))
    float PlayerMeleeCooldownSeconds = 0.8f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.5", ClampMax="5.0", Units="s"))
    float BoarMeleeCooldownSeconds = 1.6f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1.0", ClampMax="5.0", Units="m"))
    float MeleeRangeMetres = 2.8f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Targeting", meta=(ClampMin="10", ClampMax="60", Units="m"))
    float TabTargetRangeMetres = 30.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Targeting", meta=(ClampMin="10", ClampMax="100", Units="m"))
    float TabTargetDropRangeMetres = 45.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Targeting", meta=(ClampMin="60", ClampMax="360", Units="deg"))
    float TabTargetConeDegrees = 120.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Targeting")
    bool bTabTargetRequireLineOfSight = true;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Targeting", meta=(ClampMin="0.5", ClampMax="5", Units="s"))
    float TabTargetCycleResetSeconds = 1.5f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1.0", ClampMax="60.0", Units="s", ToolTip="Server respawn delay; adopted at next zone start."))
    float PlayerRespawnDelaySeconds = 4.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Regeneration", meta=(ClampMin="5.0", ClampMax="5.0", Units="s", ToolTip="Combat exit after no hostile action and no threat; adopted at zone start."))
    float CombatExitSeconds = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Regeneration", meta=(ClampMin="5.0", ClampMax="5.0", Units="s", ToolTip="Delay after leaving combat before health regeneration; adopted at zone start."))
    float RegenDelaySeconds = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Regeneration", meta=(ClampMin="0.03", ClampMax="0.03", ToolTip="Fraction of maximum health per second before the Constitution factor; adopted at zone start."))
    float HealthRegenOocPct = 0.03f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ToolTip="Choose the nearest authored respawn point to the death, or the first authored point; adopted at next zone start."))
    ESFPlayerRespawnPointRule PlayerRespawnPointRule = ESFPlayerRespawnPointRule::NearestToDeath;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="4.0", ClampMax="18.0", Units="m"))
    float BoarAggroRadiusMetres = 12.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="4.0", Units="m"))
    float BoarNearNoticeRadiusMetres = 1.5f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="4.0", Units="m"))
    float BoarAggroMinimumMetres = 4.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="0.0", ClampMax="6.0", Units="m"))
    float BoarAggroMaximumBonusMetres = 6.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="1.01", ClampMax="3.0", ToolTip="Challenger threat divided by current threat within five metres; adopted at next zone start."))
    float ThreatSwitchMelee = 1.10f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="1.01", ClampMax="3.0", ToolTip="Challenger threat divided by current threat beyond five metres; adopted at next zone start."))
    float ThreatSwitchRanged = 1.30f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="1", ClampMax="40", ToolTip="Maximum remembered attackers per creature; adopted at next zone start."))
    int32 ThreatTableMax = 40;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="0.05", ClampMax="1.0", Units="s", ToolTip="Server target-decision interval; adopted at next zone start."))
    float ThreatDecisionSeconds = 0.1f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="0.5", ClampMax="5.0", Units="s", ToolTip="Common pack assist delay; adopted at next zone start."))
    float BoarAssistDelaySeconds = 2.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="2.0", ClampMax="15.0", Units="m", ToolTip="Common pack assist radius; adopted at next zone start."))
    float BoarAssistRadiusMetres = 8.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="300.0", ClampMax="300.0", Units="s", ToolTip="Common NPC base respawn; adopted at zone start."))
    float CommonRespawnSeconds = 300.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="0.15", ClampMax="0.15", ToolTip="Player-count respawn slope; adopted at zone start."))
    float RespawnPlayerSlope = 0.15f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="0.35", ClampMax="0.35", ToolTip="Shortest player-count factor; adopted at zone start."))
    float RespawnPlayerFloor = 0.35f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="0.10", ClampMax="0.10", ToolTip="Uniform timer jitter fraction; adopted at zone start."))
    float RespawnJitter = 0.10f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="20.0", ClampMax="100.0", Units="m"))
    float BoarLeashRadiusMetres = 60.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="60.0", ClampMax="360.0", Units="deg"))
    float BoarFieldOfViewDegrees = 160.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="2.0", ClampMax="40.0", Units="m"))
    float BoarWanderRadiusMetres = 18.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="0.5", ClampMax="5.0", Units="m/s"))
    float BoarWanderSpeedMetresPerSecond = 1.3f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="10.0", Units="m/s"))
    float BoarChaseSpeedMetresPerSecond = 3.5f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="10.0", Units="s"))
    float BoarWanderMinSeconds = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar AI", meta=(ClampMin="1.0", ClampMax="20.0", Units="s"))
    float BoarWanderMaxSeconds = 6.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Rewards", meta=(ClampMin="1", ClampMax="5", ToolTip="Boar hides granted to the killer, once per boar death outside protected areas. Adopted at the next zone start."))
    int32 HidesPerBoar = 2;
    bool Validate(FString& Error) const;
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFPilotBoar : public ACharacter
{
    GENERATED_BODY()
public:
    ASFPilotBoar();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void Initialize(class ASFGameMode* OwnerMode, const FString& Group, int32 Slot,
        const TArray<FVector2D>& Area, int32 Health, int32 Serial, bool bInDefensive = false);
    void ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage);
    /** Authority-only transition when the open-world leash or safe-zone rule fires. */
    void EnterEvade();
    const FString& GroupId() const { return SpawnGroup; }
    int32 SlotIndex() const { return Slot; }
    /** One id per spawned boar, so its death rewards once. */
    FString DeathId() const { return FString::Printf(TEXT("%s:%d:%d"), *SpawnGroup, Slot, SpawnSerial); }
    int32 Health() const { return CurrentHealth; }
    bool IsLeashing() const { return bReturning; }
    bool IsDefensive() const { return bDefensive; }
    FVector SpawnAnchor() const { return Anchor; }
    ASFE1Character* ThreatTarget() const { return ReplicatedTarget; }
    bool HasThreatFor(const ASFE1Character* Character) const;
    bool ReceivePackAssist(ASFE1Character* Character, const FString& CharacterId);
private:
    UFUNCTION() void OnRep_Health();
    UPROPERTY(ReplicatedUsing=OnRep_Health) int32 CurrentHealth = 0;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
    TWeakObjectPtr<class ASFGameMode> PilotMode;
    TWeakObjectPtr<ASFE1Character> Target;
    UPROPERTY(Replicated) TObjectPtr<ASFE1Character> ReplicatedTarget;
    FSFBoarThreatTable Threat;
    FSFRecentAttackerMemory RecentAttackers;
    TSet<FString> DamageAttackers;
    TSet<FString> PendingResetAttackers;
    bool bDefensive = false;
    TMap<FString, TWeakObjectPtr<ASFE1Character>> ThreatActors;
    TMap<TWeakObjectPtr<ASFE1Character>, TPair<double, bool>> SightCache;
    TMap<FString, double> OutsideLeashSince;
    FString TargetCharacterId;
    double NextThreatDecisionAt = 0;
    double EngagedSince = -1;
    double NextAggroFixtureTraceAt = 0;
    UPROPERTY(Replicated) FString SpawnGroup;
    UPROPERTY(Replicated) int32 Slot = INDEX_NONE;
    int32 SpawnSerial = 0;
    TArray<FVector2D> Territory;
    FVector Anchor = FVector::ZeroVector;
    FRotator AnchorFacing = FRotator::ZeroRotator;
    FVector WanderDestination = FVector::ZeroVector;
    double NextWanderAt = 0;
    double NextSwingAt = 0;
    double LastDamageAt = 0;
    double UnreachableSince = -1;
    UPROPERTY(Replicated) bool bReturning = false;
};
