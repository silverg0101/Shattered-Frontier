#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "SFBackpedalMovement.generated.h"

UCLASS()
class SHATTEREDFRONTIER_API USFBackpedalMovement : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    static bool IsBackpedalling(const FVector& Facing, const FVector& Intent);
    virtual float GetMaxSpeed() const override;
};
