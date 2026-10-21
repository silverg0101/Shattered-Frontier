#include "Characters/SFE1VitalsHud.h"

#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Framework/SFScreenArt.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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

FString FSFE1VitalsModel::RespawnTitle(const FString& ZoneName, const FString& PointId)
{
    TArray<FString> Words;
    PointId.ParseIntoArray(Words, TEXT("_"));
    for (FString& Word : Words) { Word = Word.Left(1).ToUpper() + Word.Mid(1); }
    Words.Insert(ZoneName, 0);
    Words.RemoveAll([](const FString& Word) { return Word.IsEmpty(); });
    return FString::Join(Words, TEXT(" "));
}

void USFE1VitalsHud::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("VitalsCanvas"));
    WidgetTree->RootWidget = Canvas;

    // Vitals (IMPLEMENTATION-FIX table 1): 116 px hex level badge at x2%, bottom y96%, then the 380x32 HP and stamina
    // framed bars 10 px apart with centred Source Sans 20 numerals. No plate behind them.
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UCanvasPanelSlot* RowSlot = Canvas->AddChildToCanvas(Row);
    RowSlot->SetAnchors(FAnchors(0.02f, 0.96f)); RowSlot->SetAlignment(FVector2D(0.f, 1.f)); RowSlot->SetAutoSize(true);
    Row->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("hex_level_badge"), 116.f, 116.f))->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* Bars = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UHorizontalBoxSlot* BarsSlot = Row->AddChildToHorizontalBox(Bars);
    BarsSlot->SetVerticalAlignment(VAlign_Center); BarsSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
    auto AddBar = [&](const TCHAR* Base, const FLinearColor& Fallback, FSFFramedBar& Framed, TObjectPtr<UProgressBar>& Plain) -> UTextBlock*
    {
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        Framed = MakeFramedBar(WidgetTree, Base, 380.f);  // the styled UProgressBar when the kit is missing
        if (Framed.Root) { Stack->AddChildToOverlay(Framed.Root); }
        else
        {
            Plain = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
            StyleBar(Plain, Fallback);
            USizeBox* PlainBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            PlainBox->SetWidthOverride(380.f); PlainBox->SetHeightOverride(32.f); PlainBox->SetContent(Plain);
            Stack->AddChildToOverlay(PlainBox);
        }
        UTextBlock* Value = Num(WidgetTree, TEXT(""), 20, Ivory());
        Value->SetShadowOffset(FVector2D(1.f, 1.f)); Value->SetShadowColorAndOpacity(Hex(TEXT("0E171C")));
        UOverlaySlot* ValueSlot = Stack->AddChildToOverlay(Value);
        ValueSlot->SetHorizontalAlignment(HAlign_Center); ValueSlot->SetVerticalAlignment(VAlign_Center);
        Bars->AddChildToVerticalBox(Stack)->SetPadding(FMargin(0.f, Bars->GetChildrenCount() > 0 ? 10.f : 0.f, 0.f, 0.f));
        return Value;
    };
    HealthText = AddBar(TEXT("bar_hp"), Crimson(), HpBar, Bar);
    // ponytail: no stamina stat exists yet, so the bar is drawn full with no numeral; bind it when Character gains stamina.
    AddBar(TEXT("bar_stamina"), Amber(), StaminaBar, StaminaPlain);
    if (StaminaPlain) { StaminaPlain->SetPercent(1.f); }

    // Defeated (v5 defeated.png): 40% dim, then an 840 x 627 window centred at y18%, built from the v5 screen art.
    UCanvasPanel* DefeatedRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    UCanvasPanelSlot* DefeatedSlot = Canvas->AddChildToCanvas(DefeatedRoot);
    DefeatedSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); DefeatedSlot->SetOffsets(FMargin(0.f));
    DefeatedLayer = DefeatedRoot;
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Dim->SetBrush(Frame(FLinearColor(0.f, 0.f, 0.f, 0.4f), FLinearColor(0.f, 0.f, 0.f, 0.f), 0.f));
    UCanvasPanelSlot* DimSlot = DefeatedRoot->AddChildToCanvas(Dim);
    DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); DimSlot->SetOffsets(FMargin(0.f));
    UBorder* Window = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Window->SetBrush(KitOrFrame(TEXT("window_frame"), Slate(0.92f), Brass(), 0.f));
    Window->SetPadding(FMargin(40.f, 18.f, 40.f, 18.f));
    UCanvasPanelSlot* WindowSlot = DefeatedRoot->AddChildToCanvas(Window);
    WindowSlot->SetAnchors(FAnchors(0.5f, 0.178f)); WindowSlot->SetAlignment(FVector2D(0.5f, 0.f)); WindowSlot->SetAutoSize(true);  // 840 wide, height from content
    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    USizeBox* WindowWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    WindowWidth->SetWidthOverride(760.f);  // 840 minus the window padding
    WindowWidth->SetContent(Stack);
    Window->SetContent(WindowWidth);
    auto AddCentred = [Stack](UWidget* Child, const FMargin& Pad)
    { UVerticalBoxSlot* S = Stack->AddChildToVerticalBox(Child); S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(Pad); };
    auto Sized = [this](UWidget* Child, float Width, float Height)
    {
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        if (Width > 0) { Box->SetWidthOverride(Width); }
        if (Height > 0) { Box->SetHeightOverride(Height); }
        Box->SetContent(Child); return Box;
    };
    auto Picture = [this](const TCHAR* Art, const FVector2D& Size, float Shade)
    {
        UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Image->SetBrush(SFScreenArt::KitImage(Art, Size));
        Image->SetColorAndOpacity(FLinearColor(Shade, Shade, Shade, 1.f));
        return Image;
    };
    AddCentred(Heading(WidgetTree, TEXT("Defeated"), 72, Ivory()), FMargin(0.f));  // mixed-case serif as in defeated.png (Cinzel has no lowercase)
    // Crimson engraved rule with the stag at its centre: the kit divider and stag emblem, tinted crimson.
    auto KitPicture = [this](const TCHAR* Piece, const FVector2D& Size, const FLinearColor& Tint)
    {
        UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Image->SetBrush(SFScreenArt::KitImage(Piece, Size));
        Image->SetColorAndOpacity(Tint);
        return Image;
    };
    UOverlay* Rule = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UOverlaySlot* RuleLine = Rule->AddChildToOverlay(KitPicture(TEXT("divider"), FVector2D(560.f, 8.f), Crimson()));
    RuleLine->SetVerticalAlignment(VAlign_Center);
    UOverlaySlot* RuleStag = Rule->AddChildToOverlay(KitPicture(TEXT("icon_stag_emblem"), FVector2D(40.f, 40.f), Crimson()));
    RuleStag->SetHorizontalAlignment(HAlign_Center);
    AddCentred(Rule, FMargin(0.f, 0.f, 0.f, 4.f));
    CauseText = Text(WidgetTree, TEXT(""), 26, Ivory());
    AddCentred(CauseText, FMargin(0.f, 0.f, 0.f, 18.f));

    // Respawn cards: the server respawns at the nearest zone point (selected); no bedroll can be placed yet.
    UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    auto AddCard = [&](bool bSelected, const TCHAR* Art, const TCHAR* TitleText, const TCHAR* StatusText) -> UTextBlock*
    {
        UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UBorder* CardFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        CardFrame->SetBrush(KitOrFrame(TEXT("respawn_image_well"), Slate(0.8f), bSelected ? Moss() : Brass(), 0.f));
        if (bSelected) { CardFrame->SetBrushColor(FLinearColor(0.55f, 0.85f, 0.5f)); }  // moss edge for the chosen point
        CardFrame->SetPadding(FMargin(6.f, 6.f, 6.f, 10.f));
        UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Lines->AddChildToVerticalBox(Picture(Art, FVector2D(312.f, 150.f), bSelected ? 1.f : 0.55f))->SetHorizontalAlignment(HAlign_Center);
        UTextBlock* Title = Text(WidgetTree, TitleText, 24, Ivory());
        UVerticalBoxSlot* TitleSlot = Lines->AddChildToVerticalBox(Title);
        TitleSlot->SetHorizontalAlignment(HAlign_Center); TitleSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
        Lines->AddChildToVerticalBox(Text(WidgetTree, StatusText, 18, Hex(TEXT("C9C2AE"))))->SetHorizontalAlignment(HAlign_Center);
        CardFrame->SetContent(Lines);
        Card->AddChildToOverlay(CardFrame);
        // Tick roundel (moss disc + kit tick) on the chosen card, the kit clock roundel on the other.
        UOverlay* Roundel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        if (bSelected)
        {
            UBorder* Disc = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            Disc->SetBrush(Frame(Moss(), Ivory(), 19.f));
            UOverlaySlot* DiscSlot = Roundel->AddChildToOverlay(Disc);
            DiscSlot->SetHorizontalAlignment(HAlign_Fill); DiscSlot->SetVerticalAlignment(VAlign_Fill);
            UOverlaySlot* Tick = Roundel->AddChildToOverlay(KitPicture(TEXT("status_tick"), FVector2D(24.f, 24.f), Ivory()));
            Tick->SetHorizontalAlignment(HAlign_Center); Tick->SetVerticalAlignment(VAlign_Center);
        }
        else { Roundel->AddChildToOverlay(KitPicture(TEXT("icon_clock_roundel"), FVector2D(38.f, 38.f), FLinearColor::White)); }
        UOverlaySlot* RoundelSlot = Card->AddChildToOverlay(Sized(Roundel, 38.f, 38.f));
        RoundelSlot->SetHorizontalAlignment(HAlign_Right); RoundelSlot->SetVerticalAlignment(VAlign_Top); RoundelSlot->SetPadding(FMargin(0.f, 14.f, 14.f, 0.f));
        Cards->AddChildToHorizontalBox(Sized(Card, 326.f, 256.f))->SetPadding(FMargin(bSelected ? 0.f : 22.f, 0.f, 0.f, 0.f));
        return Title;
    };
    PointText = AddCard(true, TEXT("respawn_silverbrook_shrine"), TEXT("Nearest respawn"), TEXT("Available now"));
    AddCard(false, TEXT("respawn_bedroll"), TEXT("Bedroll"), TEXT("None placed"));
    AddCentred(Cards, FMargin(0.f, 0.f, 0.f, 24.f));

    // The shared Primary button (IMPLEMENTATION-FIX G3) with the 36 px hold ring at x20 inside its left end; the ring
    // fills over the server's respawn delay (the server respawns on its own, so nothing is held).
    const FSFHudButton Respawn = SFHudStyle::Button(WidgetTree, TEXT(""), EButtonKind::Primary, 480.f);
    CountdownText = Respawn.Label;
    UOverlay* RespawnFace = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    RespawnFace->AddChildToOverlay(Respawn.Root);
    // Dark disc under the ring so the moss arc reads on the brass face (defeated.png).
    UOverlay* RingStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UBorder* RingDisc = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RingDisc->SetBrush(Frame(Slate(0.92f), Hex(TEXT("5E4A28")), 18.f));
    UOverlaySlot* DiscSlot = RingStack->AddChildToOverlay(RingDisc);
    DiscSlot->SetHorizontalAlignment(HAlign_Fill); DiscSlot->SetVerticalAlignment(VAlign_Fill);
    RespawnRing = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
    RespawnRing->SetBrush(SFScreenArt::KitImage(TEXT("hold_confirm_ring"), FVector2D(36.f, 36.f)));
    UOverlaySlot* RingImageSlot = RingStack->AddChildToOverlay(RespawnRing);
    RingImageSlot->SetHorizontalAlignment(HAlign_Fill); RingImageSlot->SetVerticalAlignment(VAlign_Fill);
    UOverlaySlot* RingSlot = RespawnFace->AddChildToOverlay(Sized(RingStack, 36.f, 36.f));
    RingSlot->SetHorizontalAlignment(HAlign_Left); RingSlot->SetVerticalAlignment(VAlign_Center); RingSlot->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));
    AddCentred(RespawnFace, FMargin(0.f, 0.f, 0.f, 22.f));
    AddCentred(Text(WidgetTree, TEXT("You keep everything you carry."), 18, Hex(TEXT("BDB6A2"))), FMargin(0.f));
    DefeatedLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void USFE1VitalsHud::Present(const FSFE1VitalsModel& Model, int32 MaxHealth, float RemainingSeconds, const FSFE1DefeatView& Defeat)
{
    if (!HealthText) { return; }
    const float Fraction = FSFE1VitalsModel::HealthFraction(Model.Health, MaxHealth);
    if (HpBar.Root) { HpBar.SetFraction(Fraction); } else if (Bar) { Bar->SetPercent(Fraction); }
    HealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::Max(0, Model.Health), MaxHealth)));
    bDefeatShown = Model.IsDefeated();
    DefeatedLayer->SetVisibility(bDefeatShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bDefeatShown) { return; }
    CountdownText->SetText(FText::FromString(RemainingSeconds > 0.f ? TEXT("Respawn in ") + FSFE1VitalsModel::Countdown(RemainingSeconds) : FString(TEXT("Respawning"))));
    CauseText->SetText(FText::FromString(Defeat.Cause.IsEmpty() ? FString(TEXT("You have fallen")) : TEXT("Defeated by ") + Defeat.Cause));
    if (!Defeat.RespawnPoint.IsEmpty()) { PointText->SetText(FText::FromString(Defeat.RespawnPoint)); }
    RespawnProgress = Defeat.DelaySeconds > 0.f ? FMath::Clamp(1.f - RemainingSeconds / Defeat.DelaySeconds, 0.f, 1.f) : 1.f;
}

int32 USFE1VitalsHud::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    if (!bDefeatShown || !RespawnRing) { return LayerId; }
    // Moss arc inside the hold ring, clockwise from 12 o'clock (family rule 8).
    // Drawn in the ring image's own geometry, so DPI and layout scale match the ring art.
    const FGeometry& Ring = RespawnRing->GetCachedGeometry();
    const FVector2f Center = FVector2f(Ring.GetLocalSize()) * 0.5f;
    const float Radius = 0.36f * Ring.GetLocalSize().X;
    const float Fraction = FMath::Max(RespawnProgress, 0.01f);
    const int32 Count = FMath::Max(1, FMath::CeilToInt(64 * Fraction));
    TArray<FVector2f> Points;
    for (int32 I = 0; I <= Count; ++I)
    {
        const float Angle = -HALF_PI + TWO_PI * Fraction * I / Count;
        Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
    }
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Ring.ToPaintGeometry(), Points, ESlateDrawEffect::None, SFHudStyle::Hex(TEXT("7FA07A")), true, 5.f);
    return LayerId + 1;
}

namespace
{
struct FVitalsRespawnPoint { FString Id; FVector Location; };
struct FVitalsZone { FString Name; TArray<FVitalsRespawnPoint> Points; };

// Zone name and respawn points from the content file the server's combat pilot reads (positions in metres).
const FVitalsZone& VitalsZone()
{
    static const FVitalsZone Zone = []()
    {
        FVitalsZone Out;
        FString Source; TSharedPtr<FJsonObject> Root;
        const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/world/zones/greenlands_silverbrook.json"));
        if (!FFileHelper::LoadFileToString(Source, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Root) || !Root) { return Out; }
        Root->TryGetStringField(TEXT("display_name"), Out.Name);
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Root->TryGetArrayField(TEXT("respawn_points"), Values)) { return Out; }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject> Point = Value->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
            if (Point && Point->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
            { Out.Points.Add({Point->GetStringField(TEXT("point_id")), FVector((*Position)[0]->AsNumber() * 100.0, (*Position)[1]->AsNumber() * 100.0, 0.0)}); }
        }
        return Out;
    }();
    return Zone;
}
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
    if (Model.IsDefeated() && DefeatSeenAt != Model.DefeatedAt)
    {
        // Once per defeat: a boar nearby names the cause (boars are the pilot's only damage source), and the respawn
        // card names the nearest zone point by the server's own rule (2D distance to the defeat spot).
        DefeatSeenAt = Model.DefeatedAt;
        Defeat = FSFE1DefeatView();
        Defeat.DelaySeconds = Settings->PlayerRespawnDelaySeconds;
        const FVector At = Pawn->GetActorLocation();
        for (TActorIterator<ASFPilotBoar> It(World); It; ++It)
        { if (FVector::Dist2D(It->GetActorLocation(), At) < 1500.0) { Defeat.Cause = TEXT("Wild Boar"); break; } }
        const FVitalsZone& Zone = VitalsZone();
        double Best = TNumericLimits<double>::Max();
        for (const FVitalsRespawnPoint& Point : Zone.Points)
        {
            const double Distance = FVector::DistSquared2D(At, Point.Location);
            if (Distance < Best) { Best = Distance; Defeat.RespawnPoint = FSFE1VitalsModel::RespawnTitle(Zone.Name, Point.Id); }
        }
    }
    // Desaturate the scene while defeated with a local, unbound post-process volume.
    if (Model.IsDefeated() && !DefeatGrade)
    {
        FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
        DefeatGrade = World->SpawnActor<APostProcessVolume>(Params);
        if (DefeatGrade)
        {
            DefeatGrade->bUnbound = true; DefeatGrade->Priority = 100.f;
            DefeatGrade->Settings.bOverride_ColorSaturation = true; DefeatGrade->Settings.ColorSaturation = FVector4(0.3, 0.3, 0.3, 1.0);
        }
    }
    else if (!Model.IsDefeated() && DefeatGrade) { DefeatGrade->Destroy(); DefeatGrade = nullptr; }
    Hud->Present(Model, Settings->PlayerHealth, FSFE1VitalsModel::RemainingSeconds(Model.DefeatedAt, Now, Settings->PlayerRespawnDelaySeconds), Defeat);
}

void USFE1VitalsHudSubsystem::Deinitialize()
{
    if (DefeatGrade) { DefeatGrade->Destroy(); DefeatGrade = nullptr; }
    if (Hud) { Hud->RemoveFromParent(); Hud = nullptr; }
    Super::Deinitialize();
}
