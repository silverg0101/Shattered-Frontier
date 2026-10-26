#include "Framework/SFGraphicsMenu.h"

#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Framework/SFGameUserSettings.h"
#include "Framework/SFPlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/SFWindowStack.h"

namespace
{
const TCHAR* const LevelNames[5] = {TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Ultra")};
constexpr TCHAR CommonDir[] = TEXT("/Game/Frontier/UI/Widgets/Common");

template <class T> TSubclassOf<T> CommonClass(const TCHAR* Wbp)
{
    return LoadClass<T>(nullptr, *FString::Printf(TEXT("%s/%s.%s_C"), CommonDir, Wbp, Wbp));
}
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

USFGraphicsClick* USFGraphicsMenu::Handler()
{
    USFGraphicsClick* Handler = NewObject<USFGraphicsClick>(this);
    Clicks.Add(Handler);
    return Handler;
}

USFSettingRowWidget* USFGraphicsMenu::AddRow(const TCHAR* Label)
{
    USFSettingRowWidget* Row = SettingRowClass ? CreateWidget<USFSettingRowWidget>(this, SettingRowClass) : nullptr;
    if (!Row) { return nullptr; }
    (RowTarget ? RowTarget : Rows.Get())->AddChild(Row);
    if (RowTarget == GameplayRows && GameplayRows)
    { (bAddingControlRows ? ControlRows : InterfaceRows).Add(Row); }
    if (Label) { Row->SetLabel(FText::FromString(Label)); }
    if (Label && FString(Label).StartsWith(TEXT("Mouse ")))
    { Row->SetToolTipText(FText::FromString(TEXT("Higher values turn the camera farther for the same mouse movement. 100% is the former response; default 200%. Range 25–800%. Changes take effect when you Apply."))); }
    ++RowCount;
    return Row;
}

void USFGraphicsMenu::AddDropdown(const TCHAR* Label, const TArray<FString>& Options, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
    USFSettingRowWidget* Row = AddRow(Label);
    USFDropdownWidget* Dropdown = Row && DropdownClass ? CreateWidget<USFDropdownWidget>(this, DropdownClass) : nullptr;
    if (!Dropdown) { return; }
    Row->SetControl(Dropdown);
    Dropdown->SetOptions(Options, Get());
    TWeakObjectPtr<USFGraphicsMenu> Self(this);
    USFGraphicsClick* Click = Handler();
    Click->IntFn = [Self, Set](int32 Index)
    {
        USFGraphicsMenu* Menu = Self.Get();
        if (!Menu || Menu->bRefreshing || Index == INDEX_NONE) { return; }
        Set(Index); Menu->Refresh();
    };
    Dropdown->OnSelectionChanged.AddDynamic(Click, &USFGraphicsClick::FireInt);
    Dropdowns.Add({Dropdown, MoveTemp(Get)});
}

void USFGraphicsMenu::AddStepper(const TCHAR* Label, int32 Min, int32 Max, int32 Step, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
    USFSettingRowWidget* Row = AddRow(Label);
    USFStepperWidget* Stepper = Row && StepperClass ? CreateWidget<USFStepperWidget>(this, StepperClass) : nullptr;
    if (!Stepper) { return; }
    Row->SetControl(Stepper);
    Stepper->Step = Step;
    Stepper->SetRange(Min, Max);
    Stepper->SetValue(Get());
    TWeakObjectPtr<USFGraphicsMenu> Self(this);
    USFGraphicsClick* Click = Handler();
    Click->IntFn = [Self, Set](int32 Value)
    {
        USFGraphicsMenu* Menu = Self.Get();
        if (!Menu || Menu->bRefreshing) { return; }
        Set(Value); Menu->Refresh();
    };
    Stepper->OnValueChanged.AddDynamic(Click, &USFGraphicsClick::FireInt);
    Steppers.Add({Stepper, MoveTemp(Get)});
}

void USFGraphicsMenu::AddToggle(const TCHAR* Label, TFunction<bool()> Get, TFunction<void(bool)> Set)
{
    USFSettingRowWidget* Row = AddRow(Label);
    USFToggleWidget* Toggle = Row && ToggleClass ? CreateWidget<USFToggleWidget>(this, ToggleClass) : nullptr;
    if (!Toggle) { return; }
    Row->SetControl(Toggle);
    Toggle->SetChecked(Get());
    TWeakObjectPtr<USFGraphicsMenu> Self(this);
    USFGraphicsClick* Click = Handler();
    Click->BoolFn = [Self, Set](bool bOn)
    {
        USFGraphicsMenu* Menu = Self.Get();
        if (!Menu || Menu->bRefreshing) { return; }
        Set(bOn); Menu->Refresh();
    };
    Toggle->OnToggled.AddDynamic(Click, &USFGraphicsClick::FireBool);
    Toggles.Add({Toggle, MoveTemp(Get)});
}

void USFGraphicsMenu::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);

    const USFGraphicsPresetSettings* Presets = GetDefault<USFGraphicsPresetSettings>();
    const USFGameUserSettings* Live = LiveSettings();
    AppliedControls = PendingControls = Live ? Live->CaptureControls() : FSFPlayerControlOptions();
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

    SettingRowClass = CommonClass<USFSettingRowWidget>(TEXT("WBP_SettingRow"));
    DropdownClass = CommonClass<USFDropdownWidget>(TEXT("WBP_Dropdown"));
    StepperClass = CommonClass<USFStepperWidget>(TEXT("WBP_Stepper"));
    ToggleClass = CommonClass<USFToggleWidget>(TEXT("WBP_Toggle"));

    if (TabBar)
    {
        TabBar->SetTabs({FSFTabSpec(FText::FromString(TEXT("Graphics"))), FSFTabSpec(FText::FromString(TEXT("Audio"))),
            FSFTabSpec(FText::FromString(TEXT("Controls"))), FSFTabSpec(FText::FromString(TEXT("Gameplay")))});
        TabBar->OnTabSelected.AddDynamic(this, &USFGraphicsMenu::HandleTab);
    }
    if (ComingSoon) { ComingSoon->SetVisibility(ESlateVisibility::Collapsed); }

    // Preset row (the dropdown and Auto-Detect are the WBP's), then the Display and Quality groups as setting rows.
    if (PresetDropdown)
    {
        PresetDropdown->SetOptions({TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Ultra"), TEXT("Custom")}, 2);
        USFGraphicsClick* Click = Handler();
        Click->IntFn = [Self](int32 Index)
        {
            USFGraphicsMenu* Menu = Self.Get();
            if (!Menu || Menu->bRefreshing || Index == INDEX_NONE) { return; }
            if (Index < 5) { Menu->Session().SelectPreset(static_cast<ESFGraphicsPreset>(Index)); }  // Custom is a result, not a choice
            Menu->Refresh();
        };
        PresetDropdown->OnSelectionChanged.AddDynamic(Click, &USFGraphicsClick::FireInt);
    }
    if (AutoDetectButton) { AutoDetectButton->SetLabel(FText::FromString(TEXT("Auto-Detect"))); USFGraphicsClick* Click = Handler(); Click->Fn = [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoAutoDetect(); } }; AutoDetectButton->OnActivated.AddDynamic(Click, &USFGraphicsClick::FireButton); }
    if (ApplyButton) { ApplyButton->SetLabel(FText::FromString(TEXT("Apply"))); USFGraphicsClick* Click = Handler(); Click->Fn = [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoApply(); } }; ApplyButton->OnActivated.AddDynamic(Click, &USFGraphicsClick::FireButton); }
    if (ResetButton) { ResetButton->SetLabel(FText::FromString(TEXT("Reset"))); USFGraphicsClick* Click = Handler(); Click->Fn = [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoRevert(); } }; ResetButton->OnActivated.AddDynamic(Click, &USFGraphicsClick::FireButton); }
    if (KeepButton) { KeepButton->SetLabel(FText::FromString(TEXT("Keep Changes"))); USFGraphicsClick* Click = Handler(); Click->Fn = [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoKeep(); } }; KeepButton->OnActivated.AddDynamic(Click, &USFGraphicsClick::FireButton); }
    if (UndoButton) { UndoButton->SetLabel(FText::FromString(TEXT("Revert"))); USFGraphicsClick* Click = Handler(); Click->Fn = [Self]() { if (USFGraphicsMenu* Menu = Self.Get()) { Menu->DoUndo(); } }; UndoButton->OnActivated.AddDynamic(Click, &USFGraphicsClick::FireButton); }
    if (KeepLayer) { KeepLayer->SetVisibility(ESlateVisibility::Collapsed); }

    FSFGraphicsEditSession* Edit = EditSession.Get();
    bRefreshing = true;  // building the rows selects their options; none of that is an edit
    if (USFSettingRowWidget* Heading = AddRow(nullptr)) { Heading->SetHeading(FText::FromString(TEXT("Display"))); }
    TArray<FString> ResolutionNames;
    for (const FIntPoint& Resolution : Resolutions) { ResolutionNames.Add(FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y)); }
    AddDropdown(TEXT("Resolution"), ResolutionNames, [this, Edit]() { return FMath::Max(0, Resolutions.IndexOfByKey(Edit->Pending().Resolution)); },
        [this, Edit](int32 Index) { if (Resolutions.IsValidIndex(Index)) { Edit->EditPending().Resolution = Resolutions[Index]; } });
    static const EWindowMode::Type Modes[3] = {EWindowMode::Fullscreen, EWindowMode::WindowedFullscreen, EWindowMode::Windowed};
    AddDropdown(TEXT("Window Mode"), {WindowModeName(Modes[0]), WindowModeName(Modes[1]), WindowModeName(Modes[2])},
        [Edit]() { for (int32 I = 0; I < 3; ++I) { if (Modes[I] == Edit->Pending().WindowMode) { return I; } } return 0; },
        [Edit](int32 Index) { if (Index >= 0 && Index < 3) { Edit->EditPending().WindowMode = Modes[Index]; } });
    TArray<FString> RateNames;
    for (const int32 Rate : FrameRates) { RateNames.Add(FrameRateName(Rate)); }
    AddDropdown(TEXT("Frame Rate Limit"), RateNames, [this, Edit]() { return FMath::Max(0, FrameRates.IndexOfByKey(Edit->Pending().FrameRateLimit)); },
        [this, Edit](int32 Index) { if (FrameRates.IsValidIndex(Index)) { Edit->EditPending().FrameRateLimit = FrameRates[Index]; } });
    AddToggle(TEXT("V-Sync"), [Edit]() { return Edit->Pending().bVSync; }, [Edit](bool bOn) { Edit->EditPending().bVSync = bOn; });
    AddStepper(TEXT("Render Scale (%)"), 67, 100, 3, [Edit]() { return Edit->Pending().RenderScale; }, [Edit](int32 Value) { Edit->EditPending().RenderScale = Value; });
    AddToggle(TEXT("Motion Blur"), [Edit]() { return Edit->Pending().bMotionBlur; }, [Edit](bool bOn) { Edit->EditPending().bMotionBlur = bOn; });

    if (USFSettingRowWidget* Heading = AddRow(nullptr)) { Heading->SetHeading(FText::FromString(TEXT("Quality"))); }
    struct FLevelRow { const TCHAR* Label; int32 FSFGraphicsOptions::* Field; bool bTessellation; };
    static const FLevelRow LevelRows[10] = {
        {TEXT("View Distance"), &FSFGraphicsOptions::ViewDistance, false}, {TEXT("Shadows"), &FSFGraphicsOptions::Shadows, false},
        {TEXT("Global Illumination"), &FSFGraphicsOptions::GlobalIllumination, false}, {TEXT("Reflections"), &FSFGraphicsOptions::Reflections, false},
        {TEXT("Textures"), &FSFGraphicsOptions::Textures, false}, {TEXT("Effects"), &FSFGraphicsOptions::Effects, false},
        {TEXT("Foliage"), &FSFGraphicsOptions::Foliage, false}, {TEXT("Post Processing"), &FSFGraphicsOptions::PostProcessing, false},
        {TEXT("Anti-Aliasing"), &FSFGraphicsOptions::AntiAliasing, false}, {TEXT("Nanite Tessellation"), &FSFGraphicsOptions::NaniteTessellation, true}};
    for (const FLevelRow& LevelRow : LevelRows)
    {
        TArray<FString> Names;
        for (int32 Level = 0; Level <= FSFGraphicsOptions::MaxLevel; ++Level) { Names.Add(LevelName(Level, LevelRow.bTessellation)); }
        int32 FSFGraphicsOptions::* const Field = LevelRow.Field;
        AddDropdown(LevelRow.Label, Names, [Edit, Field]() { return Edit->Pending().*Field; },
            [Edit, Field](int32 Index) { Edit->EditPending().*Field = FMath::Clamp(Index, 0, FSFGraphicsOptions::MaxLevel); });
    }
    if (GameplayRows)
    {
        RowTarget = GameplayRows;
        if (USFSettingRowWidget* Heading = AddRow(nullptr)) { Heading->SetHeading(FText::FromString(TEXT("Interface"))); }
        AddStepper(TEXT("Hotbar Slots"), USFGameUserSettings::MinHotbarSlots, USFGameUserSettings::MaxHotbarSlots, 1,
            [Self]() { const USFGraphicsMenu* Menu = Self.Get(); const USFGameUserSettings* Live = Menu ? Menu->LiveSettings() : nullptr; return Live ? Live->HotbarSlots : 6; },
            [Self](int32 Value)
            {
                USFGraphicsMenu* Menu = Self.Get();
                if (USFGameUserSettings* Live = Menu ? Menu->LiveSettings() : nullptr)
                { Live->HotbarSlots = FMath::Clamp(Value, USFGameUserSettings::MinHotbarSlots, USFGameUserSettings::MaxHotbarSlots); Live->SaveSettings(); }
            });
        // Share the WBP's existing canonical row host; tab visibility selects its Controls or Gameplay rows.
        bAddingControlRows = true;
        if (USFSettingRowWidget* Heading = AddRow(nullptr)) { Heading->SetHeading(FText::FromString(TEXT("Mouse and camera"))); }
        AddStepper(TEXT("Mouse horizontal (%)"), FSFPlayerControlOptions::MinMousePercent, FSFPlayerControlOptions::MaxMousePercent, 25,
            [this]() { return PendingControls.MouseHorizontalPercent; }, [this](int32 V) { PendingControls.MouseHorizontalPercent = V; });
        AddStepper(TEXT("Mouse vertical (%)"), FSFPlayerControlOptions::MinMousePercent, FSFPlayerControlOptions::MaxMousePercent, 25,
            [this]() { return PendingControls.MouseVerticalPercent; }, [this](int32 V) { PendingControls.MouseVerticalPercent = V; });
        AddToggle(TEXT("Invert vertical mouse"), [this]() { return PendingControls.bInvertMouseY; }, [this](bool V) { PendingControls.bInvertMouseY = V; });
        AddToggle(TEXT("Combat camera assist"), [this]() { return PendingControls.bCombatFraming; }, [this](bool V) { PendingControls.bCombatFraming = V; });
        AddToggle(TEXT("Combat depth of field"), [this]() { return PendingControls.bCombatDepthOfField; }, [this](bool V) { PendingControls.bCombatDepthOfField = V; });
        AddDropdown(TEXT("Camera shoulder"), {TEXT("Right"), TEXT("Left")}, [this]() { return int32(PendingControls.Shoulder); },
            [this](int32 V) { PendingControls.Shoulder = V == 1 ? ESFCombatShoulderSide::Left : ESFCombatShoulderSide::Right; });
        bAddingControlRows = false;
        RowTarget = nullptr;
        GameplayRows->SetVisibility(ESlateVisibility::Collapsed);
    }
    bRefreshing = false;
    Refresh();
}

void USFGraphicsMenu::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFGraphicsMenu::Reload()
{
    if (const USFGameUserSettings* Live = LiveSettings())
    { EditSession->Reset(Live->CaptureGraphics()); AppliedControls = PendingControls = Live->CaptureControls(); }
    Refresh();
}

bool USFGraphicsMenu::IsKeepPromptOpen() const { return EditSession && EditSession->IsAwaitingKeep(); }

void USFGraphicsMenu::CloseMenu()
{
    if (ASFPlayerController* PC = Cast<ASFPlayerController>(GetOwningPlayer())) { PC->GetWindowStack()->Close(this); }
    else { SetVisibility(ESlateVisibility::Collapsed); }
    OnClosed.Broadcast();
}

bool USFGraphicsMenu::HandleEscape()
{
    if (IsKeepPromptOpen()) { DoUndo(); return true; }
    if (GetVisibility() == ESlateVisibility::Collapsed) { return false; }
    CloseMenu();
    return true;
}

void USFGraphicsMenu::HandleTab(int32 Index)
{
    ActiveTab = Index;
    const bool bGraphics = Index == 0, bGameplay = Index == 3 && GameplayRows, bControls = Index == 2 && GameplayRows;
    if (GraphicsPage) { GraphicsPage->SetVisibility(bGraphics ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    if (GameplayRows) { GameplayRows->SetVisibility(bGameplay || bControls ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    for (const auto& Row : ControlRows) if (Row.IsValid()) Row->SetVisibility(bControls ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    for (const auto& Row : InterfaceRows) if (Row.IsValid()) Row->SetVisibility(bGameplay ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (ComingSoon)
    {
        static const TCHAR* const Names[4] = {TEXT("Graphics"), TEXT("Audio"), TEXT("Controls"), TEXT("Gameplay")};
        ComingSoon->SetText(FText::FromString(FString::Printf(TEXT("%s settings are coming soon."), Names[FMath::Clamp(Index, 0, 3)])));
        ComingSoon->SetVisibility(bGraphics || bGameplay || bControls ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    if (ApplyButton) { ApplyButton->SetVisibility(bGraphics || bControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
    if (ResetButton) { ResetButton->SetVisibility(bGraphics || bControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
    Refresh();
}

void USFGraphicsMenu::DoApply()
{
    if (ActiveTab == 2)
    { if (auto* Live = LiveSettings()) { Live->ApplyControls(PendingControls); AppliedControls = PendingControls = Live->CaptureControls(); } }
    else { EditSession->Apply(); }
    Refresh();
}
void USFGraphicsMenu::DoRevert()
{
    if (ActiveTab == 2) { PendingControls = FSFPlayerControlOptions(); }
    else { EditSession->RevertPending(); }
    Refresh();
}
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
    if (KeepCountdown) { KeepCountdown->SetText(FText::FromString(FString::Printf(TEXT("Reverting in %d seconds"), FMath::CeilToInt(Left)))); }
    if (KeepBar) { KeepBar->SetPercent(Left / FMath::Max(1.f, GetDefault<USFGraphicsPresetSettings>()->KeepSecondsClamped())); }
}

void USFGraphicsMenu::Refresh()
{
    const FSFGraphicsOptions& Pending = EditSession->Pending();
    TGuardValue<bool> Guard(bRefreshing, true);  // pushing values into the controls must not read back as edits
    if (PresetDropdown) { PresetDropdown->SetSelectedIndex(static_cast<int32>(EditSession->PendingPreset())); }
    for (const FDropdownEntry& Entry : Dropdowns) { if (Entry.Widget.IsValid()) { Entry.Widget->SetSelectedIndex(Entry.Get()); } }
    for (const FStepperEntry& Entry : Steppers) { if (Entry.Widget.IsValid()) { Entry.Widget->SetValue(Entry.Get()); } }
    for (const FToggleEntry& Entry : Toggles) { if (Entry.Widget.IsValid()) { Entry.Widget->SetChecked(Entry.Get()); } }
    // With nothing to apply, Apply is disabled (Apply() is a no-op when clean anyway).
    if (ApplyButton) { ApplyButton->SetButtonEnabled(ActiveTab == 2 ? PendingControls != AppliedControls : EditSession->IsDirty()); }
    if (KeepLayer) { KeepLayer->SetVisibility(EditSession->IsAwaitingKeep() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
    if (KeepDetailText) { KeepDetailText->SetText(FText::FromString(FString::Printf(TEXT("Resolution %d x %d, %s"), Pending.Resolution.X, Pending.Resolution.Y, *WindowModeName(Pending.WindowMode)))); }
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
