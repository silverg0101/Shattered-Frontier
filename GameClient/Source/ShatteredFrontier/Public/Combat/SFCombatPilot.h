#pragma once

#include "Engine/DeveloperSettings.h"
#include "Construction/SFConstructionNetwork.h"
#include "Combat/SFNpcProfiles.h"
#include "GameFramework/Character.h"
#include "SFCombatPilot.generated.h"

class ASFE1Character;
class UStaticMeshComponent;
class UTextRenderComponent;
class USFCastingComponent;
class USFStatusEffectsComponent;

namespace SFCombatPilotRules
{
    SHATTEREDFRONTIER_API bool CanMelee(double DistanceCm, double FacingDot, double Now,
        double NextAllowed, double RangeCm);
    SHATTEREDFRONTIER_API bool CanRanged(double DistanceCm, double RangeCm);
    SHATTEREDFRONTIER_API bool HasRangedLineOfSight(UWorld* World, const AActor* Source,
        const AActor* Target, const FVector& Start, const FVector& End);
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
    double Highest() const;
    double Get(const FString& CharacterId) const;
    void RaiseTo(const FString& CharacterId, double Amount, int32 MaxEntries);
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
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.0", ClampMax="0.5", DisplayName="Boar frontal dodge chance", ToolTip="Chance per admitted frontal player melee swing; adopted at zone start."))
    float BoarDodgeChance = 0.03f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.5", DisplayName="Boar frontal parry chance", ToolTip="Melee archetype default; adopted at zone start."))
    float BoarParryChance = 0.03f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.5", DisplayName="Dodge plus parry cap", ToolTip="Proportionally scales frontal avoidance; adopted at zone start."))
    float AvoidanceCap = 0.5f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="1", ClampMax="360", Units="deg", DisplayName="Frontal avoidance arc", ToolTip="Defender's dodge, parry and block arc; adopted at zone start."))
    float FrontArcDegrees = 180.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.30", DisplayName="Miss chance cap", ToolTip="Maximum attack miss chance; adopted at zone start."))
    float MissCap = 0.30f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.60", DisplayName="Critical chance cap", ToolTip="Maximum critical chance; adopted at zone start."))
    float CritCap = 0.60f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="1.0", ClampMax="3.0", DisplayName="Critical damage cap", ToolTip="Maximum critical damage multiplier; adopted at zone start."))
    float CritDamageCap = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.75", DisplayName="Block chance cap", ToolTip="Maximum block chance; adopted at zone start."))
    float BlockCap = 0.75f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.40", DisplayName="Block reduction cap", ToolTip="Maximum fraction removed by a block; adopted at zone start."))
    float BlockReductionCap = 0.40f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="1", ClampMax="100000", DisplayName="Armor divisor per attacker level", ToolTip="Physical mitigation divisor; adopted at zone start."))
    float ArmorKLevel = 100.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.85", DisplayName="Armor reduction cap", ToolTip="Physical mitigation cap; adopted at zone start."))
    float ArmorReductionCap = 0.85f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="1", ClampMax="100000", DisplayName="Resistance divisor per attacker level", ToolTip="Magic mitigation divisor; adopted at zone start."))
    float ResistK = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.75", DisplayName="Resistance reduction cap", ToolTip="Magic mitigation cap; adopted at zone start."))
    float ResistReductionCap = 0.75f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.60", DisplayName="Armor penetration cap", ToolTip="Maximum percent armor ignored; adopted at zone start."))
    float ArmorPenetrationCap = 0.60f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Resolution", meta=(ClampMin="0.0", ClampMax="0.60", DisplayName="Magic penetration cap", ToolTip="Maximum percent resistance ignored; adopted at zone start."))
    float MagicPenetrationCap = 0.60f;
    // Part 6 realm balance controls. Release formulas/content are in SFCombatPart6.
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0", ClampMax="0.15", UIMin="0", UIMax="0.15", DisplayName="Base critical chance", ToolTip="Fraction; realm balance; immediate; tooltip visible."))
    float CritBase = 0.05f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0.10", ClampMax="0.40", DisplayName="Block damage removed", ToolTip="Fraction; realm balance; immediate; tooltip visible."))
    float BlockReduction = 0.30f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0.50", ClampMax="1.00", DisplayName="Damage at zero skill", ToolTip="Fraction; realm balance; immediate; tooltip visible."))
    float SkillDamageFloor = 0.80f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="500", ClampMax="10000", DisplayName="Miss skill divisor", ToolTip="Skill points; realm balance; immediate; tooltip visible."))
    float MissDivisor = 2000.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="1.5", ClampMax="4.0", Units="s", DisplayName="Power reference time", ToolTip="Seconds; realm balance; restart; tooltip visible."))
    float PowerSpeedReferenceSeconds = 2.4f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0.10", ClampMax="1.00", DisplayName="Unarmed damage factor", ToolTip="Fraction; realm balance; immediate; tooltip visible."))
    float UnarmedDpsFactor = 0.40f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0", ClampMax="0.30", DisplayName="Weapon damage spread", ToolTip="Fraction; realm balance; restart; tooltip visible."))
    float WeaponSpread = 0.15f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="1", ClampMax="10", DisplayName="Weapon damage scale", ToolTip="Points per budget; realm balance; restart; tooltip visible."))
    float WeaponDpsK = 4.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="5", ClampMax="100", DisplayName="Armor scale", ToolTip="Points per budget; realm balance; restart; tooltip visible."))
    float ArmorK = 30.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0.005", ClampMax="0.080", DisplayName="Beast damage per second", ToolTip="Fraction of reference health per second; realm balance; next spawn; tooltip visible."))
    float BeastDpsCoefficient = 0.030f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="1.0", ClampMax="3.5", Units="s", DisplayName="Beast swing time", ToolTip="Seconds; realm balance; next spawn; tooltip visible."))
    float BeastSwingSeconds = 1.6f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="1.0", ClampMax="2.0", DisplayName="NPC critical damage", ToolTip="Multiplier; realm balance; immediate; tooltip visible."))
    float NpcCritDamage = 1.5f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0.05", ClampMax="0.40", DisplayName="NPC critical chance cap", ToolTip="Fraction; realm balance; immediate; tooltip visible."))
    float NpcCritCap = 0.20f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0", ClampMax="0.15", DisplayName="NPC frontal dodge", ToolTip="Fraction; realm balance; next spawn; tooltip visible."))
    float NpcDodge = 0.03f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0", ClampMax="0.15", DisplayName="NPC frontal parry", ToolTip="Fraction; realm balance; next spawn; tooltip visible."))
    float NpcParry = 0.03f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0.05", ClampMax="2.0", DisplayName="Medium NPC armor", ToolTip="Coefficient; realm balance; next spawn; tooltip visible."))
    float NpcMediumArmor = 0.54f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Part 6", meta=(ClampMin="0.05", ClampMax="2.0", DisplayName="Medium NPC resistance", ToolTip="Coefficient; realm balance; next spawn; tooltip visible."))
    float NpcMediumResistance = 0.54f;
    UPROPERTY(Config, EditAnywhere, Category="Development Fixture", meta=(ClampMin="0.5", ClampMax="5", DisplayName="Pilot damage unit", ToolTip="Average hit per pilot point; development fixture only; restart."))
    float DevPilotDamageUnit = 1.2133f;
    UPROPERTY(Config, EditAnywhere, Category="Development Fixture", meta=(ClampMin="1", ClampMax="10", DisplayName="Pilot armor unit", ToolTip="Armor per pilot point; development fixture only; restart."))
    float DevPilotArmorUnit = 3.64f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.3", ClampMax="5.0", Units="s"))
    float PlayerMeleeCooldownSeconds = 0.8f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.5", ClampMax="5.0", Units="s"))
    float BoarMeleeCooldownSeconds = 1.6f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="1.0", ClampMax="5.0", Units="m"))
    float MeleeRangeMetres = 2.8f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Practice", meta=(ClampMin="0.5", ClampMax="10.0", Units="s", ToolTip="Minimum time between hidden movement practice records; adopted at next zone start."))
    float StealthPracticeIntervalSeconds = 2.0f;
    UPROPERTY(Config, EditAnywhere, Category="Combat Practice", meta=(ClampMin="1.0", ClampMax="10.0", Units="cm/s", ToolTip="Minimum hidden movement speed for practice; adopted at next zone start."))
    float StealthPracticeMinimumSpeedCmPerSecond = 5.0f;
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

    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.5", ClampMax="0.5", ToolTip="Fraction of maximum health restored by spirit release; adopted at zone start."))
    float SpiritResFraction = 0.5f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="0.35", ClampMax="0.35", ToolTip="Default player-ability resurrection health fraction; adopted at zone start."))
    float PlayerResFraction = 0.35f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ClampMin="60.0", ClampMax="60.0", Units="s", ToolTip="Dead player's resurrection acceptance window; adopted at zone start."))
    float ResAcceptSeconds = 60.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Regeneration", meta=(ClampMin="5.0", ClampMax="5.0", Units="s", ToolTip="Combat exit after no hostile action and no threat; adopted at zone start."))
    float CombatExitSeconds = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Regeneration", meta=(ClampMin="5.0", ClampMax="5.0", Units="s", ToolTip="Delay after leaving combat before health regeneration; adopted at zone start."))
    float RegenDelaySeconds = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Regeneration", meta=(ClampMin="0.03", ClampMax="0.03", ToolTip="Fraction of maximum health per second before the Constitution factor; adopted at zone start."))
    float HealthRegenOocPct = 0.03f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Balance", meta=(ToolTip="Choose the nearest authored respawn point to the death, or the first authored point; adopted at next zone start."))
    ESFPlayerRespawnPointRule PlayerRespawnPointRule = ESFPlayerRespawnPointRule::NearestToDeath;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="1.01", ClampMax="3.0", ToolTip="Challenger threat divided by current threat within five metres; adopted at next zone start."))
    float ThreatSwitchMelee = 1.10f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="1.01", ClampMax="3.0", ToolTip="Challenger threat divided by current threat beyond five metres; adopted at next zone start."))
    float ThreatSwitchRanged = 1.30f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="1", ClampMax="40", ToolTip="Maximum remembered attackers per creature; adopted at next zone start."))
    int32 ThreatTableMax = 40;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="0.05", ClampMax="1.0", Units="s", ToolTip="Server target-decision interval; adopted at next zone start."))
    float ThreatDecisionSeconds = 0.1f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="1.10", ClampMax="1.10", ToolTip="Taunt threat relative to the current top; adopted at zone start."))
    float TauntThreatMultiplier = 1.10f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="3.0", ClampMax="3.0", Units="s", ToolTip="First taunt forced-target duration; adopted at zone start."))
    float TauntForceSeconds = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Boar Threat", meta=(ClampMin="18.0", ClampMax="18.0", Units="s", ToolTip="Taunt diminishing-return reset window; adopted at zone start."))
    float TauntDrWindowSeconds = 18.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="300.0", ClampMax="300.0", Units="s", ToolTip="Common NPC base respawn; adopted at zone start."))
    float CommonRespawnSeconds = 300.0f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="0.15", ClampMax="0.15", ToolTip="Player-count respawn slope; adopted at zone start."))
    float RespawnPlayerSlope = 0.15f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="0.35", ClampMax="0.35", ToolTip="Shortest player-count factor; adopted at zone start."))
    float RespawnPlayerFloor = 0.35f;
    UPROPERTY(Config, EditAnywhere, Category="Pilot Creature Respawn", meta=(ClampMin="0.10", ClampMax="0.10", ToolTip="Uniform timer jitter fraction; adopted at zone start."))
    float RespawnJitter = 0.10f;
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
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostInitializeComponents() override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    /** Release-authored actor tag; immunity/retaliation is an invariant, not balance tuning. */
    static const FName OutOfPlaceNpcTag;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void Initialize(class ASFGameMode* OwnerMode, const FString& Group, int32 Slot,
        const TArray<FVector2D>& Area, int32 Health, int32 Serial, bool bInDefensive = false);
    bool ConfigureCreature(const FString& DefinitionId);
    static bool ResolveCreatureLoot(const FString& DefinitionId, FString& TableId, bool& bSingleDrop);
    void ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage, double AbilityThreatMultiplier = 1.0);
    bool HasThreatOn(ASFE1Character* Character) const;
    void ReceivePilotHealingThreat(ASFE1Character* Healer, ASFE1Character* Subject, double Amount);
    void ReceivePilotDodge(ASFE1Character* Attacker);
    bool ReceivePilotTaunt(ASFE1Character* Taunter);
    void ApplyAbilitySnare(double Until, double SpeedMultiplier);
    float ApplyAbilityRoot(double Until);
    void DropAbilityThreat(ASFE1Character* Character);
    /** Authority-only transition when the open-world leash or safe-zone rule fires. */
    void EnterEvade();
    const FString& GroupId() const { return SpawnGroup; }
    const FString& GetCreatureId() const { return CreatureId; }
    const FString& GetLootTableId() const { return LootTableId; }
    bool HasSingleDrop() const { return bSingleDrop; }
    int32 SlotIndex() const { return Slot; }
    /** Legacy sequence key remains the combat random seed, independent of durable identity. */
    FString SpawnSequenceId() const { return FString::Printf(TEXT("%s:%d:%d"), *SpawnGroup, Slot, SpawnSerial); }
    /** Stable for this actor spawn; unique even when another world reuses the sequence key. */
    FString DeathId() const { return SpawnSequenceId() + TEXT(":") + SpawnInstanceId.ToString(EGuidFormats::Digits); }
    int32 Health() const { return CurrentHealth; }
    int32 MaximumHealth() const { return MaximumHealthValue; }
    void CompleteNpcMend(float HealthFraction);
    USFCastingComponent* GetCasting() const { return Casting; }
    USFStatusEffectsComponent* GetStatusEffects() const { return StatusEffects; }
    double CombatSwingSeconds() const { return CombatSwingSecondsValue; }
    double CombatSwingDamage() const { return CombatSwingDamageValue; }
    double CombatArmor() const { return CombatArmorValue; }
    double CombatResistance() const { return CombatResistanceValue; }
    double CombatDodge() const { return CombatDodgeValue; }
    double CombatParry() const { return CombatParryValue; }
    bool IsLeashing() const { return bReturning; }
    bool IsDefensive() const { return bDefensive; }
    FVector SpawnAnchor() const { return Anchor; }
    ASFE1Character* ThreatTarget() const { return ReplicatedTarget; }
    bool HasThreatFor(const ASFE1Character* Character) const;
    bool ReceivePackAssist(ASFE1Character* Character, const FString& CharacterId);
    const SFNpcProfiles::FProfile* GetNpcProfile() const { return SFNpcProfiles::Find(CreatureId); }
    const SFNpcProfiles::FBehavior& GetBehavior() const;
    bool IsRangedArcher() const { const auto* P = GetNpcProfile(); return P && P->IsRanged(); }
    bool CanCastMend() const { const auto* P = GetNpcProfile(); return P && P->CanMend(); }
    UFUNCTION(NetMulticast, Unreliable) void MulticastArcherTracer(FVector_NetQuantize Start, FVector_NetQuantize End);
private:
    void RetaliateAgainst(ASFE1Character* Attacker);
    UFUNCTION() void OnRep_Health();
    UFUNCTION() void OnRep_CreatureId();
    void ApplyPresentation();
    UPROPERTY(ReplicatedUsing=OnRep_Health) int32 CurrentHealth = 0;
    UPROPERTY(Replicated) int32 MaximumHealthValue = 0;
    double CombatSwingSecondsValue = 1.6, CombatSwingDamageValue = 23.5;
    double CombatArmorValue = 108, CombatResistanceValue = 5.4;
    double CombatDodgeValue = .03, CombatParryValue = .03;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFCastingComponent> Casting;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFStatusEffectsComponent> StatusEffects;
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
    FString ForcedTauntId;
    double ForcedTauntUntil = 0;
    double LastTauntAt = -1;
    double AbilitySnareUntil = 0, AbilitySnareMultiplier = 1, AbilityRootUntil = 0;
    double AbilityRootLastAt = -1e9;
    int32 AbilityRootApplications = 0;
    int32 TauntApplications = 0;
    double EngagedSince = -1;
    double NextAggroFixtureTraceAt = 0;
    UPROPERTY(Replicated) FString SpawnGroup;
    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_CreatureId, Category="Loot") FString CreatureId = TEXT("npc/boar");
    UPROPERTY(EditAnywhere, Category="Loot") FString LootTableId = TEXT("loot/greenlands_silverbrook_boar");
    UPROPERTY(EditAnywhere, Category="Loot") bool bSingleDrop = false;
    UPROPERTY(Replicated) int32 Slot = INDEX_NONE;
    int32 SpawnSerial = 0;
    FGuid SpawnInstanceId = FGuid::NewGuid();
    TArray<FVector2D> Territory;
    FVector Anchor = FVector::ZeroVector;
    FRotator AnchorFacing = FRotator::ZeroRotator;
    FVector WanderDestination = FVector::ZeroVector;
    double NextWanderAt = 0;
    double NextSwingAt = 0;
    double LastDamageAt = 0;
    double UnreachableSince = -1;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bool bClientLoggedNpcCast = false;
#endif
    UPROPERTY(Replicated) bool bReturning = false;
};
