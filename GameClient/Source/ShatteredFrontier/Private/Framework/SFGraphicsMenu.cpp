#include "Framework/SFGraphicsMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Framework/SFGameUserSettings.h"
#include "Framework/SFHudStyle.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
// Layout is authored in the concept's 1672 x 941 pixels and scaled to the 1920 x 1080 design size (docs/blueprints/menus.md).
constexpr float S = 1920.f / 1672.f;
constexpr float WindowX = 293.f, WindowY = 100.f, WindowW = 1088.f, WindowH = 705.f;
FVector2D P(float X, float Y) { return FVector2D((X - WindowX) * S, (Y - WindowY) * S); }

UCanvasPanelSlot* Put(UCanvasPanel* Stage, UWidget* Child, float X, float Y, float W, float H)
{
    UCanvasPanelSlot* Slot = Stage->AddChildToCanvas(Child);
    Slot->SetPosition(P(X, Y)); Slot->SetSize(FVector2D(W * S, H * S));
    return Slot;
}
// Text anchored at (X, CenterY), aligned by AlignX (0 left, 0.5 centre, 1 right).
UCanvasPanelSlot* PutText(UCanvasPanel* Stage, UWidget* Child, float X, float CenterY, float AlignX = 0.f)
{
    UCanvasPanelSlot* Slot = Stage->AddChildToCanvas(Child);
    Slot->SetAutoSize(true); Slot->SetAlignment(FVector2D(AlignX, 0.5)); Slot->SetPosition(P(X, CenterY));
    return Slot;
}

UButton* FlatButton(UWidgetTree* Tree, UWidget* Content)
{
    UButton* Control = Tree->ConstructWidget<USFHudButton>(USFHudButton::StaticClass());
    FButtonStyle Flat = Control->GetStyle();
    Flat.Normal = Flat.Hovered = Flat.Pressed = Flat.Disabled = FSlateNoResource();
    Flat.NormalPadding = FMargin(0.f); Flat.PressedPadding = FMargin(0.f);
    Control->SetStyle(Flat);
    Control->SetContent(Content);
    return Control;
}

UWidget* Hairline1(UWidgetTree* Tree, float Height = 1.f)
{
    USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Box->SetHeightOverride(Height);
    Box->SetContent(SFHudStyle::Panel(Tree, SFHudStyle::Hairline(), SFHudStyle::Hairline(), FMargin(0.f), 0.f));
    return Box;
}

const TCHAR* const LevelNames[5] = {TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Ultra")};
constexpr float PresetX[5] = {336.f, 531.f, 733.f, 943.f, 1145.f};
constexpr float PresetW[5] = {187.f, 193.f, 201.f, 194.f, 192.f};
}

FString USFGraphicsMenu::LevelName(int32 Level, bool bTessellation)
{
    if (bTessellation && Level <= 0) { return TEXT("Off"); }
    return LevelNames[FMath::Clamp(Level, 0, 4)];
}

FString USFGraphicsMenu::WindowModeName(EWindowMode::Type Mode)
{
    return Mode == EWindowMode::Fullscreen ? TEXT("Fullscreen") : Mode == EWindowMode::Windowed ? TEXT("Windowed") : TEXT("Borderless");
}

FString USFGraphicsMenu::FrameRateName(int32 Fps) { return Fps <= 0 ? FString(TEXT("Unlimited")) : FString::FromInt(Fps); }

USFGameUserSettings* USFGraphicsMenu::LiveSettings() const { return GEngine ? Cast<USFGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr; }

void USFGraphicsMenu::Click(UButton* Button, TFunction<void()> Fn)
{
    USFGraphicsClick* Handler = NewObject<USFGraphicsClick>(this);
    Handler->Fn = MoveTemp(Fn);
    Clicks.Add(Handler);
    FScriptDelegate Delegate; Delegate.BindUFunction(Handler, TEXT("Fire"));
    Button->OnClicked.Add(Delegate);
}

void USFGraphicsMenu::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    const USFGraphicsPresetSettings* Presets = GetDefault<USFGraphicsPresetSettings>();
    const USFGameUserSettings* Live = LiveSettings();
    const FSFGraphicsOptions Start = Live ? Live->CaptureGraphics() : Presets->PresetOptions(ESFGraphicsPreset::High);
    TWeakObjectPtr<USFGraphicsMenu> Self(this);
    EditSession = MakeUnique<FSFGraphicsEditSession>(Start, [Self](const FSFGraphicsOptions& Options)
    {
        if (USFGraphicsMenu* Menu = Self.Get()) { if (USFGameUserSettings* Settings = Menu->LiveSettings(); Settings && !Menu->bSuppressEngineApply) { Settings->ApplyGraphics(Options); } }
    }, Presets->KeepSecondsClamped());

    UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
    if (Resolutions.IsEmpty()) { Resolutions = {FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3840, 2160)}; }
    Resolutions.AddUnique(Start.Resolution);
    Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
    FrameRates = {30, 60, 90, 120, 144, 165, 240, 0};
    FrameRates.AddUnique(Start.FrameRateLimit);

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    UCanvasPanel* Stage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    UCanvasPanelSlot* StageSlot = Root->AddChildToCanvas(Stage);
    StageSlot->SetAnchors(FAnchors(0.5f, 0.5f)); StageSlot->SetAlignment(FVector2D(0.5, 0.5));
    StageSlot->SetPosition(FVector2D(1.f, -20.f)); StageSlot->SetSize(FVector2D(WindowW * S, WindowH * S));

    UBorder* WindowFrame = KitPanel(WidgetTree, TEXT("window_frame"), 1.f, Slate(0.94f), Brass(), FMargin(0.f), 6.f);
    FLinearColor WellOpaque = WellFill(); WellOpaque.A = 1.f;
    UBorder* WindowBacking = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    WindowBacking->SetBrush(Frame(WellOpaque, FLinearColor::Transparent, 6.f));
    UCanvasPanelSlot* BackingSlot = Stage->AddChildToCanvas(WindowBacking);
    BackingSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); BackingSlot->SetOffsets(FMargin(8.f));
    UCanvasPanelSlot* FrameSlot = Stage->AddChildToCanvas(WindowFrame);
    FrameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); FrameSlot->SetOffsets(FMargin(0.f));

    // Header: crest, title, flanking rules, close, tabs with the LB/RB keycaps.
    Put(Stage, KitImage(WidgetTree, TEXT("window_crest"), 150.f, 60.f), 774.f, 65.f, 150.f / S, 60.f / S);  // arch with the stag, centred on the title
    UTextBlock* Title = Heading(WidgetTree, TEXT("Settings"), 52, Ivory()); Title->SetJustification(ETextJustify::Center);
    PutText(Stage, Title, 839.f, 140.6f, 0.5f);
    // Title rules: a section_rule and a quest_diamond each side of "Settings".
    Put(Stage, KitImage(WidgetTree, TEXT("section_rule"), 110.f, 4.f), 640.f, 139.f, 110.f / S, 4.f / S);
    Put(Stage, KitImage(WidgetTree, TEXT("quest_diamond"), 12.f), 745.f, 135.f, 12.f / S, 12.f / S);
    Put(Stage, KitImage(WidgetTree, TEXT("quest_diamond"), 12.f), 925.f, 135.f, 12.f / S, 12.f / S);
    Put(Stage, KitImage(WidgetTree, TEXT("section_rule"), 110.f, 4.f), 937.f, 139.f, 110.f / S, 4.f / S);
    UBorder* CloseBox = Panel(WidgetTree, WellFill(), Brass(), FMargin(9.f), 4.f);
    CloseBox->SetContent(KitImage(WidgetTree, TEXT("close_x"), 24.f));
    UButton* Close = FlatButton(WidgetTree, CloseBox);
    Click(Close, [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->HandleEscape(); } });
    Put(Stage, Close, 1319.f, 121.f, 40.f, 40.f);
    Put(Stage, KitImage(WidgetTree, TEXT("keycap_lb"), 48.f, 30.f), 330.f, 189.f, 48.f / S, 30.f / S);
    Put(Stage, KitImage(WidgetTree, TEXT("keycap_rb"), 48.f, 30.f), 1297.f, 189.f, 48.f / S, 30.f / S);
    static const TCHAR* const TabNames[5] = {TEXT("Graphics"), TEXT("Audio"), TEXT("Controls"), TEXT("Interface"), TEXT("Gameplay")};
    static const float TabEdges[6] = {394.f, 570.f, 742.f, 920.f, 1091.f, 1273.f};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        // Only Graphics exists so far; the other tabs are the rest of the settings screen and do nothing yet.
        UBorder* Tab = SFHudStyle::Tab(WidgetTree, Index == 0);
        UTextBlock* Label = Text(WidgetTree, TabNames[Index], 25, Ivory()); Label->SetJustification(ETextJustify::Center);
        Tab->SetVerticalAlignment(VAlign_Center); Tab->SetHorizontalAlignment(HAlign_Center); Tab->SetContent(Label);
        Put(Stage, Tab, TabEdges[Index], 180.f, TabEdges[Index + 1] - TabEdges[Index], 48.f);
    }


    // Preset row.
    UTextBlock* PresetHeading = Heading(WidgetTree, TEXT("Graphics Preset"), 33, Ivory());
    PutText(Stage, PresetHeading, 335.f, 266.f);
    const FSFHudButton Detect = Button(WidgetTree, TEXT("Auto-Detect"), EButtonKind::Secondary, 0.f);
    Detect.Label->SetFont(RoleFont(TextFamily(), 20, false));
    if (UBorder* DetectPad = Cast<UBorder>(Detect.Label->GetParent())) { DetectPad->SetPadding(FMargin(6.f, 0.f)); }
    Click(Detect.Button, [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoAutoDetect(); } });
    Put(Stage, Detect.Root, 1112.f, 253.f, 110.f, 31.f);
    Put(Stage, Panel(WidgetTree, Hairline(), Hairline(), FMargin(0.f), 0.f), 1241.f, 258.f, 1.f / S, 22.f);
    UTextBlock* Status = Text(WidgetTree, TEXT("Preset: High"), 20, Ivory());
    StatusText = Status;
    PutText(Stage, Status, 1340.f, 269.f, 1.f);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const FSFHudButton Preset = Button(WidgetTree, LevelNames[Index], EButtonKind::Secondary, 0.f);
        Preset.Label->SetFont(RoleFont(TextFamily(), 23, false));
        Click(Preset.Button, [Self, Index]()
        {
            if (USFGraphicsMenu* Menu = Self.Get()) { Menu->Session().SelectPreset(static_cast<ESFGraphicsPreset>(Index)); Menu->Refresh(); }
        });
        PresetButtons[Index] = Preset.Button;
        Put(Stage, Preset.Root, PresetX[Index], 296.f, PresetW[Index], 41.f);
    }
    Put(Stage, Hairline1(WidgetTree), 335.f, 355.f, 1022.f, 1.f / S);

    FSFGraphicsEditSession* Edit = EditSession.Get();
    // ---- builders shared by both columns
    auto Stepper = [this, Self](int32 FontSize, TFunction<FString()> Value, TFunction<void(int32)> Step)
    {
        UOverlay* Box = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UBorder* Plate = KitPanel(WidgetTree, TEXT("field_well"), 1.f, WellFill(), Brass(), FMargin(0.f), 3.f, false);
        UOverlaySlot* PlateSlot = Box->AddChildToOverlay(Plate);
        PlateSlot->SetHorizontalAlignment(HAlign_Fill); PlateSlot->SetVerticalAlignment(VAlign_Fill);
        UTextBlock* ValueText = Text(WidgetTree, Value(), FontSize, Ivory()); ValueText->SetJustification(ETextJustify::Center);
        UOverlaySlot* ValueSlot = Box->AddChildToOverlay(ValueText);
        ValueSlot->SetHorizontalAlignment(HAlign_Center); ValueSlot->SetVerticalAlignment(VAlign_Center);
        for (int32 Dir : {-1, 1})
        {
            USizeBox* Arrow = KitImage(WidgetTree, TEXT("stepper_chevron"), 26.f, 26.f);
            if (Dir < 0) { Arrow->SetRenderTransform(FWidgetTransform(FVector2D::ZeroVector, FVector2D(-1.f, 1.f), FVector2D::ZeroVector, 0.f)); Arrow->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); }
            // 36 px end cell with the 16 px chevron centred in it.
            UBorder* Center = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
            Center->SetBrush(FSlateNoResource()); Center->SetPadding(FMargin(0.f)); Center->SetHorizontalAlignment(HAlign_Center); Center->SetVerticalAlignment(VAlign_Center); Center->SetContent(Arrow);
            USizeBox* Hit = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            Hit->SetWidthOverride(36.f); Hit->SetContent(Center);
            UButton* ArrowButton = FlatButton(WidgetTree, Hit);
            Click(ArrowButton, [Self, Step, Dir]() { Step(Dir); if (USFGraphicsMenu* Menu = Self.Get()) { Menu->Refresh(); } });
            UOverlaySlot* ArrowSlot = Box->AddChildToOverlay(ArrowButton);
            ArrowSlot->SetHorizontalAlignment(Dir < 0 ? HAlign_Left : HAlign_Right); ArrowSlot->SetVerticalAlignment(VAlign_Fill); ArrowSlot->SetPadding(FMargin(2.f, 0.f));
        }
        for (const bool bLeft : {true, false})
        {
            USizeBox* Divider = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            Divider->SetWidthOverride(1.f); Divider->SetContent(Panel(WidgetTree, Hairline(), Hairline(), FMargin(0.f), 0.f));
            Divider->SetVisibility(ESlateVisibility::HitTestInvisible);
            UOverlaySlot* DividerSlot = Box->AddChildToOverlay(Divider);
            DividerSlot->SetHorizontalAlignment(bLeft ? HAlign_Left : HAlign_Right); DividerSlot->SetVerticalAlignment(VAlign_Fill);
            DividerSlot->SetPadding(FMargin(bLeft ? 40.f : 0.f, 4.f, bLeft ? 0.f : 40.f, 4.f));
        }
        Steppers.Add({ValueText, MoveTemp(Value)});
        return Box;
    };
    auto Cycle = [](int32 Count, int32 Index, int32 Dir) { return (Index + Dir + Count) % Count; };
    auto Toggle = [this, Self, Stage](float CenterY, TFunction<bool()> Get, TFunction<void(bool)> Set)
    {
        FToggle Entry; Entry.Get = MoveTemp(Get);
        for (const bool bOnSide : {true, false})
        {
            const FSFHudButton Segment = SFHudStyle::Button(WidgetTree, TEXT(""), EButtonKind::Secondary, 0.f);
            UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
            UOverlaySlot* RootSlot = Face->AddChildToOverlay(Segment.Root);
            RootSlot->SetHorizontalAlignment(HAlign_Fill); RootSlot->SetVerticalAlignment(VAlign_Fill);
            UHorizontalBox* Content = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
            Content->SetVisibility(ESlateVisibility::HitTestInvisible);
            // The status_cross art is red, so the chosen Off plate (crimson) carries the ivory close_x glyph instead.
            UWidget* Glyph = bOnSide ? static_cast<UWidget*>(SFHudStyle::KitImage(WidgetTree, TEXT("status_tick"), 24.f))
                : static_cast<UWidget*>(SFHudStyle::KitImage(WidgetTree, TEXT("close_x"), 16.f));
            Content->AddChildToHorizontalBox(Glyph)->SetVerticalAlignment(VAlign_Center);
            Content->AddChildToHorizontalBox(SFHudStyle::Text(WidgetTree, bOnSide ? TEXT("On") : TEXT("Off"), 24, SFHudStyle::Ivory()))->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
            UOverlaySlot* ContentSlot = Face->AddChildToOverlay(Content);
            ContentSlot->SetHorizontalAlignment(HAlign_Center); ContentSlot->SetVerticalAlignment(VAlign_Center);
            Click(Segment.Button, [Self, Set, bOnSide]() { Set(bOnSide); if (USFGraphicsMenu* Menu = Self.Get()) { Menu->Refresh(); } });
            (bOnSide ? Entry.On : Entry.Off) = Segment.Button;
            (bOnSide ? Entry.OnTick : Entry.OffCross) = Glyph;
            Put(Stage, Face, bOnSide ? 583.f : 709.f, CenterY - 17.f, bOnSide ? 115.f : 112.f, 34.f);
        }
        Toggles.Add(MoveTemp(Entry));
    };

    // ---- Display column
    PutText(Stage, Heading(WidgetTree, TEXT("Display"), 32, Ivory()), 337.f, 387.f);
    Put(Stage, Hairline1(WidgetTree), 337.f, 407.f, 483.f, 1.f / S);
    auto RowLabel = [&](const TCHAR* Name, float CenterY) { PutText(Stage, Text(WidgetTree, Name, 23, Ivory()), 337.f, CenterY); };
    RowLabel(TEXT("Resolution"), 432.f);
    Put(Stage, Stepper(23, [Edit]() { return FString::Printf(TEXT("%d x %d"), Edit->Pending().Resolution.X, Edit->Pending().Resolution.Y); },
        [Self, Edit, Cycle](int32 Dir)
        {
            const TArray<FIntPoint>& List = Self->Resolutions;
            Edit->EditPending().Resolution = List[Cycle(List.Num(), FMath::Max(0, List.IndexOfByKey(Edit->Pending().Resolution)), Dir)];
        }), 583.f, 416.f, 237.f, 32.f);
    Put(Stage, Hairline1(WidgetTree), 337.f, 455.f, 483.f, 1.f / S);
    RowLabel(TEXT("Window Mode"), 478.f);
    Put(Stage, Stepper(23, [Edit]() { return WindowModeName(Edit->Pending().WindowMode); },
        [Edit, Cycle](int32 Dir)
        {
            static const EWindowMode::Type Modes[3] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
            int32 Index = 0; for (int32 I = 0; I < 3; ++I) { if (Modes[I] == Edit->Pending().WindowMode) { Index = I; } }
            Edit->EditPending().WindowMode = Modes[Cycle(3, Index, Dir)];
        }), 583.f, 462.f, 237.f, 32.f);
    Put(Stage, Hairline1(WidgetTree), 337.f, 501.f, 483.f, 1.f / S);
    RowLabel(TEXT("Frame Rate Limit"), 524.f);
    Put(Stage, Stepper(23, [Edit]() { return FrameRateName(Edit->Pending().FrameRateLimit); },
        [Self, Edit, Cycle](int32 Dir)
        {
            const TArray<int32>& List = Self->FrameRates;
            Edit->EditPending().FrameRateLimit = List[Cycle(List.Num(), FMath::Max(0, List.IndexOfByKey(Edit->Pending().FrameRateLimit)), Dir)];
        }), 583.f, 508.f, 237.f, 32.f);
    Put(Stage, Hairline1(WidgetTree), 337.f, 548.f, 483.f, 1.f / S);
    RowLabel(TEXT("V-Sync"), 573.f);
    Toggle(573.f, [Edit]() { return Edit->Pending().bVSync; }, [Edit](bool bOn) { Edit->EditPending().bVSync = bOn; });
    Put(Stage, Hairline1(WidgetTree), 337.f, 597.f, 483.f, 1.f / S);
    RowLabel(TEXT("Render Scale"), 615.f);
    PutText(Stage, Text(WidgetTree, TEXT("67%"), 20, Ivory()), 338.f, 650.f);
    USlider* Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
    Slider->SetMinValue(67.f); Slider->SetMaxValue(100.f); Slider->SetStepSize(1.f);
    FSliderStyle SliderStyle = Slider->GetWidgetStyle();
    // The track and its moss fill are separate layers under the slider; the slider itself draws only the thumb.
    SliderStyle.NormalBarImage = SliderStyle.HoveredBarImage = SliderStyle.DisabledBarImage = FSlateNoResource();
    if (const FSFHudKitElement* Thumb = Kit(TEXT("slider_thumb"))) { FSlateBrush ThumbBrush = KitBrush(*Thumb); ThumbBrush.ImageSize = FVector2D(25.f, 25.f); SliderStyle.NormalThumbImage = SliderStyle.HoveredThumbImage = SliderStyle.DisabledThumbImage = ThumbBrush; }
    SliderStyle.BarThickness = 12.f;
    Slider->SetWidgetStyle(SliderStyle);
    Slider->OnValueChanged.AddDynamic(this, &USFGraphicsMenu::HandleSlider);
    ScaleSlider = Slider;
    UProgressBar* ScaleFill = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    StyleBar(ScaleFill, Moss());
    FProgressBarStyle FillStyle = ScaleFill->GetWidgetStyle(); FillStyle.BackgroundImage = FSlateNoResource(); FillStyle.FillImage = Frame(FLinearColor::White, FLinearColor::Transparent, 2.f);
    ScaleFill->SetWidgetStyle(FillStyle); ScaleFill->SetFillColorAndOpacity(Hex(TEXT("465A41")));  // flat concept moss, not the lit bar_fill_moss
    ScaleFillBar = ScaleFill;
    Put(Stage, KitPanel(WidgetTree, TEXT("slider_track"), 1.f, WellFill(), Hairline(), FMargin(0.f), 3.f, false), 389.f, 644.f, 286.f, 12.f);
    Put(Stage, ScaleFill, 391.f, 646.f, 282.f, 8.f);
    Put(Stage, Slider, 383.f, 641.f, 298.f, 18.f);
    PutText(Stage, Text(WidgetTree, TEXT("100%"), 20, Ivory()), 693.f, 650.f);
    UOverlay* ScaleBox = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UBorder* ScalePlate = KitPanel(WidgetTree, TEXT("field_well"), 1.f, WellFill(), Brass(), FMargin(0.f), 3.f, false);
    UOverlaySlot* ScalePlateSlot = ScaleBox->AddChildToOverlay(ScalePlate); ScalePlateSlot->SetHorizontalAlignment(HAlign_Fill); ScalePlateSlot->SetVerticalAlignment(VAlign_Fill);
    UTextBlock* ScaleValue = Num(WidgetTree, TEXT("75%"), 27, Ivory()); ScaleText = ScaleValue;
    UOverlaySlot* ScaleValueSlot = ScaleBox->AddChildToOverlay(ScaleValue); ScaleValueSlot->SetHorizontalAlignment(HAlign_Center); ScaleValueSlot->SetVerticalAlignment(VAlign_Center);
    Put(Stage, ScaleBox, 748.f, 632.f, 76.f, 35.f);
    Put(Stage, Hairline1(WidgetTree), 337.f, 684.f, 483.f, 1.f / S);
    RowLabel(TEXT("Motion Blur"), 706.f);
    Toggle(707.f, [Edit]() { return Edit->Pending().bMotionBlur; }, [Edit](bool bOn) { Edit->EditPending().bMotionBlur = bOn; });

    // ---- Quality column: a "set all" strip, then one stepper per scalability group
    PutText(Stage, Heading(WidgetTree, TEXT("Quality"), 29, Ivory()), 875.f, 386.f);
    static const float LevelCenters[5] = {1021.f, 1093.f, 1167.f, 1233.f, 1301.f};
    for (int32 Index = 0; Index < 5; ++Index)
    {
        UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        UTextBlock* Name = Text(WidgetTree, LevelNames[Index], 17, Ivory()); Name->SetJustification(ETextJustify::Center);
        Stack->AddChildToVerticalBox(Name)->SetHorizontalAlignment(HAlign_Center);
        USizeBox* Underline = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Underline->SetHeightOverride(2.f); Underline->SetContent(Panel(WidgetTree, Amber(), Amber(), FMargin(0.f), 0.f));
        Stack->AddChildToVerticalBox(Underline)->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
        LevelUnderlines[Index] = Underline;
        UBorder* Center = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Center->SetBrush(FSlateNoResource()); Center->SetHorizontalAlignment(HAlign_Center); Center->SetVerticalAlignment(VAlign_Center); Center->SetContent(Stack);
        UButton* Pick = FlatButton(WidgetTree, Center);
        Click(Pick, [Self, Edit, Index]()
        {
            FSFGraphicsOptions& O = Edit->EditPending();
            O.ViewDistance = O.Shadows = O.GlobalIllumination = O.Reflections = O.Textures = O.Effects = O.Foliage = O.PostProcessing = O.AntiAliasing = O.NaniteTessellation = Index;
            if (USFGraphicsMenu* Menu = Self.Get()) { Menu->Refresh(); }
        });
        LevelButtons[Index] = Pick;
        Put(Stage, Pick, LevelCenters[Index] - 32.f, 372.f, 64.f, 28.f);
    }
    static const float LevelDividers[4] = {1050.f, 1130.f, 1203.f, 1267.f};
    for (const float X : LevelDividers) { Put(Stage, Panel(WidgetTree, Hairline(), Hairline(), FMargin(0.f), 0.f), X, 379.f, 1.f / S, 14.f); }

    struct FLevelRow { const TCHAR* Label; int32 FSFGraphicsOptions::* Field; bool bTessellation; };
    static const FLevelRow LevelRows[10] = {
        {TEXT("View Distance"), &FSFGraphicsOptions::ViewDistance, false}, {TEXT("Shadows"), &FSFGraphicsOptions::Shadows, false},
        {TEXT("Global Illumination"), &FSFGraphicsOptions::GlobalIllumination, false}, {TEXT("Reflections"), &FSFGraphicsOptions::Reflections, false},
        {TEXT("Textures"), &FSFGraphicsOptions::Textures, false}, {TEXT("Effects"), &FSFGraphicsOptions::Effects, false},
        {TEXT("Foliage"), &FSFGraphicsOptions::Foliage, false}, {TEXT("Post Processing"), &FSFGraphicsOptions::PostProcessing, false},
        {TEXT("Anti-Aliasing"), &FSFGraphicsOptions::AntiAliasing, false}, {TEXT("Nanite Tessellation"), &FSFGraphicsOptions::NaniteTessellation, true}};
    UScrollBox* List = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
    FScrollBarStyle BarStyle = List->GetWidgetBarStyle();
    BarStyle.NormalThumbImage = BarStyle.HoveredThumbImage = BarStyle.DraggedThumbImage = Frame(Amber(), Amber(), 3.f);
    BarStyle.HorizontalBackgroundImage = BarStyle.VerticalBackgroundImage = BarStyle.VerticalTopSlotImage = BarStyle.VerticalBottomSlotImage = Frame(FLinearColor::Transparent, FLinearColor::Transparent, 0.f);
    List->SetWidgetBarStyle(BarStyle);
    List->SetScrollbarThickness(FVector2D(6.f, 6.f));
    for (const FLevelRow& Row : LevelRows)
    {
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(Text(WidgetTree, Row.Label, 21, Ivory()));
        LabelSlot->SetPadding(FMargin(7.f * S, 0.f, 0.f, 0.f));
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center);
        const bool bTess = Row.bTessellation; int32 FSFGraphicsOptions::* const Field = Row.Field;
        UOverlay* Control = Stepper(21, [Edit, Field, bTess]() { return LevelName(Edit->Pending().*Field, bTess); },
            [Edit, Field](int32 Dir) { int32& Level = Edit->EditPending().*Field; Level = FMath::Clamp(Level + Dir, 0, FSFGraphicsOptions::MaxLevel); });
        USizeBox* ControlSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        ControlSize->SetWidthOverride(212.f * S); ControlSize->SetHeightOverride(27.f * S); ControlSize->SetContent(Control);
        UHorizontalBoxSlot* ControlSlot = Line->AddChildToHorizontalBox(ControlSize);
        ControlSlot->SetVerticalAlignment(VAlign_Center); ControlSlot->SetPadding(FMargin(0.f, 0.f, 10.f * S, 0.f));
        USizeBox* RowSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowSize->SetHeightOverride(30.9f * S);
        UOverlay* RowStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UOverlaySlot* LineSlot = RowStack->AddChildToOverlay(Line); LineSlot->SetHorizontalAlignment(HAlign_Fill); LineSlot->SetVerticalAlignment(VAlign_Fill);
        UOverlaySlot* RuleSlot = RowStack->AddChildToOverlay(Hairline1(WidgetTree)); RuleSlot->SetHorizontalAlignment(HAlign_Fill); RuleSlot->SetVerticalAlignment(VAlign_Bottom);
        RuleSlot->SetPadding(FMargin(7.f * S, 0.f, 10.f * S, 0.f));
        RowSize->SetContent(RowStack);
        List->AddChild(RowSize);
    }
    Put(Stage, List, 871.f, 411.f, 469.f, 312.f);

    // ---- Footer
    Put(Stage, Hairline1(WidgetTree), 300.f, 739.f, 1072.f, 1.f / S);
    const FSFHudButton Apply = Button(WidgetTree, TEXT("Apply"), EButtonKind::Secondary, 0.f);
    ApplyButtonStyle(Apply.Button, EButtonKind::Secondary, true);
    Click(Apply.Button, [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoApply(); } });
    Apply.Label->SetFont(RoleFont(TextFamily(), 28, false));
    ApplyButton = Apply.Button; ApplyLabel = Apply.Label;
    Put(Stage, Apply.Root, 595.f, 750.f, 218.f, 47.f);
    const FSFHudButton Revert = Button(WidgetTree, TEXT("Revert"), EButtonKind::Secondary, 0.f);
    Revert.Label->SetFont(RoleFont(TextFamily(), 28, false));
    Click(Revert.Button, [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoRevert(); } });
    Put(Stage, Revert.Root, 858.f, 750.f, 216.f, 47.f);
    Put(Stage, KitImage(WidgetTree, TEXT("quest_diamond"), 16.f), 828.f, 767.f, 16.f / S, 16.f / S);

    // ---- Keep-or-revert prompt over a dimmed window
    UCanvasPanel* Prompt = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    UCanvasPanelSlot* PromptSlot = Root->AddChildToCanvas(Prompt);
    PromptSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); PromptSlot->SetOffsets(FMargin(0.f));
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Dim->SetBrush(Frame(FLinearColor(0.f, 0.f, 0.f, 0.55f), FLinearColor::Transparent, 0.f));
    UCanvasPanelSlot* DimSlot = Prompt->AddChildToCanvas(Dim); DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); DimSlot->SetOffsets(FMargin(0.f));
    UBorder* Modal = KitPanel(WidgetTree, TEXT("modal_frame"), 1.f, Slate(0.96f), Brass(), FMargin(29.f, 41.f, 29.f, 30.f), 6.f);
    UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Modal->SetContent(Body);
    UTextBlock* KeepHead = Heading(WidgetTree, TEXT("Keep these display settings?"), 48, Ivory()); KeepHead->SetJustification(ETextJustify::Center);
    KeepTitle = KeepHead;
    Body->AddChildToVerticalBox(KeepHead)->SetHorizontalAlignment(HAlign_Center);
    UTextBlock* KeepDetail = Text(WidgetTree, TEXT(""), 29, Ivory()); KeepDetail->SetJustification(ETextJustify::Center);
    KeepDetailText = KeepDetail;
    Body->AddChildToVerticalBox(KeepDetail)->SetPadding(FMargin(0.f, 5.f, 0.f, 0.f));
    UTextBlock* Countdown = Text(WidgetTree, TEXT(""), 27, Ivory()); Countdown->SetJustification(ETextJustify::Center);
    KeepCountdown = Countdown;
    Body->AddChildToVerticalBox(Countdown)->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));
    UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
    StyleBar(Bar, Moss()); Bar->SetPercent(1.f); KeepBar = Bar;
    FProgressBarStyle KeepStyle = Bar->GetWidgetStyle();
    KeepStyle.FillImage = Frame(FLinearColor::White, FLinearColor::Transparent, 2.f); KeepStyle.BackgroundImage = FSlateNoResource();
    Bar->SetWidgetStyle(KeepStyle); Bar->SetFillColorAndOpacity(Hex(TEXT("557F3E")));  // the concept keep-bar moss
    USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    BarSize->SetWidthOverride(574.f * S); BarSize->SetHeightOverride(18.f);
    UBorder* BarFrame = Panel(WidgetTree, WellFill(), Brass(), FMargin(2.f), 3.f); BarFrame->SetContent(Bar); BarSize->SetContent(BarFrame);
    UVerticalBoxSlot* BarSlot = Body->AddChildToVerticalBox(BarSize);
    BarSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f)); BarSlot->SetHorizontalAlignment(HAlign_Center);
    UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    const FSFHudButton Keep = Button(WidgetTree, TEXT("Keep Changes"), EButtonKind::Secondary, 305.f * S);
    ApplyButtonStyle(Keep.Button, EButtonKind::Secondary, true);
    Keep.Root->SetHeightOverride(58.f * S); Keep.Label->SetFont(RoleFont(TextFamily(), 32, false));
    Click(Keep.Button, [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoKeep(); } });
    const FSFHudButton Undo = Button(WidgetTree, TEXT("Revert"), EButtonKind::Secondary, 302.f * S);
    Undo.Root->SetHeightOverride(58.f * S); Undo.Label->SetFont(RoleFont(TextFamily(), 32, false));
    Click(Undo.Button, [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoUndo(); } });
    Buttons->AddChildToHorizontalBox(Keep.Root)->SetPadding(FMargin(0.f, 0.f, 29.f, 0.f));
    Buttons->AddChildToHorizontalBox(Undo.Root);
    Body->AddChildToVerticalBox(Buttons)->SetHorizontalAlignment(HAlign_Center);
    USizeBox* ModalBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    // The modal_frame centre is translucent: an opaque slate backing keeps the menu from reading through.
    UOverlay* ModalStack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UBorder* Backing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Backing->SetBrush(Frame(WellOpaque, FLinearColor::Transparent, 6.f));
    UOverlaySlot* BackSlot = ModalStack->AddChildToOverlay(Backing);
    BackSlot->SetHorizontalAlignment(HAlign_Fill); BackSlot->SetVerticalAlignment(VAlign_Fill); BackSlot->SetPadding(FMargin(6.f));
    UOverlaySlot* ModalFrameSlot = ModalStack->AddChildToOverlay(Modal);
    ModalFrameSlot->SetHorizontalAlignment(HAlign_Fill); ModalFrameSlot->SetVerticalAlignment(VAlign_Fill);
    ModalBox->SetWidthOverride(718.f * S); ModalBox->SetHeightOverride(297.f * S); ModalBox->SetContent(ModalStack);
    UCanvasPanelSlot* ModalSlot = Prompt->AddChildToCanvas(ModalBox);
    ModalSlot->SetAnchors(FAnchors(0.5f, 0.5f)); ModalSlot->SetAlignment(FVector2D(0.5, 0.5)); ModalSlot->SetAutoSize(true); ModalSlot->SetPosition(FVector2D(0.f, 10.f));
    Prompt->SetVisibility(ESlateVisibility::Collapsed);
    KeepLayer = Prompt;

    Refresh();
}

void USFGraphicsMenu::Reload()
{
    if (const USFGameUserSettings* Live = LiveSettings()) { EditSession->Reset(Live->CaptureGraphics()); }
    Refresh();
}

bool USFGraphicsMenu::IsKeepPromptOpen() const { return EditSession && EditSession->IsAwaitingKeep(); }

bool USFGraphicsMenu::HandleEscape()
{
    if (IsKeepPromptOpen()) { DoUndo(); return true; }
    if (GetVisibility() == ESlateVisibility::Collapsed) { return false; }
    CloseMenu();
    return true;
}

void USFGraphicsMenu::HandleSlider(float Value)
{
    if (bRefreshingSlider) { return; }
    EditSession->EditPending().RenderScale = FMath::RoundToInt(Value);
    Refresh();
}

void USFGraphicsMenu::DoApply() { EditSession->Apply(); Refresh(); }
void USFGraphicsMenu::DoRevert() { EditSession->RevertPending(); Refresh(); }
void USFGraphicsMenu::DoKeep() { EditSession->Keep(); Refresh(); }
void USFGraphicsMenu::DoUndo() { EditSession->Undo(); Refresh(); }

void USFGraphicsMenu::DoAutoDetect()
{
    if (USFGameUserSettings* Live = LiveSettings()) { EditSession->SelectPreset(Live->BenchmarkPreset()); }
    Refresh();
}

void USFGraphicsMenu::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!EditSession->IsAwaitingKeep()) { return; }
    EditSession->Tick(InDeltaTime);
    if (!EditSession->IsAwaitingKeep()) { Refresh(); return; }
    const float Left = EditSession->SecondsLeft();
    if (KeepCountdown.IsValid()) { KeepCountdown->SetText(FText::FromString(FString::Printf(TEXT("Reverting in %d seconds"), FMath::CeilToInt(Left)))); }
    if (KeepBar.IsValid()) { KeepBar->SetPercent(Left / FMath::Max(1.f, GetDefault<USFGraphicsPresetSettings>()->KeepSecondsClamped())); }
}

void USFGraphicsMenu::Refresh()
{
    using namespace SFHudStyle;
    const FSFGraphicsOptions& Pending = EditSession->Pending();
    const ESFGraphicsPreset Preset = EditSession->PendingPreset();
    if (StatusText.IsValid()) { StatusText->SetText(FText::FromString(FString::Printf(TEXT("Preset: %s"), USFGraphicsPresetSettings::PresetName(Preset)))); }
    for (int32 Index = 0; Index < 5; ++Index)
    {
        if (PresetButtons[Index].IsValid()) { ApplyButtonStyle(PresetButtons[Index].Get(), EButtonKind::Secondary, static_cast<int32>(Preset) == Index); }
    }
    for (const FStepper& Entry : Steppers) { if (Entry.Value.IsValid()) { Entry.Value->SetText(FText::FromString(Entry.Text())); } }
    for (const FToggle& Entry : Toggles)
    {
        const bool bOn = Entry.Get();
        if (Entry.On.IsValid()) { ApplyButtonStyle(Entry.On.Get(), EButtonKind::Secondary, bOn); }
        if (Entry.Off.IsValid()) { ApplyButtonStyle(Entry.Off.Get(), bOn ? EButtonKind::Secondary : EButtonKind::Destructive, false); }
        if (Entry.OnTick.IsValid()) { Entry.OnTick->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
        if (Entry.OffCross.IsValid()) { Entry.OffCross->SetVisibility(bOn ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    }
    if (ScaleSlider.IsValid()) { bRefreshingSlider = true; ScaleSlider->SetValue(static_cast<float>(Pending.RenderScale)); if (ScaleFillBar.IsValid()) { ScaleFillBar->SetPercent((Pending.RenderScale - 67) / 33.f); } bRefreshingSlider = false; }
    if (ScaleText.IsValid()) { ScaleText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Pending.RenderScale))); }
    int32 All = -1;
    if (Pending.ViewDistance == Pending.Shadows && Pending.Shadows == Pending.GlobalIllumination && Pending.GlobalIllumination == Pending.Reflections && Pending.Reflections == Pending.Textures
        && Pending.Textures == Pending.Effects && Pending.Effects == Pending.Foliage && Pending.Foliage == Pending.PostProcessing && Pending.PostProcessing == Pending.AntiAliasing
        && Pending.AntiAliasing == Pending.NaniteTessellation) { All = Pending.ViewDistance; }
    for (int32 Index = 0; Index < 5; ++Index)
    {
        if (LevelUnderlines[Index].IsValid()) { LevelUnderlines[Index]->SetVisibility(Index == All ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden); }
    }
    // Concept: with nothing to apply, Apply keeps its moss plate and its label dims; clicking it then does nothing (Apply() is a no-op when clean).
    if (ApplyLabel.IsValid()) { ApplyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(Ivory().R, Ivory().G, Ivory().B, EditSession->IsDirty() ? 1.f : 0.45f))); }
    if (KeepLayer.IsValid()) { KeepLayer->SetVisibility(EditSession->IsAwaitingKeep() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
    if (KeepDetailText.IsValid()) { KeepDetailText->SetText(FText::FromString(FString::Printf(TEXT("Resolution %d x %d, %s"), Pending.Resolution.X, Pending.Resolution.Y, *WindowModeName(Pending.WindowMode)))); }
}

void USFGraphicsMenu::StagePreset(ESFGraphicsPreset Preset) { EditSession->SelectPreset(Preset); Refresh(); }

void USFGraphicsMenu::StageCustom() { EditSession->SelectPreset(ESFGraphicsPreset::High); EditSession->EditPending().Shadows = 4; Refresh(); }

void USFGraphicsMenu::StageKeepPrompt()
{
    bSuppressEngineApply = true;
    EditSession->SelectPreset(ESFGraphicsPreset::High);
    EditSession->EditPending().Resolution = FIntPoint(2560, 1440);
    EditSession->EditPending().WindowMode = EWindowMode::Fullscreen;
    EditSession->Apply();
    Refresh();
}
