#include "FrontEnd/SFFrontEndWidgets.h"

#include "Components/EditableText.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"

using namespace SFHudStyle;

namespace
{
template <class T> TSubclassOf<T> CommonClass(const TCHAR* Name)
{
    return LoadClass<T>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Widgets/Common/%s.%s_C"), Name, Name));
}
UTexture2D* KitTexture(const TCHAR* Path) { return LoadObject<UTexture2D>(nullptr, Path); }
UTexture2D* PlusSlotArt() { return KitTexture(TEXT("/Game/Frontier/UI/Kit/v7/T_KitV7_plus_slot.T_KitV7_plus_slot")); }
UTexture2D* PortraitPlaceholder() { return KitTexture(TEXT("/Game/Frontier/UI/Kit/T_Kit_icon_stag_emblem.T_Kit_icon_stag_emblem")); }
}

void USFFrontEndPage::Say(UTextBlock* Block, const FText& Text, bool bError)
{
    if (!Block) { return; }
    Block->SetText(Text);
    Block->SetColorAndOpacity(FSlateColor(bError ? Crimson() : Ivory()));  // a state tint, like a have / need count
}

// ---- title

USFButtonWidget* USFFrontEndTitle::GetButton(ESFTitleAction Action) const
{
    switch (Action)
    {
    case ESFTitleAction::Play: return PlayButton;
    case ESFTitleAction::Settings: return SettingsButton;
    case ESFTitleAction::Credits: return CreditsButton;
    default: return QuitButton;
    }
}

void USFFrontEndTitle::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    for (const ESFTitleAction Action : {ESFTitleAction::Play, ESFTitleAction::Settings, ESFTitleAction::Credits, ESFTitleAction::Quit})
    {
        if (USFButtonWidget* Button = GetButton(Action)) { Button->OnActivated.AddUniqueDynamic(this, &USFFrontEndTitle::HandleButton); }
    }
}

void USFFrontEndTitle::HandleButton(USFButtonWidget* Source)
{
    for (const ESFTitleAction Action : {ESFTitleAction::Play, ESFTitleAction::Settings, ESFTitleAction::Credits, ESFTitleAction::Quit})
    {
        if (GetButton(Action) == Source) { Choose(Action); return; }
    }
}

void USFFrontEndTitle::SetRealm(const FSFRealmStatus& Realm)
{
    if (RealmText) { RealmText->SetText(FText::FromString(FString::Printf(TEXT("%s - %s"), *Realm.Name, *Realm.Detail))); }
    if (RealmMark) { RealmMark->SetVisibility(Realm.bOnline ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void USFFrontEndTitle::SetNews(const FString& Headline, const FString& Body)
{
    if (NewsTitle) { NewsTitle->SetText(FText::FromString(Headline)); }
    if (NewsText) { NewsText->SetText(FText::FromString(Body)); }
}

void USFFrontEndTitle::SetVersion(const FString& Version)
{
    if (VersionText) { VersionText->SetText(FText::FromString(Version)); }
}

// ---- login

void USFFrontEndLogin::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (SignInButton) { SignInButton->OnActivated.AddUniqueDynamic(this, &USFFrontEndLogin::HandleSignInButton); }
    if (PasswordField) { PasswordField->OnTextCommitted.AddUniqueDynamic(this, &USFFrontEndLogin::HandleCommitted); }
    if (AccountField) { AccountField->OnTextCommitted.AddUniqueDynamic(this, &USFFrontEndLogin::HandleCommitted); }
    if (StatusText) { StatusText->SetText(FText::GetEmpty()); }
}

FString USFFrontEndLogin::GetAccount() const { return AccountField ? AccountField->GetText().ToString().TrimStartAndEnd() : FString(); }

void USFFrontEndLogin::Submit()
{
    if (bBusy) { return; }
    OnSignIn.ExecuteIfBound(GetAccount(), PasswordField ? PasswordField->GetText().ToString() : FString(), RememberToggle && RememberToggle->IsChecked());
}

void USFFrontEndLogin::SetAccount(const FString& Account, bool bRemember)
{
    if (AccountField) { AccountField->SetText(FText::FromString(Account)); }
    if (RememberToggle) { RememberToggle->SetChecked(bRemember); }
}

void USFFrontEndLogin::ClearPassword() { if (PasswordField) { PasswordField->SetText(FText::GetEmpty()); } }
void USFFrontEndLogin::SetStatus(const FText& Text, bool bError) { Say(StatusText, Text, bError); }

void USFFrontEndLogin::SetBusy(bool bInBusy)
{
    bBusy = bInBusy;
    if (SignInButton) { SignInButton->SetButtonEnabled(!bBusy); }
}

void USFFrontEndLogin::SetPasswordForTest(const FString& Password) { if (PasswordField) { PasswordField->SetText(FText::FromString(Password)); } }
void USFFrontEndLogin::SetAccountForTest(const FString& Account) { if (AccountField) { AccountField->SetText(FText::FromString(Account)); } }

// ---- account

void USFFrontEndAccount::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (SignOutButton) { SignOutButton->OnActivated.AddUniqueDynamic(this, &USFFrontEndAccount::HandleSignOut); }
}

void USFFrontEndAccount::Show(const FString& Account, const FSFRealmStatus& Realm)
{
    if (AccountText) { AccountText->SetText(FText::FromString(Account)); }
    if (RealmText) { RealmText->SetText(FText::FromString(FString::Printf(TEXT("%s - %s"), *Realm.Name, *Realm.Detail))); }
}

// ---- character select

void USFFrontEndSelect::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    RowClass = CommonClass<USFKitRowWidget>(TEXT("WBP_KitRow"));
    for (USFButtonWidget* Button : {CreateButton.Get(), DeleteButton.Get(), EnterButton.Get(), AccountButton.Get()}) { if (Button) { Button->OnActivated.AddUniqueDynamic(this, &USFFrontEndSelect::HandleButton); } }
    if (DeleteButton) { DeleteButton->SetHoldMode(true, DeleteHoldSeconds); }  // hold to confirm
    if (StatusText) { StatusText->SetText(FText::GetEmpty()); }
}

USFKitRowWidget* USFFrontEndSelect::GetRow(int32 Index) const { return Rows ? Cast<USFKitRowWidget>(Rows->GetChildAt(Index)) : nullptr; }

void USFFrontEndSelect::SetRealm(const FSFRealmStatus& Realm)
{
    if (RealmText) { RealmText->SetText(FText::FromString(Realm.Name)); }
    if (RealmMark) { RealmMark->SetVisibility(Realm.bOnline ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (StatusText && StatusText->GetText().IsEmpty()) { /* status is only set by events */ }
}

void USFFrontEndSelect::SetStatus(const FText& Text, bool bError) { Say(StatusText, Text, bError); }

void USFFrontEndSelect::SetCharacters(const TArray<FSFCharacterSummary>& InCharacters, int32 Slots, int32 InSelected)
{
    Characters = InCharacters;
    SlotCount = FMath::Max(Slots, Characters.Num());
    Selected = Characters.IsEmpty() ? INDEX_NONE : FMath::Clamp(InSelected == INDEX_NONE ? 0 : InSelected, 0, Characters.Num() - 1);
    Refresh();
}

void USFFrontEndSelect::Refresh()
{
    if (!Rows || !RowClass) { return; }
    Rows->ClearChildren();
    UTexture2D* Portrait = PortraitPlaceholder();
    for (int32 i = 0; i < Characters.Num(); ++i)
    {
        USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
        if (!Row) { continue; }
        Row->SetEntry(FText::FromString(Characters[i].Name), FText::GetEmpty());
        Row->SetCells(FText::FromString(FString::Printf(TEXT("Level %d"), Characters[i].Level)), FText::FromString(Characters[i].Location));
        Row->SetIconTexture(Portrait);
        Row->SetSelected(i == Selected);
        Row->OnClicked.AddUniqueDynamic(this, &USFFrontEndSelect::HandleRow);
        Rows->AddChild(Row);
    }
    const int32 Empty = FMath::Min(MaxEmptyShown, SlotCount - Characters.Num());
    for (int32 i = 0; i < Empty; ++i)
    {
        USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
        if (!Row) { continue; }
        Row->SetEntry(FText::FromString(TEXT("Empty Slot")), FText::GetEmpty());
        Row->SetCells(FText::GetEmpty(), FText::GetEmpty());
        Row->SetIconTexture(PlusSlotArt());
        Row->SetTint(FLinearColor(0.66f, 0.65f, 0.58f), FLinearColor::White);  // dimmed like the other empty guides
        Row->OnClicked.AddUniqueDynamic(this, &USFFrontEndSelect::HandleRow);
        Rows->AddChild(Row);
    }
    const bool bAny = Selected != INDEX_NONE;
    if (DeleteButton) { DeleteButton->SetButtonEnabled(bAny); }
    if (EnterButton) { EnterButton->SetButtonEnabled(bAny); }
    if (CreateButton) { CreateButton->SetButtonEnabled(Characters.Num() < SlotCount); }
}

void USFFrontEndSelect::HandleRow(USFSelectableWidget* Source)
{
    for (int32 i = 0; i < Rows->GetChildrenCount(); ++i)
    {
        if (Rows->GetChildAt(i) != Source) { continue; }
        if (i >= Characters.Num()) { OnCreate.ExecuteIfBound(); return; }  // an empty slot creates
        Selected = i;
        for (int32 j = 0; j < Characters.Num(); ++j) { if (USFKitRowWidget* Row = GetRow(j)) { Row->SetSelected(j == Selected); } }
        OnSelected.ExecuteIfBound(i);
        return;
    }
}

void USFFrontEndSelect::HandleButton(USFButtonWidget* Source)
{
    if (Source == CreateButton) { OnCreate.ExecuteIfBound(); }
    else if (Source == AccountButton) { OnAccount.ExecuteIfBound(); }
    else if (Source == DeleteButton && Selected != INDEX_NONE) { OnDelete.ExecuteIfBound(Selected); }
    else if (Source == EnterButton && Selected != INDEX_NONE) { OnEnter.ExecuteIfBound(Selected); }
}

// ---- creator

namespace
{
struct FOptionDef { ESFCreatorStep Step; const TCHAR* Label; int32 Min, Max, Stride; int32 FSFAppearance::* Member; };
const FOptionDef OptionDefs[] = {
    {ESFCreatorStep::Body, TEXT("Height (cm)"), FSFAppearance::MinHeight, FSFAppearance::MaxHeight, 2, &FSFAppearance::HeightCm},
    {ESFCreatorStep::Body, TEXT("Build"), 0, 100, 5, &FSFAppearance::Build},
    {ESFCreatorStep::Face, TEXT("Face shape"), 1, FSFAppearance::MaxFace, 1, &FSFAppearance::Face},
    {ESFCreatorStep::Face, TEXT("Eye colour"), 1, FSFAppearance::MaxEyes, 1, &FSFAppearance::Eyes},
    {ESFCreatorStep::Hair, TEXT("Hairstyle"), 1, FSFAppearance::MaxHair, 1, &FSFAppearance::Hair},
    {ESFCreatorStep::Hair, TEXT("Beard"), 0, FSFAppearance::MaxBeard, 1, &FSFAppearance::Beard},
    {ESFCreatorStep::Hair, TEXT("Hair colour"), 1, FSFAppearance::MaxColour, 1, &FSFAppearance::HairColour},
    {ESFCreatorStep::Skin, TEXT("Skin tone"), 1, FSFAppearance::MaxColour, 1, &FSFAppearance::Skin},
    {ESFCreatorStep::Outfit, TEXT("Starting outfit"), 1, FSFAppearance::MaxOutfit, 1, &FSFAppearance::Outfit},
};
const TCHAR* const Heritages[3] = {TEXT("Valefolk"), TEXT("Northkin"), TEXT("Seaborn")};
}

void USFFrontEndCreator::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    RowClass = CommonClass<USFKitRowWidget>(TEXT("WBP_KitRow"));
    TileClass = CommonClass<USFSlotWidget>(TEXT("WBP_ItemTile"));
    SettingRowClass = CommonClass<USFSettingRowWidget>(TEXT("WBP_SettingRow"));
    StepperClass = CommonClass<USFStepperWidget>(TEXT("WBP_Stepper"));
    for (USFButtonWidget* Button : {RandomizeButton.Get(), BackButton.Get(), NextButton.Get()}) { if (Button) { Button->OnActivated.AddUniqueDynamic(this, &USFFrontEndCreator::HandleButton); } }
    if (NameField) { NameField->OnTextChanged.AddUniqueDynamic(this, &USFFrontEndCreator::HandleName); }
    Random.GenerateNewSeed();
    BuildStepList();
    BuildTiles();
    Begin();
}

void USFFrontEndCreator::BuildStepList()
{
    if (!StepList || !RowClass) { return; }
    StepList->ClearChildren();
    StepRows.Reset();
    for (int32 i = 0; i < FSFCreatorState::NumSteps; ++i)
    {
        USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
        if (!Row) { continue; }
        Row->SetEntry(FText::FromString(FSFCreatorState::StepName(i)), FText::GetEmpty());
        Row->SetCells(FText::GetEmpty(), FText::GetEmpty());
        Row->SetIconTexture(nullptr);  // no step icons yet: no empty well
        Row->OnClicked.AddUniqueDynamic(this, &USFFrontEndCreator::HandleStepRow);
        StepList->AddChild(Row);
        StepRows.Add(Row);
    }
}

void USFFrontEndCreator::BuildTiles()
{
    if (!GenderTiles || !TileClass) { return; }
    GenderTiles->ClearChildren();
    Tiles.Reset();
    UTexture2D* Placeholder = PortraitPlaceholder();  // no portrait art yet
    for (int32 i = 0; i < 6; ++i)
    {
        USFSlotWidget* Tile = CreateWidget<USFSlotWidget>(this, TileClass);
        if (!Tile) { continue; }
        Tile->SetIcon(Placeholder);
        Tile->SetCaption(FText::FromString(Heritages[i % 3]));
        Tile->OnClicked.AddUniqueDynamic(this, &USFFrontEndCreator::HandleTile);
        if (UUniformGridSlot* Cell = GenderTiles->AddChildToUniformGrid(Tile, i / 3, i % 3)) { Cell->SetHorizontalAlignment(HAlign_Fill); Cell->SetVerticalAlignment(VAlign_Fill); }
        Tiles.Add(Tile);
    }
}

void USFFrontEndCreator::Begin()
{
    State = FSFCreatorState();
    if (NameField) { NameField->SetText(FText::GetEmpty()); }
    ShowStep();
    Changed();
}

void USFFrontEndCreator::GoToStep(int32 Step)
{
    if (State.Jump(Step)) { ShowStep(); Changed(); }
}

void USFFrontEndCreator::Back()
{
    if (State.Prev()) { ShowStep(); Changed(); }
    else { OnLeave.ExecuteIfBound(); }
}

void USFFrontEndCreator::Next()
{
    if (State.Next()) { ShowStep(); Changed(); return; }
    FString Error;
    if (!SFFrontEnd::ValidateCharacterName(State.Name, Error)) { Say(NameStatus, FText::FromString(Error), true); return; }
    OnCreate.ExecuteIfBound(State.Name, State.Look);
}

void USFFrontEndCreator::Randomize()
{
    State.Look.Randomize(Random);
    State.Look.Clamp();
    RebuildOptions();
    Changed();
}

void USFFrontEndCreator::SetStatus(const FText& Text, bool bError) { Say(NameStatus, Text, bError); }

void USFFrontEndCreator::SetNameForTest(const FString& Name)
{
    if (NameField) { NameField->SetText(FText::FromString(Name)); }
    State.Name = Name;
}

void USFFrontEndCreator::ShowStep()
{
    const ESFCreatorStep Step = static_cast<ESFCreatorStep>(State.Step);
    if (PageWindow) { PageWindow->SetTitle(FText::FromString(FSFCreatorState::StepName(State.Step))); }
    if (StepText) { StepText->SetText(FText::FromString(State.Progress())); }
    for (int32 i = 0; i < StepRows.Num(); ++i) { if (StepRows[i]) { StepRows[i]->SetSelected(i == State.Step); } }
    auto Show = [](UWidget* Page, bool bOn) { if (Page) { Page->SetVisibility(bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); } };
    Show(GenderPage, Step == ESFCreatorStep::Gender);
    Show(NamePage, Step == ESFCreatorStep::Name);
    Show(OptionsPage, Step != ESFCreatorStep::Gender && Step != ESFCreatorStep::Name);
    for (int32 i = 0; i < Tiles.Num(); ++i) { if (Tiles[i]) { Tiles[i]->SetSelected(i == State.Look.GenderTile()); } }
    if (NextButton) { NextButton->SetLabel(FText::FromString(State.OnLastStep() ? TEXT("Create") : TEXT("Next"))); }
    if (BackButton) { BackButton->SetLabel(FText::FromString(TEXT("Back"))); }
    if (RandomizeButton) { RandomizeButton->SetLabel(FText::FromString(TEXT("Randomize"))); }
    Say(NameStatus, FText::GetEmpty());
    RebuildOptions();
}

void USFFrontEndCreator::RebuildOptions()
{
    Options.Reset();
    if (!OptionRows) { return; }
    OptionRows->ClearChildren();
    if (!SettingRowClass || !StepperClass) { return; }
    for (const FOptionDef& Def : OptionDefs)
    {
        if (static_cast<int32>(Def.Step) != State.Step) { continue; }
        USFSettingRowWidget* Row = CreateWidget<USFSettingRowWidget>(this, SettingRowClass);
        USFStepperWidget* Stepper = CreateWidget<USFStepperWidget>(this, StepperClass);
        if (!Row || !Stepper) { continue; }
        Stepper->MinValue = Def.Min; Stepper->MaxValue = Def.Max; Stepper->Step = Def.Stride;
        Stepper->SetRange(Def.Min, Def.Max);
        Stepper->SetValue(State.Look.*Def.Member);
        Row->SetLabel(FText::FromString(Def.Label));
        Row->SetControl(Stepper);
        OptionRows->AddChild(Row);
        Options.Add({Stepper, Def.Member});
    }
}

void USFFrontEndCreator::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    bool bChanged = false;
    for (const FOption& Option : Options)  // the steppers are the input; the state follows them
    {
        if (Option.Stepper.IsValid() && State.Look.*Option.Member != Option.Stepper->GetValue()) { State.Look.*Option.Member = Option.Stepper->GetValue(); bChanged = true; }
    }
    if (bChanged) { Changed(); }
}

void USFFrontEndCreator::HandleStepRow(USFSelectableWidget* Source)
{
    for (int32 i = 0; i < StepRows.Num(); ++i) { if (StepRows[i] == Source) { GoToStep(i); return; } }
}

void USFFrontEndCreator::HandleTile(USFSelectableWidget* Source)
{
    for (int32 i = 0; i < Tiles.Num(); ++i)
    {
        if (Tiles[i] != Source) { continue; }
        State.Look.SetFromGenderTile(i);
        for (int32 j = 0; j < Tiles.Num(); ++j) { Tiles[j]->SetSelected(j == i); }
        Changed();
        return;
    }
}

void USFFrontEndCreator::HandleButton(USFButtonWidget* Source)
{
    if (Source == RandomizeButton) { Randomize(); }
    else if (Source == BackButton) { Back(); }
    else if (Source == NextButton) { Next(); }
}

void USFFrontEndCreator::HandleName(const FText& Text)
{
    State.Name = Text.ToString();
    CheckName();
}

void USFFrontEndCreator::CheckName()
{
    FString Error;
    if (State.Name.IsEmpty()) { Say(NameStatus, FText::GetEmpty()); }
    else if (SFFrontEnd::ValidateCharacterName(State.Name, Error)) { Say(NameStatus, FText::FromString(TEXT("This name can be used."))); }
    else { Say(NameStatus, FText::FromString(Error), true); }
}

void USFFrontEndCreator::Changed() { OnChanged.ExecuteIfBound(State); }
