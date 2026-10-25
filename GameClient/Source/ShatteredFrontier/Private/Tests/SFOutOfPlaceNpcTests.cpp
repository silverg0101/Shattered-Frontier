#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCombatResolution.h"
#include "Combat/SFStatusEffectsComponent.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFOutOfPlaceNpcTest,
    "ShatteredFrontier.Combat.OutOfPlaceNpc.AuthorityAndRetaliation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFOutOfPlaceNpcTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASFPilotBoar* Npc = World->SpawnActor<ASFPilotBoar>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    ASFE1Character* Attacker = World->SpawnActor<ASFE1Character>(FVector(200, 0, 100), FRotator::ZeroRotator, Spawn);
    ASFPilotBoar* Ordinary = World->SpawnActor<ASFPilotBoar>(FVector(500, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!Npc || !Attacker || !Ordinary) return false;
    const TArray<FVector2D> Area = { {-10, -10}, {10, -10}, {10, 10}, {-10, 10} };
    Npc->Initialize(nullptr, TEXT("out_of_place_test"), 0, Area, 100, 1);
    Ordinary->Initialize(nullptr, TEXT("ordinary_test"), 0, Area, 100, 2);
    Npc->Tags.Add(ASFPilotBoar::OutOfPlaceNpcTag);
    Attacker->CombatSetHealth(100);

    Npc->ReceivePilotDamage(nullptr, 1000);
    Npc->ReceivePilotDamage(Attacker, 0);
    Npc->ReceivePilotDamage(Attacker, -1);
    TestEqual(TEXT("Invalid or non-damaging hits preserve NPC health"), Npc->Health(), 100);
    TestEqual(TEXT("Invalid or non-damaging hits do not retaliate"), Attacker->CombatHealth(), 100);

    Npc->SetRole(ROLE_SimulatedProxy);
    Npc->ReceivePilotDamage(Attacker, 1000);
    UGameplayStatics::ApplyDamage(Npc, 1000, nullptr, Attacker, nullptr);
    Npc->SetRole(ROLE_Authority);
    TestEqual(TEXT("A client NPC cannot alter authoritative player health"), Attacker->CombatHealth(), 100);
    TestEqual(TEXT("Client damage cannot alter NPC health"), Npc->Health(), 100);
    Attacker->SetRole(ROLE_SimulatedProxy);
    Npc->ReceivePilotDamage(Attacker, 1000);
    Attacker->SetRole(ROLE_Authority);
    TestEqual(TEXT("A non-authoritative attacker is refused"), Attacker->CombatHealth(), 100);

    // Stack the actual player damage-reduction effects; lethality is not a tunable damage number.
    Attacker->ToggleDefensiveStance();
    Attacker->SetShieldWallUntil(World->GetTimeSeconds() + 10);
    Attacker->SetBulwarkUntil(World->GetTimeSeconds() + 10);
    Attacker->GrantCombatAbsorb(1000);
    TestTrue(TEXT("Player has active damage reduction"), Attacker->DamageTakenMultiplier(World->GetTimeSeconds()) < 1);
    Npc->ReceivePilotDamage(Attacker, 1);
    TestEqual(TEXT("A landed resolved combat hit never damages the NPC"), Npc->Health(), 100);
    TestEqual(TEXT("One hit defeats the attacker through all active reductions/absorb"), Attacker->CombatHealth(), 0);
    TestEqual(TEXT("Defeat uses the normal disabled-movement path"), Attacker->GetCharacterMovement()->MovementMode, MOVE_None);
    Npc->ReceivePilotDamage(Attacker, 1);
    TestEqual(TEXT("Already defeated attackers stay defeated"), Attacker->CombatHealth(), 0);

    Attacker->CombatSetHealth(100);
    const float Applied = UGameplayStatics::ApplyDamage(Npc, 1000, nullptr, Attacker, nullptr);
    TestEqual(TEXT("Engine damage reports zero applied damage"), Applied, 0.0f);
    TestEqual(TEXT("Engine damage preserves NPC health"), Npc->Health(), 100);
    TestEqual(TEXT("Engine damage causer receives the same lethal retaliation"), Attacker->CombatHealth(), 0);

    Attacker->CombatSetHealth(100);
    AActor* Projectile = World->SpawnActor<AActor>();
    if (!Projectile) return false;
    Projectile->SetInstigator(Attacker);
    UGameplayStatics::ApplyDamage(Npc, 1, nullptr, Projectile, nullptr);
    TestEqual(TEXT("Projectile hits retaliate against the instigating player"), Attacker->CombatHealth(), 0);
    Attacker->CombatSetHealth(100);
    UGameplayStatics::ApplyDamage(Npc, 1000, nullptr, nullptr, nullptr);
    TestEqual(TEXT("Unattributed environmental damage is harmless to the NPC"), Npc->Health(), 100);
    TestEqual(TEXT("Unattributed damage never chooses an unrelated player"), Attacker->CombatHealth(), 100);

    // The combat path may apply poison after the lethal hit; respawn reuses this player actor.
    ASFGameMode* Mode = World->SpawnActor<ASFGameMode>();
    if (!Mode) return false;
    SFCombatResolution::FAttack Poison;
    Poison.RawDamage = 20;
    Mode->ApplyStarterWeakPoison(Attacker, Npc, Poison);
    TestEqual(TEXT("A tagged NPC refuses weak poison"),
        Npc->GetStatusEffects()->GetStacks(TEXT("effect/weak_poison"), Attacker), 0);
    Npc->ReceivePilotDamage(Attacker, 1);
    Mode->ApplyStarterWeakPoison(Attacker, Npc, Poison);
    Attacker->CombatSetHealth(100); // Same-pawn health restore, as used by RespawnPlayer.
    Npc->GetStatusEffects()->Advance(GetDefault<USFStatusEffectsSettings>()->WeakPoisonDurationSeconds);
    TestEqual(TEXT("A refused poison cannot kill the restored player again"), Attacker->CombatHealth(), 100);
    int32 StatusTicks = 0;
    TestFalse(TEXT("Other damaging statuses are also refused on the tagged NPC"),
        Npc->GetStatusEffects()->Apply(TEXT("test/rend"), ESFStatusKind::Physical, ESFDispelType::None,
            Attacker, 2, 1, 1, false, [&StatusTicks]() { ++StatusTicks; }));
    TestTrue(TEXT("Ordinary NPCs still accept the same status"),
        Ordinary->GetStatusEffects()->Apply(TEXT("test/rend"), ESFStatusKind::Physical, ESFDispelType::None,
            Attacker, 2, 1, 1, false, [&StatusTicks]() { ++StatusTicks; }));
    Npc->GetStatusEffects()->Advance(1);
    Ordinary->GetStatusEffects()->Advance(1);
    TestEqual(TEXT("Only the ordinary NPC executes a status tick"), StatusTicks, 1);

    Ordinary->ReceivePilotDamage(Attacker, 10);
    TestEqual(TEXT("Untagged NPCs still receive ordinary combat damage"), Ordinary->Health(), 90);
    TestEqual(TEXT("Untagged NPC hits do not retaliate"), Attacker->CombatHealth(), 100);
    Attacker->ToggleDefensiveStance(); // Death clears stance/buffs; reapply for the ordinary-hit control.
    Attacker->SetShieldWallUntil(World->GetTimeSeconds() + 10);
    Attacker->SetBulwarkUntil(World->GetTimeSeconds() + 10);
    ASFGameMode::HitPlayer(Ordinary, Attacker, 100, true, true);
    TestEqual(TEXT("Untagged sources retain ordinary mitigation instead of lethal retaliation"), Attacker->CombatHealth(), 50);
    return true;
}
#endif
