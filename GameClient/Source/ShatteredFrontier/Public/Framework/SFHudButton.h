#pragma once

#include "Components/Button.h"
#include "SFHudButton.generated.h"

// A UButton that never takes keyboard focus, so HUD buttons do not steal gameplay input. UButton's focus initializer is
// protected and meant for constructors; SFHudStyle::Button builds these.
UCLASS()
class SHATTEREDFRONTIER_API USFHudButton : public UButton
{
    GENERATED_BODY()
public:
    USFHudButton(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) { InitIsFocusable(false); }
};
