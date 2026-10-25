#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCombatPilotRewards.h"
#include "Combat/SFAutoAttackRules.h"
#include "Combat/SFBoarDodgeRules.h"
#include "Combat/SFCombatResolution.h"
#include "Combat/SFCombatPart6.h"
#include "Characters/SFE1Character.h"
#include "Services/SFGameplayServicesClient.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFArcherRangedAttackTest,
    "ShatteredFrontier.Combat.Pilot.ArcherRangedAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFArcherRangedAttackTest::RunTest(const FString& Parameters)
{
    const auto* Tune = GetDefault<USFCombatPilotSettings>();
    TestTrue(TEXT("In-range archer shot admitted"), SFCombatPilotRules::CanRanged(1500, Tune->ArcherRangeMetres * 100));
    TestFalse(TEXT("Out-of-range archer shot refused"), SFCombatPilotRules::CanRanged(3100, Tune->ArcherRangeMetres * 100));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    AActor* Archer = World->SpawnActor<AActor>();
    AActor* Victim = World->SpawnActor<AActor>();
    AActor* Wall = World->SpawnActor<AActor>();
    if (!Archer || !Victim || !Wall) return false;
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(50, 150, 150));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Box->RegisterComponent();
    const FVector Start(0, 0, 80), End(1500, 0, 60);
    Wall->SetActorLocation(FVector(750, 500, 70));
    TestTrue(TEXT("Clear archer shot has LOS"), SFCombatPilotRules::HasRangedLineOfSight(World, Archer, Victim, Start, End));
    Wall->SetActorLocation(FVector(750, 0, 70));
    TestFalse(TEXT("World geometry blocks archer shot"), SFCombatPilotRules::HasRangedLineOfSight(World, Archer, Victim, Start, End));
    Wall->SetActorLocation(FVector(750, 500, 70));

    SFCombatResolution::FAttack Attack;
    Attack.RawDamage = 100;
    Attack.AttackerLevel = 5;
    Attack.bHasArmorSnapshot = true;
    Attack.ArmorKLevel = Tune->ArmorKLevel;
    Attack.ArmorReductionCap = Tune->ArmorReductionCap;
    SFCombatResolution::FDraws Draws{.5, .5, .5, .5, .5};
    const int32 Unarmored = SFCombatResolution::Resolve(Attack, Draws).Damage;
    TestTrue(TEXT("In-range clear shot resolves a hit"),
        SFCombatPilotRules::CanRanged(1500, Tune->ArcherRangeMetres * 100) &&
        SFCombatPilotRules::HasRangedLineOfSight(World, Archer, Victim, Start, End) && Unarmored > 0);
    Attack.Armor = 300;
    const int32 Armored = SFCombatResolution::Resolve(Attack, Draws).Damage;
    TestTrue(TEXT("Ranged physical FAttack applies defender armor"), Armored > 0 && Armored < Unarmored);
    Attack.bFrontArc = true;
    Attack.bCanDodge = true;
    Attack.DodgeChance = .25;
    Draws.Avoid = .1;
    TestEqual(TEXT("Ranged physical FAttack can dodge before mitigation"),
        SFCombatResolution::Resolve(Attack, Draws).Outcome, SFCombatResolution::EOutcome::Dodge);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHostileDefinitionsTest,
    "ShatteredFrontier.Combat.Pilot.HostileDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFHostileDefinitionsTest::RunTest(const FString& Parameters)
{
    struct FExpected { const TCHAR* Creature; const TCHAR* Table; int32 Level; int32 Health; double SwingDamage; bool bSingle; };
    const FExpected Rows[] = {
        {TEXT("npc/wolf"), TEXT("loot/greenlands_silverbrook_wolf"), 6, 667, 32.0, false},
        {TEXT("npc/bandit_thug"), TEXT("loot/greenlands_silverbrook_bandit_thug"), 5, 623, 37.4, false},
        {TEXT("npc/bandit_archer"), TEXT("loot/greenlands_silverbrook_bandit_archer"), 5, 623, 34.9, false},
        {TEXT("npc/wolf_alpha"), TEXT("loot/unique_greenlands_wolf_alpha"), 7, 1779, 54.0, true},
        {TEXT("npc/bandit_captain"), TEXT("loot/boss_greenlands_bandit_captain"), 7, 3558, 95.5, true}
    };
    for (const FExpected& Row : Rows)
    {
        FString Table;
        bool bSingle = false;
        TestTrue(FString::Printf(TEXT("%s has a loot binding"), Row.Creature),
            ASFPilotBoar::ResolveCreatureLoot(Row.Creature, Table, bSingle));
        TestEqual(TEXT("Bound table"), Table, FString(Row.Table));
        TestEqual(TEXT("Shared single drop"), bSingle, Row.bSingle);
        const auto Stats = SFCombatPart6::Npc(Row.Creature);
        TestEqual(TEXT("Part 6 level"), Stats.Level, Row.Level);
        TestEqual(TEXT("Part 6 health"), Stats.Health, Row.Health);
        TestEqual(TEXT("Part 6 swing"), Stats.SwingDamage, Row.SwingDamage);
    }
    FString UnknownTable;
    bool bUnknownSingle = false;
    TestFalse(TEXT("Unknown creature cannot spawn"),
        ASFPilotBoar::ResolveCreatureLoot(TEXT("npc/unknown"), UnknownTable, bUnknownSingle));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatResolutionTest,
    "ShatteredFrontier.Combat.Resolution.OrderAndValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatResolutionTest::RunTest(const FString& Parameters)
{
    using namespace SFCombatResolution;
    FRandomStream Stream(9001), Expected(9001);
    const FDraws Seeded = Draw(Stream);
    TestEqual(TEXT("Hit draw first"), Seeded.Hit, double(Expected.FRand()));
    TestEqual(TEXT("Avoidance draw second"), Seeded.Avoid, double(Expected.FRand()));
    TestEqual(TEXT("Crit draw third"), Seeded.Crit, double(Expected.FRand()));
    TestEqual(TEXT("Block draw fourth"), Seeded.Block, double(Expected.FRand()));
    TestEqual(TEXT("Spread draw fifth"), Seeded.Spread, double(Expected.FRand()));
    TestEqual(TEXT("Exactly five draws consumed"), double(Stream.FRand()), double(Expected.FRand()));

    FAttack Attack;
    Attack.RawDamage = 100;
    Attack.bFrontArc = true;
    Attack.bCanMiss = Attack.bCanDodge = Attack.bCanParry = Attack.bCanCrit = Attack.bCanBlock = true;
    Attack.MissChance = .1; Attack.DodgeChance = .3; Attack.ParryChance = .3;
    Attack.AvoidanceCap = GetDefault<USFCombatPilotSettings>()->AvoidanceCap;
    Attack.CritChance = .2; Attack.CritDamage = 2;
    Attack.BlockChance = .5; Attack.BlockReduction = .3;
    const auto* Settings = GetDefault<USFCombatPilotSettings>();
    Attack.ArmorKLevel = Settings->ArmorKLevel; Attack.ArmorReductionCap = Settings->ArmorReductionCap;
    Attack.ResistK = Settings->ResistK; Attack.ResistReductionCap = Settings->ResistReductionCap;
    Attack.ArmorPenetrationCap = Settings->ArmorPenetrationCap;
    Attack.MagicPenetrationCap = Settings->MagicPenetrationCap;
    Attack.bHasArmorSnapshot = true; Attack.Armor = 100; Attack.AttackerLevel = 1;
    FDraws D{.05, .1, .1, .1, .5};
    TestEqual(TEXT("Miss precedes all later outcomes"), Resolve(Attack, D).Outcome, EOutcome::Miss);
    D.Hit = .5;
    TestEqual(TEXT("Scaled dodge precedes parry"), Resolve(Attack, D).Outcome, EOutcome::Dodge);
    D.Avoid = .3;
    TestEqual(TEXT("Parry uses the same avoidance draw"), Resolve(Attack, D).Outcome, EOutcome::Parry);
    D.Avoid = .9;
    const FResult CritBlock = Resolve(Attack, D);
    TestEqual(TEXT("Crit then armor then block"), CritBlock.Outcome, EOutcome::CritBlock);
    TestEqual(TEXT("200 raw, 50 percent armor, 30 percent block"), CritBlock.Damage, 70);
    Attack.bPhysical = false; Attack.bSpell = true; Attack.bFrontArc = false;
    Attack.bHasResistanceSnapshot = true; Attack.Resistance = 5;
    D.Hit = .05;
    TestEqual(TEXT("Spell miss is resist"), Resolve(Attack, D).Outcome, EOutcome::Resist);
    D.Hit = .5; D.Crit = .9; D.Block = .9;
    TestEqual(TEXT("Spell resistance uses resist divisor"), Resolve(Attack, D).Damage, 50);
    Attack.MagicPenetrationFraction = 0.60;
    TestEqual(TEXT("Magic penetration uses the design cap"), Resolve(Attack, D).Damage, 71);
    Attack.MagicPenetrationFraction = 0;
    Attack.Absorb = 100;
    TestEqual(TEXT("Full absorb has its own outcome"), Resolve(Attack, D).Outcome, EOutcome::Absorb);
    FAttack Unbound;
    Unbound.RawDamage = 2.5;
    Unbound.bFrontArc = true;
    Unbound.DodgeChance = 1;
    Unbound.CritChance = 1;
    Unbound.Armor = 10000;
    Unbound.AttackerLevel = 1;
    TestEqual(TEXT("Unavailable outcomes and armor snapshot stay off; half rounds up"),
        Resolve(Unbound, FDraws{}).Damage, 3);
    Unbound.RawDamage = 0.1;
    TestEqual(TEXT("Nonzero unabsorbed hit remains at least one"), Resolve(Unbound, FDraws{}).Damage, 1);
    Unbound.RawDamage = 100; Unbound.bHasSpread = true;
    Unbound.SpreadWeaponDamage = 40; Unbound.SpreadMinimum = .85; Unbound.SpreadMaximum = 1.15;
    FDraws SpreadDraws; SpreadDraws.Spread = 1;
    TestEqual(TEXT("Spread changes weapon portion, not power portion"), Resolve(Unbound, SpreadDraws).Damage, 106);
    Unbound.bHasSpread = false; Unbound.RawDamage = 14; Unbound.PilotFlatArmor = 5;
    TestEqual(TEXT("Starter gear flat armor preserves boar damage"), Resolve(Unbound, FDraws{}).Damage, 9);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatPart6Test,
    "ShatteredFrontier.Combat.Resolution.Part6OutcomesAndFight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatPart6Test::RunTest(const FString& Parameters)
{
    using namespace SFCombatPart6;
    using namespace SFCombatResolution;
    const auto* Tune = GetDefault<USFCombatPilotSettings>();
    FString Error;
    TestTrue(TEXT("Part 6 controls stay within bounds"), Tune->Validate(Error));
    const FStats Fighter = Player(EKit::Fighter, 2, Tune->CritBase, Tune->SkillDamageFloor);
    const FStats Guardian = Player(EKit::Guardian, 2);
    const FStats Mage = Player(EKit::Mage, 2);
    const FStats BackendFighter = FromAttributes(10, 45, 19, 36, 10, 10, 10,
        Tune->CritBase, Tune->SkillDamageFloor);
    const FStats BackendArcher = FromAttributes(10, 10, 63, 28, 10, 10, 10,
        Tune->CritBase, Tune->SkillDamageFloor);
    const FAttack FighterSnapshotAttack = PlayerAuto(BackendFighter, ReferenceWeapon(10),
        Tune->WeaponSpread, Tune->PowerSpeedReferenceSeconds, Tune->UnarmedDpsFactor, Tune->WeaponDpsK);
    const FAttack ArcherSnapshotAttack = PlayerAuto(BackendArcher, ReferenceWeapon(10),
        Tune->WeaponSpread, Tune->PowerSpeedReferenceSeconds, Tune->UnarmedDpsFactor, Tune->WeaponDpsK);
    TestTrue(TEXT("Server L10 attributes feed FAttack physical power"),
        FighterSnapshotAttack.RawDamage > ArcherSnapshotAttack.RawDamage);
    const FStats Untrained = WithSkills(BackendFighter, 0, 0, Tune->SkillDamageFloor);
    const FStats Trained = WithSkills(BackendFighter, 100, 50, Tune->SkillDamageFloor);
    const FAttack WeakStrike = PlayerAuto(Untrained, ReferenceWeapon(10), Tune->WeaponSpread,
        Tune->PowerSpeedReferenceSeconds, Tune->UnarmedDpsFactor, Tune->WeaponDpsK);
    const FAttack StrongStrike = PlayerAuto(Trained, ReferenceWeapon(10), Tune->WeaponSpread,
        Tune->PowerSpeedReferenceSeconds, Tune->UnarmedDpsFactor, Tune->WeaponDpsK);
    TestTrue(TEXT("A persisted governing skill changes resolved attack damage"),
        Resolve(StrongStrike, FDraws{}).Damage > Resolve(WeakStrike, FDraws{}).Damage);
    TestTrue(TEXT("Tactics raises the hit skill used by FAttack"),
        Trained.Skill + .1 * Trained.Tactics > Untrained.Skill + .1 * Untrained.Tactics);
    TestEqual(TEXT("Fighter level 2 health"), Fighter.Health, 540);
    TestTrue(TEXT("Guardian has higher health"), Guardian.Health > Fighter.Health);
    TestTrue(TEXT("Mage has higher spell power"), Mage.Intelligence > Fighter.Intelligence);
    const FNpc Boar = Npc(TEXT("npc/boar"));
    TestEqual(TEXT("Boar authored health"), Boar.Health, 489);
    TestEqual(TEXT("Wolf alpha authored health"), Npc(TEXT("npc/wolf_alpha")).Health, 1779);
    const auto* Pommel = Ability(TEXT("pommel_strike"));
    const auto* Ward = Ability(TEXT("arcane_ward"));
    TestTrue(TEXT("Published pommel coefficient is authored"), Pommel &&
        FMath::IsNearlyEqual(Pommel->Modifier, .3) && FMath::IsNearlyEqual(Pommel->ReferenceSeconds, .5));
    TestTrue(TEXT("Arcane ward is a non-critical absorb"), Ward && Ward->bAbsorb &&
        !AuthoredAbility(Mage, ReferenceWeapon(2, 3.4, .7, 1.35), *Ward, .15).bCanCrit);
    FAttack PlayerHit = PlayerAuto(Fighter, ReferenceWeapon(2), Tune->WeaponSpread,
        Tune->PowerSpeedReferenceSeconds, Tune->UnarmedDpsFactor, Tune->WeaponDpsK);
    PlayerHit.bHasArmorSnapshot = true; PlayerHit.Armor = Boar.Armor;
    FDraws Rolls{.5, .5, .5, .5, 0};
    const int32 Low = Resolve(PlayerHit, Rolls).Damage;
    Rolls.Spread = 1;
    const int32 High = Resolve(PlayerHit, Rolls).Damage;
    TestTrue(TEXT("Weapon-only spread reaches the resolver"), High > Low);
    PlayerHit.bHasArmorSnapshot = false;
    TestTrue(TEXT("Boar armor mitigates the same swing"), Resolve(PlayerHit, Rolls).Damage > High);
    FAttack BoarHit; BoarHit.RawDamage = Boar.SwingDamage; BoarHit.AttackerLevel = Boar.Level;
    BoarHit.bHasArmorSnapshot = true; BoarHit.Armor = 64;
    BoarHit.bFrontArc = true; BoarHit.bCanBlock = true;
    BoarHit.BlockChance = .10; BoarHit.BlockReduction = Tune->BlockReduction;
    Rolls = {.5, .5, .5, .5, .5};
    const int32 Open = Resolve(BoarHit, Rolls).Damage;
    Rolls.Block = .05;
    const FResult Blocked = Resolve(BoarHit, Rolls);
    TestEqual(TEXT("Shield block outcome"), Blocked.Outcome, EOutcome::Block);
    TestTrue(TEXT("Shield removes post-mitigation damage"), Blocked.Damage < Open);
    TestTrue(TEXT("Real blocked hit grants shield practice"), GrantsShieldPractice(BoarHit, Blocked));
    TestFalse(TEXT("Unblocked hit grants no shield practice"), GrantsShieldPractice(BoarHit, Resolve(BoarHit, FDraws{.5, .5, .5, .5, .5})));
    FAttack SpellHit; SpellHit.RawDamage = 30; SpellHit.AttackerLevel = Boar.Level;
    SpellHit.bSpell = true; SpellHit.bPhysical = false; SpellHit.bHasResistanceSnapshot = true;
    SpellHit.Resistance = 10;
    TestTrue(TEXT("Hostile spell mitigation grants resist practice"),
        GrantsResistPractice(SpellHit, Resolve(SpellHit, Rolls)));
    SpellHit.Resistance = 0;
    TestFalse(TEXT("Unmitigated spell grants no resist practice"),
        GrantsResistPractice(SpellHit, Resolve(SpellHit, Rolls)));
    BoarHit.Absorb = 100;
    const FResult Absorbed = Resolve(BoarHit, Rolls);
    TestEqual(TEXT("Ward fully absorbs the blocked hit"), Absorbed.Outcome, EOutcome::Absorb);
    TestTrue(TEXT("Absorbed amount is available for pool consumption"), Absorbed.Absorbed > 0);
    TestEqual(TEXT("Fully absorbed hit deals zero"), Absorbed.Damage, 0);
    // Reference auto + filler throughput: expected crit and the NPC's 6% frontal
    // avoidance. This is the Part 6 design fight, not a live AI timing assertion.
    const double Mitigation = 1.0 - Boar.Armor / (Boar.Armor + 100.0 * Fighter.Level);
    const double Auto = PlayerHit.RawDamage;
    const double Strike = (ReferenceWeapon(2).Dps * 1.5 + 2.0 * Fighter.Strength *
        1.5 / Tune->PowerSpeedReferenceSeconds) * .7 * Throughput(2) * Fighter.SkillMultiplier;
    const double Dps = (Auto / 2.0 + Strike / 1.5) * Mitigation * .94 * (1.0 + Fighter.CritChance);
    const double Seconds = Boar.Health / Dps;
    const double NpcDps = Boar.SwingDamage / Boar.SwingSeconds *
        (1.0 - 64.0 / (64.0 + 100.0 * Boar.Level)) *
        (1.0 + Boar.CritChance * (Boar.CritDamage - 1.0)) *
        (1.0 - FMath::Clamp((100.0 - Boar.Skill) / Tune->MissDivisor, 0.0, .30));
    const double Remaining = 1.0 - Seconds * NpcDps / Fighter.Health;
    TestTrue(TEXT("Fighter L2 versus boar is about 36 seconds"), FMath::Abs(Seconds - 36.0) <= 5.0);
    TestTrue(TEXT("Fighter finishes near 27 percent health"), FMath::Abs(Remaining - .27) <= .10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBoarDodgeRulesTest,
    "ShatteredFrontier.Combat.Pilot.BoarDodge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFBoarDodgeRulesTest::RunTest(const FString& Parameters)
{
    const auto* Settings = GetDefault<USFCombatPilotSettings>();
    TestEqual(TEXT("Shipped NPC dodge default"), Settings->BoarDodgeChance, 0.03f);
    TestTrue(TEXT("Low draw in 180-degree front arc dodges"),
        SFBoarDodgeRules::Dodges(FVector::ForwardVector, FVector(1, 0, 0), 0.029, Settings->BoarDodgeChance));
    TestFalse(TEXT("Chance boundary does not dodge"),
        SFBoarDodgeRules::Dodges(FVector::ForwardVector, FVector(1, 0, 0), 0.03, Settings->BoarDodgeChance));
    TestFalse(TEXT("Rear attack cannot be dodged"),
        SFBoarDodgeRules::Dodges(FVector::ForwardVector, FVector(-1, 0, 0), 0.0, Settings->BoarDodgeChance));
    TestTrue(TEXT("Side boundary remains in front half"),
        SFBoarDodgeRules::IsInFrontArc(FVector::ForwardVector, FVector(0, 1, 0)));
    TestFalse(TEXT("Narrow configured arc excludes the side"),
        SFBoarDodgeRules::IsInFrontArc(FVector::ForwardVector, FVector(0, 1, 0), 90.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFAutoAttackRulesTest,
    "ShatteredFrontier.Combat.Pilot.AutoAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFAutoAttackRulesTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Unarmed weapon swing is 2 seconds"), SFAutoAttackRules::UnarmedSwingSeconds, 2.0);
    TestFalse(TEXT("Cannot swing before due"), SFAutoAttackRules::CanSwing(true, true, true, false, 1.99, 2.0));
    TestTrue(TEXT("Swing at due time"), SFAutoAttackRules::CanSwing(true, true, true, false, 2.0, 2.0));
    TestFalse(TEXT("Toggle stops swings"), SFAutoAttackRules::CanSwing(false, true, true, false, 3.0, 2.0));
    TestFalse(TEXT("Target loss stops swings"), SFAutoAttackRules::CanSwing(true, false, true, false, 3.0, 2.0));
    TestFalse(TEXT("Dead player cannot swing"), SFAutoAttackRules::CanSwing(true, true, false, false, 3.0, 2.0));
    TestFalse(TEXT("Casting pauses swings"), SFAutoAttackRules::CanSwing(true, true, true, true, 3.0, 2.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatPilotRulesTest,
    "ShatteredFrontier.Combat.Pilot.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatPilotRulesTest::RunTest(const FString& Parameters)
{
    const auto* Settings = GetDefault<USFCombatPilotSettings>();
    FString Error;
    TestTrue(TEXT("Shipped combat pilot settings are bounded"), Settings->Validate(Error));
    TestTrue(TEXT("Front target inside range and off cooldown"), SFCombatPilotRules::CanMelee(220, 0.9, 2, 1, 280));
    TestFalse(TEXT("Out of reach"), SFCombatPilotRules::CanMelee(281, 0.9, 2, 1, 280));
    TestFalse(TEXT("Cooldown enforced"), SFCombatPilotRules::CanMelee(220, 0.9, 0.9, 1, 280));
    TestFalse(TEXT("Behind attacker"), SFCombatPilotRules::CanMelee(220, -0.2, 2, 1, 280));
    TestEqual(TEXT("Overkill clamps at zero"), SFCombatPilotRules::RemainingHealth(8, 20), 0);
    TestEqual(TEXT("Negative damage cannot heal"), SFCombatPilotRules::RemainingHealth(8, -2), 8);

    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    auto* Mode = Fixture.GetTestWorld()->SpawnActor<ASFGameMode>();
    if (!Mode) return false;
    const TArray<FVector2D> Town = {{-4,-4},{4,-4},{4,4},{-4,4}};
    TestTrue(TEXT("Town polygon contains its centre"), Mode->IsInsideTerritory(FVector::ZeroVector, Town));
    TestTrue(TEXT("Town boundary is protected"), Mode->IsInsideTerritory(FVector(400,0,0), Town));
    TestFalse(TEXT("Town boundary excludes countryside"), Mode->IsInsideTerritory(FVector(500,0,0), Town));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatDeathServicesTest,
    "ShatteredFrontier.Combat.Death.ServicesClient",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatDeathServicesTest::RunTest(const FString& Parameters)
{
    USFGameplayServicesSettings* Settings = NewObject<USFGameplayServicesSettings>();
    Settings->bEnabled = true;
    int32 Sent = 0;
    FSFGameplayServicesClient::SetSendOverrideForTests([&Sent](const FSFGameplayRequest& Request, float, TFunction<void(const FSFGameplayResponse&)> Done)
    {
        ++Sent;
        FSFGameplayResponse Reply; Reply.Status = ESFGameplayStatus::Ok; Reply.GrpcCode = 0;
        Reply.bHasDeath = true; Reply.bIsDead = Request.Rpc == ESFGameplayRpc::ReportDeath;
        Reply.DeathEventId = Request.DeathEventId; Reply.RespawnAtUnixMs = Request.DeathAtUnixMs + Request.RespawnDelayMs;
        Done(Reply);
    });
    auto Client = FSFGameplayServicesClient::Create(*Settings);
    TestTrue(TEXT("Services client created"), Client.IsValid());
    if (Client)
    {
        FSFGameplayRequest Report; Report.Rpc = ESFGameplayRpc::ReportDeath;
        Report.CharacterId = TEXT("chr_test"); Report.DeathEventId = TEXT("00000000-0000-0000-0000-000000000001");
        Report.DeathAtUnixMs = 100000; Report.RespawnDelayMs = 4000; Report.ZoneId = TEXT("silverbrook");
        Report.Position = FVector(12, 20, 30); Report.RespawnPosition = FVector(100, 200, 30);
        Client->Call(Report, [this](const FSFGameplayResponse& Reply)
        { TestTrue(TEXT("Death acknowledgement carries deadline and event"), Reply.bIsDead && Reply.RespawnAtUnixMs == 104000 && !Reply.DeathEventId.IsEmpty()); });
        FSFGameplayRequest Clear; Clear.Rpc = ESFGameplayRpc::ClearDeath; Clear.CharacterId = Report.CharacterId; Clear.DeathEventId = Report.DeathEventId;
        Client->Call(Clear, [this](const FSFGameplayResponse& Reply)
        { TestTrue(TEXT("Respawn acknowledgement is alive for same event"), !Reply.bIsDead && !Reply.DeathEventId.IsEmpty()); });
    }
    TestEqual(TEXT("Both death calls sent"), Sent, 2);
    FSFGameplayServicesClient::SetSendOverrideForTests({});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatPilotRewardTest,
    "ShatteredFrontier.Combat.Pilot.Reward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatPilotRewardTest::RunTest(const FString& Parameters)
{
    const int32 PerBoar = GetDefault<USFCombatPilotSettings>()->HidesPerBoar;
    TSet<FString> Granted;
    TestEqual(TEXT("First death rewards"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:0:1"), false, PerBoar), PerBoar);
    TestEqual(TEXT("Duplicate death event rewards nothing"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:0:1"), false, PerBoar), 0);
    TestEqual(TEXT("Respawned boar is a new death"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:0:2"), false, PerBoar), PerBoar);
    TestEqual(TEXT("Protected-town kill rewards nothing"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:1:3"), true, PerBoar), 0);
    TestEqual(TEXT("...and stays spent once the boar leaves town"), SFCombatPilotRewards::HidesFor(Granted, TEXT("boar_farm_edge:1:3"), false, PerBoar), 0);

    const FSFGameplayRequest First = SFCombatPilotRewards::BuildDefeatRequest(TEXT("epoch:boar_farm_edge:0:1"),
        TEXT("chr_dev_alpha"), TEXT("chr_dev_beta"), TEXT("ctn_dev_beta"), 4, false, false,
        TEXT("npc/boar"), TEXT("loot/greenlands_silverbrook_boar"));
    const FSFGameplayRequest Retry = SFCombatPilotRewards::BuildDefeatRequest(First.OperationId,
        First.KillerCharacterId, First.CharacterId, First.ContainerId, 5, false, false,
        First.CreatureId, First.TableId);
    TestTrue(TEXT("Service reward targets the durable backpack with stable event identity"),
        First.Rpc == ESFGameplayRpc::GrantDefeatReward && First.KillerCharacterId == TEXT("chr_dev_alpha") &&
        First.CharacterId == TEXT("chr_dev_beta") && First.ContainerId == TEXT("ctn_dev_beta") && First.CreatureId == TEXT("npc/boar") &&
        First.TableId == TEXT("loot/greenlands_silverbrook_boar") &&
        First.OperationId == Retry.OperationId && Retry.ExpectedContainerRevision == 5 &&
        !First.bKillerProtected && !First.bBoarProtected);

    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    auto* Controller = Fixture.GetTestWorld()->SpawnActor<APlayerController>();
    if (!Controller) return false;
    SFCombatPilotRewards::AddPilotBag(Controller);
    TSet<FString> Deaths;
    for (int32 Event = 0; Event < 2; ++Event)  // the same death reported twice
    {
        const int32 Hides = SFCombatPilotRewards::HidesFor(Deaths, TEXT("boar_forest_edge:2:7"), false, PerBoar);
        if (Hides > 0) { TestTrue(TEXT("Hides fit the pilot bag"), SFCombatPilotRewards::GrantHides(Controller, Hides)); }
    }
    TestEqual(TEXT("Bag holds one death's hides"), SFCombatPilotRewards::CountHides(Controller), PerBoar);
    TestFalse(TEXT("No bag, no grant"), SFCombatPilotRewards::GrantHides(nullptr, PerBoar));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatEvadeTest,
    "ShatteredFrontier.Combat.Pilot.Evade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatEvadeTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Return travel is 1.5 times chase speed"),
        SFCombatPilotRules::EvadeReturnSpeedCmPerSecond(3.5), 525.0);
    TestFalse(TEXT("Unreachable timer waits for ten seconds"), SFCombatPilotRules::UnreachableEvade(9.99, 11.0));
    TestFalse(TEXT("Recent damage suppresses unreachable evade"), SFCombatPilotRules::UnreachableEvade(10.0, 9.99));
    TestTrue(TEXT("Ten unreachable seconds without damage evades"), SFCombatPilotRules::UnreachableEvade(10.0, 10.0));
    TestFalse(TEXT("Two evades do not signal"), SFCombatPilotRules::LeashAbuseSignal({ 0.0, 10.0 }, 20.0));
    TestTrue(TEXT("Three evades in 120 seconds signal"), SFCombatPilotRules::LeashAbuseSignal({ 0.0, 10.0, 120.0 }, 120.0));
    TestFalse(TEXT("Expired evade leaves two in window"), SFCombatPilotRules::LeashAbuseSignal({ 0.0, 10.0, 120.0 }, 131.0));
    TestFalse(TEXT("Fourth evade does not duplicate signal"), SFCombatPilotRules::LeashAbuseSignal({ 0.0, 10.0, 20.0, 30.0 }, 30.0));
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    ASFGameMode* Mode = World->SpawnActor<ASFGameMode>();
    ASFPilotBoar* Boar = World->SpawnActor<ASFPilotBoar>(FVector(300, 0, 115), FRotator(0, 45, 0));
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    ASFE1Character* Character = World->SpawnActor<ASFE1Character>();
    if (!Mode || !Boar || !Controller || !Character) return false;
    Controller->Possess(Character);
    Boar->Initialize(Mode, TEXT("evade_test"), 0, {}, 45, 1);
    Character->SetCombatTarget(Boar);
    Boar->EnterEvade();
    TestTrue(TEXT("Evade begins on authority"), Boar->IsLeashing());
    TestNull(TEXT("Server clears the player's selected target"), Character->GetCombatTarget());
    Boar->ReceivePilotDamage(Character, 10);
    TestEqual(TEXT("Evading creature is damage immune"), Boar->Health(), 45);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatRecentAttackersTest,
    "ShatteredFrontier.Combat.Pilot.RecentAttackers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatRecentAttackersTest::RunTest(const FString& Parameters)
{
    FSFRecentAttackerMemory Memory;
    TestEqual(TEXT("Authored memory lasts 60 seconds"), FSFRecentAttackerMemory::RetentionSeconds, 60.0);
    TestFalse(TEXT("No memory before reset"), Memory.Contains(TEXT("chr_attacker"), 100.0));
    Memory.RecordReset({TEXT("chr_attacker")}, 100.0);
    TestTrue(TEXT("Actual attacker remembered"), Memory.Contains(TEXT("chr_attacker"), 159.999));
    TestFalse(TEXT("Bystander has no memory"), Memory.Contains(TEXT("chr_bystander"), 101.0));
    TestFalse(TEXT("Memory expires at exact deadline"), Memory.Contains(TEXT("chr_attacker"), 160.0));
    Memory.RecordReset({TEXT("chr_attacker")}, 200.0);
    TestTrue(TEXT("New reset renews only its attacker"), Memory.Contains(TEXT("chr_attacker"), 259.0));
    TestFalse(TEXT("Renewed memory expires"), Memory.Contains(TEXT("chr_attacker"), 260.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatPackAssistTest,
    "ShatteredFrontier.Combat.Pilot.PackAssist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatPackAssistTest::RunTest(const FString& Parameters)
{
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    TestEqual(TEXT("Common assist waits two seconds"), Settings->BoarAssistDelaySeconds, 2.0f);
    TestEqual(TEXT("Common assist reaches eight metres"), Settings->BoarAssistRadiusMetres, 8.0f);
    TestFalse(TEXT("Early pack call rejected"), SFCombatPilotRules::CanAssist(1.999, 1.0, 2.0, 8.0));
    TestTrue(TEXT("Exact delay and radius accepted"), SFCombatPilotRules::CanAssist(2.0, 8.0, 2.0, 8.0));
    TestFalse(TEXT("Out-of-range packmate rejected"), SFCombatPilotRules::CanAssist(3.0, 8.001, 2.0, 8.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatThreatTest,
    "ShatteredFrontier.Combat.Pilot.Threat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatThreatTest::RunTest(const FString& Parameters)
{
    const auto* Settings = GetDefault<USFCombatPilotSettings>();
    TestEqual(TEXT("Melee switch threshold"), Settings->ThreatSwitchMelee, 1.10f);
    TestEqual(TEXT("Ranged switch threshold"), Settings->ThreatSwitchRanged, 1.30f);
    TestEqual(TEXT("Ten decisions per second"), Settings->ThreatDecisionSeconds, 0.1f);
    TestEqual(TEXT("Threat table cap"), Settings->ThreatTableMax, 40);
    FSFBoarThreatTable Table;
    Table.Add(TEXT("chr_b"), 1, 40);
    Table.Add(TEXT("chr_a"), 1, 40);
    TMap<FString, double> Near = {{TEXT("chr_a"), 400}, {TEXT("chr_b"), 400}};
    TestEqual(TEXT("Equal initial threat breaks by ascending ID"), Table.Select({}, Near, 1.10, 1.30), FString(TEXT("chr_a")));
    Table.Add(TEXT("chr_b"), 10, 40);
    TestEqual(TEXT("Effective damage adds one threat per point and switches"), Table.Select(TEXT("chr_a"), Near, 1.10, 1.30), FString(TEXT("chr_b")));
    Table.Add(TEXT("chr_a"), 11, 40);
    TestEqual(TEXT("Challenger below 110 percent stays put"), Table.Select(TEXT("chr_b"), Near, 1.10, 1.30), FString(TEXT("chr_b")));
    Table.Add(TEXT("chr_a"), 1, 40);
    TestEqual(TEXT("Challenger over 110 percent switches"), Table.Select(TEXT("chr_b"), Near, 1.10, 1.30), FString(TEXT("chr_a")));
    Near[TEXT("chr_b")] = 600;
    Table.Add(TEXT("chr_b"), 4, 40);
    TestEqual(TEXT("Ranged challenger below 130 percent stays put"), Table.Select(TEXT("chr_a"), Near, 1.10, 1.30), FString(TEXT("chr_a")));
    Table.Add(TEXT("chr_b"), 2, 40);
    TestEqual(TEXT("Ranged challenger over 130 percent switches"), Table.Select(TEXT("chr_a"), Near, 1.10, 1.30), FString(TEXT("chr_b")));
    Table.Remove(TEXT("chr_b"));
    TestEqual(TEXT("Dead or leashed entry cannot retain target"), Table.Select(TEXT("chr_b"), Near, 1.10, 1.30), FString(TEXT("chr_a")));
    Table.Clear();
    TestTrue(TEXT("Evade clears threat"), Table.Select({}, Near, 1.10, 1.30).IsEmpty());
    FSFBoarThreatTable MixedRange;
    MixedRange.Add(TEXT("chr_current"), 100, 40);
    MixedRange.Add(TEXT("chr_far"), 120, 40);
    MixedRange.Add(TEXT("chr_near"), 115, 40);
    TMap<FString, double> MixedDistances;
    MixedDistances.Add(TEXT("chr_current"), 200);
    MixedDistances.Add(TEXT("chr_far"), 600);
    MixedDistances.Add(TEXT("chr_near"), 400);
    TestEqual(TEXT("A nearer qualifying challenger wins despite a farther higher raw threat"),
        MixedRange.Select(TEXT("chr_current"), MixedDistances, 1.10, 1.30), FString(TEXT("chr_near")));
    FSFBoarThreatTable Capped;
    for (int32 I = 0; I < 40; ++I) { Capped.Add(FString::Printf(TEXT("chr_%02d"), I), I + 1, 40); }
    Capped.Add(TEXT("chr_new"), 1, 40);
    TestFalse(TEXT("Lowest entry evicted at cap"), Capped.Contains(TEXT("chr_00")));
    TestTrue(TEXT("New entrant retained"), Capped.Contains(TEXT("chr_new")));
    Capped.Add(TEXT("chr_new"), -1, 40);
    TestTrue(TEXT("Invalid negative threat ignored"), Capped.Contains(TEXT("chr_new")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatAggroTest,
    "ShatteredFrontier.Combat.Pilot.Aggro",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFCombatAggroTest::RunTest(const FString& Parameters)
{
    const auto* Settings = GetDefault<USFCombatPilotSettings>();
    TestEqual(TEXT("Common base radius"), Settings->BoarAggroRadiusMetres, 12.0f);
    TestEqual(TEXT("Proximity exception"), Settings->BoarNearNoticeRadiusMetres, 1.5f);
    TestEqual(TEXT("Minimum radius"), Settings->BoarAggroMinimumMetres, 4.0f);
    TestEqual(TEXT("Maximum bonus"), Settings->BoarAggroMaximumBonusMetres, 6.0f);
    TestEqual(TEXT("Beast field of view"), Settings->BoarFieldOfViewDegrees, 160.0f);
    TestEqual(TEXT("Higher level character"), SFCombatPilotRules::AggroRadiusMetres(12, 20, 23, 4, 6), 9.0);
    TestEqual(TEXT("Lower level character"), SFCombatPilotRules::AggroRadiusMetres(12, 20, 14, 4, 6), 18.0);
    TestEqual(TEXT("Minimum clamps"), SFCombatPilotRules::AggroRadiusMetres(12, 20, 40, 4, 6), 4.0);
    TestEqual(TEXT("Maximum clamps"), SFCombatPilotRules::AggroRadiusMetres(12, 20, 1, 4, 6), 18.0);
    TestFalse(TEXT("Outside level-adjusted radius"), SFCombatPilotRules::DetectBySight(9.01, 1, true, 9, 1.5, 160));
    TestTrue(TEXT("Inside radius and facing"), SFCombatPilotRules::DetectBySight(9, 1, true, 9, 1.5, 160));
    TestFalse(TEXT("Wall blocks within radius"), SFCombatPilotRules::DetectBySight(5, 1, false, 9, 1.5, 160));
    TestFalse(TEXT("Behind outside proximity"), SFCombatPilotRules::DetectBySight(1.51, -1, true, 9, 1.5, 160));
    TestTrue(TEXT("Behind inside proximity"), SFCombatPilotRules::DetectBySight(1.5, -1, true, 9, 1.5, 160));
    return true;
}
#endif
