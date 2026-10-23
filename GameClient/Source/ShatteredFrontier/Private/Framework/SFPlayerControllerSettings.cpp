// Settings > Graphics on the player controller (F10, SFUi settings). Local presentation only; nothing here is sent to the server.
#include "Framework/SFPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"
#include "Framework/SFGameUserSettings.h"
#include "Framework/SFGraphicsMenu.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/SFGameMenu.h"
#include "UI/SFWindowStack.h"

void ASFPlayerController::ToggleSettingsPanel()
{
    if (!IsLocalController()) { return; }
    if (!SettingsPanel)
    {
        const TSubclassOf<USFGraphicsMenu> SettingsClass = LoadClass<USFGraphicsMenu>(nullptr, USFGraphicsMenu::AssetPath);
        if (!SettingsClass) { UE_LOG(LogTemp, Error, TEXT("SETTINGS_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFGraphicsMenu::AssetPath); return; }
        SettingsPanel = CreateWidget<USFGraphicsMenu>(this, SettingsClass);
        if (!SettingsPanel) { return; }
        SettingsPanel->AddToViewport(40);
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (SettingsPanel->GetVisibility() != ESlateVisibility::Collapsed) { SettingsPanel->HandleEscape(); return; }
    CloseSidePanelsForModal();
    SettingsPanel->Reload();
    GetWindowStack()->Open(SettingsPanel);
}

bool ASFPlayerController::CloseSettingsPanel()
{
    return SettingsPanel && SettingsPanel->GetVisibility() != ESlateVisibility::Collapsed && SettingsPanel->HandleEscape();
}

void ASFPlayerController::SFGfx(const FString& Args)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TArray<FString> Words; Args.ParseIntoArray(Words, TEXT(" "));
    if (Words.IsEmpty()) { return; }
    ESFGraphicsPreset Preset = ESFGraphicsPreset::High;
    if (Words.Num() > 1) { for (const ESFGraphicsPreset Candidate : {ESFGraphicsPreset::Low, ESFGraphicsPreset::Medium, ESFGraphicsPreset::High, ESFGraphicsPreset::Epic, ESFGraphicsPreset::Ultra}) { if (Words[1].Equals(USFGraphicsPresetSettings::PresetName(Candidate), ESearchCase::IgnoreCase)) { Preset = Candidate; } } }
    if (Words[0].Equals(TEXT("apply"), ESearchCase::IgnoreCase))
    {
        if (USFGameUserSettings* Settings = Cast<USFGameUserSettings>(GEngine->GetGameUserSettings()))
        {
            FSFGraphicsOptions Options = GetDefault<USFGraphicsPresetSettings>()->WithPreset(Settings->CaptureGraphics(), Preset);
            Options.Resolution = FIntPoint(1920, 1080); Options.WindowMode = EWindowMode::Windowed;  // captures are always 1920x1080
            Settings->ApplyGraphics(Options);
            UE_LOG(LogTemp, Display, TEXT("SFGFX applied %s"), USFGraphicsPresetSettings::PresetName(Preset));
        }
        return;
    }
    if (!SettingsPanel || SettingsPanel->GetVisibility() == ESlateVisibility::Collapsed) { ToggleSettingsPanel(); }
    if (!SettingsPanel) { return; }
    if (Words[0].Equals(TEXT("menu"), ESearchCase::IgnoreCase)) { SettingsPanel->StagePreset(Preset); }
    else if (Words[0].Equals(TEXT("custom"), ESearchCase::IgnoreCase)) { SettingsPanel->StageCustom(); }
    else if (Words[0].Equals(TEXT("keep"), ESearchCase::IgnoreCase)) { SettingsPanel->StageKeepPrompt(); }
    else if (Words[0].Equals(TEXT("close"), ESearchCase::IgnoreCase)) { SettingsPanel->HandleEscape(); }
#endif
}

bool ASFPlayerController::IsGameMenuOpen() const { return GameMenu && GameMenu->GetVisibility() != ESlateVisibility::Collapsed; }

// The Esc game menu, a window of the stack: Esc closes it again through CloseTop.
void ASFPlayerController::ToggleGameMenu(bool bOpen)
{
    if (!IsLocalController()) { return; }
    if (!GameMenu)
    {
        const TSubclassOf<USFGameMenu> MenuClass = LoadClass<USFGameMenu>(nullptr, USFGameMenu::AssetPath);
        if (!MenuClass) { UE_LOG(LogTemp, Error, TEXT("GAME_MENU_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFGameMenu::AssetPath); return; }
        GameMenu = CreateWidget<USFGameMenu>(this, MenuClass);
        if (!GameMenu) { return; }
        GameMenu->OnChoice.BindUObject(this, &ASFPlayerController::HandleGameMenuChoice);
        GameMenu->AddToViewport(40);
        GameMenu->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (bOpen == IsGameMenuOpen()) { return; }
    if (bOpen) { GetWindowStack()->Open(GameMenu); } else { GetWindowStack()->Close(GameMenu); }
}

void ASFPlayerController::HandleGameMenuChoice(ESFGameMenuAction Action)
{
    ToggleGameMenu(false);
    switch (Action)
    {
    case ESFGameMenuAction::Settings: ToggleSettingsPanel(); break;
    case ESFGameMenuAction::LogOut:
        // ponytail: no session exit path exists yet; log it until the account flow can take the player back to character select.
        UE_LOG(LogTemp, Display, TEXT("GAME_MENU_LOGOUT requested"));
        break;
    case ESFGameMenuAction::Quit:
        UE_LOG(LogTemp, Display, TEXT("GAME_MENU_QUIT requested"));
        UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
        break;
    default: break;
    }
}
