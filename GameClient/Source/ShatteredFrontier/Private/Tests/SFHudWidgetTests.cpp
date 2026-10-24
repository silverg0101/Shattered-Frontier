#include "UI/SFHudWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Framework/SFGameUserSettings.h"
#include "Misc/AutomationTest.h"
#include "Combat/SFCastingComponent.h"

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
    TestNotNull(TEXT("Target bound"),Hud->Target.Get());
    TestNotNull(TEXT("Health bar bound"),Hud->Vitals->HealthBar.Get());
    TestNotNull(TEXT("Stamina bar bound"),Hud->Vitals->StaminaBar.Get());
    if (!Hud->Target) { return false; }

    // Vitals from data.
    Hud->PresentVitals(40,80,50,60);
    auto Percent=[](UImage* Bar) { float V=-1.f; if (UMaterialInstanceDynamic* M=USFUnitFrameWidget::FillMaterial(Bar)) { M->GetScalarParameterValue(TEXT("Percent"),V); } return V; };
    TestEqual(TEXT("Health bar fraction reaches the material"),Percent(Hud->Vitals->HealthBar),0.5f);
    TestEqual(TEXT("Health numeral"),Hud->Vitals->HealthText->GetText().ToString(),FString(TEXT("40 / 80")));
    TestEqual(TEXT("Stamina bar fraction reaches the material"),Percent(Hud->Vitals->StaminaBar),50.f/60.f);
    Hud->PresentVitals(-5,80,60,60);
    TestEqual(TEXT("Overkill clamps the bar"),Percent(Hud->Vitals->HealthBar),0.f);
    TestEqual(TEXT("Overkill never shows below zero"),Hud->Vitals->HealthText->GetText().ToString(),FString(TEXT("0 / 80")));

    // Part toggles per child class: badge, name, stamina, values (collapsed when off).
    struct FParts { const TCHAR* Wbp; bool bBadge,bName,bStamina,bValues; };
    const FParts Cases[]={{TEXT("WBP_PlayerFrame"),true,false,true,true},{TEXT("WBP_TargetFrame"),true,true,false,true},
        {TEXT("WBP_PartyFrame"),true,true,true,false},{TEXT("WBP_RaidFrame"),false,true,false,false}};
    auto Shown=[](const UWidget* W) { return W && W->GetVisibility()!=ESlateVisibility::Collapsed; };
    for (const FParts& Case : Cases)
    {
        const TSubclassOf<USFUnitFrameWidget> Class=LoadClass<USFUnitFrameWidget>(nullptr,*FString::Printf(TEXT("/Game/Frontier/UI/Widgets/Unit/%s.%s_C"),Case.Wbp,Case.Wbp));
        if (!TestNotNull(FString::Printf(TEXT("%s loads"),Case.Wbp),Class.Get())) { continue; }
        USFUnitFrameWidget* Frame=CreateWidget<USFUnitFrameWidget>(World,Class);
        if (!TestNotNull(FString::Printf(TEXT("%s constructs"),Case.Wbp),Frame)) { continue; }
        const FString Name(Case.Wbp);
        TestEqual(Name+TEXT(" badge"),Shown(Frame->BadgeRoot),Case.bBadge);
        TestEqual(Name+TEXT(" name"),Shown(Frame->NameText),Case.bName);
        TestEqual(Name+TEXT(" stamina"),Shown(Frame->StaminaRow),Case.bStamina);
        TestEqual(Name+TEXT(" health values"),Shown(Frame->HealthText),Case.bValues);
        Frame->SetName(FText::FromString(TEXT("Boar")));
        TestEqual(Name+TEXT(" SetName"),Frame->NameText->GetText().ToString(),FString(TEXT("Boar")));
        Frame->SetLevel(7);
        TestEqual(Name+TEXT(" SetLevel"),Frame->LevelText->GetText().ToString(),FString(TEXT("7")));
        Frame->SetStamina(30,60);
        TestEqual(Name+TEXT(" SetStamina reaches the material"),Percent(Frame->StaminaBar),0.5f);
    }

    // Target frame: shown with a name and health, hidden without a target.
    Hud->PresentTarget(true,TEXT("Boar"),1,30,60);
    TestNotEqual(TEXT("Target shows"),Hud->Target->GetVisibility(),ESlateVisibility::Collapsed);
    TestEqual(TEXT("Target name"),Hud->Target->NameText->GetText().ToString(),FString(TEXT("Boar")));
    TestEqual(TEXT("Target health reaches the material"),Percent(Hud->Target->HealthBar),0.5f);
    Hud->PresentTarget(false,FString(),0,0,0);
    TestEqual(TEXT("Target hides"),Hud->Target->GetVisibility(),ESlateVisibility::Collapsed);

    // Party helpers: empty and collapsed until a member is added.
    TestEqual(TEXT("No party members yet"),Hud->PartyCount(),0);
    Hud->AddPartyMember(TEXT("Ally"),3,50,100,40,50);
    TestEqual(TEXT("Party member added"),Hud->PartyCount(),1);
    Hud->ClearParty();
    TestEqual(TEXT("Party cleared"),Hud->PartyCount(),0);

    // Hotbar: the worn axe lights key 5, build pieces count their bundles, the selected piece lights in build mode; layout edits;
    // Settings picks how many cells show.
    FSFHotbarStock Stock; Stock.Worn.Add(TEXT("item/iron_axe")); Stock.Counts.Add(TEXT("item/iron_axe"),1); Stock.Counts.Add(TEXT("item/wall_material_bundle"),3);
    Hud->Hotbar->Present(Stock);
    TestEqual(TEXT("Equipped axe is key 5"),Hud->Hotbar->GetActiveKey(),5);
    if (TestNotNull(TEXT("Cell 5 exists"),Hud->Hotbar->GetCell(4))) { TestTrue(TEXT("Cell 5 selected"),Hud->Hotbar->GetCell(4)->IsSelected()); }
    TestNotNull(TEXT("The last authored cell exists"),Hud->Hotbar->GetCell(USFHudHotbarWidget::SlotCount-1));
    TestNull(TEXT("No cell past the authored ones"),Hud->Hotbar->GetCell(USFHudHotbarWidget::SlotCount));
    if (USFSlotWidget* Wall=Hud->Hotbar->GetCell(0)) { TestEqual(TEXT("Wall stack count"),Wall->GetCountText().ToString(),FString(TEXT("3"))); }
    Stock.BuildPiece=1; Hud->Hotbar->Present(Stock);
    TestEqual(TEXT("The selected build piece lights first"),Hud->Hotbar->GetActiveKey(),2);
    Hud->Hotbar->Assign(6,TEXT("item/stew"));
    Hud->Hotbar->SwapSlots(6,0);
    TestTrue(TEXT("Swap moves both slots"),Hud->Hotbar->GetLayout()[0]==TEXT("item/stew") && Hud->Hotbar->GetLayout()[6]==TEXT("build/wall"));
    Hud->Hotbar->SwapSlots(6,0); Hud->Hotbar->Assign(6,FString());
    TestTrue(TEXT("Clearing empties the slot"),Hud->Hotbar->GetLayout()[6].IsEmpty() && Hud->Hotbar->GetLayout()[0]==TEXT("build/wall"));
    if (USFGameUserSettings* User=GEngine ? Cast<USFGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr)
    {
        const int32 Saved=User->HotbarSlots;
        User->HotbarSlots=3; Hud->Hotbar->Present(FSFHotbarStock());
        TestTrue(TEXT("Three cells show"),Hud->Hotbar->GetCell(2)->GetVisibility()==ESlateVisibility::Visible && Hud->Hotbar->GetCell(3)->GetVisibility()!=ESlateVisibility::Visible);
        User->HotbarSlots=40; Hud->Hotbar->Present(FSFHotbarStock());
        TestTrue(TEXT("At most ten"),Hud->Hotbar->GetCell(9)->GetVisibility()==ESlateVisibility::Visible);
        User->HotbarSlots=0; Hud->Hotbar->Present(FSFHotbarStock());
        TestTrue(TEXT("At least two"),Hud->Hotbar->GetCell(1)->GetVisibility()==ESlateVisibility::Visible && Hud->Hotbar->GetCell(2)->GetVisibility()!=ESlateVisibility::Visible);
        User->HotbarSlots=Saved; Hud->Hotbar->Present(FSFHotbarStock());
    }
    Hud->Hotbar->Present(FSFHotbarStock());
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

#include "Framework/SFPlayerController.h"
#include "Gathering/SFGatheringPilot.h"
#include "GameFramework/WorldSettings.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudLevelTest,
    "ShatteredFrontier.Hud.Level",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

// The character level (restored from progression-service into the owner-replicated pilot component) reaches the player badge; a target has no level.
bool FSFHudLevelTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) { return false; }
    UWorld* World=Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* PC=World->SpawnActor<ASFPlayerController>();
    auto* Pilot=PC ? PC->FindComponentByClass<USFGatheringPilotComponent>() : nullptr;
    if (!TestNotNull(TEXT("Pilot"),Pilot)) { return false; }
    const TSubclassOf<USFHudWidget> HudClass=LoadClass<USFHudWidget>(nullptr,USFHudWidget::AssetPath);
    USFHudWidget* Hud=HudClass ? CreateWidget<USFHudWidget>(World,HudClass) : nullptr;
    if (!TestNotNull(TEXT("WBP_Hud constructs"),Hud) || !Hud->Vitals || !Hud->Target) { return false; }
    Hud->Configure(Pilot);
    TestEqual(TEXT("New character is level 1"),Pilot->GetCharacterLevel(),1);
    Pilot->RestoreLevel(12);
    Hud->SyncLevel();
    TestEqual(TEXT("Restored level reaches the player badge"),Hud->Vitals->LevelText->GetText().ToString(),FString(TEXT("12")));
    Pilot->RestoreLevel(13);
    Hud->SyncLevel();
    TestEqual(TEXT("A level-up reaches the badge"),Hud->Vitals->LevelText->GetText().ToString(),FString(TEXT("13")));
    Hud->PresentTarget(true,TEXT("Boar"),0,30,60);
    TestEqual(TEXT("A target without a level collapses its numeral"),Hud->Target->LevelText->GetVisibility(),ESlateVisibility::Collapsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudRestedTest,
    "ShatteredFrontier.Hud.Rested",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSFHudRestedTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    ASFPlayerController* PC = World->SpawnActor<ASFPlayerController>();
    USFGatheringPilotComponent* Pilot = PC ? PC->FindComponentByClass<USFGatheringPilotComponent>() : nullptr;
    const TSubclassOf<USFHudWidget> HudClass = LoadClass<USFHudWidget>(nullptr, USFHudWidget::AssetPath);
    USFHudWidget* Hud = HudClass ? CreateWidget<USFHudWidget>(World, HudClass) : nullptr;
    if (!TestNotNull(TEXT("Pilot"), Pilot) || !TestNotNull(TEXT("HUD"), Hud)) return false;
    Hud->Configure(Pilot);
    Pilot->RestoreRestedMinutes(37.5);
    Hud->SyncRestedMinutes();
    TestEqual(TEXT("Owner progression pool reaches HUD API"), Hud->GetRestedMinutes(), 37.5);
    Pilot->RestoreRestedMinutes(-4);
    Hud->SyncRestedMinutes();
    TestEqual(TEXT("Invalid negative pool is clamped"), Hud->GetRestedMinutes(), 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudCastBarTest,
    "ShatteredFrontier.Hud.CastBar",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSFHudCastBarTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const TSubclassOf<USFHudWidget> HudClass = LoadClass<USFHudWidget>(nullptr, USFHudWidget::AssetPath);
    USFHudWidget* Hud = World && HudClass ? CreateWidget<USFHudWidget>(World, HudClass) : nullptr;
    if (!TestNotNull(TEXT("HUD"), Hud) || !TestNotNull(TEXT("CastBar bound"), Hud->CastBar.Get())) return false;
    FSFCastView View; View.Phase = ESFCastPhase::Casting; View.AbilityId = 1;
    Hud->CastBar->Present(View, 0.25f);
    TestEqual(TEXT("Cast progress"), Hud->CastBar->CastProgress->GetPercent(), 0.25f);
    TestEqual(TEXT("Ability name"), Hud->CastBar->AbilityName->GetText().ToString(), FString(TEXT("Training Cast")));
    View.Phase = ESFCastPhase::Channeling; View.AbilityId = 2;
    Hud->CastBar->Present(View, 0.75f);
    TestEqual(TEXT("Channel progress"), Hud->CastBar->CastProgress->GetPercent(), 0.75f);
    TestEqual(TEXT("Channel state"), Hud->CastBar->CastState->GetText().ToString(), FString(TEXT("Channeling")));
    View.Phase = ESFCastPhase::Idle; View.bInterrupted = true;
    Hud->CastBar->Present(View, 0.f);
    TestEqual(TEXT("Interrupted state"), Hud->CastBar->CastState->GetText().ToString(), FString(TEXT("Interrupted")));
    TestNotEqual(TEXT("Interrupted remains visible"), Hud->CastBar->GetVisibility(), ESlateVisibility::Collapsed);
    View.bInterrupted = false;
    Hud->CastBar->Present(View, 0.f);
    TestEqual(TEXT("Idle hides cast bar"), Hud->CastBar->GetVisibility(), ESlateVisibility::Collapsed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHudMinimapUvTest,
    "ShatteredFrontier.Hud.Minimap.Uv",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFHudMinimapUvTest::RunTest(const FString& Parameters)
{
    const FVector4 Bounds(-1000.0,-1000.0,1000.0,1000.0);  // 2000 cm square; window 500 cm = a quarter of it
    FBox2f Mid=USFHudMinimapWidget::MapWindowUV(Bounds,500.0,0.0,0.0);
    TestEqual(TEXT("Centre u0"),Mid.Min.X,0.375f); TestEqual(TEXT("Centre v1"),Mid.Max.Y,0.625f);
    FBox2f East=USFHudMinimapWidget::MapWindowUV(Bounds,500.0,500.0,500.0);  // +X is up, +Y is right
    TestEqual(TEXT("+Y moves the window right"),East.Min.X,0.625f); TestEqual(TEXT("+X moves the window up"),East.Max.Y,0.375f);
    FBox2f Edge=USFHudMinimapWidget::MapWindowUV(Bounds,500.0,9999.0,-9999.0);
    TestEqual(TEXT("Clamped inside the map"),Edge.Min.X,0.0f); TestEqual(TEXT("Clamped top"),Edge.Min.Y,0.0f);
    return true;
}
#endif
