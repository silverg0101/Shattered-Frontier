#pragma once

#include "UI/SFUiWidgets.h"
#include "SFGameMenu.generated.h"

enum class ESFGameMenuAction : uint8 { Resume, Settings, LogOut, Quit };

// The Esc game menu (WBP_GameMenu, docs/blueprints/menus.md section 0): a small WBP_WindowFrame (emblem icon_window_settings, title "Menu") with
// the buttons Resume, Settings, Log Out and Quit. Esc opens it when nothing else uses Esc (ASFPlayerController::HandleEscape); it is a window of
// USFWindowStack, so Esc closes it again. C++ only reports which button was pressed; the controller acts on it.
// Binds, all optional: Window, ResumeButton, SettingsButton, LogOutButton, QuitButton (WBP_Button).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFGameMenu : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Menu/WBP_GameMenu.WBP_GameMenu_C");

    DECLARE_DELEGATE_OneParam(FChoiceDelegate, ESFGameMenuAction);
    FChoiceDelegate OnChoice;
    // The same request a button click makes (tests and the dev command).
    void Choose(ESFGameMenuAction Action) { OnChoice.ExecuteIfBound(Action); }
    USFButtonWidget* GetButton(ESFGameMenuAction Action) const;

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFWindowFrameWidget> Window;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> ResumeButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> SettingsButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> LogOutButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> QuitButton;

public:
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

private:
    UFUNCTION() void HandleButton(USFButtonWidget* Source);
};
