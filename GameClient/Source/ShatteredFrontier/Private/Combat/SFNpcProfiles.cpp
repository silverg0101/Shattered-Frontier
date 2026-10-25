#include "Combat/SFNpcProfiles.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    thread_local const TMap<FString, SFNpcProfiles::FProfile>* TestCatalogue = nullptr;
#endif
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

    bool Behavior(const FJsonObject& Row, SFNpcProfiles::FBehavior& Out)
    {
        return Number(Row, TEXT("aggro_radius_metres"), 4, 18, Out.AggroRadiusMetres) &&
            Number(Row, TEXT("near_notice_radius_metres"), 1, 4, Out.NearNoticeRadiusMetres) &&
            Number(Row, TEXT("aggro_minimum_metres"), 1, 4, Out.AggroMinimumMetres) &&
            Number(Row, TEXT("aggro_maximum_bonus_metres"), 0, 6, Out.AggroMaximumBonusMetres) &&
            Out.NearNoticeRadiusMetres <= Out.AggroMinimumMetres &&
            Number(Row, TEXT("field_of_view_degrees"), 60, 360, Out.FieldOfViewDegrees) &&
            Number(Row, TEXT("assist_delay_seconds"), .5, 5, Out.AssistDelaySeconds) &&
            Number(Row, TEXT("assist_radius_metres"), 2, 15, Out.AssistRadiusMetres) &&
            Number(Row, TEXT("leash_radius_metres"), 20, 100, Out.LeashRadiusMetres) &&
            Number(Row, TEXT("wander_radius_metres"), 2, 40, Out.WanderRadiusMetres) &&
            Number(Row, TEXT("wander_speed_metres_per_second"), .5, 5, Out.WanderSpeedMetresPerSecond) &&
            Number(Row, TEXT("chase_speed_metres_per_second"), 1, 10, Out.ChaseSpeedMetresPerSecond) &&
            Number(Row, TEXT("wander_min_seconds"), 1, 10, Out.WanderMinSeconds) &&
            Number(Row, TEXT("wander_max_seconds"), Out.WanderMinSeconds, 20, Out.WanderMaxSeconds) &&
            Number(Row, TEXT("melee_range_metres"), 1, 5, Out.MeleeRangeMetres) &&
            Number(Row, TEXT("ranged_range_metres"), 5, 30, Out.RangedRangeMetres) &&
            Number(Row, TEXT("retreat_range_metres"), 2, 15, Out.RetreatRangeMetres) &&
            Number(Row, TEXT("preferred_range_metres"), Out.RetreatRangeMetres, 20, Out.PreferredRangeMetres) &&
            Out.PreferredRangeMetres < Out.RangedRangeMetres &&
            Number(Row, TEXT("ranged_swing_seconds"), 1, 4, Out.RangedSwingSeconds);
    }

    bool Presentation(const FJsonObject& Row, SFNpcProfiles::FPresentation& Out, FString& Error)
    {
        FString Mesh, Animation;
        const TArray<TSharedPtr<FJsonValue>>* Scale = nullptr;
        if (!Row.TryGetStringField(TEXT("static_mesh"), Mesh) ||
            !(Mesh.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive) || Mesh.StartsWith(TEXT("/Engine/"), ESearchCase::CaseSensitive)) ||
            Mesh.Contains(TEXT("..")) || Mesh.Contains(TEXT(":")) || !FPackageName::IsValidObjectPath(Mesh) ||
            !Row.TryGetStringField(TEXT("animation_profile"), Animation) || Animation != TEXT("none") ||
            !Row.TryGetArrayField(TEXT("body_scale"), Scale) || Scale->Num() != 3 ||
            !Number(Row, TEXT("capsule_radius_cm"), 1, 55, Out.CapsuleRadiusCm) ||
            !Number(Row, TEXT("capsule_half_height_cm"), Out.CapsuleRadiusCm, 65, Out.CapsuleHalfHeightCm) ||
            !Number(Row, TEXT("label_height_cm"), 0, 130, Out.LabelHeightCm) ||
            !Number(Row, TEXT("label_world_size_cm"), 1, 34, Out.LabelWorldSizeCm)) return false;
        double Values[3];
        for (int32 Index = 0; Index < 3; ++Index)
            if (!(*Scale)[Index]->TryGetNumber(Values[Index]) || !FMath::IsFinite(Values[Index]) || Values[Index] < .01 || Values[Index] > 1.3) return false;
        Out.StaticMeshPath = FSoftObjectPath(Mesh);
        Out.BodyScale = FVector(Values[0], Values[1], Values[2]);
        // Render assets may be stripped from dedicated cooks. Editor/client release admission must close the asset reference.
        if (!IsRunningDedicatedServer() && !Cast<UStaticMesh>(Out.StaticMeshPath.TryLoad()))
        { Error = FString::Printf(TEXT("NPC static_mesh is missing or not a StaticMesh: %s"), *Mesh); return false; }
        return true;
    }

    template<typename T, typename FRead>
    bool Presets(const FJsonObject& Root, const TCHAR* Key, TMap<FString, T>& Out, FRead Read)
    {
        const TSharedPtr<FJsonObject>* Rows = nullptr;
        if (!Root.TryGetObjectField(Key, Rows) || !Rows->IsValid() || (*Rows)->Values.IsEmpty()) return false;
        for (const auto& Pair : (*Rows)->Values)
        {
            const auto Row = Pair.Value->Type == EJson::Object ? Pair.Value->AsObject() : nullptr;
            const FString Id(*Pair.Key);
            T Preset;
            if (!Identifier(Id, TEXT("")) || !Row || !Read(*Row, Preset)) return false;
            Out.Add(Id, MoveTemp(Preset));
        }
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
        Version != 2 || !Root->TryGetArrayField(TEXT("profiles"), Rows) || Rows->IsEmpty())
    { Error = TEXT("NPC catalogue missing, empty or unsupported schema"); return false; }
    TMap<FString, FBehavior> Behaviors;
    TMap<FString, FPresentation> Presentations;
    if (!Presets(*Root, TEXT("behavior_presets"), Behaviors, Behavior) ||
        !Presets(*Root, TEXT("presentation_presets"), Presentations,
            [&Error](const FJsonObject& Row, FPresentation& OutPreset) { return Presentation(Row, OutPreset, Error); }))
    { if (Error.IsEmpty()) Error = TEXT("Invalid NPC behavior or presentation presets"); return false; }
    TMap<FString, FProfile> Candidate;
    for (const auto& Value : *Rows)
    {
        const auto Row = Value->Type == EJson::Object ? Value->AsObject() : nullptr;
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
            !Row->TryGetStringField(TEXT("respawn_tier"), Profile.RespawnTier) ||
            (Profile.RespawnTier != TEXT("common") && Profile.RespawnTier != TEXT("elite") && Profile.RespawnTier != TEXT("mini_boss")) ||
            !Row->TryGetStringField(TEXT("attack_kind"), Attack) || (Attack != TEXT("melee") && Attack != TEXT("ranged")) ||
            !Row->TryGetArrayField(TEXT("abilities"), Abilities) ||
            !Row->TryGetBoolField(TEXT("local_hides_fixture"), Profile.bLocalHidesFixture) ||
            !Row->TryGetStringField(TEXT("loot_rule"), Rule) || !Identifier(Rule, TEXT("")))
        { Error = FString::Printf(TEXT("Invalid or duplicate NPC profile: %s"), *Profile.DefinitionId); return false; }
        if (!Row->TryGetStringField(TEXT("behavior_preset"), Profile.BehaviorPresetId) || !Behaviors.Contains(Profile.BehaviorPresetId) ||
            !Row->TryGetStringField(TEXT("presentation_preset"), Profile.PresentationPresetId) || !Presentations.Contains(Profile.PresentationPresetId))
        { Error = FString::Printf(TEXT("Unknown NPC preset reference: %s"), *Profile.DefinitionId); return false; }
        Profile.Behavior = Behaviors[Profile.BehaviorPresetId];
        Profile.Presentation = Presentations[Profile.PresentationPresetId];
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
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (TestCatalogue) return TestCatalogue->Find(DefinitionId);
#endif
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

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
SFNpcProfiles::FScopedTestCatalogue::FScopedTestCatalogue(const TMap<FString, FProfile>& ValidatedProfiles)
    : Previous(TestCatalogue)
{
    check(IsInGameThread());
    TestCatalogue = &ValidatedProfiles;
}

SFNpcProfiles::FScopedTestCatalogue::~FScopedTestCatalogue()
{
    check(IsInGameThread());
    TestCatalogue = Previous;
}
#endif
