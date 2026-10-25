#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcDeathIdentityTest,
    "ShatteredFrontier.Combat.NpcReuse.DeathIdentityAcrossWorlds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcDeathIdentityTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper FirstWorld, SecondWorld;
    if (!FirstWorld.CreateTestWorld(EWorldType::GameRPC) || !FirstWorld.BeginPlayInTestWorld() ||
        !SecondWorld.CreateTestWorld(EWorldType::GameRPC) || !SecondWorld.BeginPlayInTestWorld()) return false;
    ASFCombatNpc* First = FirstWorld.GetTestWorld()->SpawnActor<ASFCombatNpc>();
    ASFCombatNpc* Second = SecondWorld.GetTestWorld()->SpawnActor<ASFCombatNpc>();
    if (!TestNotNull(TEXT("First-world NPC"), First) || !TestNotNull(TEXT("Second-world NPC"), Second)) return false;
    First->Initialize(nullptr, TEXT("shared_group"), 0, {}, 100, 1);
    Second->Initialize(nullptr, TEXT("shared_group"), 0, {}, 100, 1);
    TestEqual(TEXT("Same group/slot/serial keeps legacy combat seed"), First->SpawnSequenceId(), Second->SpawnSequenceId());
    TestNotEqual(TEXT("Independent worlds cannot reuse durable defeat identity"), First->DeathId(), Second->DeathId());
    const FString FirstDeath = First->DeathId();
    const FString SecondDeath = Second->DeathId();
    for (int32 Retry = 0; Retry < 3; ++Retry)
    {
        TestEqual(TEXT("First spawn retry identity remains stable"), First->DeathId(), FirstDeath);
        TestEqual(TEXT("Second spawn retry identity remains stable"), Second->DeathId(), SecondDeath);
    }
    return true;
}
#endif
