#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Characters/SFE1Character.h"
#include "Characters/SFE1VitalsHud.h"
#include "Combat/SFCombatPilot.h"
#include "Framework/SFGameMode.h"
#include "Framework/SFPlayerController.h"
#include "Services/SFGameplayServicesClient.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFManualRespawnTest,
    "ShatteredFrontier.Characters.ManualRespawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFManualRespawnTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld();
    const FString Original = World->GetOutermost()->GetName();
    ON_SCOPE_EXIT { World->GetOutermost()->Rename(*Original, nullptr, REN_DontCreateRedirectors); };
    if (!World->GetOutermost()->Rename(TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook"), nullptr, REN_DontCreateRedirectors)) return false;
    World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Mode = World->SpawnActor<ASFGameMode>();
    auto* PC = World->SpawnActor<ASFPlayerController>();
    auto* Hero = World->SpawnActor<ASFE1Character>(Spawn);
    if (!TestNotNull(TEXT("Mode"), Mode) || !TestNotNull(TEXT("Controller"), PC) || !TestNotNull(TEXT("Hero"), Hero)) return false;
    Mode->bPilotActive = true;
    Mode->CapturedSettings = GetDefault<USFCombatPilotSettings>();
    Mode->RespawnPoints.Add(FVector(100, 200, 300));
    PC->Possess(Hero);
    Hero->CombatSetHealth(0);
    Hero->SetDefeatPresentation(World->GetTimeSeconds() + 1, 1, TEXT("Wild Boar"));
    TestFalse(TEXT("Early owner request rejected"), Mode->RequestNormalRespawn(Hero));
    TestFalse(TEXT("Rejected early request never claims respawning"), Hero->IsRespawnRequestAccepted());
    Hero->SetDefeatPresentation(World->GetTimeSeconds(), 1);
    World->GetTimerManager().Tick(2.f);
    TestEqual(TEXT("Deadline alone preserves dead state"), Hero->CombatHealth(), 0);
    Mode->RespawnPlayer(Hero);
    TestEqual(TEXT("Legacy automatic callback cannot restore production player"), Hero->CombatHealth(), 0);

    FSFGameplayResponse Restored;
    Restored.bHasDeath = true; Restored.bIsDead = true;
    Restored.DeathZoneId = TEXT("greenlands_silverbrook");
    Restored.DeathEventId = TEXT("manual-respawn-restore");
    Restored.DeathPosition = Hero->GetActorLocation();
    Restored.RespawnPosition = Mode->RespawnPoints[0];
    Restored.RespawnAtUnixMs = 0; // Already eligible when this owner reconnects.
    Mode->RestoreDeadPlayer(Hero, Restored);
    World->GetTimerManager().Tick(2.f);
    TestEqual(TEXT("Reconnected eligible player remains dead without click"), Hero->CombatHealth(), 0);
    TestTrue(TEXT("Restored death keeps its server event identity"), Mode->PlayerDeathEvents.Contains(Hero));
    TestTrue(TEXT("Restored deadline is known"), Hero->GetRespawnReadyAt() >= 0);
    // This isolated health-only fixture has no service transport. The durable path is checked by packaged reconnect acceptance.
    Mode->PlayerDeathEvents.Remove(Hero);
    Mode->PlayerDeathRespawnPoints.Remove(Hero);

    FSFDefeatNoticeModel Notice;
    Notice.Observe(true);
    TestTrue(TEXT("Defeat initially opens notice"), Notice.bOpen);
    Notice.Close();
    Notice.Observe(true);
    TestFalse(TEXT("Closed notice stays closed while dead"), Notice.bOpen);
    TestTrue(TEXT("Closing notice preserves defeated status"), Notice.bDead);
    TestEqual(TEXT("Closing notice never releases player"), Hero->CombatHealth(), 0);
    Notice.Reopen();
    TestTrue(TEXT("Skull reopens notice"), Notice.bOpen);
    Notice.Close();

    TestTrue(TEXT("Ready owner click accepted"), Mode->RequestNormalRespawn(Hero));
    TestEqual(TEXT("Normal respawn restores existing full health"), Hero->CombatHealth(), Hero->CombatMaximumHealth());
    TestFalse(TEXT("Repeated request cannot respawn living player"), Mode->RequestNormalRespawn(Hero));
    TestFalse(TEXT("Confirmed alive clears request state"), Hero->IsRespawnRequestAccepted());
    Notice.Observe(false);
    TestFalse(TEXT("Normal respawn while closed clears skull status"), Notice.bDead);
    Notice.Reopen();
    TestFalse(TEXT("Skull cannot reopen notice after revival"), Notice.bOpen);
    // The existing isolated resurrection route must still work when the notice is dismissed.
    if (!World->GetOutermost()->Rename(TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept"), nullptr, REN_DontCreateRedirectors)) return false;
    Hero->CombatSetHealth(0);
    Notice.Observe(true); Notice.Close();
    Hero->BeginResurrectionOffer(World->GetTimeSeconds() + 10);
    TestTrue(TEXT("Legitimate resurrection accepted with notice closed"), Mode->AcceptResurrection(Hero, Hero->GetResurrectionOfferId(), FString()));
    TestTrue(TEXT("Legitimate resurrection restores living health"), Hero->CombatHealth() > 0);
    Notice.Observe(Hero->CombatHealth() <= 0);
    TestFalse(TEXT("Legitimate resurrection clears skull status"), Notice.bDead);
    TestFalse(TEXT("Legitimate resurrection keeps dismissed notice closed"), Notice.bOpen);
    return true;
}
#endif
