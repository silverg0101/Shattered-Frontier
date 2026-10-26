#include "UI/SFCombatFeedback.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatFeedbackWidgetTest,
    "ShatteredFrontier.Hud.CombatFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCombatFeedbackWidgetTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    const TSubclassOf<USFCombatDamageWidget> Class = LoadClass<USFCombatDamageWidget>(nullptr, USFCombatDamageWidget::AssetPath);
    if (!TestNotNull(TEXT("Authored damage widget"), Class.Get())) return false;
    USFCombatDamageWidget* Widget = CreateWidget<USFCombatDamageWidget>(World, Class);
    if (!TestNotNull(TEXT("Damage widget"), Widget) || !Widget->DamageText || !Widget->CriticalText) return false;
    UTextBlock* CriticalNumber = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("CriticalNumber")));
    if (!TestNotNull(TEXT("Authored critical numeral"), CriticalNumber)) return false;
    Widget->Present(8, false);
    TestEqual(TEXT("Actual resolved ordinary damage"), Widget->DamageText->GetText().ToString(), FString(TEXT("8")));
    TestEqual(TEXT("Ordinary damage hides critical accent"), Widget->CriticalText->GetVisibility(), ESlateVisibility::Collapsed);
    Widget->Present(19, true);
    TestEqual(TEXT("Actual resolved critical damage"), CriticalNumber->GetText().ToString(), FString(TEXT("19")));
    TestEqual(TEXT("Critical has non-color distinction"), Widget->CriticalText->GetText().ToString(), FString(TEXT("CRITICAL!")));
    TestNotEqual(TEXT("Critical accent visible"), Widget->CriticalText->GetVisibility(), ESlateVisibility::Collapsed);
    TestNotEqual(TEXT("Authored critical color differs"), CriticalNumber->GetColorAndOpacity().GetSpecifiedColor(), Widget->DamageText->GetColorAndOpacity().GetSpecifiedColor());
    Widget->Present(1, false);
    TestEqual(TEXT("Later ordinary hit clears critical numeral"), CriticalNumber->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Later ordinary hit preserves individual amount"), Widget->DamageText->GetText().ToString(), FString(TEXT("1")));
    TestEqual(TEXT("Missing receiver anchor is safe"), USFCombatFeedbackSubsystem::HeadAnchor(nullptr), FVector::ZeroVector);
    return true;
}
#endif
