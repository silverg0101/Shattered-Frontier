#include "UI/SFUiWidgets.h"
#include "UI/SFWindowStack.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"

namespace
{
constexpr const TCHAR* KitCommonDir = TEXT("/Game/Frontier/UI/Widgets/Common");
template <class T> T* MakeKitWidget(UWorld* World, const TCHAR* Name)
{
    const TSubclassOf<T> Class = LoadClass<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s_C"), KitCommonDir, Name, Name));
    return Class ? CreateWidget<T>(World, Class) : nullptr;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFKitHoldTimerTest, "ShatteredFrontier.Hud.Kit.HoldTimer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFKitHoldTimerTest::RunTest(const FString& Parameters)
{
    FSFHoldTimer Timer; Timer.HoldSeconds = 1.f; Timer.DrainSeconds = 0.5f;
    TestFalse(TEXT("0.5 s of 1 s does not fire"), Timer.Advance(0.5f, true));
    TestEqual(TEXT("Half filled"), Timer.Progress, 0.5f);
    TestTrue(TEXT("Reaching 1 s fires"), Timer.Advance(0.5f, true));
    TestEqual(TEXT("Full"), Timer.Progress, 1.f);
    TestFalse(TEXT("Fires once while still held"), Timer.Advance(1.f, true));
    TestFalse(TEXT("Release does not fire"), Timer.Advance(0.25f, false));
    TestEqual(TEXT("Drains at DrainSeconds"), Timer.Progress, 0.5f);
    TestFalse(TEXT("Held again continues from the fill"), Timer.Advance(0.25f, true));
    TestEqual(TEXT("Refilling from 0.5"), Timer.Progress, 0.75f);
    TestTrue(TEXT("And fires again when full"), Timer.Advance(0.25f, true));
    Timer.Advance(5.f, false);
    TestEqual(TEXT("Drains to empty"), Timer.Progress, 0.f);
    const bool bTapHeld = Timer.Advance(0.3f, true);
    const bool bTapReleased = Timer.Advance(0.3f, false);
    TestFalse(TEXT("A short tap never fires"), bTapHeld || bTapReleased);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFKitWindowLayoutTest, "ShatteredFrontier.Hud.Kit.WindowLayout", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFKitWindowLayoutTest::RunTest(const FString& Parameters)
{
    const FVector2D View(1920.f, 1080.f);
    // Left to right in opening order from the left margin, each centred in the space above the hotbar.
    const TArray<FVector2D> Two = SFComputeWindowLayout({FVector2D(600, 800), FVector2D(400, 600)}, View, 48.f, 16.f, 140.f);
    TestEqual(TEXT("First at the left margin"), static_cast<float>(Two[0].X), 48.f);
    TestEqual(TEXT("Second a gap to its right"), static_cast<float>(Two[1].X), 48.f + 600.f + 16.f);
    TestEqual(TEXT("Centred above the hotbar"), static_cast<float>(Two[0].Y), (1080.f - 140.f - 800.f) * 0.5f);
    TestEqual(TEXT("Each window centred on its own height"), static_cast<float>(Two[1].Y), (1080.f - 140.f - 600.f) * 0.5f);
    // Taller than the space: held at the top margin.
    const TArray<FVector2D> Tall = SFComputeWindowLayout({FVector2D(600, 1000)}, View, 48.f, 16.f, 140.f);
    TestEqual(TEXT("Clamped to the top margin"), static_cast<float>(Tall[0].Y), 48.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFKitWindowStackTest, "ShatteredFrontier.Hud.Kit.WindowStack", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFKitWindowStackTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"), World)) { return false; }
    UUserWidget* A = MakeKitWidget<UUserWidget>(World, TEXT("WBP_Separator"));
    UUserWidget* B = MakeKitWidget<UUserWidget>(World, TEXT("WBP_Separator"));
    UUserWidget* C = MakeKitWidget<UUserWidget>(World, TEXT("WBP_Separator"));
    if (!TestNotNull(TEXT("Widgets"), A) || !B || !C) { return false; }
    USFWindowStack* Stack = NewObject<USFWindowStack>(GetTransientPackage());
    Stack->Open(A); Stack->Open(B); Stack->Open(C);
    TestEqual(TEXT("Three windows open at once"), Stack->Num(), 3);
    TestTrue(TEXT("Open shows the window"), A->GetVisibility() == ESlateVisibility::Visible);
    TestTrue(TEXT("Top is the newest"), Stack->Top() == C);
    Stack->Open(A);  // reopening moves it to the top
    TestTrue(TEXT("Reopened window is on top"), Stack->Top() == A);
    TestEqual(TEXT("Still three"), Stack->Num(), 3);
    TestTrue(TEXT("Esc closes the top"), Stack->CloseTop());
    TestTrue(TEXT("Closed window is hidden"), A->GetVisibility() == ESlateVisibility::Collapsed);
    TestFalse(TEXT("And no longer open"), Stack->IsOpen(A));
    TestTrue(TEXT("Next top"), Stack->Top() == C);
    TestTrue(TEXT("Esc again"), Stack->CloseTop());
    TestTrue(TEXT("Then the oldest"), Stack->Top() == B);
    TestTrue(TEXT("Esc again"), Stack->CloseTop());
    TestFalse(TEXT("Nothing left to close"), Stack->CloseTop());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFKitStepperTest, "ShatteredFrontier.Hud.Kit.Stepper", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFKitStepperTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Clamp above"), USFStepperWidget::ClampValue(12, 1, 10), 10);
    TestEqual(TEXT("Clamp below"), USFStepperWidget::ClampValue(-3, 1, 10), 1);
    TestEqual(TEXT("Max below min collapses to min"), USFStepperWidget::ClampValue(5, 4, 2), 4);
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    USFStepperWidget* Stepper = World ? MakeKitWidget<USFStepperWidget>(World, TEXT("WBP_Stepper")) : nullptr;  // needs create_widgets.py
    if (!TestNotNull(TEXT("WBP_Stepper constructs"), Stepper)) { return false; }
    for (const TCHAR* Name : {TEXT("MinusButton"), TEXT("PlusButton"), TEXT("ValueText"), TEXT("MinusFill"), TEXT("PlusFill")}) { TestNotNull(FString::Printf(TEXT("%s bound"), Name), Stepper->GetWidgetFromName(Name)); }
    Stepper->SetRange(1, 3);
    TestEqual(TEXT("Starts at 1"), Stepper->GetValue(), 1);
    TestTrue(TEXT("+ changes the value"), Stepper->Increment());
    TestTrue(TEXT("+ again"), Stepper->Increment());
    TestFalse(TEXT("+ at the maximum does nothing"), Stepper->Increment());
    TestEqual(TEXT("Clamped at 3"), Stepper->GetValue(), 3);
    TestTrue(TEXT("- changes the value"), Stepper->Decrement());
    TestEqual(TEXT("Down to 2"), Stepper->GetValue(), 2);
    TestTrue(TEXT("SetValue clamps and reports the change"), Stepper->SetValue(99));
    TestEqual(TEXT("99 clamps to 3"), Stepper->GetValue(), 3);
    Stepper->SetRange(1, 2);
    TestEqual(TEXT("Range shrink clamps the value"), Stepper->GetValue(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFKitHoldButtonTest, "ShatteredFrontier.Hud.Kit.HoldButton", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFKitHoldButtonTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    USFButtonWidget* Button = World ? MakeKitWidget<USFButtonWidget>(World, TEXT("WBP_Button")) : nullptr;  // needs create_widgets.py
    if (!TestNotNull(TEXT("WBP_Button constructs"), Button)) { return false; }
    TestNotNull(TEXT("HoldFill bound"), Button->GetWidgetFromName(TEXT("HoldFill")));
    int32 Activated = 0;  // AdvanceHold returns true on the call that raises OnActivated
    Button->bHoldToActivate = true; Button->HoldSeconds = 1.f; Button->HoldDrainSeconds = 0.5f;
    Activated += Button->AdvanceHold(0.9f, true) ? 1 : 0;
    TestEqual(TEXT("No activation yet"), Activated, 0);
    const bool bFired = Button->AdvanceHold(0.2f, true);
    TestTrue(TEXT("Activates when the fill completes"), bFired);
    Activated += bFired ? 1 : 0;
    TestEqual(TEXT("Exactly one activation"), Activated, 1);
    Activated += Button->AdvanceHold(1.f, true) ? 1 : 0;
    TestEqual(TEXT("Still one while held"), Activated, 1);
    Button->AdvanceHold(0.25f, false);
    TestEqual(TEXT("Fill drains after release"), Button->GetHoldProgress(), 0.5f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFKitPartsTest, "ShatteredFrontier.Hud.Kit.Parts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFKitPartsTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"), World)) { return false; }
    TestNotNull(TEXT("WBP_WindowFrame"), MakeKitWidget<USFWindowFrameWidget>(World, TEXT("WBP_WindowFrame")));
    TestNotNull(TEXT("WBP_Separator"), MakeKitWidget<USFSeparatorWidget>(World, TEXT("WBP_Separator")));
    TestNotNull(TEXT("WBP_ScrollList"), MakeKitWidget<USFScrollListWidget>(World, TEXT("WBP_ScrollList")));
    TestNotNull(TEXT("WBP_SearchField"), MakeKitWidget<USFTextFieldWidget>(World, TEXT("WBP_SearchField")));
    TestNotNull(TEXT("WBP_InputField"), MakeKitWidget<USFTextFieldWidget>(World, TEXT("WBP_InputField")));
    TestNotNull(TEXT("WBP_Dropdown"), MakeKitWidget<USFDropdownWidget>(World, TEXT("WBP_Dropdown")));
    USFGoldDisplayWidget* Gold = MakeKitWidget<USFGoldDisplayWidget>(World, TEXT("WBP_GoldDisplay"));
    if (TestNotNull(TEXT("WBP_GoldDisplay"), Gold)) { Gold->SetAmount(1234567); }
    TestEqual(TEXT("Gold groups digits"), USFGoldDisplayWidget::FormatAmount(1234567).ToString(), FString(TEXT("1,234,567")));
    TestEqual(TEXT("Digits-only filter"), USFTextFieldWidget::DigitsOnly(TEXT("a1b2-3 ")), FString(TEXT("123")));
    USFTabBarWidget* Bar = MakeKitWidget<USFTabBarWidget>(World, TEXT("WBP_TabBar"));
    if (TestNotNull(TEXT("WBP_TabBar"), Bar))
    {
        Bar->SetTabs({FSFTabSpec(FText::FromString(TEXT("A"))), FSFTabSpec(FText::FromString(TEXT("B")))});
        TestEqual(TEXT("Two tabs built"), Bar->NumTabs(), 2);
        TestEqual(TEXT("First tab selected"), Bar->GetSelectedIndex(), 0);
        Bar->SelectTab(1, true);
        TestEqual(TEXT("Second tab selected"), Bar->GetSelectedIndex(), 1);
        if (Bar->GetTab(0) && Bar->GetTab(1)) { TestTrue(TEXT("Tab 1 selected, tab 0 not"), Bar->GetTab(1)->IsSelected() && !Bar->GetTab(0)->IsSelected()); }
    }
    return true;
}
#endif
