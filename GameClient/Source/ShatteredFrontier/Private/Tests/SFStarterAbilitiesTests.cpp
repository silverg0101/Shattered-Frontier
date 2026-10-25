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
#endif
