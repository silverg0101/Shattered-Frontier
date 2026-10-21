#include "Gathering/SFGatheringHud.h"
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

    FSFStationRecipe Recipe; Recipe.Id=TEXT("recipe/test"); Recipe.Station=TEXT("workbench");
    Recipe.Inputs={{TEXT("item/iron_ingot"),2},{TEXT("item/rough_timber"),1}}; Recipe.Outputs={{TEXT("item/iron_nails"),8}};
    const FSFRecipeView View=USFGatheringHud::RecipeFor(Recipe,TEXT("Workbench"),3.f);
    TestTrue(TEXT("Recipe view is valid"),View.bValid && View.Id==Recipe.Id && View.Title==TEXT("Workbench"));
    TestEqual(TEXT("First input count"),View.InputCount,2);
    TestEqual(TEXT("Output count"),View.OutputCount,8);
    TestEqual(TEXT("Inputs are joined"),View.InputLabel,FString(TEXT("Iron Ingot x2 + Rough Timber")));
    TestEqual(TEXT("Output label"),View.OutputLabel,FString(TEXT("Iron Nails")));
    TestFalse(TEXT("A recipe without outputs is not shown"),USFGatheringHud::RecipeFor(FSFStationRecipe(),TEXT("X"),1.f).bValid);

    TestEqual(TEXT("Slab is the Foundation tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Slab),0);
    TestEqual(TEXT("Cube is the Wall tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Cube),1);
    TestEqual(TEXT("Door tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Door),2);
    TestEqual(TEXT("Slope is the Roof tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Slope),3);
    TestEqual(TEXT("Mode actions select no tile"),USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Rotate),(int32)INDEX_NONE);
    TestEqual(TEXT("Roof label"),FString(USFConstructionSandboxPalette::TileLabel(3)),FString(TEXT("Roof")));
    return true;
}
#endif
