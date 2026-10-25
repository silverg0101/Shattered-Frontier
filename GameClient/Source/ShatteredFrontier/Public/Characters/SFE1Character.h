#pragma once

#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "Combat/SFOutOfCombatRegen.h"
#include "SFE1Character.generated.h"

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
    ASFE1Character();
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
    bool IsPilotInCombat() const { return bPilotInCombat; }
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
    UPROPERTY(Replicated) bool bPilotInCombat = false;
    UPROPERTY(Replicated) int32 GreenlandsNetcheckStage = 0;
    UPROPERTY(Replicated) int32 GreenlandsLeadSlot = -1;
    UPROPERTY(Replicated) int32 GreenlandsMateSlot = -1;
    UPROPERTY(Replicated) bool bGreenlandsKill = false;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    double GreenlandsNextSampleAt = 0;
    double GreenlandsNextAttackAt = 0;
#endif
    FSFOutOfCombatRegen PilotRegen;
    UPROPERTY(Replicated) TObjectPtr<AActor> CombatTarget;
    FTimerHandle CombatScenarioTimer;
    int32 CombatScenarioAttackCount = 0;
    double CombatScenarioNextAttackAt = 0;
    bool bM3ClientCaptureStarted = false;
    bool bM3ClientStopQueued = false;
    TWeakObjectPtr<class ASFPilotBoar> M3ClientTarget;
    int32 TargetScenarioStage = 0;
    int32 EvadeScenarioStage = 0;
    int32 ThreatScenarioStage = 0;
    int32 AggroScenarioStage = 0;
    int32 MemoryScenarioStage = 0;
    int32 AssistScenarioStage = 0;
    double AssistScenarioReadyAt = -1;
    int32 RespawnScenarioStage = 0;
    double RespawnScenarioReadyAt = -1;
    double RespawnScenarioDeathAt = -1;
    int32 RegenScenarioStage = 0;
    double NextRegenSampleAt = 0;
    double NextFallSampleAt = 0;
    double NextSwimSampleAt = 0;
    int32 SwimScenarioStage = 0;
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
    void TickAggroScenario();
    void TickMemoryScenario();
    void TickAssistScenario();
    void TickRespawnScenario();
    void TickRegenScenario();
    void TickFallScenario();
    void TickSwimScenario();
    void TickGreenlandsEnvironmentScenario();
    UFUNCTION(Server, Reliable) void ServerTargetScenarioStep(int32 Step);
    UFUNCTION(Server, Reliable) void ServerPilotAttack(AActor* SelectedCombatTarget);
    void PilotAttack();
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
