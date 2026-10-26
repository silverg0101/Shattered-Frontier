#include "Characters/SFE1VitalsHud.h"
#include "Construction/SFConstructionSandboxPalette.h"
#include "Dialogue/SFDialogue.h"
#include "Framework/SFGraphicsMenu.h"
#include "Gathering/SFGatheringSkillsPanel.h"
#include "Quest/SFQuestBoard.h"
#include "UI/SFGameMenu.h"
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

// ---- second pass: quests, dialogue, gathering skills, settings, game menu, defeated, toggle and setting row.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuToggleTest, "ShatteredFrontier.Hud.Menus.Toggle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuToggleTest::RunTest(const FString& Parameters)
{
    UWorld* World = EditorWorld();
    USFToggleWidget* Toggle = MakeScreen<USFToggleWidget>(World, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_Toggle.WBP_Toggle_C"));
    if (!TestNotNull(TEXT("WBP_Toggle constructs"), Toggle)) { return false; }
    TestFalse(TEXT("Starts off"), Toggle->IsChecked());
    Toggle->SetChecked(true);
    TestTrue(TEXT("SetChecked turns it on"), Toggle->IsChecked());
    Toggle->OnClicked.Broadcast(Toggle);  // a click flips it
    TestFalse(TEXT("A click turns it off"), Toggle->IsChecked());
    TestNotNull(TEXT("Select frame bound"), Toggle->GetWidgetFromName(TEXT("SelectFrame")));
    TestNotNull(TEXT("Check glyph bound"), Toggle->GetWidgetFromName(TEXT("Check")));
    USFSettingRowWidget* Row = MakeScreen<USFSettingRowWidget>(World, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_SettingRow.WBP_SettingRow_C"));
    if (TestNotNull(TEXT("WBP_SettingRow constructs"), Row))
    {
        Row->SetLabel(FText::FromString(TEXT("V-Sync")));
        Row->SetControl(Toggle);
        TestTrue(TEXT("The row holds the control"), Toggle->GetParent() != nullptr);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuGameMenuTest, "ShatteredFrontier.Hud.Menus.GameMenu", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuGameMenuTest::RunTest(const FString& Parameters)
{
    USFGameMenu* Menu = MakeScreen<USFGameMenu>(EditorWorld(), USFGameMenu::AssetPath);
    if (!TestNotNull(TEXT("WBP_GameMenu constructs"), Menu)) { return false; }
    TArray<ESFGameMenuAction> Chosen;
    Menu->OnChoice.BindLambda([&Chosen](ESFGameMenuAction Action) { Chosen.Add(Action); });
    for (const ESFGameMenuAction Action : {ESFGameMenuAction::Resume, ESFGameMenuAction::Settings, ESFGameMenuAction::LogOut, ESFGameMenuAction::Quit})
    {
        USFButtonWidget* Button = Menu->GetButton(Action);
        if (!TestNotNull(TEXT("Menu button bound"), Button)) { return false; }
        Button->OnActivated.Broadcast(Button);
    }
    TestEqual(TEXT("Every button reports its own action"), Chosen.Num(), 4);
    TestTrue(TEXT("In order"), Chosen.Num() == 4 && Chosen[0] == ESFGameMenuAction::Resume && Chosen[1] == ESFGameMenuAction::Settings && Chosen[2] == ESFGameMenuAction::LogOut && Chosen[3] == ESFGameMenuAction::Quit);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuSettingsTest, "ShatteredFrontier.Hud.Menus.Settings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuSettingsTest::RunTest(const FString& Parameters)
{
    USFGraphicsMenu* Menu = MakeScreen<USFGraphicsMenu>(EditorWorld(), USFGraphicsMenu::AssetPath);
    if (!TestNotNull(TEXT("WBP_Settings constructs"), Menu)) { return false; }
    TestEqual(TEXT("Graphics twenty existing rows plus seven Controls rows"), Menu->SettingRowCount(), 27);
    Menu->StagePreset(ESFGraphicsPreset::Low);
    TestTrue(TEXT("The preset dropdown edits the pending preset"), Menu->Session().PendingPreset() == ESFGraphicsPreset::Low && Menu->Session().IsDirty());
    Menu->StageCustom();
    TestTrue(TEXT("One changed option reads as Custom"), Menu->Session().PendingPreset() == ESFGraphicsPreset::Custom);
    TestFalse(TEXT("No keep prompt before a display change"), Menu->IsKeepPromptOpen());
    TestEqual(TEXT("Level names"), USFGraphicsMenu::LevelName(3), FString(TEXT("Epic")));
    TestEqual(TEXT("Nanite tessellation level 0 reads Off"), USFGraphicsMenu::LevelName(0, true), FString(TEXT("Off")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuSkillsTest, "ShatteredFrontier.Hud.Menus.GatheringSkills", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuSkillsTest::RunTest(const FString& Parameters)
{
    UWorld* World = EditorWorld();
    USFGatheringSkillsPanel* Panel = MakeScreen<USFGatheringSkillsPanel>(World, USFGatheringSkillsPanel::AssetPath);
    if (!TestNotNull(TEXT("WBP_GatheringSkills constructs"), Panel)) { return false; }
    for (int32 Row = 0; Row < USFGatheringSkillsPanel::RowCount; ++Row)
    {
        TestNotNull(TEXT("Skill level text"), Panel->GetWidgetFromName(*FString::Printf(TEXT("Skill%dLevel"), Row)));
        TestNotNull(TEXT("Skill bar"), Panel->GetWidgetFromName(*FString::Printf(TEXT("Skill%dBar"), Row)));
        TestNotNull(TEXT("Skill lock button"), Panel->GetWidgetFromName(*FString::Printf(TEXT("Skill%dLock"), Row)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuQuestBoardTest, "ShatteredFrontier.Hud.Menus.QuestBoard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuQuestBoardTest::RunTest(const FString& Parameters)
{
    USFQuestBoardPanel* Panel = MakeScreen<USFQuestBoardPanel>(EditorWorld(), USFQuestBoardPanel::AssetPath);
    if (!TestNotNull(TEXT("WBP_QuestBoard constructs"), Panel)) { return false; }
    TestEqual(TEXT("No rows before a quest log is bound"), Panel->RowCount(), 0);
    for (const TCHAR* Name : {TEXT("AcceptButton"), TEXT("TrackButton"), TEXT("AbandonButton"), TEXT("Reward0"), TEXT("Objective0Row"), TEXT("Tooltip")})
    { TestNotNull(Name, Panel->GetWidgetFromName(Name)); }
    if (const USFButtonWidget* Abandon = Cast<USFButtonWidget>(Panel->GetWidgetFromName(TEXT("AbandonButton")))) { TestTrue(TEXT("Abandon is a hold button"), Abandon->bHoldToActivate); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuDialogueTest, "ShatteredFrontier.Hud.Menus.Dialogue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuDialogueTest::RunTest(const FString& Parameters)
{
    USFDialoguePanel* Panel = MakeScreen<USFDialoguePanel>(EditorWorld(), USFDialoguePanel::AssetPath);
    if (!TestNotNull(TEXT("WBP_Dialogue constructs"), Panel)) { return false; }
    TestEqual(TEXT("Four response rows"), Panel->ChoiceCount(), FSFDialogueTree::MaxChoices);
    TSharedRef<FSFDialogueTree> Tree = MakeShared<FSFDialogueTree>();
    FString Error;
    if (!TestTrue(TEXT("The Millwarden tree loads"), FSFDialogueTree::Load(TEXT("millwarden_oswin.json"), *Tree, Error))) { return false; }
    Panel->Open(Tree, nullptr, false);
    const FSFDialogueNode* Node = Panel->GetSession().Node();
    if (!TestNotNull(TEXT("The first node"), Node)) { return false; }
    for (int32 Index = 0; Index < Panel->ChoiceCount(); ++Index)
    {
        TestEqual(TEXT("A row is shown for each choice"), Panel->GetChoiceRow(Index)->GetVisibility() != ESlateVisibility::Collapsed, Node->Choices.IsValidIndex(Index));
    }
    TArray<ESFDialogueStep> Steps;
    Panel->OnStep.BindLambda([&Steps](ESFDialogueStep Step) { Steps.Add(Step); });
    TestFalse(TEXT("There is no fifth response"), Panel->ChooseIndex(FSFDialogueTree::MaxChoices));
    int32 Farewell = INDEX_NONE;
    for (int32 Index = 0; Index < Node->Choices.Num(); ++Index) { if (Node->Choices[Index].Action == TEXT("end")) { Farewell = Index; } }
    if (TestTrue(TEXT("A Farewell response exists"), Farewell != INDEX_NONE))
    {
        TestTrue(TEXT("Choosing Farewell"), Panel->ChooseIndex(Farewell));
        TestTrue(TEXT("It closes the conversation"), Steps.Num() == 1 && Steps[0] == ESFDialogueStep::Closed);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuDefeatedTest, "ShatteredFrontier.Hud.Menus.Defeated", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuDefeatedTest::RunTest(const FString& Parameters)
{
    USFE1VitalsHud* Screen = MakeScreen<USFE1VitalsHud>(EditorWorld(), USFE1VitalsHud::AssetPath);
    if (!TestNotNull(TEXT("WBP_Defeated constructs"), Screen)) { return false; }
    FSFE1VitalsModel Model;
    Model.Observe(80, 0.0);
    Screen->Present(Model, 0.f);
    TestFalse(TEXT("Hidden while alive"), Screen->IsDefeatShown());
    Model.Observe(0, 1.0);
    FSFE1DefeatView View; View.DelaySeconds = 10.f; View.Cause = TEXT("Wild Boar");
    Screen->Present(Model, 5.f, View);
    TestTrue(TEXT("Shown once defeated"), Screen->IsDefeatShown());
    TestTrue(TEXT("The respawn fill is half way after half the delay"), FMath::IsNearlyEqual(Screen->GetRespawnProgress(), 0.5f, 0.01f));
    return true;
}
#endif
