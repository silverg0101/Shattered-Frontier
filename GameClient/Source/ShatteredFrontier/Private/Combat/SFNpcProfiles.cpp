#include "Combat/SFNpcProfiles.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool ReadObject(const FString& Path, TSharedPtr<FJsonObject>& Object)
    {
        FString Json;
        return FFileHelper::LoadFileToString(Json, *Path) &&
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object) && Object.IsValid();
    }

    bool Number(const FJsonObject& Object, const TCHAR* Key, double Min, double Max, double& Out)
    {
        return Object.TryGetNumberField(Key, Out) && FMath::IsFinite(Out) && Out >= Min && Out <= Max;
    }

    bool Integer(const FJsonObject& Object, const TCHAR* Key, int32 Min, int32 Max, int32& Out)
    {
        double Value;
        if (!Number(Object, Key, Min, Max, Value) || FMath::FloorToDouble(Value) != Value) return false;
        Out = static_cast<int32>(Value);
        return true;
    }

    bool Identifier(const FString& Id, const TCHAR* Prefix)
    {
        if ((FCString::Strlen(Prefix) > 0 && !Id.StartsWith(Prefix, ESearchCase::CaseSensitive)) || Id.Len() <= FCString::Strlen(Prefix)) return false;
        for (const TCHAR C : Id.RightChop(FCString::Strlen(Prefix)))
            if (!(C >= 'a' && C <= 'z') && !(C >= '0' && C <= '9') && C != '_') return false;
        return true;
    }
}

bool SFNpcProfiles::Load(const FString& CataloguePath, const FString& LootRulesDirectory,
    TMap<FString, FProfile>& Out, FString& Error)
{
    TSharedPtr<FJsonObject> Root;
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    double Version;
    Error.Empty();
    if (!ReadObject(CataloguePath, Root) || !Root->TryGetNumberField(TEXT("schema_version"), Version) ||
        Version != 1 || !Root->TryGetArrayField(TEXT("profiles"), Rows) || Rows->IsEmpty())
    { Error = TEXT("NPC catalogue missing, empty or unsupported schema"); return false; }
    TMap<FString, FProfile> Candidate;
    for (const auto& Value : *Rows)
    {
        const auto Row = Value->AsObject();
        FProfile Profile;
        FString Attack, Rule;
        const TArray<TSharedPtr<FJsonValue>>* Abilities = nullptr;
        // Admission envelope is bounded by the six shipped profiles; widening needs a reviewed content/code release.
        if (!Row.IsValid() || !Row->TryGetStringField(TEXT("definition_id"), Profile.DefinitionId) ||
            !Identifier(Profile.DefinitionId, TEXT("npc/")) || Candidate.Contains(Profile.DefinitionId) ||
            !Integer(*Row, TEXT("level"), 1, 7, Profile.Stats.Level) ||
            !Integer(*Row, TEXT("health"), 1, 3558, Profile.Stats.Health) ||
            !Integer(*Row, TEXT("skill"), 0, 194, Profile.Stats.Skill) ||
            !Number(*Row, TEXT("swing_seconds"), 1.6, 2.0, Profile.Stats.SwingSeconds) ||
            !Number(*Row, TEXT("swing_damage"), 0, 95.5, Profile.Stats.SwingDamage) ||
            !Number(*Row, TEXT("armor"), 0, 378, Profile.Stats.Armor) ||
            !Number(*Row, TEXT("resistance"), 0, 18.9, Profile.Stats.Resistance) ||
            !Number(*Row, TEXT("crit_chance"), 0, 1, Profile.Stats.CritChance) ||
            !Number(*Row, TEXT("crit_damage"), 1, 1.5, Profile.Stats.CritDamage) ||
            !Row->TryGetStringField(TEXT("attack_kind"), Attack) || (Attack != TEXT("melee") && Attack != TEXT("ranged")) ||
            !Row->TryGetArrayField(TEXT("abilities"), Abilities) ||
            !Row->TryGetBoolField(TEXT("local_hides_fixture"), Profile.bLocalHidesFixture) ||
            !Row->TryGetStringField(TEXT("loot_rule"), Rule) || !Identifier(Rule, TEXT("")))
        { Error = FString::Printf(TEXT("Invalid or duplicate NPC profile: %s"), *Profile.DefinitionId); return false; }
        Profile.AttackKind = Attack == TEXT("ranged") ? EAttackKind::Ranged : EAttackKind::Melee;
        for (const auto& AbilityValue : *Abilities)
        {
            FString Ability;
            if (!AbilityValue->TryGetString(Ability) || Ability != TEXT("mend") || Profile.Abilities.Contains(FName(*Ability)))
            { Error = TEXT("Unsupported or duplicate NPC ability"); return false; }
            Profile.Abilities.Add(FName(*Ability));
        }
        TSharedPtr<FJsonObject> Loot;
        FString LootCreature;
        if (!ReadObject(FPaths::Combine(LootRulesDirectory, Rule + TEXT(".json")), Loot) ||
            !Loot->TryGetStringField(TEXT("creature_id"), LootCreature) || LootCreature != Profile.DefinitionId ||
            !Loot->TryGetStringField(TEXT("table_id"), Profile.LootTableId) || !Identifier(Profile.LootTableId, TEXT("loot/")) ||
            !Loot->TryGetBoolField(TEXT("unique_or_boss"), Profile.bSingleDrop))
        { Error = FString::Printf(TEXT("Invalid canonical loot rule for %s"), *Profile.DefinitionId); return false; }
        const FString DefinitionId = Profile.DefinitionId;
        Candidate.Add(DefinitionId, MoveTemp(Profile));
    }
    Out = MoveTemp(Candidate);
    return true;
}

const SFNpcProfiles::FProfile* SFNpcProfiles::Find(const FString& DefinitionId)
{
    static const TMap<FString, FProfile> Catalogue = []
    {
        TMap<FString, FProfile> Profiles;
        FString Error;
        const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/creatures"));
        if (!Load(FPaths::Combine(Directory, TEXT("profiles.json")),
            FPaths::Combine(Directory, TEXT("loot_rules")), Profiles, Error))
        { UE_LOG(LogTemp, Error, TEXT("NPC_PROFILES_REJECTED %s"), *Error); }
        return Profiles;
    }();
    return Catalogue.Find(DefinitionId);
}
