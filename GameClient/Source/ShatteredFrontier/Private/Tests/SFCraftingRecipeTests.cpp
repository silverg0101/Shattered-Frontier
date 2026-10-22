#include "Gathering/SFStationRecipes.h"
#include "Gathering/SFGatheringPilot.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCraftingRecipesTest,
    "ShatteredFrontier.Crafting.Recipes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)

bool FSFCraftingRecipesTest::RunTest(const FString& Parameters)
{
    const auto* Starter = SFStationRecipes::Find(TEXT("recipe/rough_timber"));
    const auto* Trainer = SFStationRecipes::Find(TEXT("recipe/worked_timber"));
    const auto* Scroll = SFStationRecipes::Find(TEXT("recipe/iron_brackets"));
    if (!TestNotNull(TEXT("starter"), Starter) || !TestNotNull(TEXT("trainer"), Trainer) || !TestNotNull(TEXT("scroll"), Scroll))
    { return false; }
    auto* Pilot = NewObject<USFGatheringPilotComponent>();
    TestTrue(TEXT("starter known"), Pilot->KnowsRecipe(*Starter));
    TestFalse(TEXT("trainer unknown"), Pilot->KnowsRecipe(*Trainer));
    TestFalse(TEXT("scroll unknown"), Pilot->KnowsRecipe(*Scroll));
    TestEqual(TEXT("trainer hint"), Trainer->LearnHint, FString(TEXT("Learn from a trainer")));
    TestEqual(TEXT("scroll hint"), Scroll->LearnHint, FString(TEXT("Learn from a recipe scroll")));
    TestEqual(TEXT("grade asset path"), SFStationRecipes::ItemAssetPath(TEXT("item/rough_timber/fine")),
        FString(TEXT("/Game/Frontier/Gameplay/Pilot/DA_RoughTimber_Fine.DA_RoughTimber_Fine")));
    TestEqual(TEXT("grade display"), SFStationRecipes::ItemDisplayName(TEXT("item/rough_timber/fine")), FString(TEXT("Rough Timber (Fine)")));
    return true;
}
#endif
