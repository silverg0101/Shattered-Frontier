#include "Construction/SFConstructionSandboxSettings.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConstructionDevStockTest,
    "ShatteredFrontier.Construction.DevStock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

// DEC-02: unpaid edits (no Housing) need the explicit -SFDevUnlimitedStock fixture flag; the normal path pays.
bool FSFConstructionDevStockTest::RunTest(const FString& Parameters)
{
    const FString Original=FCommandLine::Get();
    FCommandLine::Set(*Original.Replace(TEXT("-SFDevUnlimitedStock"),TEXT("")));
    TestFalse(TEXT("Without the fixture flag edits must be paid"),USFConstructionSandboxSettings::AllowsUnpaidEdits());
    FCommandLine::Set(*(Original+TEXT(" -SFDevUnlimitedStock")));
    TestTrue(TEXT("The fixture flag allows unpaid development edits"),USFConstructionSandboxSettings::AllowsUnpaidEdits());
    FCommandLine::Set(*Original);
    return true;
}
#endif
