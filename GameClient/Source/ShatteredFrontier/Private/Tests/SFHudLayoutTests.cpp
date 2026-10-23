#include "Framework/SFHudLayout.h"
#include "Gathering/SFGatheringHud.h"
#include "UI/SFHudWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudLayoutModelTest,
    "ShatteredFrontier.Hud.Layout.Models",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFHudLayoutModelTest::RunTest(const FString& Parameters)
{
    const TArray<FString> Layout=USFHudHotbarWidget::DefaultLayout();
    if (!TestEqual(TEXT("Ten hotbar slots"),Layout.Num(),USFHudHotbarWidget::SlotCount)) { return false; }
    TestEqual(TEXT("1 is the wall piece"),Layout[0],FString(TEXT("build/wall")));
    TestEqual(TEXT("4 is the door piece"),Layout[3],FString(TEXT("build/door")));
    TestEqual(TEXT("5 is the axe"),Layout[4],FString(TEXT("item/iron_axe")));
    TestEqual(TEXT("6 is the pickaxe"),Layout[5],FString(TEXT("item/iron_pickaxe")));
    TestTrue(TEXT("7-0 start empty"),Layout[6].IsEmpty() && Layout[9].IsEmpty());
    FSFHotbarStock Stock; Stock.Counts.Add(TEXT("item/wall_material_bundle"),3); Stock.Counts.Add(TEXT("item/stew"),2);
    TestEqual(TEXT("A build piece counts its material bundles"),USFHudHotbarWidget::CountFor(TEXT("build/wall"),Stock),3);
    TestEqual(TEXT("An item counts itself"),USFHudHotbarWidget::CountFor(TEXT("item/stew"),Stock),2);
    TestEqual(TEXT("An item not held counts 0"),USFHudHotbarWidget::CountFor(TEXT("item/iron_ring"),Stock),0);
    TestEqual(TEXT("A single item shows no count"),USFSlotWidget::CountText(1).ToString(),FString());
    TestEqual(TEXT("A stack shows its count"),USFSlotWidget::CountText(7).ToString(),FString(TEXT("7")));

    TestEqual(TEXT("Build prompts"),USFHudLayout::PromptsFor(ESFPromptContext::Build).Num(),4);
    TestEqual(TEXT("Build starts with the mouse"),USFHudLayout::PromptsFor(ESFPromptContext::Build)[0].Key,FString(TEXT("LMB")));
    TestEqual(TEXT("Harvest prompt"),USFHudLayout::PromptsFor(ESFPromptContext::Harvest)[0].Action,FString(TEXT("Harvest")));
    TestEqual(TEXT("Interact is F under CTL-01"),USFHudLayout::PromptsFor(ESFPromptContext::Harvest)[0].Key,FString(TEXT("F")));
    TestEqual(TEXT("Refine prompt"),USFHudLayout::PromptsFor(ESFPromptContext::Refine)[0].Action,FString(TEXT("Refine")));
    TestEqual(TEXT("No context, no prompt"),USFHudLayout::PromptsFor(ESFPromptContext::None).Num(),0);

    TestEqual(TEXT("Zone from a map name"),USFHudMinimapWidget::ZoneFromMapName(TEXT("Greenlands_Silverbrook")),FString(TEXT("Silverbrook")));
    TestEqual(TEXT("A bare map name is kept"),USFHudMinimapWidget::ZoneFromMapName(TEXT("Silverbrook")),FString(TEXT("Silverbrook")));
    TestEqual(TEXT("Footer with a distance"),USFHudMinimapWidget::FooterText(TEXT("Silverbrook"),120),FString(TEXT("Silverbrook · 120 m")));
    TestEqual(TEXT("Footer without a pawn is the zone"),USFHudMinimapWidget::FooterText(TEXT("Silverbrook"),-1),FString(TEXT("Silverbrook")));
    TestEqual(TEXT("Full charges"),USFHudLayout::ChargeFraction(3,3),1.f);
    TestEqual(TEXT("Half charges"),USFHudLayout::ChargeFraction(1,2),0.5f);
    TestEqual(TEXT("No maximum is empty, not a divide by zero"),USFHudLayout::ChargeFraction(2,0),0.f);

    TestEqual(TEXT("Have / need text"),USFGatheringHud::NeedText(2,4),FString(TEXT("2 / 4")));
    TestTrue(TEXT("Met need"),USFGatheringHud::NeedMet(4,4));
    TestFalse(TEXT("Short need"),USFGatheringHud::NeedMet(3,4));
    return true;
}
#endif
