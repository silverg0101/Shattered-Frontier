#pragma once

#if WITH_EDITOR
class USFLootComponent;
// Explicit, editor-only two-process fixture. Uses production death, widget and
// service paths; its state and privileged positioning never ship in the game.
class FSFLootWorkflowNetcheck
{
public:
    static void Tick(USFLootComponent& Loot);
};
#endif
