#include "UI/SFGameMenu.h"

USFButtonWidget* USFGameMenu::GetButton(ESFGameMenuAction Action) const
{
    switch (Action)
    {
    case ESFGameMenuAction::Resume: return ResumeButton;
    case ESFGameMenuAction::Settings: return SettingsButton;
    case ESFGameMenuAction::LogOut: return LogOutButton;
    default: return QuitButton;
    }
}

void USFGameMenu::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    struct FButtonLabel { USFButtonWidget* Button; const TCHAR* Label; };
    for (const FButtonLabel& Entry : {FButtonLabel{ResumeButton, TEXT("Resume")}, FButtonLabel{SettingsButton, TEXT("Settings")},
        FButtonLabel{LogOutButton, TEXT("Log Out")}, FButtonLabel{QuitButton, TEXT("Quit")}})
    {
        if (!Entry.Button) { continue; }
        Entry.Button->SetLabel(FText::FromString(Entry.Label));
        Entry.Button->OnActivated.AddDynamic(this, &USFGameMenu::HandleButton);
    }
}

void USFGameMenu::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFGameMenu::HandleButton(USFButtonWidget* Source)
{
    for (const ESFGameMenuAction Action : {ESFGameMenuAction::Resume, ESFGameMenuAction::Settings, ESFGameMenuAction::LogOut, ESFGameMenuAction::Quit})
    {
        if (GetButton(Action) == Source) { Choose(Action); return; }
    }
}
