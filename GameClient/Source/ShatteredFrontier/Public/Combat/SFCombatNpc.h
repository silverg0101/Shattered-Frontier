#pragma once

#include "GameFramework/Character.h"
#include "Combat/SFNpcProfiles.h"
#include "Combat/SFCombatFixtures.h"
#include "Combat/SFNpcThreat.h"
#include "SFCombatNpc.generated.h"

class ASFE1Character;
class UStaticMeshComponent;
class UTextRenderComponent;
class USFCastingComponent;
class USFStatusEffectsComponent;

/** One reusable authority-owned combat actor; species inputs come from canonical release profiles. */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFCombatNpc : public ACharacter
{
    GENERATED_BODY()
public:
    ASFCombatNpc();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void PostInitializeComponents() override;
    virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
        AController* EventInstigator, AActor* DamageCauser) override;
    /** Release-authored actor tag; immunity/retaliation is an invariant, not balance tuning. */
    static const FName OutOfPlaceNpcTag;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void Initialize(class ASFGameMode* OwnerMode, const FString& Group, int32 Slot,
        const TArray<FVector2D>& Area, int32 Health, int32 Serial, bool bInDefensive = false,
        ESFNpcFixtureFlags InFixtureFlags = ESFNpcFixtureFlags::None);
    bool ConfigureCreature(const FString& DefinitionId);
    static bool ResolveCreatureLoot(const FString& DefinitionId, FString& TableId, bool& bSingleDrop);
    void ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage, double AbilityThreatMultiplier = 1.0, bool bCritical = false);
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
    FString DisplayName() const;
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
    FTransform GetBodyWorldTransform() const;
    bool IsRangedArcher() const { const auto* P = GetNpcProfile(); return P && P->IsRanged(); }
    bool CanCastMend() const { const auto* P = GetNpcProfile(); return P && P->CanMend(); }
    UFUNCTION(NetMulticast, Unreliable) void MulticastArcherTracer(FVector_NetQuantize Start, FVector_NetQuantize End);
private:
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    friend class FSFNpcFixtureExtractionTest;
#endif
    void RetaliateAgainst(ASFE1Character* Attacker);
    UFUNCTION() void OnRep_Health();
    UFUNCTION() void OnRep_CreatureId();
    bool ApplyPresentation(const SFNpcProfiles::FProfile* Profile = nullptr);
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
    ESFNpcFixtureFlags FixtureFlags = ESFNpcFixtureFlags::None;
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
