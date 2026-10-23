#include "FrontEnd/SFFrontEnd.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString USFFrontEndSettings::SafeRealmName() const { return RealmName.IsEmpty() ? FString(TEXT("Realm")) : RealmName.Left(40); }
FString USFFrontEndSettings::SafeNewsBody() const { return NewsBody.Left(240); }

namespace
{
enum class EScript : uint8 { None, Latin, Greek, Cyrillic };

EScript ScriptOf(TCHAR C)
{
    const uint32 U = static_cast<uint32>(C);
    if ((U >= 'A' && U <= 'Z') || (U >= 'a' && U <= 'z') || (U >= 0xC0 && U <= 0x24F && U != 0xD7 && U != 0xF7)) { return EScript::Latin; }
    if (U >= 0x370 && U <= 0x3FF) { return EScript::Greek; }
    if (U >= 0x400 && U <= 0x4FF) { return EScript::Cyrillic; }
    return EScript::None;
}
}

bool SFFrontEnd::ValidateCharacterName(const FString& Name, FString& OutError)
{
    if (Name.Len() < 3) { OutError = TEXT("A name needs at least 3 letters."); return false; }
    if (Name.Len() > 16) { OutError = TEXT("A name can be at most 16 letters."); return false; }
    int32 Marks = 0;
    EScript Script = EScript::None;
    for (int32 i = 0; i < Name.Len(); ++i)
    {
        const TCHAR C = Name[i];
        if (C == '\'' || C == '-')
        {
            if (i == 0) { OutError = TEXT("A name starts with a letter."); return false; }
            if (++Marks > 1) { OutError = TEXT("Use at most one apostrophe or hyphen."); return false; }
            continue;
        }
        const EScript Letter = ScriptOf(C);
        if (Letter == EScript::None) { OutError = i == 0 ? TEXT("A name starts with a letter.") : TEXT("Use letters, one apostrophe or one hyphen only."); return false; }
        if (Script != EScript::None && Letter != Script) { OutError = TEXT("Do not mix alphabets in one name."); return false; }
        Script = Letter;
    }
    if (Name.EndsWith(TEXT("'")) || Name.EndsWith(TEXT("-"))) { OutError = TEXT("A name ends with a letter."); return false; }
    for (const TCHAR* Reserved : {TEXT("gm"), TEXT("admin"), TEXT("sys"), TEXT("moderator")})
    {
        if (Name.StartsWith(Reserved, ESearchCase::IgnoreCase)) { OutError = TEXT("That name is reserved."); return false; }
    }
    OutError.Reset();
    return true;
}

bool SFFrontEnd::ValidateAccountName(const FString& Name, FString& OutError)
{
    if (Name.Len() < 3 || Name.Len() > 24) { OutError = TEXT("An account name is 3 to 24 characters."); return false; }
    for (const TCHAR C : Name)
    {
        if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_')) { OutError = TEXT("Use letters, digits and underscores only."); return false; }
    }
    OutError.Reset();
    return true;
}

void FSFAppearance::Clamp()
{
    Body = FMath::Clamp(Body, 0, 1);
    Heritage = FMath::Clamp(Heritage, 1, 3);
    HeightCm = FMath::Clamp(HeightCm, MinHeight, MaxHeight);
    Build = FMath::Clamp(Build, 0, 100);
    Face = FMath::Clamp(Face, 1, MaxFace);
    Hair = FMath::Clamp(Hair, 1, MaxHair);
    Beard = FMath::Clamp(Beard, 0, MaxBeard);
    HairColour = FMath::Clamp(HairColour, 1, MaxColour);
    Skin = FMath::Clamp(Skin, 1, MaxColour);
    Eyes = FMath::Clamp(Eyes, 1, MaxEyes);
    Outfit = FMath::Clamp(Outfit, 1, MaxOutfit);
}

bool FSFAppearance::IsValid() const
{
    FSFAppearance Copy = *this;
    Copy.Clamp();
    return Copy == *this;
}

bool FSFAppearance::operator==(const FSFAppearance& O) const
{
    return Body == O.Body && Heritage == O.Heritage && HeightCm == O.HeightCm && Build == O.Build && Face == O.Face && Hair == O.Hair
        && Beard == O.Beard && HairColour == O.HairColour && Skin == O.Skin && Eyes == O.Eyes && Outfit == O.Outfit;
}

void FSFAppearance::Randomize(FRandomStream& Random)
{
    HeightCm = Random.RandRange(MinHeight, MaxHeight);
    Build = Random.RandRange(0, 100);
    Face = Random.RandRange(1, MaxFace);
    Hair = Random.RandRange(1, MaxHair);
    Beard = Body == 0 ? Random.RandRange(0, MaxBeard) : 0;
    HairColour = Random.RandRange(1, MaxColour);
    Skin = Random.RandRange(1, MaxColour);
    Eyes = Random.RandRange(1, MaxEyes);
    Outfit = Random.RandRange(1, MaxOutfit);
}

FVector FSFAppearance::PreviewScale() const
{
    const float Tall = static_cast<float>(HeightCm) / 175.f;
    const float Wide = FMath::Lerp(0.92f, 1.1f, static_cast<float>(Build) / 100.f);
    return FVector(Tall * Wide, Tall * Wide, Tall);
}

TSharedRef<FJsonObject> FSFAppearance::ToJson() const
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("body"), Body); Json->SetNumberField(TEXT("heritage"), Heritage); Json->SetNumberField(TEXT("height"), HeightCm);
    Json->SetNumberField(TEXT("build"), Build); Json->SetNumberField(TEXT("face"), Face); Json->SetNumberField(TEXT("hair"), Hair);
    Json->SetNumberField(TEXT("beard"), Beard); Json->SetNumberField(TEXT("hair_colour"), HairColour); Json->SetNumberField(TEXT("skin"), Skin);
    Json->SetNumberField(TEXT("eyes"), Eyes); Json->SetNumberField(TEXT("outfit"), Outfit);
    return Json;
}

FSFAppearance FSFAppearance::FromJson(const FJsonObject& Json)
{
    FSFAppearance Look;
    auto Get = [&Json](const TCHAR* Key, int32 Default) { double V = Default; Json.TryGetNumberField(Key, V); return static_cast<int32>(V); };
    Look.Body = Get(TEXT("body"), 0); Look.Heritage = Get(TEXT("heritage"), 1); Look.HeightCm = Get(TEXT("height"), 172); Look.Build = Get(TEXT("build"), 50);
    Look.Face = Get(TEXT("face"), 1); Look.Hair = Get(TEXT("hair"), 1); Look.Beard = Get(TEXT("beard"), 0); Look.HairColour = Get(TEXT("hair_colour"), 1);
    Look.Skin = Get(TEXT("skin"), 1); Look.Eyes = Get(TEXT("eyes"), 1); Look.Outfit = Get(TEXT("outfit"), 1);
    Look.Clamp();
    return Look;
}

const TCHAR* FSFCreatorState::StepName(int32 Index)
{
    static const TCHAR* const Names[NumSteps] = {TEXT("Gender"), TEXT("Body"), TEXT("Face"), TEXT("Hair"), TEXT("Skin"), TEXT("Outfit"), TEXT("Name")};
    return Names[FMath::Clamp(Index, 0, NumSteps - 1)];
}

// ---- remember account

namespace
{
constexpr const TCHAR* FrontEndSection = TEXT("SFFrontEnd");
}

FString SFFrontEnd::RememberedAccount()
{
    FString Account;
    GConfig->GetString(FrontEndSection, TEXT("RememberedAccount"), Account, GGameUserSettingsIni);
    return Account;
}

void SFFrontEnd::RememberAccount(const FString& Account)
{
    GConfig->SetString(FrontEndSection, TEXT("RememberedAccount"), *Account, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

// ---- identity

namespace
{
// Shipping builds and builds without a gateway client: every call says so. There is no bypass.
class FUnavailableIdentity final : public ISFFrontEndIdentity
{
public:
    FString Label() const override { return TEXT("unavailable"); }
    FSFRealmStatus Realm() const override { return {GetDefault<USFFrontEndSettings>()->SafeRealmName(), false, TEXT("Not connected")}; }
    void SignIn(const FString&, const FString&, TFunction<void(bool, const FString&, const FString&)> Done) override { Done(false, FString(), Message()); }
    void SignOut() override {}
    void ListCharacters(TFunction<void(bool, const TArray<FSFCharacterSummary>&, const FString&)> Done) override { Done(false, {}, Message()); }
    void CreateCharacter(const FString&, const FSFAppearance&, TFunction<void(bool, const FSFCharacterSummary&, const FString&)> Done) override { Done(false, FSFCharacterSummary(), Message()); }
    void DeleteCharacter(const FString&, TFunction<void(bool, const FString&)> Done) override { Done(false, Message()); }
    void EnterWorld(const FSFCharacterSummary&, TFunction<void(bool, const FString&, const FString&)> Done) override { Done(false, FString(), Message()); }
private:
    static FString Message() { return TEXT("Account sign-in is not available in this build yet."); }
};
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
namespace
{
// Development identity (non-shipping only): any valid account name with a non-empty password signs in; characters live in
// Saved/FrontEnd/Dev_<account>.json. The password is checked for presence only: never stored, never logged. -SFFrontEndSeed adds two demo
// characters to an empty account so captures have rows.
class FDevIdentity final : public ISFFrontEndIdentity
{
public:
    FString Label() const override { return TEXT("dev"); }
    FSFRealmStatus Realm() const override { return {GetDefault<USFFrontEndSettings>()->SafeRealmName(), true, TEXT("Online")}; }

    void SignIn(const FString& InAccount, const FString& Password, TFunction<void(bool, const FString&, const FString&)> Done) override
    {
        FString Error;
        if (!SFFrontEnd::ValidateAccountName(InAccount, Error)) { Done(false, FString(), Error); return; }
        if (Password.IsEmpty()) { Done(false, FString(), TEXT("Enter your password.")); return; }
        Account = InAccount.ToLower();
        UE_LOG(LogTemp, Display, TEXT("FRONTEND_SIGNIN Identity=dev Account=%s Ok=1"), *Account);  // never the password
        Load();
        if (Characters.IsEmpty() && FParse::Param(FCommandLine::Get(), TEXT("SFFrontEndSeed"))) { Seed(); }
        Done(true, Account, FString());
    }

    void SignOut() override { Account.Reset(); Characters.Reset(); }

    void ListCharacters(TFunction<void(bool, const TArray<FSFCharacterSummary>&, const FString&)> Done) override
    {
        if (Account.IsEmpty()) { Done(false, {}, TEXT("Sign in first.")); return; }
        Done(true, Characters, FString());
    }

    void CreateCharacter(const FString& Name, const FSFAppearance& Look, TFunction<void(bool, const FSFCharacterSummary&, const FString&)> Done) override
    {
        FString Error;
        if (Account.IsEmpty()) { Done(false, FSFCharacterSummary(), TEXT("Sign in first.")); return; }
        if (!SFFrontEnd::ValidateCharacterName(Name, Error) || !Look.IsValid()) { Done(false, FSFCharacterSummary(), Error.IsEmpty() ? FString(TEXT("That look is not valid.")) : Error); return; }
        if (Characters.Num() >= GetDefault<USFFrontEndSettings>()->SafeSlots()) { Done(false, FSFCharacterSummary(), TEXT("All character slots are used.")); return; }
        if (NameTaken(Name)) { Done(false, FSFCharacterSummary(), TEXT("That name is taken.")); return; }  // NAME_TAKEN: the creator keeps the name
        FSFCharacterSummary Made;
        Made.Id = TEXT("chr_dev_") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(12).ToLower();
        Made.Name = Name; Made.Level = 1; Made.Location = TEXT("Silverbrook"); Made.Look = Look;
        Characters.Add(Made);
        Save();
        Done(true, Made, FString());
    }

    void DeleteCharacter(const FString& Id, TFunction<void(bool, const FString&)> Done) override
    {
        const int32 Removed = Characters.RemoveAll([&Id](const FSFCharacterSummary& C) { return C.Id == Id; });
        if (Removed) { Save(); }
        Done(Removed > 0, Removed ? FString() : FString(TEXT("That character no longer exists.")));
    }

    void EnterWorld(const FSFCharacterSummary& Character, TFunction<void(bool, const FString&, const FString&)> Done) override
    {
        // The server's -SFDevIdentity flag binds ?SFDevCharacter=<lowercase letters, digits, underscore, at most 24>.
        FString Dev = Character.Name.ToLower();
        for (TCHAR& C : Dev) { if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9'))) { C = '_'; } }
        Done(true, FString::Printf(TEXT("%s?SFDevCharacter=%s"), *GetDefault<USFFrontEndSettings>()->DevServerAddress, *Dev.Left(24)), FString());
    }

private:
    FString Account;
    TArray<FSFCharacterSummary> Characters;

    static FString Dir() { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FrontEnd")); }
    FString File() const { return FPaths::Combine(Dir(), TEXT("Dev_") + Account + TEXT(".json")); }

    void Load()
    {
        Characters.Reset();
        FString Text;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Text, *File()) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { return; }
        const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
        if (!Root->TryGetArrayField(TEXT("characters"), List)) { return; }
        for (const TSharedPtr<FJsonValue>& Value : *List)
        {
            const TSharedPtr<FJsonObject>* Item = nullptr;
            if (!Value->TryGetObject(Item)) { continue; }
            FSFCharacterSummary C;
            (*Item)->TryGetStringField(TEXT("id"), C.Id); (*Item)->TryGetStringField(TEXT("name"), C.Name); (*Item)->TryGetStringField(TEXT("location"), C.Location);
            double Level = 1; (*Item)->TryGetNumberField(TEXT("level"), Level); C.Level = FMath::Max(1, static_cast<int32>(Level));
            const TSharedPtr<FJsonObject>* Look = nullptr;
            if ((*Item)->TryGetObjectField(TEXT("look"), Look)) { C.Look = FSFAppearance::FromJson(**Look); }
            if (!C.Id.IsEmpty() && !C.Name.IsEmpty()) { Characters.Add(C); }
        }
    }

    void Save() const
    {
        TArray<TSharedPtr<FJsonValue>> List;
        for (const FSFCharacterSummary& C : Characters)
        {
            TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("id"), C.Id); Item->SetStringField(TEXT("name"), C.Name); Item->SetStringField(TEXT("location"), C.Location);
            Item->SetNumberField(TEXT("level"), C.Level); Item->SetObjectField(TEXT("look"), C.Look.ToJson());
            List.Add(MakeShared<FJsonValueObject>(Item));
        }
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetArrayField(TEXT("characters"), List);
        FString Text;
        FJsonSerializer::Serialize(Root, TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Text));
        IFileManager::Get().MakeDirectory(*Dir(), true);
        FFileHelper::SaveStringToFile(Text, *File());
    }

    bool NameTaken(const FString& Name) const
    {
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir(), TEXT("Dev_*.json")), true, false);
        for (const FString& Name2 : Files)
        {
            FString Text;
            if (FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir(), Name2)) && Text.Contains(FString::Printf(TEXT("\"name\": \"%s\""), *Name), ESearchCase::IgnoreCase)) { return true; }
        }
        return Characters.ContainsByPredicate([&Name](const FSFCharacterSummary& C) { return C.Name.Equals(Name, ESearchCase::IgnoreCase); });
    }

    void Seed()
    {
        FSFCharacterSummary A; A.Id = TEXT("chr_dev_seed_a"); A.Name = TEXT("Aldren"); A.Level = 18; A.Location = TEXT("Silverbrook");
        FSFCharacterSummary B; B.Id = TEXT("chr_dev_seed_b"); B.Name = TEXT("Mira"); B.Level = 7; B.Location = TEXT("Greenlands"); B.Look.Body = 1;
        Characters = {A, B};
        Save();
    }
};
}
#endif

TSharedRef<ISFFrontEndIdentity> SFMakeFrontEndIdentity()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return MakeShared<FDevIdentity>();
#else
    return MakeShared<FUnavailableIdentity>();
#endif
}
