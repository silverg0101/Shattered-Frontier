#pragma once

#include "Framework/SFGraphicsPresets.h"
#include "UI/SFUiWidgets.h"
#include "SFGraphicsMenu.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;
class USFGameUserSettings;

// Runs a stored closure from a widget delegate (a button, a dropdown, a stepper or a toggle).
UCLASS()
class USFGraphicsClick : public UObject
{
    GENERATED_BODY()
public:
    TFunction<void()> Fn;
    TFunction<void(int32)> IntFn;
    TFunction<void(bool)> BoolFn;
    UFUNCTION() void Fire() { if (Fn) { Fn(); } }
    UFUNCTION() void FireButton(USFButtonWidget* Source) { if (Fn) { Fn(); } }
    UFUNCTION() void FireInt(int32 Value) { if (IntFn) { IntFn(Value); } }
    UFUNCTION() void FireBool(bool bValue) { if (BoolFn) { BoolFn(bValue); } }
};

// Settings window (WBP_Settings, docs/blueprints/menus.md section 0; concepts 14b / 14c): a WBP_WindowFrame (emblem icon_window_settings) with the
// Graphics / Audio / Controls / Gameplay tabs (only Graphics has settings; the others say "Coming soon"), the preset dropdown with Auto-Detect, a
// WBP_ScrollList of WBP_SettingRow rows (label + WBP_Dropdown, WBP_Stepper or WBP_Toggle) and the Apply / Reset footer. Edits are pending until Apply; a
// resolution or window mode change then asks to keep within USFGraphicsPresetSettings::KeepSeconds or reverts itself (the keep prompt is a framed
// card over the window). A window of USFWindowStack; Esc closes the keep prompt as Revert, else the window.
// Binds (required): Rows (VerticalBox in a WBP_ScrollList; C++ adds the WBP_SettingRow rows).
// Optional: Window, TabBar (WBP_TabBar), GraphicsPage (everything of the Graphics tab), ComingSoon (TextBlock), PresetDropdown (WBP_Dropdown),
// AutoDetectButton, ApplyButton, ResetButton (WBP_Button), KeepLayer (the keep prompt), KeepDetailText, KeepCountdown (TextBlocks), KeepBar (ProgressBar),
// KeepButton, UndoButton (WBP_Button).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFGraphicsMenu : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Settings/WBP_Settings.WBP_Settings_C");

    // Re-reads the live settings and drops unapplied edits; call each time the menu opens.
    void Reload();
    bool IsKeepPromptOpen() const;
    // Esc: closes the keep prompt as Revert, else hides the window. Returns true when anything closed.
    bool HandleEscape();
    // Raised when the window closes itself (Esc, or the keep prompt resolving into a close); the front end, which has no ASFPlayerController window stack, listens.
    FSimpleMulticastDelegate OnClosed;
    FSFGraphicsEditSession& Session() { return *EditSession; }
    // Development captures (SFGfx): select a preset, make one option Custom, or raise the keep prompt without touching the real display.
    void StagePreset(ESFGraphicsPreset Preset);
    void StageCustom();
    void StageKeepPrompt();
    int32 SettingRowCount() const { return RowCount; }

    static FString LevelName(int32 Level, bool bTessellation = false);
    static FString WindowModeName(EWindowMode::Type Mode);
    static FString FrameRateName(int32 Fps);

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> Rows;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFWindowFrameWidget> Window;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabBarWidget> TabBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> GraphicsPage;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ComingSoon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFDropdownWidget> PresetDropdown;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> AutoDetectButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> ApplyButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> ResetButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> KeepLayer;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KeepDetailText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KeepCountdown;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> KeepBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> KeepButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> UndoButton;

private:
    struct FDropdownEntry { TWeakObjectPtr<USFDropdownWidget> Widget; TFunction<int32()> Get; };
    struct FStepperEntry { TWeakObjectPtr<USFStepperWidget> Widget; TFunction<int32()> Get; };
    struct FToggleEntry { TWeakObjectPtr<USFToggleWidget> Widget; TFunction<bool()> Get; };

    USFGameUserSettings* LiveSettings() const;
    USFGraphicsClick* Handler();
    USFSettingRowWidget* AddRow(const TCHAR* Label);
    void AddDropdown(const TCHAR* Label, const TArray<FString>& Options, TFunction<int32()> Get, TFunction<void(int32)> Set);
    void AddStepper(const TCHAR* Label, int32 Min, int32 Max, int32 Step, TFunction<int32()> Get, TFunction<void(int32)> Set);
    void AddToggle(const TCHAR* Label, TFunction<bool()> Get, TFunction<void(bool)> Set);
    void Refresh();
    void DoApply();
    void DoRevert();
    void DoAutoDetect();
    void DoKeep();
    void DoUndo();
    void CloseMenu();
    UFUNCTION() void HandleTab(int32 Index);

    TUniquePtr<FSFGraphicsEditSession> EditSession;
    UPROPERTY(Transient) TArray<TObjectPtr<USFGraphicsClick>> Clicks;
    TArray<FIntPoint> Resolutions;
    TArray<int32> FrameRates;
    TArray<FDropdownEntry> Dropdowns;
    TArray<FStepperEntry> Steppers;
    TArray<FToggleEntry> Toggles;
    TSubclassOf<USFSettingRowWidget> SettingRowClass;
    TSubclassOf<USFDropdownWidget> DropdownClass;
    TSubclassOf<USFStepperWidget> StepperClass;
    TSubclassOf<USFToggleWidget> ToggleClass;
    int32 RowCount = 0;
    bool bRefreshing = false;
    bool bSuppressEngineApply = false;
};
