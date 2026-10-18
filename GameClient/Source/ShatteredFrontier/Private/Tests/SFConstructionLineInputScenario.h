#pragma once
#include "CoreMinimal.h"
class UWorld;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void StartConstructionLineInputScenario(UWorld* World);
#endif
