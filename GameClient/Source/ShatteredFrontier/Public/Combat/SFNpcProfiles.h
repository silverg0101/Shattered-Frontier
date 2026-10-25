#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

/** Immutable release content. Operators cannot edit formulas or individual profiles live. */
namespace SFNpcProfiles
{
    enum class EAttackKind : uint8 { Melee, Ranged };
    struct FStats
    {
        int32 Level = 0, Health = 0, Skill = 0;
        double SwingSeconds = 0, SwingDamage = 0, Armor = 0, Resistance = 0,
            CritChance = 0, CritDamage = 0;
    };
    /** Species inputs only. Threat/combat algorithms and realm caps remain shared code/settings. */
    struct FBehavior
    {
        double AggroRadiusMetres = 0, NearNoticeRadiusMetres = 0, AggroMinimumMetres = 0,
            AggroMaximumBonusMetres = 0, FieldOfViewDegrees = 0;
        double AssistDelaySeconds = 0, AssistRadiusMetres = 0, LeashRadiusMetres = 0,
            WanderRadiusMetres = 0, WanderSpeedMetresPerSecond = 0, ChaseSpeedMetresPerSecond = 0,
            WanderMinSeconds = 0, WanderMaxSeconds = 0;
        double MeleeRangeMetres = 0, RangedRangeMetres = 0, RetreatRangeMetres = 0,
            PreferredRangeMetres = 0, RangedSwingSeconds = 0;
    };
    // The current body is static and consumes no animation asset. Future animation modes need a compatible release.
    enum class EAnimationProfile : uint8 { None };
    struct FPresentation
    {
        FSoftObjectPath StaticMeshPath;
        FVector BodyScale = FVector::ZeroVector;
        double CapsuleRadiusCm = 0, CapsuleHalfHeightCm = 0, LabelHeightCm = 0, LabelWorldSizeCm = 0;
        EAnimationProfile AnimationProfile = EAnimationProfile::None;
    };
    struct FProfile
    {
        FString DefinitionId, LootTableId, RespawnTier, BehaviorPresetId, PresentationPresetId;
        FStats Stats;
        FBehavior Behavior;
        FPresentation Presentation;
        EAttackKind AttackKind = EAttackKind::Melee;
        TSet<FName> Abilities;
        bool bSingleDrop = false;
        bool bLocalHidesFixture = false;
        bool IsRanged() const { return AttackKind == EAttackKind::Ranged; }
        bool CanMend() const { return Abilities.Contains(TEXT("mend")); }
    };

    /** All-or-nothing validation; a malformed catalogue never partially replaces a snapshot. */
    SHATTEREDFRONTIER_API bool Load(const FString& CataloguePath, const FString& LootRulesDirectory,
        TMap<FString, FProfile>& Out, FString& Error);
    /** One restart-bound snapshot, sourced from staged repository content. Unknown IDs return null. */
    SHATTEREDFRONTIER_API const FProfile* Find(const FString& DefinitionId);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    /** Editor automation only: isolate actor/component tests using an already validated catalogue. */
    class SHATTEREDFRONTIER_API FScopedTestCatalogue
    {
    public:
        explicit FScopedTestCatalogue(const TMap<FString, FProfile>& ValidatedProfiles);
        ~FScopedTestCatalogue();
        FScopedTestCatalogue(const FScopedTestCatalogue&) = delete;
        FScopedTestCatalogue& operator=(const FScopedTestCatalogue&) = delete;
    private:
        const TMap<FString, FProfile>* Previous;
    };
#endif
}
