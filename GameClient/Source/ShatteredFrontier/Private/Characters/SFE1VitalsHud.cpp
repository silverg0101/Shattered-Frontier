#include "Characters/SFE1VitalsHud.h"

#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void FSFE1VitalsModel::Observe(int32 InHealth, double Now)
{
    if (InHealth > 0) { bActive = true; DefeatedAt = -1; }
    else if (bActive && Health > 0) { DefeatedAt = Now; }
    Health = InHealth;
}

float FSFE1VitalsModel::HealthFraction(int32 InHealth, int32 MaxHealth)
{
    return MaxHealth > 0 ? FMath::Clamp(float(InHealth) / float(MaxHealth), 0.f, 1.f) : 0.f;
}

float FSFE1VitalsModel::RemainingSeconds(double DefeatedAt, double Now, float DelaySeconds)
{
    return DefeatedAt < 0 ? 0.f : FMath::Max(0.f, float(DefeatedAt + DelaySeconds - Now));
}

void USFE1VitalsHud::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("VitalsHud"));
    Panel->SetPadding(FMargin(10.f));
    Panel->SetBrushColor(FLinearColor(0.035f, 0.045f, 0.055f, 0.85f));
    UVerticalBox* Contents = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Panel->SetContent(Contents);
    WidgetTree->RootWidget = Panel;

    HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    HealthText->SetFontSize(13.f);
    Contents->AddChildToVerticalBox(HealthText)->SetPadding(FMargin(2.f));
    Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    Bar->SetFillColorAndOpacity(FLinearColor(0.75f, 0.12f, 0.1f, 1.f));
    Contents->AddChildToVerticalBox(Bar)->SetPadding(FMargin(2.f));
    DefeatText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    DefeatText->SetFontSize(18.f);
    DefeatText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.7f, 0.4f, 1.f)));
    Contents->AddChildToVerticalBox(DefeatText)->SetPadding(FMargin(2.f, 6.f, 2.f, 2.f));
}

void USFE1VitalsHud::Present(const FSFE1VitalsModel& Model, int32 MaxHealth, float RemainingSeconds)
{
    if (!Bar) { return; }
    Bar->SetPercent(FSFE1VitalsModel::HealthFraction(Model.Health, MaxHealth));
    HealthText->SetText(FText::FromString(FString::Printf(TEXT("Health %d / %d"), FMath::Max(0, Model.Health), MaxHealth)));
    DefeatText->SetVisibility(Model.IsDefeated() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    DefeatText->SetText(FText::FromString(FString::Printf(TEXT("Defeated, respawning in %ds"), FMath::CeilToInt(RemainingSeconds))));
}

void USFE1VitalsHudSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    const ASFE1Character* Pawn = PC && PC->IsLocalController() ? Cast<ASFE1Character>(PC->GetPawn()) : nullptr;
    if (!Pawn) { return; }
    const double Now = World->GetTimeSeconds();
    Model.Observe(Pawn->CombatHealth(), Now);
    if (!Model.bActive) { return; }
    if (!Hud)
    {
        Hud = CreateWidget<USFE1VitalsHud>(PC, USFE1VitalsHud::StaticClass());
        if (!Hud) { return; }
        Hud->AddToViewport(15);
        Hud->SetVisibility(ESlateVisibility::HitTestInvisible);
        Hud->SetAnchorsInViewport(FAnchors(0.5f, 1.f));
        Hud->SetAlignmentInViewport(FVector2D(0.5f, 1.f));
        Hud->SetPositionInViewport(FVector2D(0, -24), false);
        Hud->SetDesiredSizeInViewport(FVector2D(320, 96));
    }
    // Client reads the shipped release defaults; the server applies the same captured values.
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    Hud->Present(Model, Settings->PlayerHealth, FSFE1VitalsModel::RemainingSeconds(Model.DefeatedAt, Now, Settings->PlayerRespawnDelaySeconds));
}

void USFE1VitalsHudSubsystem::Deinitialize()
{
    if (Hud) { Hud->RemoveFromParent(); Hud = nullptr; }
    Super::Deinitialize();
}
