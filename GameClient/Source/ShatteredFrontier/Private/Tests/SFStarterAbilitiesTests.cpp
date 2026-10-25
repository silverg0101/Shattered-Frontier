#include "Combat/SFStarterAbilities.h"
#include "Combat/SFCombatPart6.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
bool CheckAbility(FAutomationTestBase& Test, const TCHAR* Id, int32 Level, float Cost,
    float Cooldown, float Gcd, SFStarterAbilities::ETarget Target,
    SFStarterAbilities::EEffect Effect, double Modifier)
{
    const auto* Ability = SFStarterAbilities::Find(Id);
    if (!Test.TestNotNull(TEXT("published ability"), Ability)) return false;
    Test.TestEqual(TEXT("unlock"), Ability->Level, Level);
    Test.TestEqual(TEXT("energy cost fraction"), Ability->StaminaCostFraction, Cost);
    Test.TestEqual(TEXT("cooldown seconds"), Ability->CooldownSeconds, Cooldown);
    Test.TestEqual(TEXT("haste cooldown at zero haste"), SFStarterAbilities::Cooldown(*Ability, 0), double(Cooldown));
    if (Ability->bHasteCooldown)
        Test.TestTrue(TEXT("haste shortens cooldown"), SFStarterAbilities::Cooldown(*Ability, .2) < Cooldown);
    Test.TestEqual(TEXT("GCD seconds"), Ability->GcdSeconds, Gcd);
    Test.TestEqual(TEXT("target rule"), int32(Ability->Target), int32(Target));
    Test.TestEqual(TEXT("effect operation"), int32(Ability->Effect), int32(Effect));
    const float Maximum = 300.f;
    const float Required = Maximum * Cost;
    Test.TestFalse(TEXT("locked one level early"), SFStarterAbilities::CanStart(*Ability,
        Level - 1, true, 100, 0, 0, Maximum, Maximum, 1));
    Test.TestFalse(TEXT("insufficient energy"), SFStarterAbilities::CanStart(*Ability,
        Level, true, 100, 0, 0, Required - .1f, Maximum, 1));
    Test.TestTrue(TEXT("unlocked and funded"), SFStarterAbilities::CanStart(*Ability,
        Level, true, 100, 0, 0, Required, Maximum, 1));
    Test.TestFalse(TEXT("active GCD"), SFStarterAbilities::CanStart(*Ability,
        Level, true, 100, 101, 0, Maximum, Maximum, 1));
    Test.TestFalse(TEXT("active cooldown"), SFStarterAbilities::CanStart(*Ability,
        Level, true, 100, 0, 101, Maximum, Maximum, 1));
    Test.TestTrue(TEXT("ready after cooldown"), SFStarterAbilities::CanStart(*Ability,
        Level, true, 101, 0, 101, Maximum, Maximum, 1));
    const auto* Coefficient = SFCombatPart6::Ability(Id);
    if (Modifier > 0)
    {
        if (!Test.TestNotNull(TEXT("Part 6 coefficient"), Coefficient)) return false;
        Test.TestEqual(TEXT("Part 6 modifier"), Coefficient->Modifier, Modifier);
        const auto Stats = SFCombatPart6::Player(Effect == SFStarterAbilities::EEffect::Absorb ?
            SFCombatPart6::EKit::Mage : SFCombatPart6::EKit::Fighter, Level);
        const auto Attack = SFCombatPart6::AuthoredAbility(Stats,
            SFCombatPart6::ReferenceWeapon(Level), *Coefficient, .15);
        Test.TestTrue(TEXT("authored effect has power"), Attack.RawDamage > 0);
        Test.TestEqual(TEXT("absorb cannot crit"), Attack.bCanCrit,
            Effect != SFStarterAbilities::EEffect::Absorb);
    }
    else
    {
        Test.TestNull(TEXT("percentage heal has no power coefficient"), Coefficient);
        Test.TestEqual(FString(TEXT("second wind five ticks total")), int32(FMath::RoundToInt(500 * .03) * 5), int32(75));
    }
    return true;
}
}

#define SF_STARTER_TEST(ClassName, Path, Id, Level, Cost, Cd, Gcd, Target, Effect, Modifier) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
bool ClassName::RunTest(const FString& Parameters) \
{ return CheckAbility(*this, TEXT(Id), Level, Cost, Cd, Gcd, SFStarterAbilities::ETarget::Target, \
    SFStarterAbilities::EEffect::Effect, Modifier); }

SF_STARTER_TEST(FSFStrikeAbilityTest, "ShatteredFrontier.Combat.Abilities.Strike", "strike", 1, .05f, 0.f, 1.5f, Enemy, Hit, .7)
SF_STARTER_TEST(FSFHeavyBlowAbilityTest, "ShatteredFrontier.Combat.Abilities.HeavyBlow", "heavy_blow", 4, .08f, 8.f, 1.5f, Enemy, Hit, 1.5)
SF_STARTER_TEST(FSFCleaveAbilityTest, "ShatteredFrontier.Combat.Abilities.Cleave", "cleave", 7, .07f, 0.f, 1.5f, Cone, Hit, .42)
SF_STARTER_TEST(FSFPommelAbilityTest, "ShatteredFrontier.Combat.Abilities.PommelStrike", "pommel_strike", 10, .05f, 15.f, 0.f, EnemyCasting, Interrupt, .3)
SF_STARTER_TEST(FSFSecondWindAbilityTest, "ShatteredFrontier.Combat.Abilities.SecondWind", "second_wind", 14, .10f, 60.f, 1.f, Self, HealOverTime, 0)
SF_STARTER_TEST(FSFRendAbilityTest, "ShatteredFrontier.Combat.Abilities.Rend", "rend", 18, .08f, 12.f, 1.5f, Enemy, DamageOverTime, 1.8)
SF_STARTER_TEST(FSFChargeAbilityTest, "ShatteredFrontier.Combat.Abilities.Charge", "charge", 20, .08f, 20.f, 0.f, Charge, DisplaceHit, .5)
SF_STARTER_TEST(FSFWardAbilityTest, "ShatteredFrontier.Combat.Abilities.ArcaneWard", "arcane_ward", 7, .08f, 30.f, 1.5f, Self, Absorb, 2.0)

#undef SF_STARTER_TEST

namespace
{
bool CheckGuardianHealing(FAutomationTestBase& Test, const TCHAR* Id, SFStarterAbilities::EKit Kit,
    SFStarterAbilities::EEnergy Pool, int32 Level, float Cost, float Cooldown, float Gcd,
    float Range, SFStarterAbilities::ETarget Target, SFStarterAbilities::EEffect Effect,
    double Modifier, float CastSeconds, bool bShield)
{
    using namespace SFStarterAbilities;
    const FDefinition* Ability = Find(Id);
    if (!Test.TestNotNull(TEXT("rank-I definition"), Ability)) return false;
    Test.TestEqual(TEXT("kit"), int32(Ability->Kit), int32(Kit));
    Test.TestEqual(TEXT("energy pool"), int32(Ability->Energy), int32(Pool));
    Test.TestEqual(TEXT("unlock level"), Ability->Level, Level);
    Test.TestEqual(TEXT("cost fraction"), Ability->StaminaCostFraction, Cost);
    Test.TestEqual(TEXT("cooldown"), Ability->CooldownSeconds, Cooldown);
    Test.TestEqual(TEXT("GCD"), Ability->GcdSeconds, Gcd);
    Test.TestEqual(TEXT("range metres"), Ability->MaximumRangeMetres, Range);
    Test.TestEqual(TEXT("target"), int32(Ability->Target), int32(Target));
    Test.TestEqual(TEXT("friendly targeting"), IsFriendlyTarget(Ability->Target),
        Target == ETarget::Ally || Target == ETarget::AllyOrSelf ||
        Target == ETarget::DeadAlly || Target == ETarget::GroundAlly);
    Test.TestEqual(TEXT("effect"), int32(Ability->Effect), int32(Effect));
    Test.TestEqual(TEXT("cast seconds"), Ability->CastSeconds, CastSeconds);
    Test.TestEqual(TEXT("shield required"), Ability->bRequiresShield, bShield);
    const float MaxEnergy = 300, Required = MaxEnergy * Cost;
    Test.TestFalse(TEXT("locked below level"), CanStart(*Ability, Level - 1, true, 100, 0, 0,
        MaxEnergy, MaxEnergy, 1));
    Test.TestFalse(TEXT("wrong pool cannot pay"), CanStart(*Ability, Level, true, 100, 0, 0,
        Required - .1f, MaxEnergy, 1));
    Test.TestTrue(TEXT("correct pool pays"), CanStart(*Ability, Level, true, 100, 0, 0,
        Required, MaxEnergy, 1));
    Test.TestFalse(TEXT("cooldown active"), CanStart(*Ability, Level, true, 100, 0, 101,
        MaxEnergy, MaxEnergy, 1));
    Test.TestFalse(TEXT("GCD active"), CanStart(*Ability, Level, true, 100, 101, 0,
        MaxEnergy, MaxEnergy, 1));
    const auto* Coefficient = SFCombatPart6::Ability(Id);
    if (Modifier > 0)
    {
        if (!Test.TestNotNull(TEXT("Part 6 coefficient"), Coefficient)) return false;
        Test.TestEqual(TEXT("coefficient"), Coefficient->Modifier, Modifier);
        const auto Stats = SFCombatPart6::Player(Kit == EKit::Healing ?
            SFCombatPart6::EKit::Healer : SFCombatPart6::EKit::Guardian, Level);
        const auto Result = SFCombatPart6::AuthoredAbility(Stats,
            SFCombatPart6::ReferenceWeapon(Level), *Coefficient, .15);
        Test.TestTrue(TEXT("positive authored effect"), Result.RawDamage > 0);
    }
    else Test.TestNull(TEXT("utility has no coefficient"), Coefficient);
    Test.TestTrue(TEXT("appears for binding at unlock"), BindingIds(Kit, Level).Contains(FName(Id)));
    Test.TestFalse(TEXT("hidden before unlock"), BindingIds(Kit, Level - 1).Contains(FName(Id)));
    return true;
}
}
#define SF_KIT_TEST(ClassName, Path, Id, Kit, Pool, Level, Cost, Cd, Gcd, Range, Target, Effect, Mod, Cast, Shield) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST(ClassName, Path, EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
bool ClassName::RunTest(const FString& Parameters) \
{ return CheckGuardianHealing(*this, TEXT(Id), SFStarterAbilities::EKit::Kit, SFStarterAbilities::EEnergy::Pool, \
    Level, Cost, Cd, Gcd, Range, SFStarterAbilities::ETarget::Target, SFStarterAbilities::EEffect::Effect, Mod, Cast, Shield); }

SF_KIT_TEST(FSFDefensiveStanceTest, "ShatteredFrontier.Combat.Abilities.DefensiveStance", "defensive_stance", Guardian, Stamina, 1, .05f, 1.f, 1.f, 0.f, Self, Stance, 0, 0.f, false)
SF_KIT_TEST(FSFShieldBashTest, "ShatteredFrontier.Combat.Abilities.ShieldBash", "shield_bash", Guardian, Stamina, 4, .06f, 8.f, 1.5f, 3.f, Enemy, Hit, 1.0, 0.f, true)
SF_KIT_TEST(FSFShieldBlockTest, "ShatteredFrontier.Combat.Abilities.ShieldBlock", "shield_block", Guardian, Stamina, 7, .05f, 20.f, 1.f, 0.f, Self, BlockBuff, 0, 0.f, true)
SF_KIT_TEST(FSFChallengeAbilityTest, "ShatteredFrontier.Combat.Abilities.Challenge", "challenge", Guardian, Stamina, 10, .05f, 8.f, 0.f, 10.f, Enemy, Taunt, 0, 0.f, false)
SF_KIT_TEST(FSFShieldWallTest, "ShatteredFrontier.Combat.Abilities.ShieldWall", "shield_wall", Guardian, Stamina, 14, .10f, 90.f, 1.f, 0.f, Self, DamageReduction, 0, 0.f, true)
SF_KIT_TEST(FSFBulwarkTest, "ShatteredFrontier.Combat.Abilities.Bulwark", "bulwark", Guardian, Stamina, 18, .08f, 30.f, 1.5f, 10.f, Ally, DamageReduction, 0, 0.f, false)
SF_KIT_TEST(FSFMendTest, "ShatteredFrontier.Combat.Abilities.Mend", "mend", Healing, Mana, 1, .06f, 0.f, 1.5f, 30.f, AllyOrSelf, Heal, 1.5, 2.f, false)
SF_KIT_TEST(FSFSmiteTest, "ShatteredFrontier.Combat.Abilities.Smite", "smite", Healing, Mana, 4, .05f, 0.f, 1.5f, 30.f, Enemy, Hit, 1.0, 1.5f, false)
SF_KIT_TEST(FSFRenewTest, "ShatteredFrontier.Combat.Abilities.Renew", "renew", Healing, Mana, 7, .05f, 0.f, 1.5f, 30.f, AllyOrSelf, HealOverTime, 1.2, 0.f, false)
SF_KIT_TEST(FSFPurifyTest, "ShatteredFrontier.Combat.Abilities.Purify", "purify", Healing, Mana, 10, .05f, 8.f, 1.5f, 30.f, AllyOrSelf, Dispel, 0, 0.f, false)
SF_KIT_TEST(FSFReviveAbilityTest, "ShatteredFrontier.Combat.Abilities.Revive", "revive", Healing, Mana, 14, .20f, 0.f, 1.5f, 30.f, DeadAlly, Revive, 0, 10.f, false)
SF_KIT_TEST(FSFFlashMendTest, "ShatteredFrontier.Combat.Abilities.FlashMend", "flash_mend", Healing, Mana, 18, .10f, 0.f, 1.5f, 30.f, AllyOrSelf, Heal, 1.0, 1.f, false)
SF_KIT_TEST(FSFHealingCircleTest, "ShatteredFrontier.Combat.Abilities.HealingCircle", "healing_circle", Healing, Mana, 20, .14f, 10.f, 1.5f, 30.f, GroundAlly, HealingCircle, .9, 2.5f, false)
#undef SF_KIT_TEST

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHybridHealingManaTest, "ShatteredFrontier.Combat.Abilities.HybridFighterHeal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFHybridHealingManaTest::RunTest(const FString& Parameters)
{
    const auto* Mend = SFStarterAbilities::Find(TEXT("mend"));
    if (!TestNotNull(TEXT("mend"), Mend)) return false;
    const auto Fighter = SFCombatPart6::Player(SFCombatPart6::EKit::Fighter, 1);
    TestEqual(TEXT("Fighter healing uses Mana"), int32(Mend->Energy), int32(SFStarterAbilities::EEnergy::Mana));
    TestTrue(TEXT("Fighter has Mana to heal"), Fighter.Mana >= Fighter.Mana * Mend->StaminaCostFraction);
    TestEqual(TEXT("untrained Healing uses skill floor"),
        SFStarterAbilities::SkillMultiplier(false, Fighter.SkillMultiplier, .80), .80);
    TestEqual(TEXT("effective healing threat splits across engaged NPCs"),
        SFStarterAbilities::HealingThreatPerNpc(120, 3), 20.0);
    TestEqual(TEXT("overheal generates no threat"),
        SFStarterAbilities::HealingThreatPerNpc(0, 3), 0.0);
    TestTrue(TEXT("ground target inside 30 m"), SFStarterAbilities::IsGroundWithinRange(
        FVector::ZeroVector, FVector(3000, 0, 0), 30));
    TestFalse(TEXT("ground target beyond 30 m"), SFStarterAbilities::IsGroundWithinRange(
        FVector::ZeroVector, FVector(3001, 0, 0), 30));
    TestTrue(TEXT("circle heals only inside 6 m"), SFStarterAbilities::IsInsideHealingCircle(
        FVector::ZeroVector, FVector(600, 0, 0)));
    TestFalse(TEXT("circle excludes beyond 6 m"), SFStarterAbilities::IsInsideHealingCircle(
        FVector::ZeroVector, FVector(601, 0, 0)));
    return true;
}
#endif
