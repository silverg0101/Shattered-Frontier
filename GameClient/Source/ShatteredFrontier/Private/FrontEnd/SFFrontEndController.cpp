#include "FrontEnd/SFFrontEndController.h"

#include "Animation/AnimInstance.h"
#include "Animation/SkeletalMeshActor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Framework/SFGraphicsMenu.h"
#include "FrontEnd/SFFrontEndWidgets.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"
#include "UI/SFWindowStack.h"

#include "Misc/ConfigCacheIni.h"

ASFFrontEndGameMode::ASFFrontEndGameMode()
{
    DefaultPawnClass = nullptr;  // the scene is a fixed camera and a preview actor; nothing is possessed
    PlayerControllerClass = ASFFrontEndController::StaticClass();
    HUDClass = nullptr;
    bUseSeamlessTravel = false;
}

ASFFrontEndController::ASFFrontEndController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
}

namespace
{
template <class T> T* MakePage(APlayerController* Owner, const TCHAR* Path, int32 ZOrder)
{
    const TSubclassOf<T> Class = LoadClass<T>(nullptr, Path);
    if (!Class) { UE_LOG(LogTemp, Error, TEXT("FRONTEND_WBP_MISSING %s: run tools/ui/create_widgets.py"), Path); return nullptr; }
    T* Page = CreateWidget<T>(Owner, Class);
    if (Page) { Page->AddToViewport(ZOrder); Page->SetVisibility(ESlateVisibility::Collapsed); }
    return Page;
}
}

void ASFFrontEndController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) { return; }
    Identity = SFMakeFrontEndIdentity().ToSharedPtr();
    UE_LOG(LogTemp, Display, TEXT("FRONTEND_START Identity=%s"), *Identity->Label());
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Mode);
    Stack = NewObject<USFWindowStack>(this);
    BuildScene();
    BuildPages();
    Flow = FSFFrontEndFlow();
    ShowPages();
}

void ASFFrontEndController::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Preview) { Preview->Destroy(); }
    if (SceneCamera) { SceneCamera->Destroy(); }
    Super::EndPlay(Reason);
}

void ASFFrontEndController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) { return; }
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ASFFrontEndController::GoBack);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ASFFrontEndController::StartDrag);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &ASFFrontEndController::StopDrag);
}

void ASFFrontEndController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!bDragging || !Preview) { return; }
    if (!IsInputKeyDown(EKeys::LeftMouseButton)) { bDragging = false; return; }
    float DX = 0.f, DY = 0.f;
    GetInputMouseDelta(DX, DY);
    if (!FMath::IsNearlyZero(DX))
    {
        PreviewYaw += DX * 0.4f;
        FRotator Rotation = Preview->GetActorRotation();
        Rotation.Yaw += DX * 0.4f;
        Preview->SetActorRotation(Rotation);
    }
}

// ---- scene

void ASFFrontEndController::BuildScene()
{
    UWorld* World = GetWorld();
    if (!World) { return; }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SceneCamera = World->SpawnActor<ACameraActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (SceneCamera)
    {
        UCameraComponent* Lens = Cast<ACameraActor>(SceneCamera)->GetCameraComponent();
        Lens->SetFieldOfView(GetDefault<USFFrontEndSettings>()->CameraFov);
        Lens->bConstrainAspectRatio = false;
        SetViewTarget(SceneCamera);
    }
    Preview = World->SpawnActor<ASkeletalMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (Preview)
    {
        USkeletalMeshComponent* Body = Preview->GetSkeletalMeshComponent();
        Body->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin")));  // the player character's body (ASFE1Character)
        if (const TSubclassOf<UAnimInstance> Locomotion = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP.ThirdPerson_AnimBP_C")))
        {
            Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
            Body->SetAnimInstanceClass(Locomotion);  // idle: no pawn owner, speed stays 0
        }
        Preview->SetActorHiddenInGame(true);
    }
    PlaceForScreen(ESFFrontEndScreen::Title);
}

// Title and sign-in look at the river bank; character select and the creator at the Market Square with the character in front of the camera.
void ASFFrontEndController::PlaceForScreen(ESFFrontEndScreen Screen)
{
    if (bScoutCamera) { return; }
    const USFFrontEndSettings* Cfg = GetDefault<USFFrontEndSettings>();
    const bool bTitle = Screen == ESFFrontEndScreen::Title || Screen == ESFFrontEndScreen::Login;
    PlaceScene(bTitle ? Cfg->TitleCameraLocation : Cfg->CameraLocation, bTitle ? Cfg->TitleCameraRotation : Cfg->CameraRotation);
}

// The camera at (X, Y) with Z metres-free "height above the terrain" (the settings value), and the character on the ground in front of it.
float ASFFrontEndController::GroundZ(const FVector& XY) const
{
    FHitResult Ground;
    if (GetWorld() && GetWorld()->LineTraceSingleByChannel(Ground, FVector(XY.X, XY.Y, 100000.0), FVector(XY.X, XY.Y, -100000.0), ECC_Visibility)) { return static_cast<float>(Ground.Location.Z); }
    return 0.f;
}

void ASFFrontEndController::PlaceScene(const FVector& CameraXYHeight, const FRotator& Rotation)
{
    const USFFrontEndSettings* Cfg = GetDefault<USFFrontEndSettings>();
    FVector Eye = CameraXYHeight;
    Eye.Z = GroundZ(CameraXYHeight) + CameraXYHeight.Z;
    if (SceneCamera) { SceneCamera->SetActorLocationAndRotation(Eye, Rotation); }
    CameraYaw = static_cast<float>(Rotation.Yaw);
    const FVector Forward = FRotationMatrix(FRotator(0.0, Rotation.Yaw, 0.0)).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(FRotator(0.0, Rotation.Yaw, 0.0)).GetUnitAxis(EAxis::Y);
    FVector Feet = Eye + Forward * Cfg->CharacterFromCamera.X + Right * Cfg->CharacterFromCamera.Y;
    Feet.Z = GroundZ(Feet);
    PreviewYaw = 0.f;
    if (Preview) { Preview->SetActorLocationAndRotation(Feet, FRotator(0.0, CameraYaw + 90.0, 0.0)); }  // the mannequin's front is +Y: +90 turns it to face the camera
}

void ASFFrontEndController::ApplyPreview(const FSFAppearance& Look)
{
    if (Preview) { Preview->SetActorScale3D(Look.PreviewScale()); }
}

// ---- pages

void ASFFrontEndController::BuildPages()
{
    const USFFrontEndSettings* Cfg = GetDefault<USFFrontEndSettings>();
    Title = MakePage<USFFrontEndTitle>(this, USFFrontEndTitle::AssetPath, 10);
    Login = MakePage<USFFrontEndLogin>(this, USFFrontEndLogin::AssetPath, 10);
    Select = MakePage<USFFrontEndSelect>(this, USFFrontEndSelect::AssetPath, 10);
    Creator = MakePage<USFFrontEndCreator>(this, USFFrontEndCreator::AssetPath, 10);
    AccountPanel = MakePage<USFFrontEndAccount>(this, USFFrontEndAccount::AssetPath, 20);
    Credits = MakePage<USFFrontEndCredits>(this, USFFrontEndCredits::AssetPath, 20);
    if (Title)
    {
        Title->OnChoice.BindUObject(this, &ASFFrontEndController::OnTitleChoice);
        Title->SetRealm(Identity->Realm());
        Title->SetNews(Cfg->NewsHeadline, Cfg->SafeNewsBody());
        FString Version;
        GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
        Title->SetVersion(FString::Printf(TEXT("Version %s"), Version.IsEmpty() ? TEXT("0.1") : *Version));
    }
    if (Login)
    {
        Login->OnSignIn.BindUObject(this, &ASFFrontEndController::DoSignIn);
        const FString Remembered = SFFrontEnd::RememberedAccount();
        Login->SetAccount(Remembered, !Remembered.IsEmpty());
    }
    if (Select)
    {
        Select->OnSelected.BindUObject(this, &ASFFrontEndController::OnSelected);
        Select->OnCreate.BindWeakLambda(this, [this]() { if (Flow.BeginCreate()) { ShowPages(); } });
        Select->OnDelete.BindUObject(this, &ASFFrontEndController::DoDelete);
        Select->OnEnter.BindUObject(this, &ASFFrontEndController::DoEnter);
        Select->OnAccount.BindWeakLambda(this, [this]() { if (Flow.OpenAccount()) { ShowPages(); } });
    }
    if (Creator)
    {
        Creator->OnChanged.BindUObject(this, &ASFFrontEndController::OnCreatorChanged);
        Creator->OnCreate.BindUObject(this, &ASFFrontEndController::DoCreate);
        Creator->OnLeave.BindWeakLambda(this, [this]() { GoBack(); });
    }
    if (AccountPanel) { AccountPanel->OnSignOut.BindUObject(this, &ASFFrontEndController::DoSignOut); }
}

void ASFFrontEndController::SetPageVisible(UUserWidget* Page, bool bVisible)
{
    if (Page) { Page->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
}

void ASFFrontEndController::ShowPages()
{
    const bool bSettings = IsSettingsOpen();
    SetPageVisible(Title, Flow.Screen == ESFFrontEndScreen::Title && !bSettings);
    SetPageVisible(Login, Flow.Screen == ESFFrontEndScreen::Login);
    SetPageVisible(Select, Flow.Screen == ESFFrontEndScreen::CharacterSelect);
    SetPageVisible(Creator, Flow.Screen == ESFFrontEndScreen::CharacterCreate);
    SetPageVisible(AccountPanel, Flow.bAccountOpen);
    if (Flow.Screen != ESFFrontEndScreen::Title) { SetPageVisible(Credits, false); }
    const bool bScene = Flow.Screen == ESFFrontEndScreen::CharacterSelect || Flow.Screen == ESFFrontEndScreen::CharacterCreate;
    if (Preview) { Preview->SetActorHiddenInGame(!bScene && Flow.Screen != ESFFrontEndScreen::EnteringWorld); }
    if (Flow.Screen == ESFFrontEndScreen::Title && Title) { Title->SetRealm(Identity->Realm()); }
    if (Flow.bAccountOpen && AccountPanel) { AccountPanel->Show(AccountName, Identity->Realm()); }
}

void ASFFrontEndController::Show(ESFFrontEndScreen Screen)
{
    Flow.Screen = Screen;
    PlaceForScreen(Screen);
    PreviewYaw = 0.f;
    if (Preview) { Preview->SetActorRotation(FRotator(0.0, CameraYaw + 90.0, 0.0)); }
    if (Screen == ESFFrontEndScreen::CharacterSelect) { RefreshCharacters(SelectedIndex); }
    if (Screen == ESFFrontEndScreen::CharacterCreate && Creator) { Creator->Begin(); }
    ShowPages();
}

void ASFFrontEndController::RefreshCharacters(int32 ToSelect)
{
    Identity->ListCharacters([this, ToSelect](bool bOk, const TArray<FSFCharacterSummary>& List, const FString& Error)
    {
        Characters = bOk ? List : TArray<FSFCharacterSummary>();
        SelectedIndex = Characters.IsEmpty() ? INDEX_NONE : FMath::Clamp(ToSelect == INDEX_NONE ? 0 : ToSelect, 0, Characters.Num() - 1);
        if (Select)
        {
            Select->SetCharacters(Characters, GetDefault<USFFrontEndSettings>()->SafeSlots(), SelectedIndex);
            Select->SetRealm(Identity->Realm());
            Select->SetStatus(FText::FromString(Error), true);
        }
        ApplyPreview(SelectedIndex != INDEX_NONE ? Characters[SelectedIndex].Look : FSFAppearance());
    });
}

void ASFFrontEndController::OnSelected(int32 Index)
{
    SelectedIndex = Index;
    if (Characters.IsValidIndex(Index)) { ApplyPreview(Characters[Index].Look); }
}

void ASFFrontEndController::OnCreatorChanged(const FSFCreatorState& State) { ApplyPreview(State.Look); }

void ASFFrontEndController::OnTitleChoice(ESFTitleAction Action)
{
    switch (Action)
    {
    case ESFTitleAction::Play:
        Flow.Play();
        if (Flow.Screen == ESFFrontEndScreen::CharacterSelect) { Show(ESFFrontEndScreen::CharacterSelect); } else { ShowPages(); }
        break;
    case ESFTitleAction::Settings: OpenSettings(); break;
    case ESFTitleAction::Credits: OpenCredits(); break;
    case ESFTitleAction::Quit: UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false); break;
    }
}

// ---- flow

void ASFFrontEndController::GoBack()
{
    if (LastBackFrame == GFrameCounter) { return; }  // a widget and the key binding can both see the same press
    LastBackFrame = GFrameCounter;
    if (IsSettingsOpen()) { Settings->HandleEscape(); return; }
    if (IsCreditsOpen()) { SetPageVisible(Credits, false); return; }
    if (Flow.Back()) { Show(Flow.Screen); }
}

bool ASFFrontEndController::IsSettingsOpen() const { return Settings && Settings->GetVisibility() != ESlateVisibility::Collapsed; }
bool ASFFrontEndController::IsCreditsOpen() const { return Credits && Credits->GetVisibility() != ESlateVisibility::Collapsed; }

void ASFFrontEndController::OpenSettings()
{
    if (!Settings)
    {
        const TSubclassOf<USFGraphicsMenu> SettingsClass = LoadClass<USFGraphicsMenu>(nullptr, USFGraphicsMenu::AssetPath);
        if (!SettingsClass) { UE_LOG(LogTemp, Error, TEXT("SETTINGS_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFGraphicsMenu::AssetPath); return; }
        Settings = CreateWidget<USFGraphicsMenu>(this, SettingsClass);
        if (!Settings) { return; }
        Settings->AddToViewport(40);
        Settings->SetVisibility(ESlateVisibility::Collapsed);
        Settings->OnClosed.AddWeakLambda(this, [this]() { Stack->Close(Settings); ShowPages(); });
    }
    Settings->Reload();
    Stack->Open(Settings);
    ShowPages();
}

void ASFFrontEndController::OpenCredits()
{
    SetPageVisible(Credits, true);
}

void ASFFrontEndController::DoSignIn(const FString& InAccount, const FString& Password, bool bRemember)
{
    if (!Login) { return; }
    FString Error;
    if (!SFFrontEnd::ValidateAccountName(InAccount, Error)) { Login->SetStatus(FText::FromString(Error), true); return; }
    if (Password.IsEmpty()) { Login->SetStatus(FText::FromString(TEXT("Enter your password.")), true); return; }
    Login->SetBusy(true);
    Login->SetStatus(FText::FromString(TEXT("Signing in...")), false);
    Identity->SignIn(InAccount, Password, [this, bRemember](bool bOk, const FString& Name, const FString& Failure)
    {
        Login->SetBusy(false);
        Login->ClearPassword();
        if (!bOk) { Login->SetStatus(FText::FromString(Failure), true); return; }
        SFFrontEnd::RememberAccount(bRemember ? Name : FString());
        AccountName = Name;
        Login->SetStatus(FText::GetEmpty(), false);
        Flow.SignedIn();
        SelectedIndex = INDEX_NONE;
        Show(ESFFrontEndScreen::CharacterSelect);
    });
}

void ASFFrontEndController::DoSignOut()
{
    Identity->SignOut();
    Flow.SignOut();
    Characters.Reset();
    SelectedIndex = INDEX_NONE;
    AccountName.Reset();
    Show(ESFFrontEndScreen::Title);
}

void ASFFrontEndController::DoCreate(const FString& Name, const FSFAppearance& Look)
{
    Identity->CreateCharacter(Name, Look, [this](bool bOk, const FSFCharacterSummary& Made, const FString& Error)
    {
        if (!bOk) { if (Creator) { Creator->SetStatus(FText::FromString(Error), true); } return; }  // NAME_TAKEN and the rest keep the creator and the typed name
        Flow.Created();
        SelectedIndex = INDEX_NONE;
        RefreshCharacters(INDEX_NONE);
        const int32 Index = Characters.IndexOfByPredicate([&Made](const FSFCharacterSummary& C) { return C.Id == Made.Id; });
        if (Index != INDEX_NONE) { RefreshCharacters(Index); }  // the new character is the selected one
        Show(ESFFrontEndScreen::CharacterSelect);
    });
}

void ASFFrontEndController::DoDelete(int32 Index)
{
    if (!Characters.IsValidIndex(Index)) { return; }
    Identity->DeleteCharacter(Characters[Index].Id, [this, Index](bool bOk, const FString& Error)
    {
        if (!bOk) { if (Select) { Select->SetStatus(FText::FromString(Error), true); } return; }
        RefreshCharacters(FMath::Max(0, Index - 1));
    });
}

void ASFFrontEndController::DoEnter(int32 Index)
{
    if (!Characters.IsValidIndex(Index)) { return; }
    Identity->EnterWorld(Characters[Index], [this](bool bOk, const FString& Url, const FString& Error)
    {
        if (!bOk) { if (Select) { Select->SetStatus(FText::FromString(Error), true); } return; }
        if (!Flow.Enter()) { return; }
        ShowPages();
        UE_LOG(LogTemp, Display, TEXT("FRONTEND_ENTER_WORLD Character=%s"), *Characters[SelectedIndex].Id);
        ClientTravel(Url, TRAVEL_Absolute);
    });
}

// ---- development driver

void ASFFrontEndController::SFFrontEndShow(const FString& Command)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TArray<FString> Words;
    Command.ParseIntoArray(Words, TEXT(" "));
    if (Words.IsEmpty() || !Identity) { return; }
    const FString& What = Words[0];
    auto EnsureSignedIn = [this]()
    {
        if (Flow.bSignedIn) { return; }
        Identity->SignIn(TEXT("demo"), TEXT("dev"), [this](bool bOk, const FString& Name, const FString&) { if (bOk) { AccountName = Name; Flow.SignedIn(); } });
    };
    if (What.Equals(TEXT("title"), ESearchCase::IgnoreCase)) { if (IsSettingsOpen()) { Settings->HandleEscape(); } Flow = FSFFrontEndFlow(); Show(ESFFrontEndScreen::Title); }
    else if (What.Equals(TEXT("login"), ESearchCase::IgnoreCase)) { Flow = FSFFrontEndFlow(); Show(ESFFrontEndScreen::Login); }
    else if (What.Equals(TEXT("select"), ESearchCase::IgnoreCase)) { EnsureSignedIn(); Flow.bAccountOpen = false; Show(ESFFrontEndScreen::CharacterSelect); }
    else if (What.Equals(TEXT("account"), ESearchCase::IgnoreCase)) { EnsureSignedIn(); Show(ESFFrontEndScreen::CharacterSelect); Flow.OpenAccount(); ShowPages(); }
    else if (What.Equals(TEXT("create"), ESearchCase::IgnoreCase))
    {
        EnsureSignedIn();
        Flow.Screen = ESFFrontEndScreen::CharacterSelect;
        Flow.BeginCreate();
        Show(ESFFrontEndScreen::CharacterCreate);
        if (Creator && Words.IsValidIndex(1)) { Creator->GoToStep(FCString::Atoi(*Words[1]) - 1); }
        if (Creator && Words.IsValidIndex(2)) { Creator->SetNameForTest(Words[2]); }
    }
    else if ((What.Equals(TEXT("cam"), ESearchCase::IgnoreCase) || What.Equals(TEXT("camc"), ESearchCase::IgnoreCase)) && Words.Num() >= 6)  // cam (title) / camc (character select) <x m> <y m> <height cm> <pitch> <yaw>: scouting a vantage
    {
        bScoutCamera = false;
        PlaceScene(FVector(FCString::Atod(*Words[1]) * 100.0, FCString::Atod(*Words[2]) * 100.0, FCString::Atod(*Words[3])), FRotator(FCString::Atod(*Words[4]), FCString::Atod(*Words[5]), 0.0));
        bScoutCamera = true;
        if (What.Equals(TEXT("camc"), ESearchCase::IgnoreCase)) { EnsureSignedIn(); Flow.bAccountOpen = false; Show(ESFFrontEndScreen::CharacterSelect); }
        else { Flow = FSFFrontEndFlow(); Show(ESFFrontEndScreen::Title); }
    }
    else if (What.Equals(TEXT("settings"), ESearchCase::IgnoreCase)) { Flow = FSFFrontEndFlow(); Show(ESFFrontEndScreen::Title); OpenSettings(); }
    else if (What.Equals(TEXT("credits"), ESearchCase::IgnoreCase)) { Flow = FSFFrontEndFlow(); Show(ESFFrontEndScreen::Title); OpenCredits(); }
#endif
}
