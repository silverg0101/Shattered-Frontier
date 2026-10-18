#pragma once

#include "CoreMinimal.h"

class UWorld;

#if WITH_DEV_AUTOMATION_TESTS
// Opt-in, finite development scenario; never part of shipping gameplay.
void StartE1NetworkScenario(UWorld* World);
#endif
