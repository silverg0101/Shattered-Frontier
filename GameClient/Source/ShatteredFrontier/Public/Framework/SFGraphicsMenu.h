#pragma once

#include "Blueprint/UserWidget.h"
#include "Framework/SFGraphicsPresets.h"
#include "SFGraphicsMenu.generated.h"

class UBorder;
class UButton;
class UImage;
class UProgressBar;
class USlider;
class UTextBlock;
class UWidget;
class USFGameUserSettings;

// Runs a stored closure from a UButton's dynamic OnClicked.
UCLASS()
class USFGraphicsClick : public UObject
{
    GENERATED_BODY()
public:
    TFunction<void()> Fn;
    UFUNCTION() void Fire() { if (Fn) { Fn(); } }
};

// Settings > Graphics (docs/Game-Design/concept-art/UI/v5/14b-settings-graphics.png, 14c for the keep prompt). Edits are pending until
// Apply; a resolution or window mode change then asks to keep within USFGraphicsPresetSettings::KeepSeconds or reverts itself.
UCLASS()
class SHATTEREDFRONTIER_API USFGraphicsMenu : public UUserWidget
{
    GENERATED_BODY()
public:
    // Re-reads the live settings and drops unapplied edits; call each time the menu opens.
    void Reload();
    bool IsKeepPromptOpen() const;
    // Esc: closes the keep prompt as Revert, else hides the menu. Returns true when anything closed.
    bool HandleEscape();
    FSFGraphicsEditSession& Session() { return *EditSession; }
    // Development captures (SFGfx): select a preset, make one option Custom, or raise the keep prompt without touching the real display.
    void StagePreset(ESFGraphicsPreset Preset);
    void StageCustom();
    void StageKeepPrompt();

    static FString LevelName(int32 Level, bool bTessellation = false);
    static FString WindowModeName(EWindowMode::Type Mode);
    static FString FrameRateName(int32 Fps);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    struct FStepper { TWeakObjectPtr<UTextBlock> Value; TFunction<FString()> Text; };
    struct FToggle { TWeakObjectPtr<UButton> On, Off; TWeakObjectPtr<UWidget> OnTick, OffCross; TFunction<bool()> Get; };

    USFGameUserSettings* LiveSettings() const;
    void Click(UButton* Button, TFunction<void()> Fn);
    void Refresh();
    void DoApply();
    void DoRevert();
    void DoAutoDetect();
    void DoKeep();
    void DoUndo();
    void CloseMenu() { SetVisibility(ESlateVisibility::Collapsed); }
    UFUNCTION() void HandleSlider(float Value);

    TUniquePtr<FSFGraphicsEditSession> EditSession;
    UPROPERTY(Transient) TArray<TObjectPtr<USFGraphicsClick>> Clicks;
    TArray<FIntPoint> Resolutions;
    TArray<int32> FrameRates;
    TArray<FStepper> Steppers;
    TArray<FToggle> Toggles;
    TWeakObjectPtr<UButton> PresetButtons[5];
    TWeakObjectPtr<UButton> LevelButtons[5];
    TWeakObjectPtr<UWidget> LevelUnderlines[5];
    TWeakObjectPtr<UTextBlock> StatusText, ScaleText, KeepTitle, KeepDetailText, KeepCountdown;
    TWeakObjectPtr<UButton> ApplyButton;
    TWeakObjectPtr<USlider> ScaleSlider;
    TWeakObjectPtr<UProgressBar> KeepBar, ScaleFillBar;
    TWeakObjectPtr<UWidget> KeepLayer;
    bool bRefreshingSlider = false;
    bool bSuppressEngineApply = false;
};
