// Settings > Graphics on the player controller (F10, SFUi settings). Local presentation only; nothing here is sent to the server.
#include "Framework/SFPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Engine.h"
#include "Framework/SFGameUserSettings.h"
#include "Framework/SFGraphicsMenu.h"

void ASFPlayerController::ToggleSettingsPanel()
{
    if (!IsLocalController()) { return; }
    if (!SettingsPanel)
    {
        SettingsPanel = CreateWidget<USFGraphicsMenu>(this, USFGraphicsMenu::StaticClass());
        if (!SettingsPanel) { return; }
        SettingsPanel->AddToViewport(40);
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (SettingsPanel->GetVisibility() != ESlateVisibility::Collapsed) { SettingsPanel->HandleEscape(); return; }
    CloseSidePanelsForModal();
    SettingsPanel->Reload();
    int32 Width = 0, Height = 0; GetViewportSize(Width, Height);
    const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    SettingsPanel->SetDesiredSizeInViewport(FVector2D(Width / Scale, Height / Scale));
    SettingsPanel->SetPositionInViewport(FVector2D::ZeroVector, false);
    SettingsPanel->SetVisibility(ESlateVisibility::Visible);
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
