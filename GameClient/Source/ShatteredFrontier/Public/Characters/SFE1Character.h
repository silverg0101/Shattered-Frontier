#pragma once

#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Combat/SFOutOfCombatRegen.h"
#include "Combat/SFCombatPart6.h"
#include "Combat/SFStarterAbilities.h"
#include "SFE1Character.generated.h"

struct FSFGameplayResponse;

class UCameraComponent;
class UIKRetargeter;
class UInputAction;
class UInputMappingContext;
class USFCrowdControlComponent;
class USFCastingComponent;
class USFQuestPilotComponent;
class USpringArmComponent;

UCLASS()
class SHATTEREDFRONTIER_API ASFE1Character : public ACharacter
{
    GENERATED_BODY()

public:
    ASFE1Character(const FObjectInitializer& ObjectInitializer);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void PawnClientRestart() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void Landed(const FHitResult& Hit) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void CombatSetHealth(int32 Value);
    void CombatMarkHostile(); // server only; sandbox and Silverbrook combat clock
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void SetGreenlandsNetcheckStage(int32 Stage, int32 LeadSlot, int32 MateSlot, bool bKill);
#endif
    int32 CombatHealth() const { return PilotHealth; }
    UFUNCTION(BlueprintPure, Category="Combat|Energy") float GetStamina() const { return Stamina; }
    UFUNCTION(BlueprintPure, Category="Combat|Energy") float GetMaxStamina() const { return MaxStamina; }
    UFUNCTION(BlueprintPure, Category="Combat|Energy") float GetMana() const { return Mana; }
    UFUNCTION(BlueprintPure, Category="Combat|Energy") float GetMaxMana() const { return MaxMana; }
    bool IsSprinting() const { return bSprinting; }
    UFUNCTION(BlueprintCallable, Category="Movement") void SetSprintIntent(bool bEnable);
    int32 CombatCharacterLevel() const;
    bool HasCombatSnapshot() const { return bHasCombatSnapshot; }
    void RestoreCombatSnapshot(const FSFGameplayResponse& State);
    SFCombatPart6::FStats CombatStats(double CritBase = 0.05, double SkillFloor = 0.80,
        const FString& SkillId = FString()) const;
    int32 CombatMaximumHealth() const;
    int32 CombatConstitution() const;
    double CombatResistance() const;
    double CombatAbsorb() const;
    void GrantCombatAbsorb(double Amount, double DurationSeconds = 20.0, ASFE1Character* Caster = nullptr);
    void ConsumeCombatAbsorb(double Amount);
    UFUNCTION(BlueprintPure, Category="Combat|Abilities") TArray<FName> GetBindableCombatAbilities() const;
    UFUNCTION(BlueprintCallable, Category="Combat|Abilities") void RequestCombatAbility(FName AbilityId, AActor* SelectedTarget = nullptr);
    UFUNCTION(BlueprintCallable, Category="Combat|Abilities") void RequestCombatGroundAbility(FName AbilityId, FVector GroundLocation);
    bool CommitCombatAbility(const SFStarterAbilities::FDefinition& Ability, double Now);
    bool CanBeginCombatAbilityCast(bool bSpell, double Now) const
    { return ActiveCombatCastId == 0 && (!bSpell || Now >= SpellCastLockoutUntil); }
    uint32 BeginCombatAbilityCast(bool bSpell)
    { ActiveCombatCastId = ++NextCombatCastId; bCombatCastSpell = bSpell; return ActiveCombatCastId; }
    bool CompleteCombatAbilityCast(uint32 CastId)
    { if (ActiveCombatCastId != CastId || CastId == 0) return false; ActiveCombatCastId = 0; return true; }
    bool IsCombatAbilityCasting() const { return ActiveCombatCastId != 0; }
    void InterruptCombatAbilityCast();
    bool HasCombatStealth(double Now) const { return CombatStealthLevel > 0 && Now < CombatStealthUntil; }
    void EnterCombatStealth(int32 Level, double Until);
    void BreakCombatStealth();
    UFUNCTION() void OnRep_CombatStealth();
    bool HasPoisonedBlade(double Now) const { return Now < PoisonBladeUntil; }
    void SetPoisonedBladeUntil(double Until) { PoisonBladeUntil = Until; }
    bool TryPoisonProc(double Now);
    void SetSteadyFocusUntil(double Until) { SteadyFocusUntil = Until; }
    double CombatHasteFraction(double Now) const { return Now < SteadyFocusUntil ? .20 : 0.0; }
    bool HasNativeCombatKit(SFStarterAbilities::EKit Kit) const
    {
        return (Kit == SFStarterAbilities::EKit::Weapon && CombatArchetype == TEXT("fighter")) ||
            (Kit == SFStarterAbilities::EKit::Guardian && CombatArchetype == TEXT("guardian")) ||
            (Kit == SFStarterAbilities::EKit::Archery && CombatArchetype == TEXT("archer")) ||
            (Kit == SFStarterAbilities::EKit::Magic && CombatArchetype == TEXT("mage")) ||
            (Kit == SFStarterAbilities::EKit::Healing && CombatArchetype == TEXT("healer")) ||
            (Kit == SFStarterAbilities::EKit::Shadow && CombatArchetype == TEXT("shadow"));
    }
    void ToggleDefensiveStance() { bDefensiveStance = !bDefensiveStance; }
    bool HasDefensiveStance() const { return bDefensiveStance; }
    void SetShieldBlockUntil(double Until) { ShieldBlockUntil = Until; }
    void SetShieldWallUntil(double Until) { ShieldWallUntil = Until; }
    void SetBulwarkUntil(double Until) { BulwarkUntil = Until; }
    double DamageTakenMultiplier(double Now) const
    { return (bDefensiveStance ? .90 : 1.0) * (Now < ShieldWallUntil ? .70 : 1.0) * (Now < BulwarkUntil ? .80 : 1.0); }
    double DamageDoneMultiplier() const { return bDefensiveStance ? .90 : 1.0; }
    double ThreatMultiplier() const { return bDefensiveStance ? 2.0 : 1.0; }
    double BlockBonus(double Now) const { return Now < ShieldBlockUntil ? .30 : 0.0; }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void SetCombatArchetypeFixture(const FString& Archetype) { if (HasAuthority()) { CombatArchetype = Archetype; ForceNetUpdate(); } }
#endif
    bool SpendStaminaFraction(float Fraction);
    bool SpendManaFraction(float Fraction);
    void RestoreCombatManaFraction(float Fraction)
    { if (HasAuthority()) { Mana = FMath::Clamp(MaxMana * Fraction, 0.f, MaxMana); ForceNetUpdate(); } }
    bool CanUseChallenge(double Now) const;
    void CommitChallenge(double Now);
    uint32 GetResurrectionOfferId() const { return ResurrectionOfferId; }
    const FString& GetResurrectionOfferDeathEventId() const { return ResurrectionOfferDeathEventId; }
    void BeginResurrectionOffer(double ExpiresAt, const FString& DeathEventId = FString());
    void ClearResurrectionOffer();
    bool AcceptResurrectionOffer(uint32 RequestedOffer, double Now, int32 MaximumHealth, float Fraction);
    bool CanAcceptResurrectionOffer(uint32 RequestedOffer, double Now) const;
    bool IsPilotInCombat() const { return bPilotInCombat; }
    bool IsAutoAttacking() const { return bAutoAttacking; }
    bool IsTravelMounted() const { return bTravelMounted; }
    bool IsTravelMountCasting() const { return bTravelMountCasting; }
    void RequestTravelMountToggle();
    UFUNCTION(BlueprintPure, Category="Combat") int32 GetPilotHealthForCapture() const { return PilotHealth; }
    AActor* GetCombatTarget() const { return CombatTarget; }
    void SetCombatTarget(AActor* Target);
    // Local dialogue UI calls this on open/close; never sent to the server.
    UFUNCTION(BlueprintCallable, Category="Camera|Combat Framing")
    void SetDialogueFramingTarget(AActor* Speaker);
    static FVector DialogueHeadPoint(const AActor* Speaker);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFCrowdControlComponent> CrowdControl;
    /** Authoritative cast/channel timing and owner-replicated cast bar data. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<USFCastingComponent> Casting;
    // The visible body (not on a dedicated server): the player MetaHuman for FaceId, built by tools/Build-PlayerMetaHumans.py, attached
    // to the mannequin and posed from it through MetaHumanRetargeter (tools/Create-PlayerRetarget.py). The mannequin stays the hidden
    // animation driver; when the MetaHuman is missing (not built on this machine) the mannequin shows as before.
    // FaceId is an id from USFE1Settings::PlayerFaceIds, mapped to the fixed path FaceClassPath gives (clients never pick a class).
    // Server-set: the character creator will call SetFaceId from the player's saved appearance (persistence and the creator packet
    // are a later step); today only the SFFace dev command and SFCrowdTest do.
    FName GetFaceId() const { return FaceId; }
    void SetFaceId(FName NewFaceId);  // authority only; an id not in PlayerFaceIds is logged and the old face kept
    static bool IsAllowedFaceId(FName Id);
    static FSoftClassPath FaceClassPath(FName Id);  // empty when Id is not allowed
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TSoftObjectPtr<UIKRetargeter> MetaHumanRetargeter;
    // The player's Mutable Customizable Object (tools/Create-PlayerOutfit.py): the bare body wearing the starter tunic and leggings, with
    // the skin under them removed. Its mesh replaces the MetaHuman's Body mesh and default outfit. UObject so the Server target, which has
    // no Mutable, still builds; client-only.
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TSoftObjectPtr<UObject> Outfit;
    // The same outfit built on the female bare body (faces in USFE1Settings::FemaleFaceIds).
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TSoftObjectPtr<UObject> OutfitFemale;
    AActor* GetMetaHuman() const { return MetaHuman; }
    DECLARE_MULTICAST_DELEGATE(FOnFaceChanged);
    mutable FOnFaceChanged OnFaceChanged;  // after the MetaHuman was respawned for a new face (client side; the Equipment preview rebuilds its copy)
    // A MetaHuman of this character's class, attached under Parent at Relative and posed from this character's mannequin
    // (the world body, and the Equipment window's preview). Null when the class or retargeter is not built here.
    AActor* SpawnPosedMetaHuman(USceneComponent* Parent, const FTransform& Relative) const;

private:
    friend class USFQuestPilotComponent;  // dev scenario driver swings through the same PilotAttack as the key
    void SpawnMetaHuman();
    UPROPERTY(ReplicatedUsing=OnRep_FaceId) FName FaceId = TEXT("Orlando");
    UFUNCTION() void OnRep_FaceId();  // swaps the MetaHuman body for the new face (client side only)
    UPROPERTY(Transient) TObjectPtr<AActor> MetaHuman;
    UPROPERTY(Replicated) int32 PilotHealth = 0;  // shown by the vitals HUD (SFE1VitalsHud), not in the world
    UPROPERTY(Replicated) double CombatAbsorbRemaining = 0;
    struct FAbsorbInstance
    {
        TWeakObjectPtr<ASFE1Character> Caster;
        double Remaining = 0, ExpiresAt = 0;
        uint64 InstanceId = 0;
    };
    TArray<FAbsorbInstance> CombatAbsorbs;
    uint64 NextCombatAbsorbId = 0;
    int32 CombatHealthMaximum = 500;
    int64 CombatSnapshotRevision = -1;
    bool bHasCombatSnapshot = false;
    UPROPERTY(Replicated) FString CombatArchetype;
    SFCombatPart6::FStats CombatAttributes;
    TMap<FString, int32> CombatSkills;
    UPROPERTY(Replicated) uint32 ResurrectionOfferId = 0; // owning client only
    UPROPERTY(Replicated) FString ResurrectionOfferDeathEventId;
    uint32 NextResurrectionOfferId = 0;
    double ResurrectionOfferExpiresAt = 0.0;
    UPROPERTY(Replicated) bool bPilotInCombat = false;
    UPROPERTY(Replicated) float Stamina = 0.f;
    UPROPERTY(Replicated) float MaxStamina = 0.f;
    UPROPERTY(Replicated) float Mana = 0.f;
    UPROPERTY(Replicated) float MaxMana = 0.f;
    UPROPERTY(Replicated) bool bSprinting = false;
    bool bLocalSprintIntent = false;
    double NextEnergyTickAt = 0.0;
    double EnergyOutOfCombatAt = 0.0;
    bool bEnergyWasInCombat = false;
    double LastSprintEndedAt = -100.0;
    double SprintExhaustAt = 0.0;
    double NextChallengeAt = 0.0;
    double CombatAbilityGcdEndsAt = 0.0;
    TMap<FName, double> CombatAbilityCooldowns;
    uint32 NextCombatCastId = 0, ActiveCombatCastId = 0;
    bool bCombatCastSpell = false;
    double SpellCastLockoutUntil = 0;
    UPROPERTY(ReplicatedUsing=OnRep_CombatStealth) int32 CombatStealthLevel = 0;
    double CombatStealthUntil = 0, PoisonBladeUntil = 0, NextPoisonProcAt = 0, SteadyFocusUntil = 0;
    bool bDefensiveStance = false;
    double ShieldBlockUntil = 0, ShieldWallUntil = 0, BulwarkUntil = 0;
    void TickEnergy();
    UFUNCTION(Server, Reliable) void ServerSetSprintIntent(bool bEnable);
    UPROPERTY(Replicated) bool bAutoAttacking = false;
    UPROPERTY(Replicated) bool bTravelMounted = false;
    UPROPERTY(Replicated) bool bTravelMountCasting = false;
    double TravelMountCastEndsAt = 0.0;
    void TickTravelMount();
    void DismountTravelMount(const TCHAR* Reason);
    UFUNCTION(Server, Reliable) void ServerToggleTravelMount();
    double NextAutoSwingAt = 0.0;
    UPROPERTY(Replicated) int32 GreenlandsNetcheckStage = 0;
    UPROPERTY(Replicated) int32 GreenlandsLeadSlot = -1;
    UPROPERTY(Replicated) int32 GreenlandsMateSlot = -1;
    UPROPERTY(Replicated) bool bGreenlandsKill = false;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    int32 GreenlandsDeathClientStage = 0;
    double GreenlandsDeathClientAt = 0;
    double GreenlandsDeathPeerAt = -1;
    uint32 GreenlandsDeathAcceptedOffer = 0;
    double GreenlandsNextSampleAt = 0;
    double GreenlandsNextAttackAt = 0;
#endif
    FSFOutOfCombatRegen PilotRegen;
    UPROPERTY(Replicated) TObjectPtr<AActor> CombatTarget;
    FTimerHandle CombatScenarioTimer;
    int32 CombatScenarioAttackCount = 0;
    bool bPersonalLootInventoryObserved = false;
    double CombatScenarioNextAttackAt = 0;
    bool bM3ClientCaptureStarted = false;
    bool bM3ClientStopQueued = false;
    TWeakObjectPtr<class ASFPilotBoar> M3ClientTarget;
    int32 TargetScenarioStage = 0;
    int32 EvadeScenarioStage = 0;
    int32 ThreatScenarioStage = 0;
    int32 TauntScenarioStage = 0;
    double TauntScenarioAt = 0;
    bool bTauntScenarioTriedRange = false;
    int32 AggroScenarioStage = 0;
    int32 MemoryScenarioStage = 0;
    int32 AssistScenarioStage = 0;
    double AssistScenarioReadyAt = -1;
    int32 RespawnScenarioStage = 0;
    int32 AutoAttackScenarioStage = 0;
    int32 DodgeScenarioStage = 0;
    int32 UnstuckScenarioStage = 0;
    int32 SpiritReleaseScenarioStage = 0;
    int32 ResurrectionAcceptScenarioStage = 0;
    double ResurrectionAcceptScenarioAt = 0.0;
    double SpiritReleaseScenarioAt = 0.0;
    double NextSpiritReleaseSampleAt = 0.0;
    double UnstuckScenarioAt = 0.0;
    FVector UnstuckScenarioInitial = FVector::ZeroVector;
    double AutoAttackScenarioAt = 0.0;
    double DodgeScenarioAt = 0.0;
    double NextDodgeSampleAt = 0.0;
    double NextAutoAttackSampleAt = 0.0;
    double RespawnScenarioReadyAt = -1;
    double RespawnScenarioDeathAt = -1;
    int32 RegenScenarioStage = 0;
    double NextRegenSampleAt = 0;
    int32 RestRegenScenarioStage = 0;
    int32 RestRegenHurtHealth = 0;
    double NextRestRegenSampleAt = 0;
    double NextFallSampleAt = 0;
    double NextSwimSampleAt = 0;
    int32 SwimScenarioStage = 0;
    int32 CrouchScenarioStage = 0;
    double CrouchScenarioStartAt = -1;
    double NextCrouchSampleAt = 0;
    double BackpedalScenarioStartAt = -1;
    double NextBackpedalSampleAt = 0;
    bool bBackpedalScenarioComplete = false;
    double RoadScenarioStartAt = -1;
    double NextRoadSampleAt = 0;
    bool bRoadScenarioComplete = false;
    bool bLastServerDeepWater = false;
    bool bMemorySawCombat = false;
    double MemoryScenarioStartedAt = -1;
    double AggroScenarioStartedAt = -1;
    double NextEvadeSampleAt = 0;
    TWeakObjectPtr<AActor> TargetScenarioFirst;
    TWeakObjectPtr<AActor> TargetScenarioSecond;
    void TickCombatScenario();
    void TickTargetScenario();
    void TickEvadeScenario();
    void TickThreatScenario();
    void TickTauntScenario();
    void TickAggroScenario();
    void TickMemoryScenario();
    void TickAssistScenario();
    void TickRespawnScenario();
    void TickAutoAttackScenario();
    void TickDodgeScenario();
    void TickUnstuckScenario();
    void TickSpiritReleaseScenario();
    void TickResurrectionAcceptScenario();
    void TickRegenScenario();
    void TickRestRegenScenario();
    void TickFallScenario();
    void TickSwimScenario();
    void TickCrouchScenario();
    void TickBackpedalScenario();
    void TickRoadScenario();
    void TickTravelMountScenario();
    double TravelMountScenarioStartedAt = -1.0;
    double NextTravelMountSampleAt = 0.0;
    double NextEnergyScenarioSampleAt = 0.0;
    TWeakObjectPtr<ASFE1Character> EnergyScenarioSubject;
    bool bTravelMountScenarioCompleted = false;
    int32 TravelMountScenarioStage = 0;
    void TickGreenlandsMovementScenario();
    void TickGreenlandsEnvironmentScenario();
    UFUNCTION(Server, Reliable) void ServerTargetScenarioStep(int32 Step);
    UFUNCTION(Server, Reliable) void ServerPilotAttack(AActor* SelectedCombatTarget);
    UFUNCTION(Server, Reliable) void ServerPilotTaunt(AActor* SelectedCombatTarget);
    UFUNCTION(Server, Reliable) void ServerRequestCombatAbility(FName AbilityId, AActor* SelectedTarget);
    UFUNCTION(Server, Reliable) void ServerRequestCombatGroundAbility(FName AbilityId, FVector GroundLocation);
    UFUNCTION(Server, Reliable) void ServerSetAutoAttack(bool bEnabled);
    void SetAutoAttack(bool bEnabled);
    void PilotAttack();
    void PilotTaunt(AActor* Target);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> FollowCamera;

    void Interact();

    // CTL-01 WoW-style camera and movement (docs/decisions/CTL-01-player-camera-controls.md) under every
    // ASFPlayerController; in build mode (B) the build keys in IMC_Build take priority.
    struct FSFMouseButtonState { bool bDown = false; bool bDragging = false; double DownAt = 0; FVector2D DownCursor = FVector2D::ZeroVector; };
    bool UsesCameraControls() const;
    void ApplyControlScheme();
    void TickCameraControls(float DeltaSeconds);
    void SetMouseCaptured(bool bCaptured);
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> PlayerMapping;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> BuildMapping;
    UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> PlayerActions;
    FSFMouseButtonState OrbitButton, MouseLookButton;
    FRotator CameraRotation = FRotator::ZeroRotator;
    TWeakObjectPtr<AActor> DialogueFramingTarget;
    TWeakObjectPtr<AActor> LastFramingTarget;
    float CombatFramingBlend = 0.0f;
    float CombatFocusDistanceCm = 0.0f;
    float CombatDofBlend = 0.0f;
    double LastFramingSteerAt = -100.0;
    double LastFramingProbeAt = -100.0;
    double FramingOccludedSince = -1.0;
    bool bFramingEngaged = false;
    bool bFramingVisible = true;
    float LastControlYaw = 0, LastControlPitch = 0, DesiredArmCm = 0;
    uint32 HeldLastTick = 0;
    bool bFirstPerson = false, bAutorun = false, bWalkToggled = false, bMouseCaptured = false, bBuildMappingActive = false;
};
