#pragma once

#include "CoreMinimal.h"

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
    struct FProfile
    {
        FString DefinitionId, LootTableId, RespawnTier;
        FStats Stats;
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
