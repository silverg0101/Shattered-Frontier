#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SFFrontEnd.generated.h"

class FJsonObject;

UENUM()
enum class ESFFrontEndIdentitySource : uint8 { DevStore, Gateway };

// Front end (title, sign-in, character select, character creator): pure model, settings and the identity interface. No UObject state
// here except the settings, so the flow and the validators are tested without a world. Widgets: FrontEnd/SFFrontEndWidgets.h; the scene and
// the screen flow: FrontEnd/SFFrontEndController.h. Design: docs/decisions/CHR-01-character-creation-and-appearance.md section 3.6 and the
// backend Identity / Gateway / Characters spec (name rules 4.3, slots 4.4).

// Release-controlled declarations (typed, bounded, read when the front end opens). No console expressions.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Front End (Release Only)"))
class SHATTEREDFRONTIER_API USFFrontEndSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category = "Front End|Identity", meta = (ToolTip = "Development uses the local store by default. Shipping always uses the gateway."))
    ESFFrontEndIdentitySource IdentitySource = ESFFrontEndIdentitySource::DevStore;

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Identity", meta = (ToolTip = "Gateway root URL. HTTPS is required except for loopback development."))
    FString GatewayBaseUrl = TEXT("http://127.0.0.1:5084");

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Identity", meta = (ToolTip = "Server binding expected in enter-world tickets."))
    FString WorldServerId = TEXT("srvi_dev_local");

    UPROPERTY(Config, EditAnywhere, Category = "Front End", meta = (ToolTip = "Realm line on the title, character select and account screens (dev text until the gateway reports realms)."))
    FString RealmName = TEXT("Greenlands Realm");

    UPROPERTY(Config, EditAnywhere, Category = "Front End", meta = (ToolTip = "Title screen news panel headline."))
    FString NewsHeadline = TEXT("Silverbrook awaits.");

    UPROPERTY(Config, EditAnywhere, Category = "Front End", meta = (MultiLine = true, ToolTip = "Title screen news panel body, up to 240 characters."))
    FString NewsBody = TEXT("The frontier town is open. Gather, craft and build your claim.");

    UPROPERTY(Config, EditAnywhere, Category = "Front End", meta = (ClampMin = "1", ClampMax = "8", UIMin = "1", UIMax = "8", ToolTip = "Character slots per realm (backend CHARACTER_SLOTS_PER_REALM is 8)."))
    int32 MaxCharacterSlots = 8;

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Dev", meta = (ToolTip = "Game server the development Enter World travels to (non-shipping builds only)."))
    FString DevServerAddress = TEXT("127.0.0.1:7777");

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Scene", meta = (ToolTip = "Camera of character select and the creator (the Market Square view): X and Y world centimetres, Z the camera height above the terrain there (a Silverbrook vantage)."))
    FVector CameraLocation = FVector(0.0, -6000.0, 150.0);

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Scene", meta = (ToolTip = "Fixed camera rotation, degrees (pitch, yaw, roll)."))
    FRotator CameraRotation = FRotator(-6.0, 90.0, 0.0);

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Scene", meta = (ToolTip = "Camera of the title and sign-in screens (the river bank view of Silverbrook): X, Y centimetres, Z height above the terrain."))
    FVector TitleCameraLocation = FVector(6000.0, -25000.0, 250.0);

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Scene", meta = (ToolTip = "Rotation of the title camera, degrees."))
    FRotator TitleCameraRotation = FRotator(-5.0, 110.0, 0.0);

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Scene", meta = (ClampMin = "30", ClampMax = "100", Units = "deg"))
    float CameraFov = 40.f;

    UPROPERTY(Config, EditAnywhere, Category = "Front End|Scene", meta = (Units = "cm", ToolTip = "Where the character stands on the ground, from the camera: forward distance, sideways offset (+ is right of the view); Z is unused (the character stands on the terrain)."))
    FVector CharacterFromCamera = FVector(640.0, 0.0, 0.0);

    // Copy with ranges and lengths enforced.
    FString SafeRealmName() const;
    FString SafeNewsBody() const;
    int32 SafeSlots() const { return FMath::Clamp(MaxCharacterSlots, 1, 8); }
    FString SafeGatewayBaseUrl() const;
};

namespace SFFrontEnd
{
// Backend 4.3 (client pre-check; the server is authoritative: NFKC folding, confusable skeletons and the full denylist run there).
// 3-16 characters, starts with a letter, letters plus at most one apostrophe or hyphen, one script, no reserved staff prefix.
SHATTEREDFRONTIER_API bool ValidateCharacterName(const FString& Name, FString& OutError);
// Gateway account names: 3-32 ASCII letters, digits and underscores, beginning with a letter.
SHATTEREDFRONTIER_API bool ValidateAccountName(const FString& Name, FString& OutError);
struct FFaceChoice { FString Id; int32 Index = INDEX_NONE; };
// Ordered entries from content/appearance/human.json, filtered by body.
SHATTEREDFRONTIER_API TArray<FFaceChoice> FacesForBody(int32 Body);
}

// CHR-01 3.1 appearance, the fields the MVP creator edits. Every value is a 1-based index into the race content (0 = none for the beard).
struct FSFAppearance
{
    int32 Body = 0;       // 0 male, 1 female (the Gender step)
    int32 Heritage = 1;   // 1 Valefolk, 2 Northkin, 3 Seaborn (appearance only)
    int32 HeightCm = 172; // 150-200
    int32 Build = 50;     // 0-100, 50 = neutral
    int32 Face = 1;       // 1-based position among this body's faces
    int32 Hair = 1;       // 1-10
    int32 Beard = 0;      // 0-6
    int32 HairColour = 1; // 1-16
    int32 Skin = 1;       // 1-16
    int32 Eyes = 1;       // 1-8
    int32 Outfit = 1;     // 1-3

    static constexpr int32 MinHeight = 150, MaxHeight = 200, MaxHair = 10, MaxBeard = 5, MaxColour = 16, MaxEyes = 8, MaxOutfit = 3;

    void Clamp();
    bool IsValid() const;
    // Gender step tile 0..5: row 0 male, row 1 female; column = heritage.
    void SetFromGenderTile(int32 Tile) { const int32 NewBody = FMath::Clamp(Tile / 3, 0, 1); if (Body != NewBody) { Face = 1; } Body = NewBody; Heritage = FMath::Clamp(Tile % 3, 0, 2) + 1; }
    int32 GenderTile() const { return Body * 3 + (Heritage - 1); }
    // Random look; the Gender step choice (body, heritage) stays.
    void Randomize(FRandomStream& Random);
    // Preview scale of the placeholder body: height over the 175 cm base, build widens the body.
    FVector PreviewScale() const;
    bool operator==(const FSFAppearance& Other) const;
    TSharedRef<FJsonObject> ToJson() const;
    static FSFAppearance FromJson(const FJsonObject& Json);
    // Backend CharacterAppearance v2. Empty when the selected face/content is invalid.
    TArray<uint8> PackV2() const;
};

struct FSFCharacterSummary
{
    FString Id;
    FString Name;
    int32 Level = 1;
    FString Location;
    FSFAppearance Look;
};

enum class ESFCreatorStep : uint8 { Gender, Body, Face, Hair, Skin, Outfit, Name, Count };

// The creator's progress: seven steps, one value set, one name.
struct FSFCreatorState
{
    int32 Step = 0;
    FSFAppearance Look;
    FString Name;
    FString ArchetypeId = TEXT("fighter");
    static const TArray<FString>& AvailableArchetypeIds();
    bool SetArchetypeId(const FString& Id);

    static constexpr int32 NumSteps = static_cast<int32>(ESFCreatorStep::Count);
    static const TCHAR* StepName(int32 Index);
    bool OnLastStep() const { return Step == NumSteps - 1; }
    bool Next() { if (OnLastStep()) { return false; } ++Step; return true; }  // false on the last step: the caller creates
    bool Prev() { if (Step == 0) { return false; } --Step; return true; }    // false on the first step: the caller leaves the creator
    bool Jump(int32 To) { if (To < 0 || To >= NumSteps || To == Step) { return false; } Step = To; return true; }
    FString Progress() const { return FString::Printf(TEXT("Step %d of %d"), Step + 1, NumSteps); }
};

enum class ESFFrontEndScreen : uint8 { Title, Login, CharacterSelect, CharacterCreate, EnteringWorld };

// Which screen follows which. Esc is Back. Pure and tested.
struct FSFFrontEndFlow
{
    ESFFrontEndScreen Screen = ESFFrontEndScreen::Title;
    bool bSignedIn = false;
    bool bAccountOpen = false;  // the Account panel over character select

    // Play: the login screen, or straight to character select when already signed in.
    void Play() { if (Screen == ESFFrontEndScreen::Title) { Screen = bSignedIn ? ESFFrontEndScreen::CharacterSelect : ESFFrontEndScreen::Login; } }
    void SignedIn() { bSignedIn = true; bAccountOpen = false; Screen = ESFFrontEndScreen::CharacterSelect; }
    void SignOut() { bSignedIn = false; bAccountOpen = false; Screen = ESFFrontEndScreen::Title; }
    bool OpenAccount() { if (!bSignedIn || Screen != ESFFrontEndScreen::CharacterSelect) { return false; } bAccountOpen = true; return true; }
    bool BeginCreate() { if (Screen != ESFFrontEndScreen::CharacterSelect) { return false; } bAccountOpen = false; Screen = ESFFrontEndScreen::CharacterCreate; return true; }
    void Created() { if (Screen == ESFFrontEndScreen::CharacterCreate) { Screen = ESFFrontEndScreen::CharacterSelect; } }
    bool Enter() { if (Screen != ESFFrontEndScreen::CharacterSelect) { return false; } bAccountOpen = false; Screen = ESFFrontEndScreen::EnteringWorld; return true; }
    // Esc. Returns whether it went anywhere.
    bool Back()
    {
        if (bAccountOpen) { bAccountOpen = false; return true; }
        switch (Screen)
        {
        case ESFFrontEndScreen::Login: Screen = ESFFrontEndScreen::Title; return true;
        case ESFFrontEndScreen::CharacterSelect: Screen = ESFFrontEndScreen::Title; return true;  // stays signed in; Play returns here
        case ESFFrontEndScreen::CharacterCreate: Screen = ESFFrontEndScreen::CharacterSelect; return true;
        default: return false;
        }
    }
};

struct FSFRealmStatus
{
    FString Name;
    bool bOnline = false;
    FString Detail;  // "Online", "Offline", "Not connected"
};

// The account and character services as the front end sees them. Callbacks may run at once (the dev store) or later (a gateway client). Real
// implementation: the gateway routes in the backend Identity spec 3.2 (/v1/login/*, /v1/characters, /enter-world); see docs/dependency-map/ui.md.
class SHATTEREDFRONTIER_API ISFFrontEndIdentity
{
public:
    virtual ~ISFFrontEndIdentity() = default;
    virtual FString Label() const = 0;
    virtual FSFRealmStatus Realm() const = 0;
    // Passwords are passed through and never logged or stored.
    virtual void SignIn(const FString& Account, const FString& Password, TFunction<void(bool bOk, const FString& AccountName, const FString& Error)> Done) = 0;
    virtual void SignOut() = 0;
    virtual void ListCharacters(TFunction<void(bool bOk, const TArray<FSFCharacterSummary>&, const FString& Error)> Done) = 0;
    virtual void CreateCharacter(const FString& Name, const FSFAppearance& Look, const FString& ArchetypeId,
        TFunction<void(bool bOk, const FSFCharacterSummary&, const FString& Error)> Done) = 0;
    void CreateCharacter(const FString& Name, const FSFAppearance& Look,
        TFunction<void(bool bOk, const FSFCharacterSummary&, const FString& Error)> Done)
    { CreateCharacter(Name, Look, TEXT("fighter"), MoveTemp(Done)); }
    virtual void DeleteCharacter(const FString& Id, TFunction<void(bool bOk, const FString& Error)> Done) = 0;
    // The travel URL that enters the world with this character.
    virtual void EnterWorld(const FSFCharacterSummary& Character, TFunction<void(bool bOk, const FString& TravelUrl, const FString& Error)> Done) = 0;
    // Kept in memory across travel so the chat subsystem can request a fresh one-time contact ticket on every reconnect.
    virtual FString ChatSessionToken() const { return {}; }
};

// The identity of this build: the local dev store in development builds, an "unavailable" service in shipping builds (no auth bypass ships).
SHATTEREDFRONTIER_API TSharedRef<ISFFrontEndIdentity> SFMakeFrontEndIdentity();

// Remember-account convenience: only the account name is kept, in the user's local settings; never the password.
namespace SFFrontEnd
{
SHATTEREDFRONTIER_API FString RememberedAccount();
SHATTEREDFRONTIER_API void RememberAccount(const FString& Account);  // empty forgets
}
