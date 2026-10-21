#pragma once

#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "SFE1Character.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USFCrowdControlComponent;
class USFQuestPilotComponent;
class USpringArmComponent;
class UTextRenderComponent;

UCLASS()
class SHATTEREDFRONTIER_API ASFE1Character : public ACharacter
{
    GENERATED_BODY()

public:
    ASFE1Character();
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void PawnClientRestart() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void CombatSetHealth(int32 Value);
    int32 CombatHealth() const { return PilotHealth; }
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFCrowdControlComponent> CrowdControl;

private:
    friend class USFQuestPilotComponent;  // dev scenario driver swings through the same PilotAttack as the key
    UPROPERTY(ReplicatedUsing=OnRep_PilotHealth) int32 PilotHealth = 0;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> CombatHealthLabel;
    UFUNCTION() void OnRep_PilotHealth();
    FTimerHandle CombatScenarioTimer;
    int32 CombatScenarioAttackCount = 0;
    void TickCombatScenario();
    UFUNCTION(Server, Reliable) void ServerPilotAttack();
    void PilotAttack();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> FollowCamera;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookRight(float Value);
    void LookUp(float Value);
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
    float LastControlYaw = 0, LastControlPitch = 0, DesiredArmCm = 0;
    uint32 HeldLastTick = 0;
    bool bFirstPerson = false, bAutorun = false, bWalkToggled = false, bMouseCaptured = false, bBuildMappingActive = false;
};
