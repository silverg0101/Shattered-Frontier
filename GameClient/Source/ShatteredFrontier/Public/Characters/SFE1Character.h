#pragma once

#include "GameFramework/Character.h"
#include "SFE1Character.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS()
class SHATTEREDFRONTIER_API ASFE1Character : public ACharacter
{
    GENERATED_BODY()

public:
    ASFE1Character();
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> CameraBoom;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> FollowCamera;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> FixtureBody;

    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookRight(float Value);
    void LookUp(float Value);
    void Interact();
};
