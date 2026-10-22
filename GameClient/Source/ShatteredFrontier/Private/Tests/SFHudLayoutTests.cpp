#include "Framework/SFHudLayout.h"
#include "Gathering/SFGatheringHud.h"
#include "UI/SFHudWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudLayoutModelTest,
    "ShatteredFrontier.Hud.Layout.Models",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFHudLayoutModelTest::RunTest(const FString& Parameters)
{
    const TArray<FSFHotbarSlot> Slots=USFHudHotbarWidget::Slots();
    if (!TestEqual(TEXT("Six hotbar slots"),Slots.Num(),USFHudHotbarWidget::SlotCount)) { return false; }
    for (int32 I=0; I<Slots.Num(); ++I) { TestEqual(FString::Printf(TEXT("Slot %d key"),I+1),Slots[I].Key,I+1); TestFalse(TEXT("Every slot has an icon"),Slots[I].IconStem.IsEmpty()); }
    TestEqual(TEXT("Key 5 is the axe"),Slots[4].IconStem,FString(TEXT("iron_axe")));
    TestEqual(TEXT("Key 6 is the pickaxe"),Slots[5].IconStem,FString(TEXT("iron_pickaxe")));

    auto Item=[](const TCHAR* Name,int32 Count) { return TPair<FText,int32>(FText::FromString(Name),Count); };
    TestEqual(TEXT("No tool equipped"),USFHudHotbarWidget::ActiveToolKey({Item(TEXT("Oak Log"),3)}),0);
    TestEqual(TEXT("An unequipped axe in the bag does not light the slot"),USFHudHotbarWidget::ActiveToolKey({Item(TEXT("Iron Axe (pilot)"),1)}),0);
    TestEqual(TEXT("Equipped axe"),USFHudHotbarWidget::ActiveToolKey({Item(TEXT("Iron Axe (pilot) (equipped)"),1)}),5);
    TestEqual(TEXT("Equipped pickaxe is not mistaken for the axe"),USFHudHotbarWidget::ActiveToolKey({Item(TEXT("Iron Pickaxe (pilot) (equipped)"),1)}),6);
    TestEqual(TEXT("Stack counts sum the matching items"),USFHudHotbarWidget::StackCount({Item(TEXT("Wall piece"),2),Item(TEXT("Wall post"),3),Item(TEXT("Oak Log"),9)},TEXT("Wall")),5);
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
