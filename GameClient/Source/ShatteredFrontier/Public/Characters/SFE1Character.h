#pragma once

#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "SFE1Character.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class SHATTEREDFRONTIER_API ASFE1Character : public ACharacter
{
    GENERATED_BODY()

public:
    ASFE1Character();
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void CombatSetHealth(int32 Value);
    int32 CombatHealth() const { return PilotHealth; }

private:
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
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FixtureBody;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookRight(float Value);
    void LookUp(float Value);
    void Interact();
};
