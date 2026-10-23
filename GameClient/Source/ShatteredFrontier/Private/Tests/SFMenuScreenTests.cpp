#include "Construction/SFConstructionSandboxPalette.h"
#include "Exchange/SFExchangeWidgets.h"
#include "Exchange/SFFakeExchange.h"
#include "Gathering/SFStationPanel.h"
#include "UI/SFUiWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"

// The market board, trade window, recipes (station) and build catalogue on the shared kit. They need the Widget Blueprints
// (py -3 tools/Run-InEditor.py tools/ui/create_widgets.py, or the headless SF_UI_FORCE=1 SF_UI_ONLY=... run).
namespace
{
template <class T> T* MakeScreen(UWorld* World, const TCHAR* Path)
{
    const TSubclassOf<T> Class = LoadClass<T>(nullptr, Path);
    return Class && World ? CreateWidget<T>(World, Class) : nullptr;
}
UWorld* EditorWorld() { return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuMarketTest, "ShatteredFrontier.Hud.Menus.Market", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuMarketTest::RunTest(const FString& Parameters)
{
    USFMarketBoard* Board = MakeScreen<USFMarketBoard>(EditorWorld(), USFMarketBoard::AssetPath);
    if (!TestNotNull(TEXT("WBP_MarketBoard constructs"), Board)) { return false; }
    FSFFakeExchange Fake; Fake.Wallet = {100, 0, 100};
    TSharedPtr<FSFMarketViewModel> Model = MakeShared<FSFMarketViewModel>(Fake);
    Model->SetListings(FSFFakeExchange::DemoListings());
    Board->Bind(Model);
    TestEqual(TEXT("One row per listing"), Board->RowCount(), 6);
    if (Board->RowCount() < 3) { return false; }
    // Sorted by unit price: Oak Log (5 x 24 = 120 gold) is out of reach with 100 gold, Stew (12 x 4) is not.
    TestFalse(TEXT("An unaffordable row cannot buy"), Board->GetRow(0)->GetAction()->IsButtonEnabled());
    TestTrue(TEXT("An affordable row can buy"), Board->GetRow(1)->GetAction()->IsButtonEnabled());
    TestFalse(TEXT("The hold has not completed"), Board->GetRow(1)->GetAction()->AdvanceHold(0.5f, true));
    TestEqual(TEXT("No buy before the hold completes"), Fake.Buys, 0);
    TestTrue(TEXT("The hold completes"), Board->GetRow(1)->GetAction()->AdvanceHold(0.6f, true));
    TestEqual(TEXT("One buy after the hold"), Fake.Buys, 1);
    Model->Filter().Tab = ESFInventoryTab::Gear;
    Board->Bind(Model);
    TestEqual(TEXT("The Gear tab shows the axe"), Board->RowCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuTradeTest, "ShatteredFrontier.Hud.Menus.Trade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuTradeTest::RunTest(const FString& Parameters)
{
    USFTradeWindow* Window = MakeScreen<USFTradeWindow>(EditorWorld(), USFTradeWindow::AssetPath);
    if (!TestNotNull(TEXT("WBP_TradeWindow constructs"), Window)) { return false; }
    FSFFakeExchange Fake;
    TSharedPtr<FSFTradeViewModel> Model = MakeShared<FSFTradeViewModel>(Fake, FSFFakeExchange::DemoTrade(1000.0), true);
    Model->AddMyItem({TEXT("item/oak_log"), TEXT("Oak Log"), 24});
    Window->Bind(Model);
    if (!TestNotNull(TEXT("Your first slot"), Window->GetMySlot(0)) || !TestNotNull(TEXT("Their first slot"), Window->GetTheirSlot(0))) { return false; }
    TestEqual(TEXT("Your slot shows the stack"), Window->GetMySlot(0)->GetCountText().ToString(), FString(TEXT("24")));
    TestEqual(TEXT("Their slot shows the stack"), Window->GetTheirSlot(0)->GetCountText().ToString(), FString(TEXT("6")));
    TestTrue(TEXT("Accept is a hold button"), Window->GetAcceptButton() && Window->GetAcceptButton()->bHoldToActivate);
    Window->ToggleLock();
    TestTrue(TEXT("The lock toggle locks your side"), Model->Snapshot().Mine.bLocked);
    Model->OnPartnerLocked(true);
    TestTrue(TEXT("Both locked can accept"), Model->CanAccept());
    TestFalse(TEXT("The accept hold is not done yet"), Window->GetAcceptButton()->AdvanceHold(0.5f, true));
    TestTrue(TEXT("The accept hold completes"), Window->GetAcceptButton()->AdvanceHold(0.6f, true));
    TestEqual(TEXT("One accept after the hold"), Fake.Accepts, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuStationTest, "ShatteredFrontier.Hud.Menus.Recipes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuStationTest::RunTest(const FString& Parameters)
{
    UWorld* World = EditorWorld();
    USFStationPanel* Panel = MakeScreen<USFStationPanel>(World, USFStationPanel::AssetPath);
    if (TestNotNull(TEXT("WBP_StationPanel constructs"), Panel))
    {
        TestNotNull(TEXT("Craft button bound"), Panel->GetCraftButton());
        TestEqual(TEXT("No recipes before a station is presented"), Panel->RecipeCount(), 0);
    }
    TestEqual(TEXT("Icon stem"), USFStationPanel::IconStem(TEXT("item/rough_timber")), FString(TEXT("rough_timber")));
    USFKitRowWidget* Row = MakeScreen<USFKitRowWidget>(World, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_KitRow.WBP_KitRow_C"));
    if (TestNotNull(TEXT("WBP_KitRow constructs"), Row))
    {
        Row->SetEntry(FText::FromString(TEXT("Rough Timber")), FText::GetEmpty());
        Row->SetCells(FText::FromString(TEXT("12 / 4")), FText::GetEmpty());
        Row->SetPrice(FText::FromString(TEXT("40 g")));
        TestNotNull(TEXT("Row action button bound"), Row->GetAction());
        TestNotNull(TEXT("Row hover frame bound"), Row->GetWidgetFromName(TEXT("HoverFrame")));
        TestNotNull(TEXT("Row select frame bound"), Row->GetWidgetFromName(TEXT("SelectFrame")));
        Row->SetSelected(true);
        TestTrue(TEXT("Row selects"), Row->IsSelected());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuBuildTest, "ShatteredFrontier.Hud.Menus.BuildCatalogue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuBuildTest::RunTest(const FString& Parameters)
{
    USFConstructionSandboxPalette* Palette = MakeScreen<USFConstructionSandboxPalette>(EditorWorld(), USFConstructionSandboxPalette::AssetPath);
    if (!TestNotNull(TEXT("WBP_BuildCatalogue constructs"), Palette)) { return false; }
    TestEqual(TEXT("One tile per piece"), Palette->CellCount(), USFConstructionSandboxPalette::TileCount);
    TestEqual(TEXT("Wall is selected first"), Palette->GetSelectedTile(), 1);
    TArray<ESFConstructionPaletteAction> Actions;
    Palette->OnAction.BindLambda([&Actions](ESFConstructionPaletteAction Action) { Actions.Add(Action); });
    Palette->ClickTile(3);
    TestEqual(TEXT("The Roof tile dispatches Slope"), Actions.Num() == 1 ? static_cast<int32>(Actions[0]) : -1, static_cast<int32>(ESFConstructionPaletteAction::Slope));
    TestEqual(TEXT("The Roof tile is selected"), Palette->GetSelectedTile(), 3);
    if (TestNotNull(TEXT("Roof cell"), Palette->GetCell(3))) { TestTrue(TEXT("Roof cell shows the select frame"), Palette->GetCell(3)->IsSelected()); }
    TestEqual(TEXT("Slab is the Foundation tile"), USFConstructionSandboxPalette::TileForAction(ESFConstructionPaletteAction::Slab), 0);
    return true;
}
#endif
