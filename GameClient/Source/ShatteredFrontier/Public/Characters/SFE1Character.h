#pragma once

#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "SFE1Character.generated.h"

class UCameraComponent;
class UIKRetargeter;
class UInputAction;
class UInputMappingContext;
class USFCrowdControlComponent;
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
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void CombatSetHealth(int32 Value);
    int32 CombatHealth() const { return PilotHealth; }
    UFUNCTION(BlueprintPure, Category="Combat") int32 GetPilotHealthForCapture() const { return PilotHealth; }
    AActor* GetCombatTarget() const { return CombatTarget; }
    void SetCombatTarget(AActor* Target);
    // Local dialogue UI calls this on open/close; never sent to the server.
    UFUNCTION(BlueprintCallable, Category="Camera|Combat Framing")
    void SetDialogueFramingTarget(AActor* Speaker);
    static FVector DialogueHeadPoint(const AActor* Speaker);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFCrowdControlComponent> CrowdControl;
    // The visible body (not on a dedicated server): a player MetaHuman built by tools/Build-PlayerMetaHumans.py, attached to the
    // mannequin and posed from it through MetaHumanRetargeter (tools/Create-PlayerRetarget.py). The mannequin stays the hidden
    // animation driver; when the MetaHuman is missing (not built on this machine) the mannequin shows as before.
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TSoftClassPtr<AActor> MetaHumanClass;
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TSoftObjectPtr<UIKRetargeter> MetaHumanRetargeter;
    AActor* GetMetaHuman() const { return MetaHuman; }
    // A MetaHuman of this character's class, attached under Parent at Relative and posed from this character's mannequin
    // (the world body, and the Equipment window's preview). Null when the class or retargeter is not built here.
    AActor* SpawnPosedMetaHuman(USceneComponent* Parent, const FTransform& Relative) const;

private:
    friend class USFQuestPilotComponent;  // dev scenario driver swings through the same PilotAttack as the key
    void SpawnMetaHuman();
    UPROPERTY(Transient) TObjectPtr<AActor> MetaHuman;
    UPROPERTY(Replicated) int32 PilotHealth = 0;  // shown by the vitals HUD (SFE1VitalsHud), not in the world
    UPROPERTY(Replicated) TObjectPtr<AActor> CombatTarget;
    FTimerHandle CombatScenarioTimer;
    int32 CombatScenarioAttackCount = 0;
    void TickCombatScenario();
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
