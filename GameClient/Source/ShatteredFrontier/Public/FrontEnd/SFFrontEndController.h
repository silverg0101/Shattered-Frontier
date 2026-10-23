#pragma once

#include "FrontEnd/SFFrontEnd.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "SFFrontEndController.generated.h"

class ASkeletalMeshActor;
class USFFrontEndTitle;
class USFFrontEndLogin;
class USFFrontEndAccount;
class USFFrontEndSelect;
class USFFrontEndCreator;
class USFFrontEndCredits;
class USFGraphicsMenu;
class USFWindowStack;
enum class ESFTitleAction : uint8;

// The front-end mode the game enters before the world: title, sign-in, character select and creator over a fixed Silverbrook camera, with the
// character in the scene. Launch it on any world map with ?game=/Script/ShatteredFrontier.SFFrontEndGameMode (docs/dependency-map/ui.md).
UCLASS()
class SHATTEREDFRONTIER_API ASFFrontEndGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASFFrontEndGameMode();
};

// Owns the identity, the screen flow (FSFFrontEndFlow), the six page widgets and the preview character. C++ drives data and events; the pages are WBPs.
UCLASS()
class SHATTEREDFRONTIER_API ASFFrontEndController : public APlayerController
{
    GENERATED_BODY()
public:
    ASFFrontEndController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

    const FSFFrontEndFlow& GetFlow() const { return Flow; }
    ISFFrontEndIdentity& GetIdentity() { return *Identity; }
    const TArray<FSFCharacterSummary>& GetCharacters() const { return Characters; }
    int32 GetSelectedIndex() const { return SelectedIndex; }
    bool IsSettingsOpen() const;
    bool IsCreditsOpen() const;
    USFFrontEndSelect* GetSelectPage() const { return Select; }
    USFFrontEndCreator* GetCreatorPage() const { return Creator; }
    USFFrontEndLogin* GetLoginPage() const { return Login; }
    ASkeletalMeshActor* GetPreview() const { return Preview; }

    // Esc: closes the settings or credits window, else goes back a screen.
    UFUNCTION() void GoBack();
    void Show(ESFFrontEndScreen Screen);
    void DoSignIn(const FString& InAccount, const FString& Password, bool bRemember);
    void DoSignOut();
    void DoCreate(const FString& Name, const FSFAppearance& Look);
    void DoDelete(int32 Index);
    void DoEnter(int32 Index);
    void OpenSettings();
    void OpenCredits();

    // Development driver for evidence captures: SFFrontEndShow title | login | account | select | create [step] | settings | credits.
    UFUNCTION(Exec) void SFFrontEndShow(const FString& Command);

private:
    void BuildScene();
    float GroundZ(const FVector& XY) const;
    void PlaceForScreen(ESFFrontEndScreen Screen);
    void PlaceScene(const FVector& CameraXYHeight, const FRotator& Rotation);
    void BuildPages();
    void ShowPages();
    void RefreshCharacters(int32 ToSelect);
    void ApplyPreview(const FSFAppearance& Look);
    void OnTitleChoice(ESFTitleAction Action);
    void OnSelected(int32 Index);
    void OnCreatorChanged(const FSFCreatorState& State);
    void SetPageVisible(UUserWidget* Page, bool bVisible);
    void CloseSettings();
    void StartDrag() { bDragging = Flow.Screen == ESFFrontEndScreen::CharacterSelect || Flow.Screen == ESFFrontEndScreen::CharacterCreate; }
    void StopDrag() { bDragging = false; }

    FSFFrontEndFlow Flow;
    TSharedPtr<ISFFrontEndIdentity> Identity;
    TArray<FSFCharacterSummary> Characters;
    int32 SelectedIndex = INDEX_NONE;
    FString AccountName;
    bool bDragging = false;
    float PreviewYaw = 0.f;
    float CameraYaw = 0.f;
    bool bScoutCamera = false;  // dev: SFFrontEndShow cam keeps its camera while screens change
    uint64 LastBackFrame = 0;

    UPROPERTY(Transient) TObjectPtr<USFFrontEndTitle> Title;
    UPROPERTY(Transient) TObjectPtr<USFFrontEndLogin> Login;
    UPROPERTY(Transient) TObjectPtr<USFFrontEndAccount> AccountPanel;
    UPROPERTY(Transient) TObjectPtr<USFFrontEndSelect> Select;
    UPROPERTY(Transient) TObjectPtr<USFFrontEndCreator> Creator;
    UPROPERTY(Transient) TObjectPtr<USFFrontEndCredits> Credits;
    UPROPERTY(Transient) TObjectPtr<USFGraphicsMenu> Settings;
    UPROPERTY(Transient) TObjectPtr<USFWindowStack> Stack;
    UPROPERTY(Transient) TObjectPtr<ASkeletalMeshActor> Preview;
    UPROPERTY(Transient) TObjectPtr<AActor> SceneCamera;
};
