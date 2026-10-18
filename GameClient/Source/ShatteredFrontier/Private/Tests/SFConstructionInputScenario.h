#pragma once
#include "CoreMinimal.h"
class UWorld;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
// Explicit finite command-line development fixture; no normal-mode hook.
void StartConstructionInputScenario(UWorld* World);
#endif
