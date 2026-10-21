#include "Gathering/SFGatheringHud.h"
#include "Gathering/SFGatheringSkillsPanel.h"
#include "Gathering/SFStationPanel.h"
#include "Construction/SFConstructionSandboxPalette.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringHudModelTest,
    "ShatteredFrontier.Gathering.Hud.Models",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFGatheringHudModelTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Ring starts empty"),USFGatheringHud::RingFraction(3.0,3.0),0.f);
    TestEqual(TEXT("Ring half"),USFGatheringHud::RingFraction(1.5,3.0),0.5f);
    TestEqual(TEXT("Ring full when time is up"),USFGatheringHud::RingFraction(0.0,3.0),1.f);
    TestEqual(TEXT("Ring clamps overrun"),USFGatheringHud::RingFraction(-1.0,3.0),1.f);
    TestEqual(TEXT("Ring is empty with unknown total"),USFGatheringHud::RingFraction(1.0,0.0),0.f);
    TestEqual(TEXT("Toast text"),USFGatheringHud::SkillToastText(TEXT("Logging")),FString(TEXT("Logging +1")));

    FSFToastQueue Queue;
    TestEqual(TEXT("Empty queue shows nothing"),Queue.Visible(0.0).Num(),0);
    Queue.Push(TEXT("A"),0.0,3.0); Queue.Push(TEXT("B"),1.0,3.0); Queue.Push(TEXT("C"),1.5,3.0); Queue.Push(TEXT("D"),2.0,3.0);
    const TArray<FString> Shown=Queue.Visible(2.5);
    if (TestEqual(TEXT("Only the newest three show"),Shown.Num(),3)) { TestEqual(TEXT("Oldest of the three first"),Shown[0],FString(TEXT("B"))); TestEqual(TEXT("Newest last"),Shown[2],FString(TEXT("D"))); }
    const TArray<FString> Later=Queue.Visible(3.2);  // A expired at 3.0 and B at 4.0 is still alive
    TestEqual(TEXT("Expired toasts are dropped"),Later.Num(),3);
    TestEqual(TEXT("All expired"),Queue.Visible(10.0).Num(),0);

    TestEqual(TEXT("Skill points read in tenths"),USFGatheringSkillsPanel::PointsText(420),FString(TEXT("42.0 / 100.0")));
    TestEqual(TEXT("Next tier at zero"),USFGatheringSkillsPanel::NextUnlockText(ESFGatheringSkill::Logging,0),FString(TEXT("Uncommon timber at 10.0")));
    TestEqual(TEXT("Next tier past uncommon"),USFGatheringSkillsPanel::NextUnlockText(ESFGatheringSkill::Mining,100),FString(TEXT("Rare ore at 30.0")));
    TestEqual(TEXT("All tiers unlocked"),USFGatheringSkillsPanel::NextUnlockText(ESFGatheringSkill::Mining,1000),FString(TEXT("All tiers unlocked")));
    TestTrue(TEXT("Lock toggle cycles raise, hold, lower"),USFGatheringSkillsPanel::NextLock(ESFGatheringSkillLock::Up)==ESFGatheringSkillLock::Locked
        && USFGatheringSkillsPanel::NextLock(ESFGatheringSkillLock::Locked)==ESFGatheringSkillLock::Down && USFGatheringSkillsPanel::NextLock(ESFGatheringSkillLock::Down)==ESFGatheringSkillLock::Up);
    TestEqual(TEXT("Recipe icon stem"),USFStationPanel::IconStem(TEXT("item/rough_timber")),FString(TEXT("rough_timber")));
    TestTrue(TEXT("Have / need met only at the need"),USFGatheringHud::NeedMet(4,4) && !USFGatheringHud::NeedMet(1,2));

    TestEqual(TEXT("Slab is the Foundation tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Slab),0);
    TestEqual(TEXT("Cube is the Wall tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Cube),1);
    TestEqual(TEXT("Door tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Door),2);
    TestEqual(TEXT("Slope is the Roof tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Slope),3);
    TestEqual(TEXT("Mode actions select no tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Rotate),(int32)INDEX_NONE);
    TestEqual(TEXT("Roof label"),FString(USFConstructionSandboxPalette::TileLabel(3)),FString(TEXT("Roof")));
    return true;
}
#endif
