#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Combat/SFNpcZoneContent.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcContentSpawnShotTest,
    "ShatteredFrontier.Combat.NpcReuse.ContentSpawnAndShot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcContentSpawnShotTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."));
    const FString FixtureDirectory = Root / TEXT("tests/fixtures/npc-profiles");
    const FString Rules = Root / TEXT("content/creatures/loot_rules");
    const FString Scratch = FPaths::ProjectSavedDir() / TEXT("NpcReuseTests") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*Scratch, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Scratch, false, true); };
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Rules / TEXT("*.json")), true, false);
    for (const auto& File : Files) IFileManager::Get().Copy(*(Scratch / File), *(Rules / File));
    IFileManager::Get().Copy(*(Scratch / TEXT("reuse_archer_fixture.json")),
        *(FixtureDirectory / TEXT("loot_rules/reuse_archer_fixture.json")));
    TMap<FString, SFNpcProfiles::FProfile> Profiles;
    FString Error;
    if (!TestTrue(TEXT("Seven authored profiles validate"), SFNpcProfiles::Load(
        FixtureDirectory / TEXT("profiles.json"), Scratch, Profiles, Error))) return false;
    SFNpcProfiles::FScopedTestCatalogue Catalogue(Profiles);
    SFNpcZoneContent::FZone Zone;
    const FString MapId = TEXT("/Game/Frontier/Maps/Sandbox/SB_NpcReuseFixture");
    if (!TestTrue(TEXT("Arbitrary group loads from authored zone"), SFNpcZoneContent::Load(
        FixtureDirectory / TEXT("zone.json"), MapId, Zone, Error))) return false;
    TestEqual(TEXT("No six-group census constraint"), Zone.Groups.Num(), 1);
    TestTrue(TEXT("Unlisted maps cannot borrow a zone"), SFNpcZoneContent::FindMapFile(MapId).IsEmpty());
    TestFalse(TEXT("Mismatched map rejected"), SFNpcZoneContent::Load(
        FixtureDirectory / TEXT("zone.json"), TEXT("/Game/Frontier/Maps/WrongMap"), Zone, Error));
    FString Source;
    FFileHelper::LoadFileToString(Source, *(FixtureDirectory / TEXT("zone.json")));
    FFileHelper::SaveStringToFile(Source.Replace(TEXT("npc/reuse_archer_fixture"), TEXT("npc/unknown")), *(Scratch / TEXT("invalid-zone.json")));
    TestFalse(TEXT("Unknown zone definition rejected"), SFNpcZoneContent::Load(Scratch / TEXT("invalid-zone.json"), MapId, Zone, Error));
    TestEqual(TEXT("Rejected zone preserves previous snapshot"), Zone.Groups.Num(), 1);
    FTestWorldWrapper WorldFixture;
    if (!WorldFixture.CreateTestWorld(EWorldType::GameRPC) || !WorldFixture.BeginPlayInTestWorld()) return false;
    UWorld* World = WorldFixture.GetTestWorld();
    ASFGameMode* Mode = World->SpawnActor<ASFGameMode>();
    if (!TestNotNull(TEXT("Authoritative gameplay mode"), Mode)) return false;
    Mode->CapturedSettings = GetDefault<USFCombatPilotSettings>();
    Mode->bPilotActive = true;
    Mode->Groups = Zone.Groups;
    Mode->ProtectedAreas = Zone.ProtectedAreas;
    Mode->RespawnPoints = Zone.RespawnPoints;
    Mode->SpawnSlot(TEXT("rangers_across_the_river"), 0, 0, TEXT("ranger_clearing"), 0, 0, 0, TEXT("npc/unknown"));
    TestTrue(TEXT("Slot cannot spawn undeclared/unknown definition"), Mode->ActiveBoars.IsEmpty());
    Mode->SpawnSlot(TEXT("rangers_across_the_river"), 1, 0, TEXT("ranger_clearing"), 0, 0, 0, TEXT("npc/reuse_archer_fixture"));
    TestTrue(TEXT("Slot outside declared census is rejected"), Mode->ActiveBoars.IsEmpty());
    Mode->SpawnSlot(TEXT("rangers_across_the_river"), 0, 0, TEXT("ranger_clearing"), 0, 0, 0, TEXT("npc/reuse_archer_fixture"));
    ASFPilotBoar* Npc = Mode->ActiveBoars.FindRef(TEXT("rangers_across_the_river:0")).Get();
    if (!TestNotNull(TEXT("Seventh profile spawned by real slot path"), Npc)) return false;
    TestTrue(TEXT("Defensive behavior reads authored point, independent of group name"), Npc->IsDefensive());
    TestTrue(TEXT("New definition selects ranged actor behavior"), Npc->IsRangedArcher());
    ASFE1Character* Victim = World->SpawnActor<ASFE1Character>(Npc->GetActorLocation() + FVector(1200, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Live damage recipient"), Victim)) return false;
    Victim->CombatSetHealth(100);
    Mode->TryBoarAttack(Npc, Victim);
    TestTrue(TEXT("Seventh content-only actor fires ranged shot and damages beyond melee reach"), Victim->CombatHealth() < 100);
    const int32 AfterShot = Victim->CombatHealth();
    TestTrue(TEXT("Melee control configures existing profile"), Npc->ConfigureCreature(TEXT("npc/boar")));
    Mode->TryBoarAttack(Npc, Victim);
    TestEqual(TEXT("Same distance rejects melee control"), Victim->CombatHealth(), AfterShot);
    return true;
}
#endif
