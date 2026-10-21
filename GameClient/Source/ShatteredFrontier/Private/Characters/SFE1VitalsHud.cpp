#include "Characters/SFE1VitalsHud.h"

#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Framework/SFHudStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
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

FString FSFE1VitalsModel::Countdown(float RemainingSeconds)
{
    const int32 Whole = FMath::CeilToInt(FMath::Max(0.f, RemainingSeconds));
    return FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60);
}

void USFE1VitalsHud::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("VitalsCanvas"));
    WidgetTree->RootWidget = Canvas;

    // Bottom-left crest plate: left 2-27% of the width, bottom 4-14% (HUD-v1 layout).
    UBorder* Plate = Panel(WidgetTree, Slate(0.9f), Brass(), FMargin(10.f, 8.f), 4.f);
    UCanvasPanelSlot* PlateSlot = Canvas->AddChildToCanvas(Plate);
    PlateSlot->SetAnchors(FAnchors(0.f, 1.f)); PlateSlot->SetAlignment(FVector2D(0.f, 1.f));
    PlateSlot->SetPosition(FVector2D(32.f, -32.f)); PlateSlot->SetSize(FVector2D(400.f, 68.f));
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Plate->SetContent(Row);

    // Heraldic crest stand-in: a brass-edged diamond. Real stag art is not part of this change.
    USizeBox* CrestBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    CrestBox->SetWidthOverride(34.f); CrestBox->SetHeightOverride(34.f);
    UBorder* Crest = Panel(WidgetTree, Hex(TEXT("0E171C"), 1.f), Brass(), FMargin(0.f), 3.f);
    Crest->SetRenderTransformAngle(45.f);
    CrestBox->SetContent(Crest);
    UHorizontalBoxSlot* CrestSlot = Row->AddChildToHorizontalBox(CrestBox);
    CrestSlot->SetVerticalAlignment(VAlign_Center); CrestSlot->SetPadding(FMargin(6.f, 0.f, 18.f, 0.f));

    UOverlay* Health = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    USizeBox* HealthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    HealthBox->SetHeightOverride(30.f); HealthBox->SetContent(Health);
    UHorizontalBoxSlot* HealthSlot = Row->AddChildToHorizontalBox(HealthBox);
    HealthSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); HealthSlot->SetVerticalAlignment(VAlign_Center);
    Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    StyleBar(Bar, Crimson());
    UOverlaySlot* BarSlot = Health->AddChildToOverlay(Bar);
    BarSlot->SetHorizontalAlignment(HAlign_Fill); BarSlot->SetVerticalAlignment(VAlign_Fill);
    HealthText = Text(WidgetTree, TEXT(""), 14, Ivory(), true);
    UOverlaySlot* TextSlot = Health->AddChildToOverlay(HealthText);
    TextSlot->SetHorizontalAlignment(HAlign_Right); TextSlot->SetVerticalAlignment(VAlign_Center); TextSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));

    // Defeated: dimmed scene plus a centred panel (HUD-v1 hud-defeated.png, 32-68% wide).
    UOverlay* DefeatedRoot = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UCanvasPanelSlot* DefeatedSlot = Canvas->AddChildToCanvas(DefeatedRoot);
    DefeatedSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); DefeatedSlot->SetOffsets(FMargin(0.f));
    DefeatedLayer = DefeatedRoot;
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Dim->SetBrush(Frame(FLinearColor(0.f, 0.f, 0.f, 0.4f), FLinearColor(0.f, 0.f, 0.f, 0.f), 0.f));
    DefeatedRoot->AddChildToOverlay(Dim);
    USizeBox* PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    PanelBox->SetWidthOverride(460.f);
    UOverlaySlot* PanelSlot = DefeatedRoot->AddChildToOverlay(PanelBox);
    PanelSlot->SetHorizontalAlignment(HAlign_Center); PanelSlot->SetVerticalAlignment(VAlign_Center);
    UBorder* Card = Panel(WidgetTree, Slate(0.94f), Brass(), FMargin(32.f, 24.f), 6.f);
    PanelBox->SetContent(Card);
    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Card->SetContent(Stack);
    auto AddCentred = [Stack](UWidget* Child, const FMargin& Pad)
    { UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(Child); S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(Pad); };
    AddCentred(Text(WidgetTree, TEXT("Defeated"), 36, Ivory(), true), FMargin(0.f, 0.f, 0.f, 6.f));
    USizeBox* Rule = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Rule->SetHeightOverride(1.f); Rule->SetContent(Panel(WidgetTree, Brass(), Brass(), FMargin(0.f), 0.f));
    UVerticalBoxSlot* RuleSlot = Stack->AddChildToVerticalBox(Rule); RuleSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
    AddCentred(Text(WidgetTree, TEXT("Your journey continues"), 15, Ivory()), FMargin(0.f, 0.f, 0.f, 8.f));
    UHorizontalBox* Timer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Timer->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Respawn in "), 20, Ivory()))->SetVerticalAlignment(VAlign_Center);
    CountdownText = Text(WidgetTree, TEXT("00:00"), 26, Crimson(), true);
    Timer->AddChildToHorizontalBox(CountdownText)->SetVerticalAlignment(VAlign_Center);
    AddCentred(Timer, FMargin(0.f, 0.f, 0.f, 4.f));
    DefeatedLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void USFE1VitalsHud::Present(const FSFE1VitalsModel& Model, int32 MaxHealth, float RemainingSeconds)
{
    if (!Bar) { return; }
    Bar->SetPercent(FSFE1VitalsModel::HealthFraction(Model.Health, MaxHealth));
    HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::Max(0, Model.Health), MaxHealth)));
    DefeatedLayer->SetVisibility(Model.IsDefeated() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    CountdownText->SetText(FText::FromString(FSFE1VitalsModel::Countdown(RemainingSeconds)));
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
