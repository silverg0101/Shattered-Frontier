#include "Characters/SFE1VitalsHud.h"

#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Framework/SFPlayerController.h"
#include "Framework/SFScreenArt.h"
#include "UI/SFHudWidgets.h"
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
    Window->SetPadding(FMargin(40.f, 36.f, 40.f, 18.f));
    UCanvasPanelSlot* WindowSlot = DefeatedRoot->AddChildToCanvas(Window);
    WindowSlot->SetAnchors(FAnchors(0.2786f, 0.177f, 0.7177f, 0.76f)); WindowSlot->SetOffsets(FMargin(0.f));  // x 535-1378, y 191-821 at 1080p
    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    USizeBox* WindowWidth = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    WindowWidth->SetWidthOverride(763.f);  // 843 minus the window padding
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
    AddCentred(Text(WidgetTree, TEXT("Defeated"), 80, Ivory(), true), FMargin(0.f));  // mixed-case serif as in defeated.png (Cinzel has no lowercase)
    // Crimson engraved rule with the stag at its centre: the kit divider and stag emblem, tinted crimson.
    auto KitPicture = [this](const TCHAR* Piece, const FVector2D& Size, const FLinearColor& Tint)
    {
        UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Image->SetBrush(SFScreenArt::KitImage(Piece, Size));
        Image->SetColorAndOpacity(Tint);
        return Image;
    };
    UOverlay* Rule = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UBorder* RuleBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    RuleBar->SetBrush(Frame(Crimson(), Crimson(), 0.f));  // plain 545x2 crimson rule (the kit divider art collapses at 2 px)
    UOverlaySlot* RuleLine = Rule->AddChildToOverlay(Sized(RuleBar, 545.f, 2.f));
    RuleLine->SetHorizontalAlignment(HAlign_Center); RuleLine->SetVerticalAlignment(VAlign_Center);
    UOverlaySlot* RuleStag = Rule->AddChildToOverlay(KitPicture(TEXT("icon_stag_emblem"), FVector2D(30.f, 30.f), Crimson()));
    RuleStag->SetHorizontalAlignment(HAlign_Center);
    AddCentred(Rule, FMargin(0.f, 0.f, 0.f, 4.f));
    CauseText = Text(WidgetTree, TEXT(""), 28, Ivory());
    AddCentred(CauseText, FMargin(0.f, 0.f, 0.f, 18.f));

    // Respawn cards: the server respawns at the nearest zone point (selected); no bedroll can be placed yet.
    UHorizontalBox* Cards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    auto AddCard = [&](bool bSelected, const TCHAR* Art, const TCHAR* TitleText, const TCHAR* StatusText) -> UTextBlock*
    {
        // 333x241 well: the 320x160 picture on top, name (26) and status (20) centred on the slate plate below it.
        UBorder* Well = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Well->SetBrush(Frame(Hex(TEXT("17232A")), FLinearColor::Transparent, 4.f));  // no kit frame: the Edge overlay below is the only outline
        Well->SetPadding(FMargin(6.5f, 6.f, 6.5f, 0.f));
        UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Lines->AddChildToVerticalBox(Sized(Picture(Art, FVector2D(320.f, 160.f), bSelected ? 1.f : 0.55f), 320.f, 160.f));
        UTextBlock* Title = Text(WidgetTree, TitleText, 26, Ivory());
        UVerticalBoxSlot* TitleSlot = Lines->AddChildToVerticalBox(Title); TitleSlot->SetHorizontalAlignment(HAlign_Center); TitleSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
        UVerticalBoxSlot* StatusSlot = Lines->AddChildToVerticalBox(Text(WidgetTree, StatusText, 20, Hex(TEXT("C9C2AE")))); StatusSlot->SetHorizontalAlignment(HAlign_Center);
        Well->SetContent(Lines);
        UOverlay* Card = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UOverlaySlot* WellSlot = Card->AddChildToOverlay(Well);
        WellSlot->SetHorizontalAlignment(HAlign_Fill); WellSlot->SetVerticalAlignment(VAlign_Fill);
        // Selection is an outline on its own layer (2 px moss, else a 1 px brass hairline); never a brush tint.
        UBorder* Edge = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        FSlateBrush EdgeBrush = Frame(FLinearColor::Transparent, bSelected ? Hex(TEXT("8FBF6A")) : Brass(), 4.f); EdgeBrush.OutlineSettings.Width = bSelected ? 2.f : 1.f;
        Edge->SetBrush(EdgeBrush);
        UOverlaySlot* EdgeSlot = Card->AddChildToOverlay(Edge);
        EdgeSlot->SetHorizontalAlignment(HAlign_Fill); EdgeSlot->SetVerticalAlignment(VAlign_Fill);
        // 46 px roundel at the top right: solid green tick on the chosen card, dark clock on the other.
        UOverlay* Roundel = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UBorder* Disc = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        FSlateBrush DiscBrush = Frame(Hex(TEXT("17232A")), bSelected ? Hex(TEXT("8FBF6A")) : Ivory(), 23.f); DiscBrush.OutlineSettings.Width = 2.f;
        Disc->SetBrush(DiscBrush);
        UOverlaySlot* DiscSlot = Roundel->AddChildToOverlay(Disc);
        DiscSlot->SetHorizontalAlignment(HAlign_Fill); DiscSlot->SetVerticalAlignment(VAlign_Fill);
        UOverlaySlot* Glyph = Roundel->AddChildToOverlay(KitPicture(bSelected ? TEXT("status_tick") : TEXT("icon_clock_roundel"), FVector2D(26.f, 26.f), Ivory()));
        Glyph->SetHorizontalAlignment(HAlign_Center); Glyph->SetVerticalAlignment(VAlign_Center);
        UOverlaySlot* RoundelSlot = Card->AddChildToOverlay(Sized(Roundel, 46.f, 46.f));
        RoundelSlot->SetHorizontalAlignment(HAlign_Right); RoundelSlot->SetVerticalAlignment(VAlign_Top); RoundelSlot->SetPadding(FMargin(0.f, 14.f, 13.f, 0.f));
        Cards->AddChildToHorizontalBox(Sized(Card, 333.f, 241.f))->SetPadding(FMargin(bSelected ? 0.f : 21.f, 0.f, 0.f, 0.f));  // 333 + 21 = 354 px pitch (620 -> 974)
        return Title;
    };
    PointText = AddCard(true, TEXT("respawn_silverbrook_shrine"), TEXT("Nearest respawn"), TEXT("Available now"));
    AddCard(false, TEXT("respawn_bedroll"), TEXT("Bedroll"), TEXT("None placed"));
    AddCentred(Cards, FMargin(0.f, 0.f, 0.f, 24.f));

    // The shared hold-to-confirm button (36 px ring inside its left end). The server respawns on its own, so nothing is held:
    // the ring's moss arc fills over the respawn delay and the countdown is its own line under the button.
    const FSFHudHoldButton Respawn = SFHudStyle::HoldButton(WidgetTree, TEXT("Hold to Respawn"), 501.f);
    RespawnRing = Respawn.Ring;
    AddCentred(Respawn.Face, FMargin(0.f, 0.f, 0.f, 8.f));
    CountdownText = Text(WidgetTree, TEXT(""), 18, Hex(TEXT("BDB6A2")));
    AddCentred(CountdownText, FMargin(0.f));
    DefeatedLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void USFE1VitalsHud::Present(const FSFE1VitalsModel& Model, float RemainingSeconds, const FSFE1DefeatView& Defeat)
{
    bDefeatShown = Model.IsDefeated();
    DefeatedLayer->SetVisibility(bDefeatShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bDefeatShown) { return; }
    CountdownText->SetText(FText::FromString(RemainingSeconds > 0.f ? TEXT("Respawning in ") + FSFE1VitalsModel::Countdown(RemainingSeconds) + TEXT(" · dropped items remain at the defeat site") : FString(TEXT("Respawning"))));
    CauseText->SetText(FText::FromString(Defeat.Cause.IsEmpty() ? FString(TEXT("You have fallen")) : TEXT("Defeated by ") + Defeat.Cause));
    if (!Defeat.RespawnPoint.IsEmpty()) { PointText->SetText(FText::FromString(Defeat.RespawnPoint)); }
    RespawnProgress = Defeat.DelaySeconds > 0.f ? FMath::Clamp(1.f - RemainingSeconds / Defeat.DelaySeconds, 0.f, 1.f) : 1.f;
}

int32 USFE1VitalsHud::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    if (!bDefeatShown || !RespawnRing) { return LayerId; }
    return SFHudStyle::PaintHoldArc(OutDrawElements, LayerId, RespawnRing, RespawnProgress);
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
    // Always on for the local E1 pawn (the master HUD's vitals). Before combat health first replicates (no combat pilot
    // here), nothing can have hurt the player, so the bar reads full.
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
    FSFE1VitalsModel Shown = Model;
    if (!Model.bActive) { Shown.Health = Settings->PlayerHealth; }
    Hud->Present(Shown, FSFE1VitalsModel::RemainingSeconds(Model.DefeatedAt, Now, Settings->PlayerRespawnDelaySeconds), Defeat);
    // The bars live in WBP_Hud; stamina is drawn full (no stamina stat yet).
    if (const ASFPlayerController* SFPlayer = Cast<ASFPlayerController>(PC))
    { if (USFHudWidget* HudWidget = SFPlayer->GetHudWidget()) { HudWidget->PresentVitals(Shown.Health, Settings->PlayerHealth, Settings->PlayerStamina, Settings->PlayerStamina); } }
}

void USFE1VitalsHudSubsystem::Deinitialize()
{
    if (DefeatGrade) { DefeatGrade->Destroy(); DefeatGrade = nullptr; }
    if (Hud) { Hud->RemoveFromParent(); Hud = nullptr; }
    Super::Deinitialize();
}
