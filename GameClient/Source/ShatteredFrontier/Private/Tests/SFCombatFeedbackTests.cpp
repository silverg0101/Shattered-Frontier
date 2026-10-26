#include "UI/SFCombatFeedback.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Components/TextBlock.h"
#include "Framework/SFHudStyle.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatFeedbackBurstTest,
    "ShatteredFrontier.Hud.CombatFeedback.BurstSeparation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCombatFeedbackBurstTest::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    const TSubclassOf<USFCombatDamageWidget> Class = LoadClass<USFCombatDamageWidget>(nullptr, USFCombatDamageWidget::AssetPath);
    if (!TestNotNull(TEXT("Real damage WBP"), Class.Get())) return false;
    struct FHit { int32 Damage; bool bCritical; float Birth; FVector2D Size = FVector2D::ZeroVector; float Offset = 0; };
    TArray<FHit> Hits = {{8, false, 0}, {19, true, .13f}, {8, false, .26f}, {8, false, .39f},
        {19, true, .52f}, {8, false, .65f}, {2147483647, true, .66f}, {2147483647, false, .67f},
        {19, true, .68f}, {8, false, .69f}, {19, true, .70f}, {8, false, .71f},
        {19, true, .72f}, {8, false, .73f}, {19, true, .74f}, {8, false, .75f}};
    for (FHit& Hit : Hits)
    {
        USFCombatDamageWidget* Widget = CreateWidget<USFCombatDamageWidget>(World, Class);
        if (!TestNotNull(TEXT("Real widget instance"), Widget)) return false;
        Widget->ApplyKitStyle(); // Same production initialization; this editor-only widget has no player context.
        Widget->Present(Hit.Damage, Hit.bCritical);
        Hit.Size = Widget->MeasureVisualSize();
        TestTrue(TEXT("Unattached real widget has measurable glyph bounds"), Hit.Size.X > 2 && Hit.Size.Y > 2);
        TestNotNull(TEXT("Actual project numeral font loaded"), Widget->DamageText->GetFont().FontObject.Get());
        TestNotEqual(TEXT("Numeral uses the project font rather than UMG default"), Widget->DamageText->GetFont().FontObject.Get(), GetDefault<UTextBlock>()->GetFont().FontObject.Get());
        TestTrue(TEXT("Measured widget includes text shadow bounds"), Hit.Size.X > Widget->GetDesiredSize().X && Hit.Size.Y > Widget->GetDesiredSize().Y);
        if (Hit.bCritical)
        {
            TestTrue(TEXT("Full critical label contributes to measured width"), Hit.Size.X >= Widget->CriticalText->GetDesiredSize().X);
            TestTrue(TEXT("Critical label and numeral contribute to measured height"), Hit.Size.Y >= Widget->CriticalText->GetDesiredSize().Y + Widget->CriticalNumber->GetDesiredSize().Y);
        }
    }
    const FVector2D Anchor(960, 500);
    TestTrue(TEXT("Fixture reproduces the old fixed 28px collision"),
        FSFCombatDamageLayout::Bounds(Anchor, Hits[0].Size, 0, .13f, false).Intersect(
            FSFCombatDamageLayout::Bounds(Anchor, Hits[1].Size, 28, 0, true)));
    TArray<int32> Active;
    int32 Next = 0;
    int32 MaximumActive = 0, Evictions = 0;
    for (int32 Step = 0; Step <= 210; ++Step)
    {
        const float Now = Step * .01f;
        while (Next < Hits.Num() && Hits[Next].Birth <= Now)
        {
            if (Active.Num() == FSFCombatDamageLayout::MaxEntries) { Active.RemoveAt(0); ++Evictions; }
            Active.Add(Next++);
        }
        Active.RemoveAll([&](int32 Index) { return Now - Hits[Index].Birth >= FSFCombatDamageLayout::Lifetime; });
        MaximumActive = FMath::Max(MaximumActive, Active.Num());
        TArray<FBox2D> Reserved, Rendered;
        for (int32 Index : Active)
        {
            FHit& Hit = Hits[Index];
            const float Age = Now - Hit.Birth;
            Hit.Offset = FSFCombatDamageLayout::FindLane(Anchor, Hit.Size, Hit.Offset, Hit.bCritical, Reserved);
            const FBox2D Envelope = FSFCombatDamageLayout::Envelope(Anchor, Hit.Size, Hit.Offset, Hit.bCritical);
            const FBox2D Bounds = FSFCombatDamageLayout::Bounds(Anchor, Hit.Size, Hit.Offset, Age, Hit.bCritical);
            if (!TestTrue(TEXT("The entire rendered pop/float path stays inside its reservation"),
                Bounds.Min.X >= Envelope.Min.X - .001 && Bounds.Min.Y >= Envelope.Min.Y - .001 &&
                Bounds.Max.X <= Envelope.Max.X + .001 && Bounds.Max.Y <= Envelope.Max.Y + .001)) return false;
            for (const FBox2D& Other : Rendered)
                if (!TestFalse(FString::Printf(TEXT("Individual text/critical/shadow bounds overlap at %.2fs"), Now), Bounds.Intersect(Other))) return false;
            Reserved.Add(Envelope); Rendered.Add(Bounds);
        }
    }
    TestEqual(TEXT("Shipped global cap remains twelve simultaneously separated entries"), MaximumActive, 12);
    TestEqual(TEXT("Only entries beyond twelve are evicted"), Evictions, 4);
    return true;
}
#endif
