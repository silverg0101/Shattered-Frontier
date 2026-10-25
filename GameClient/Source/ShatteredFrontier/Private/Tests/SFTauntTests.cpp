#include "Combat/SFCombatPilot.h"
#include "Combat/SFTauntRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatTauntTest,
    "ShatteredFrontier.Combat.Pilot.Taunt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatTauntTest::RunTest(const FString& Parameters)
{
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    TestEqual(TEXT("Threat multiplier"), Settings->TauntThreatMultiplier, 1.10f);
    TestEqual(TEXT("Force duration"), Settings->TauntForceSeconds, 3.0f);
    TestEqual(TEXT("DR window"), Settings->TauntDrWindowSeconds, 18.0f);
    FString Error;
    TestTrue(TEXT("Settings valid"), Settings->Validate(Error));
    FSFBoarThreatTable Table;
    Table.Add(TEXT("first"), 20, 40);
    TestEqual(TEXT("Top threat"), Table.Highest(), 20.0);
    const double Raised = SFTauntRules::RaisedThreat(Table.Get(TEXT("second")), Table.Highest(), Settings->TauntThreatMultiplier);
    Table.RaiseTo(TEXT("second"), Raised, 40);
    TestTrue(TEXT("Taunter passes switch threshold"), Table.Get(TEXT("second")) > Table.Get(TEXT("first")));
    TestEqual(TEXT("Taunt preserves first threat"), Table.Get(TEXT("first")), 20.0);
    TestEqual(TEXT("First force"), SFTauntRules::ForcedDuration(0, 0, 3, 18), 3.0);
    TestEqual(TEXT("Second force"), SFTauntRules::ForcedDuration(1, 2, 3, 18), 1.5);
    TestEqual(TEXT("Third force"), SFTauntRules::ForcedDuration(2, 2, 3, 18), 0.75);
    TestEqual(TEXT("Fourth immune"), SFTauntRules::ForcedDuration(3, 2, 3, 18), 0.0);
    TestEqual(TEXT("Reset at 18 seconds"), SFTauntRules::ForcedDuration(4, 18, 3, 18), 3.0);
    return true;
}
#endif
