#include "UI/SFHudWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudWidgetTest,
    "ShatteredFrontier.Hud.Widget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

// WBP_Hud is the Widget Blueprint on USFHudWidget: it must construct, bind every required child, and show data pushed from C++.
// Needs /Game/Frontier/UI/Widgets/Hud/WBP_Hud (py -3 tools/Run-InEditor.py tools/ui/create_widgets.py).
bool FSFHudWidgetTest::RunTest(const FString& Parameters)
{
    UWorld* World=GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"),World)) { return false; }
    const TSubclassOf<USFHudWidget> HudClass=LoadClass<USFHudWidget>(nullptr,USFHudWidget::AssetPath);
    if (!TestNotNull(TEXT("WBP_Hud loads"),HudClass.Get())) { return false; }
    USFHudWidget* Hud=CreateWidget<USFHudWidget>(World,HudClass);
    if (!TestNotNull(TEXT("WBP_Hud constructs"),Hud)) { return false; }

    TestNotNull(TEXT("Vitals bound"),Hud->Vitals.Get());
    TestNotNull(TEXT("Hotbar bound"),Hud->Hotbar.Get());
    TestNotNull(TEXT("Minimap bound"),Hud->Minimap.Get());
    TestNotNull(TEXT("QuestTracker bound"),Hud->QuestTracker.Get());
    TestNotNull(TEXT("Toasts bound"),Hud->Toasts.Get());
    if (!Hud->Vitals || !Hud->Hotbar || !Hud->QuestTracker || !Hud->Toasts) { return false; }
    TestNotNull(TEXT("Health bar bound"),Hud->Vitals->HealthBar.Get());
    TestNotNull(TEXT("Stamina bar bound"),Hud->Vitals->StaminaBar.Get());

    // Vitals from data.
    Hud->PresentVitals(40,80,50,60);
    TestEqual(TEXT("Health bar fraction"),Hud->Vitals->HealthBar->GetPercent(),0.5f);
    TestEqual(TEXT("Health numeral"),Hud->Vitals->HealthText->GetText().ToString(),FString(TEXT("40 / 80")));
    TestEqual(TEXT("Stamina bar fraction"),Hud->Vitals->StaminaBar->GetPercent(),50.f/60.f);
    Hud->PresentVitals(-5,80,60,60);
    TestEqual(TEXT("Overkill clamps the bar"),Hud->Vitals->HealthBar->GetPercent(),0.f);
    TestEqual(TEXT("Overkill never shows below zero"),Hud->Vitals->HealthText->GetText().ToString(),FString(TEXT("0 / 80")));

    // Hotbar from the pilot's item list: six cells, the equipped axe lights key 5, stacks show their count.
    auto Item=[](const TCHAR* Name,int32 Count) { return TPair<FText,int32>(FText::FromString(Name),Count); };
    Hud->Hotbar->Present({Item(TEXT("Iron Axe (pilot) (equipped)"),1),Item(TEXT("Wall piece"),3)});
    TestEqual(TEXT("Equipped axe is key 5"),Hud->Hotbar->GetActiveKey(),5);
    if (TestNotNull(TEXT("Cell 5 exists"),Hud->Hotbar->GetCell(4))) { TestTrue(TEXT("Cell 5 selected"),Hud->Hotbar->GetCell(4)->IsSelected()); }
    TestNotNull(TEXT("Cell 6 exists"),Hud->Hotbar->GetCell(USFHudHotbarWidget::SlotCount-1));
    TestNull(TEXT("No seventh cell"),Hud->Hotbar->GetCell(USFHudHotbarWidget::SlotCount));
    if (USFSlotWidget* Wall=Hud->Hotbar->GetCell(0)) { TestEqual(TEXT("Wall stack count"),Wall->GetCountText().ToString(),FString(TEXT("3"))); }
    Hud->Hotbar->Present({});
    TestEqual(TEXT("Nothing equipped"),Hud->Hotbar->GetActiveKey(),0);

    // Quest tracker rows from a quest view: heading plus one row per objective line.
    FSFQuestView View;
    View.QuestId=TEXT("test_quest"); View.Title=TEXT("Mill Rations"); View.StepNumber=1;
    FSFQuestLine Logs; Logs.Label=TEXT("Gather logs"); Logs.Count=2; Logs.Target=5; View.Lines.Add(Logs);
    FSFQuestLine Boards; Boards.Label=TEXT("Saw boards"); Boards.Count=5; Boards.Target=5; View.Lines.Add(Boards);
    Hud->PresentQuests({View});
    TestEqual(TEXT("Tracker rows = heading + 2 objectives"),Hud->QuestTracker->RowCount(),3);
    TestNotEqual(TEXT("Tracker is visible with a quest"),Hud->QuestTracker->GetVisibility(),ESlateVisibility::Collapsed);
    Hud->PresentQuests({});
    TestEqual(TEXT("Tracker clears"),Hud->QuestTracker->RowCount(),0);
    TestEqual(TEXT("Tracker hides with no quest"),Hud->QuestTracker->GetVisibility(),ESlateVisibility::Collapsed);

    // Toasts from the queue: one pushed card shows, then expires.
    const double Now=World->GetTimeSeconds();
    Hud->Toasts->PushToast(TEXT("Logging +1"));
    TestEqual(TEXT("One toast card shown"),Hud->Toasts->VisibleCards(),1);
    Hud->Toasts->Refresh(Now+30.0);
    TestEqual(TEXT("Toast expires"),Hud->Toasts->VisibleCards(),0);
    return true;
}
#endif
