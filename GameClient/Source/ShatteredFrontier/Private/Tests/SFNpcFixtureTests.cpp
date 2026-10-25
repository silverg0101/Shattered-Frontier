#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "Services/SFBackpackComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcFixtureExtractionTest,
    "ShatteredFrontier.Combat.NpcReuse.ExplicitFixturePolicies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcFixtureExtractionTest::RunTest(const FString& Parameters)
{
    FSFScopedGameplayServicesForTests Offline(nullptr);
    TestNull(TEXT("Production map cannot borrow a sandbox suffix"), SFCombatFixtures::Find(TEXT("/Game/Frontier/Maps/Greenlands/SB_Aggro")));
    TestNull(TEXT("Unknown sandbox has no implicit fixture"), SFCombatFixtures::Find(TEXT("/Game/Frontier/Maps/Sandbox/SB_Unknown")));
    struct FCase { const TCHAR* Map; const TCHAR* Tag; const TCHAR* Group; bool bNpc; bool bDefensive; int32 Points; };
    const FCase Cases[] = {
        {TEXT("SB_Aggro"), TEXT("SF_AggroSandboxWildlife"), TEXT("aggro_sandbox"), true, false, 1},
        {TEXT("SB_Respawn"), TEXT("SF_RespawnSandboxWildlife"), TEXT("respawn_sandbox"), true, true, 1},
        {TEXT("SB_SpiritRelease"), TEXT("SF_SpiritReleaseSandboxWildlife"), TEXT("spirit_release_sandbox"), true, true, 1},
        {TEXT("SB_FallDamage"), TEXT("SF_CompanionSandboxWildlife"), TEXT("companions_sandbox"), false, false, 1}
    };
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FCase& Case : Cases)
    {
        FTestWorldWrapper Fixture;
        if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
        UWorld* World = Fixture.GetTestWorld();
        const FString OriginalPackage = World->GetOutermost()->GetName();
        ON_SCOPE_EXIT { World->GetOutermost()->Rename(*OriginalPackage, nullptr, REN_DontCreateRedirectors); };
        const FString Map = FString(TEXT("/Game/Frontier/Maps/Sandbox/")) + Case.Map;
        if (!World->GetOutermost()->Rename(*Map, nullptr, REN_DontCreateRedirectors)) return false;
        auto* Mode = World->SpawnActor<ASFGameMode>();
        auto* Ignored = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector(-2000,0,100), FRotator::ZeroRotator, Spawn);
        ASFPilotBoar* Npc = nullptr;
        if (Case.bNpc)
        {
            Npc = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector(0,0,100), FRotator::ZeroRotator, Spawn);
            if (!Npc) return false;
            Npc->Tags.Add(Case.Tag);
        }
        if (!Mode || !Ignored) return false;
        if (FCString::Strcmp(Case.Map, TEXT("SB_Respawn")) == 0 || FCString::Strcmp(Case.Map, TEXT("SB_SpiritRelease")) == 0)
        {
            auto* Point = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector(1000,0,100), FRotator::ZeroRotator, Spawn);
            if (!Point) return false;
            Point->Tags.Add(TEXT("SF_RespawnPoint"));
        }
        Mode->StartCombat();
        TestTrue(TEXT("Real startup admits exact named fixture"), Mode->bCompanionSandbox && Mode->bPilotActive && Mode->ActiveCombatFixture);
        TestEqual(TEXT("Untagged wildlife stays uninitialized"), Ignored->Health(), 0);
        TestEqual(TEXT("Authored/default recovery point count preserved"), Mode->RespawnPoints.Num(), Case.Points);
        if (!Npc) continue;
        TestEqual(TEXT("Existing fixture group/wire identity retained"), Npc->GroupId(), FString(Case.Group));
        TestEqual(TEXT("Fixture health retains current setting"), Npc->Health(), Mode->CapturedSettings->BoarHealth);
        TestEqual(TEXT("Fixture defensive admission retained"), Npc->IsDefensive(), Case.bDefensive);
        TestTrue(TEXT("Fixture territory retains old 85m extent"), Npc->Territory.Contains(FVector2D(85,85)));
        if (FCString::Strcmp(Case.Map, TEXT("SB_Aggro")) == 0)
        {
            TestTrue(TEXT("Startup installs explicit aggro/idle flags"), EnumHasAllFlags(Npc->FixtureFlags, ESFNpcFixtureFlags::TraceAggro | ESFNpcFixtureFlags::HoldIdle));
            Npc->WanderDestination = FVector(1000,0,100); Npc->NextWanderAt = World->GetTimeSeconds() + 30;
            Npc->Tick(1);
            TestEqual(TEXT("Actual aggro fixture holds idle actor"), Npc->GetActorLocation().X, 0.0);
        }
        if (FCString::Strcmp(Case.Map, TEXT("SB_Respawn")) == 0)
        {
            TestEqual(TEXT("Respawn captures original and alternate points"), Mode->RespawnSandboxPoints.Num(), 2);
            auto* Killer = World->SpawnActor<ASFE1Character>(ASFE1Character::StaticClass(), FVector(2000,0,100), FRotator::ZeroRotator, Spawn);
            if (!Killer) return false;
            Npc->ReceivePilotDamage(Killer, Npc->Health());
            TestTrue(TEXT("Actual fixture defeat schedules existing respawn clock"), Npc->IsActorBeingDestroyed() && Mode->RespawnSandboxDueAt > World->GetTimeSeconds());
            Mode->RespawnSandboxDueAt = World->GetTimeSeconds();
            Mode->TickCombat();
            ASFPilotBoar* Replacement = nullptr;
            for (TActorIterator<ASFPilotBoar> It(World); It; ++It)
                if (!It->IsActorBeingDestroyed() && It->GroupId() == Case.Group) Replacement = *It;
            if (!TestNotNull(TEXT("Actual fixture tick recreates defeated NPC"), Replacement)) return false;
            TestTrue(TEXT("Respawn carries explicit idle/defensive policy"), Replacement->IsDefensive() && EnumHasAnyFlags(Replacement->FixtureFlags, ESFNpcFixtureFlags::HoldIdle));
            TestTrue(TEXT("Respawn still chooses alternate authored point"), Replacement->GetActorLocation().Equals(FVector(1000,0,100)));
        }
    }
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    auto* Mode = World->SpawnActor<ASFGameMode>();
    auto* Npc = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector(0,0,100), FRotator::ZeroRotator, Spawn);
    if (!Mode || !Npc) return false;
    Mode->CapturedSettings = GetDefault<USFCombatPilotSettings>(); Mode->bPilotActive = true;
    const TArray<FVector2D> Area{FVector2D(-100,-100),FVector2D(100,-100),FVector2D(100,100),FVector2D(-100,100)};
    Npc->Initialize(Mode, TEXT("aggro_sandbox"), 0, Area, 100, 1);
    Npc->WanderDestination = FVector(1000,0,100); Npc->NextWanderAt = World->GetTimeSeconds() + 30;
    Npc->Tick(1);
    TestTrue(TEXT("Group name alone cannot activate idle fixture behavior"), FMath::IsNearlyEqual(Npc->GetActorLocation().X, 130.0, .1));
    Npc->SetActorLocation(FVector(0,0,100));
    Npc->Initialize(Mode, TEXT("arbitrary_production_group"), 0, Area, 100, 1, false, ESFNpcFixtureFlags::HoldIdle);
    Npc->WanderDestination = FVector(1000,0,100); Npc->NextWanderAt = World->GetTimeSeconds() + 30;
    Npc->Tick(1);
    TestEqual(TEXT("Explicit idle policy is independent of group spelling"), Npc->GetActorLocation().X, 0.0);
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* Player = World->SpawnActor<ASFE1Character>(ASFE1Character::StaticClass(), FVector(500,0,100), FRotator::ZeroRotator, Spawn);
    if (!Controller || !Player) return false;
    Controller->Possess(Player); Player->CombatSetHealth(100);
    Npc->Initialize(Mode, TEXT("arbitrary_production_group"), 0, Area, 100, 1, false, ESFNpcFixtureFlags::TraceAggro);
    Npc->Tick(0);
    TestTrue(TEXT("Explicit trace flag reaches actual perception path"), Npc->NextAggroFixtureTraceAt > World->GetTimeSeconds());
    TestTrue(TEXT("Perception still acquires actual target"), Npc->HasThreatFor(Player));
    return true;
}
#endif
