#pragma once
#include "CoreMinimal.h"

#if WITH_EDITOR
class USFLootComponent;
class UWorld;
struct FSFInventoryEntry;
struct FSFLootEntry;
// Explicit, editor-only two-process fixture. Uses production death, widget and
// service paths; its state and privileged positioning never ship in the game.
class FSFLootWorkflowNetcheck
{
public:
    static void Tick(USFLootComponent& Loot);
    static int32 FindAuthorizedRow(const TArray<FSFInventoryEntry>& Rows, const TArray<FSFLootEntry>& Items);
    static bool HasPersonalSource(UWorld* World);
};
#endif
