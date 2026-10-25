#pragma once

#include "FrontEnd/SFFrontEnd.h"
#include "UI/SFUiWidgets.h"
#include "SFFrontEndWidgets.generated.h"

class UUniformGridPanel;

// Front-end screens (docs/blueprints/menus.md section 0, "Front end"). Each is a Widget Blueprint under /Game/Frontier/UI/Widgets/FrontEnd/ built only from
// the kit v7 parents (seeded by tools/ui/create_widgets.py); C++ owns data and events, the WBP owns layout. The controller (SFFrontEndController.h)
// owns the flow and binds the delegates. Esc goes back a screen; there are no close buttons.

// Shared base: the menu font family on every text block the Designer left at the UMG default.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFFrontEndPage : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override { Super::NativeOnInitialized(); SetIsFocusable(false); }
    virtual void ApplyKitStyle() override { ApplyFamilyToAllText(TextStyle); }
    static void Say(UTextBlock* Block, const FText& Text, bool bError = false);
};

enum class ESFTitleAction : uint8 { Play, Settings, Credits, Quit };

// WBP_FrontEndTitle: logo, the four kit buttons, the news panel, realm line and version.
// Binds, all optional: PlayButton, SettingsButton, CreditsButton, QuitButton (WBP_Button), RealmText, RealmMark, NewsTitle, NewsText, VersionText (TextBlocks / Image).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFFrontEndTitle : public USFFrontEndPage
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/FrontEnd/WBP_FrontEndTitle.WBP_FrontEndTitle_C");
    DECLARE_DELEGATE_OneParam(FChoiceDelegate, ESFTitleAction);
    FChoiceDelegate OnChoice;
    void Choose(ESFTitleAction Action) { OnChoice.ExecuteIfBound(Action); }
    USFButtonWidget* GetButton(ESFTitleAction Action) const;
    void SetRealm(const FSFRealmStatus& Realm);
    void SetNews(const FString& Headline, const FString& Body);
    void SetVersion(const FString& Version);

protected:
    virtual void NativeOnInitialized() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> PlayButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> SettingsButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> CreditsButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> QuitButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RealmText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> RealmMark;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NewsTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NewsText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> VersionText;

private:
    UFUNCTION() void HandleButton(USFButtonWidget* Source);
};

// WBP_FrontEndCredits: a WBP_WindowFrame with the credits text; Esc closes it.
UCLASS()
class SHATTEREDFRONTIER_API USFFrontEndCredits : public USFFrontEndPage
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/FrontEnd/WBP_FrontEndCredits.WBP_FrontEndCredits_C");
};

// WBP_FrontEndLogin: account name + masked password, Remember account, Sign In, a status line.
// Binds: AccountField, PasswordField (WBP_InputField; the password one has bPassword), RememberToggle (WBP_Toggle), SignInButton (WBP_Button), StatusText.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFFrontEndLogin : public USFFrontEndPage
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/FrontEnd/WBP_FrontEndLogin.WBP_FrontEndLogin_C");
    DECLARE_DELEGATE_ThreeParams(FSignInDelegate, const FString& /*Account*/, const FString& /*Password*/, bool /*bRemember*/);
    FSignInDelegate OnSignIn;
    // The same request the button and Enter in the password field make.
    void Submit();
    void SetAccount(const FString& Account, bool bRemember);
    void ClearPassword();
    void SetStatus(const FText& Text, bool bError);
    void SetBusy(bool bBusy);
    FString GetAccount() const;
    // Text the password field holds (tests; the field masks it on screen).
    void SetPasswordForTest(const FString& Password);
    void SetAccountForTest(const FString& Account);

protected:
    virtual void NativeOnInitialized() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFTextFieldWidget> AccountField;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFTextFieldWidget> PasswordField;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFToggleWidget> RememberToggle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> SignInButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;

private:
    UFUNCTION() void HandleSignInButton(USFButtonWidget* Source) { Submit(); }
    UFUNCTION() void HandleCommitted(const FText& Text, ETextCommit::Type Method) { if (Method == ETextCommit::OnEnter) { Submit(); } }
    bool bBusy = false;
};

// WBP_FrontEndAccount: the Account panel (account name, realm, Sign Out). Binds: AccountText, RealmText (TextBlocks), SignOutButton (WBP_Button).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFFrontEndAccount : public USFFrontEndPage
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/FrontEnd/WBP_FrontEndAccount.WBP_FrontEndAccount_C");
    DECLARE_DELEGATE(FSignOutDelegate);
    FSignOutDelegate OnSignOut;
    void Show(const FString& Account, const FSFRealmStatus& Realm);
    USFButtonWidget* GetSignOutButton() const { return SignOutButton; }

protected:
    virtual void NativeOnInitialized() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> AccountText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RealmText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> SignOutButton;

private:
    UFUNCTION() void HandleSignOut(USFButtonWidget* Source) { OnSignOut.ExecuteIfBound(); }
};

// WBP_FrontEndCharacters: the "Characters" window (one WBP_KitRow per character, then empty slot rows), Create New, hold-to-confirm Delete, the realm
// plate, an Account button and the big Enter World button.
// Binds: Rows (VerticalBox in a WBP_ScrollList). Optional: Window, CreateButton, DeleteButton, EnterButton, AccountButton (WBP_Button), RealmText, RealmMark, StatusText.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFFrontEndSelect : public USFFrontEndPage
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/FrontEnd/WBP_FrontEndCharacters.WBP_FrontEndCharacters_C");
    DECLARE_DELEGATE_OneParam(FIndexDelegate, int32);
    DECLARE_DELEGATE(FVoidDelegate);
    FIndexDelegate OnSelected;  // a character row was clicked
    FVoidDelegate OnCreate;     // Create New or an empty slot
    FIndexDelegate OnDelete;    // the hold completed on the selected character
    FIndexDelegate OnEnter;
    FVoidDelegate OnAccount;

    // Characters in slot order, then empty slots up to Slots (at most MaxEmptyShown of them). Selected is clamped; INDEX_NONE = none.
    void SetCharacters(const TArray<FSFCharacterSummary>& InCharacters, int32 Slots, int32 InSelected);
    void SetRealm(const FSFRealmStatus& Realm);
    void SetStatus(const FText& Text, bool bError);
    int32 GetSelected() const { return Selected; }
    int32 NumCharacters() const { return Characters.Num(); }
    int32 NumRows() const { return Rows ? Rows->GetChildrenCount() : 0; }
    USFKitRowWidget* GetRow(int32 Index) const;
    USFButtonWidget* GetDeleteButton() const { return DeleteButton; }
    USFButtonWidget* GetEnterButton() const { return EnterButton; }
    USFButtonWidget* GetCreateButton() const { return CreateButton; }
    static constexpr int32 MaxEmptyShown = 2;
    UPROPERTY(EditAnywhere, Category = "SF", meta = (ClampMin = "0.2", Units = "s")) float DeleteHoldSeconds = 1.5f;

protected:
    virtual void NativeOnInitialized() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> Rows;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> CreateButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> DeleteButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> EnterButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> AccountButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RealmText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> RealmMark;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(Transient) TSubclassOf<USFKitRowWidget> RowClass;

private:
    UFUNCTION() void HandleRow(USFSelectableWidget* Source);
    UFUNCTION() void HandleButton(USFButtonWidget* Source);
    void Refresh();
    TArray<FSFCharacterSummary> Characters;
    int32 Selected = INDEX_NONE;
    int32 SlotCount = 8;
};

// WBP_FrontEndCreator: the left "Create" step list (a WBP_KitRow per step, gold select frame on the current one), the right window per step (Gender: 2 x 3
// WBP_ItemTile; Body / Face / Hair / Skin / Outfit: WBP_SettingRow + WBP_Stepper rows; Name: a WBP_InputField and its check), the
// "Step N of 7" line, a drag-to-rotate hint and Randomize / Back / Next. Next on the last step is Create.
// Binds: StepList (VerticalBox), GenderTiles (UniformGridPanel), OptionRows (VerticalBox). Optional: PageWindow (WBP_WindowFrame, its title = the step),
// GenderPage, OptionsPage, NamePage (the page containers), NameField, NameStatus, StepText, RandomizeButton, BackButton, NextButton.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFFrontEndCreator : public USFFrontEndPage
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/FrontEnd/WBP_FrontEndCreator.WBP_FrontEndCreator_C");
    DECLARE_DELEGATE_OneParam(FStateDelegate, const FSFCreatorState&);
    DECLARE_DELEGATE_ThreeParams(FCreateDelegate, const FString& /*Name*/, const FSFAppearance&, const FString& /*ArchetypeId*/);
    DECLARE_DELEGATE(FVoidDelegate);
    FStateDelegate OnChanged;  // the look or the step changed: the scene updates the preview
    FCreateDelegate OnCreate;  // Next on the Name step with a valid name
    FVoidDelegate OnLeave;     // Back on the first step

    void Begin();  // a fresh creator on step 1 with the default look
    const FSFCreatorState& GetState() const { return State; }
    UFUNCTION(BlueprintCallable, Category="SF|Character Creation") TArray<FString> GetAvailableArchetypeIds() const;
    UFUNCTION(BlueprintCallable, Category="SF|Character Creation") FString GetArchetypeId() const { return State.ArchetypeId; }
    UFUNCTION(BlueprintCallable, Category="SF|Character Creation") bool SetArchetypeId(const FString& Id);
    void GoToStep(int32 Step);
    void Back();
    void Next();
    void Randomize();
    void SetStatus(const FText& Text, bool bError);
    void SetNameForTest(const FString& Name);
    int32 NumOptionRows() const { return OptionRows ? OptionRows->GetChildrenCount() : 0; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> StepList;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UUniformGridPanel> GenderTiles;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> OptionRows;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFWindowFrameWidget> PageWindow;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> GenderPage;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> OptionsPage;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> NamePage;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTextFieldWidget> NameField;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameStatus;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StepText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> RandomizeButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> BackButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> NextButton;
    UPROPERTY(Transient) TSubclassOf<USFKitRowWidget> RowClass;
    UPROPERTY(Transient) TSubclassOf<USFSlotWidget> TileClass;
    UPROPERTY(Transient) TSubclassOf<USFSettingRowWidget> SettingRowClass;
    UPROPERTY(Transient) TSubclassOf<USFStepperWidget> StepperClass;

private:
    UFUNCTION() void HandleStepRow(USFSelectableWidget* Source);
    UFUNCTION() void HandleTile(USFSelectableWidget* Source);
    UFUNCTION() void HandleButton(USFButtonWidget* Source);
    UFUNCTION() void HandleName(const FText& Text);
    void BuildStepList();
    void BuildTiles();
    void ShowStep();
    void RebuildOptions();
    void Changed();
    void CheckName();

    struct FOption { TWeakObjectPtr<USFStepperWidget> Stepper; int32 FSFAppearance::* Member = nullptr; TWeakObjectPtr<USFSettingRowWidget> Row; };
    TArray<FOption> Options;
    UPROPERTY(Transient) TArray<TObjectPtr<USFKitRowWidget>> StepRows;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> Tiles;
    FSFCreatorState State;
    FRandomStream Random;
};
