#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCombatPilot.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFGameMode.h"
#include "Services/SFBackpackComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcPresetRuntimeTest,
    "ShatteredFrontier.Combat.NpcReuse.BehaviorAndPresentationPresets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFNpcPresetRuntimeTest::RunTest(const FString& Parameters)
{
    const FString Root = FPaths::ProjectDir() / TEXT("..");
    const FString Fixture = Root / TEXT("tests/fixtures/npc-profiles");
    const FString Rules = Root / TEXT("content/creatures/loot_rules");
    const FString Scratch = FPaths::ProjectSavedDir() / TEXT("NpcPresetTests") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*Scratch, true);
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Scratch, false, true); };
    TArray<FString> Files; IFileManager::Get().FindFiles(Files, *(Rules / TEXT("*.json")), true, false);
    for (const auto& File : Files) IFileManager::Get().Copy(*(Scratch / File), *(Rules / File));
    IFileManager::Get().Copy(*(Scratch / TEXT("reuse_archer_fixture.json")), *(Fixture / TEXT("loot_rules/reuse_archer_fixture.json")));
    TMap<FString, SFNpcProfiles::FProfile> Profiles;
    FString Error;
    if (!TestTrue(TEXT("Typed preset references validate"), SFNpcProfiles::Load(Fixture / TEXT("preset-variation.json"), Scratch, Profiles, Error))) return false;
    FString Source; FFileHelper::LoadFileToString(Source, *(Fixture / TEXT("preset-variation.json")));
    const TPair<FString, FString> Invalid[] = {
        {TEXT("\"schema_version\": 3"), TEXT("\"schema_version\": 2")},
        {TEXT("\"behavior_preset\": \"fixture\""), TEXT("\"behavior_preset\": \"unknown\"")},
        {TEXT("\"presentation_preset\": \"fixture\""), TEXT("\"presentation_preset\": \"unknown\"")},
        {TEXT("\"aggro_radius_metres\": 4.0"), TEXT("\"aggro_radius_metres\": 0.0")},
        {TEXT("\"chase_speed_metres_per_second\": 1.3"), TEXT("\"chase_speed_metres_per_second\": 100.0")},
        {TEXT("\"assist_radius_metres\": 4.0"), TEXT("\"assist_radius_metres\": -1.0")},
        {TEXT("\"preferred_range_metres\": 6.0"), TEXT("\"preferred_range_metres\": 10.0")},
        {TEXT("\"static_mesh\": \"/Engine/BasicShapes/Cube.Cube\""), TEXT("\"static_mesh\": \"../outside\"")},
        {TEXT("\"capsule_radius_cm\": 55.0"), TEXT("\"capsule_radius_cm\": 66.0")},
        {TEXT("\"animation_profile\": \"none\""), TEXT("\"animation_profile\": \"unimplemented\"")}
    };
    for (const auto& Case : Invalid)
    {
        TestTrue(TEXT("Negative mutation applies"), Source.Contains(Case.Key));
        FFileHelper::SaveStringToFile(Source.Replace(*Case.Key, *Case.Value), *(Scratch / TEXT("invalid.json")));
        TestFalse(TEXT("Malformed preset cannot replace catalogue"), SFNpcProfiles::Load(Scratch / TEXT("invalid.json"), Scratch, Profiles, Error));
        TestEqual(TEXT("Last valid catalogue retained"), Profiles.Num(), 7);
    }
    SFNpcProfiles::FScopedTestCatalogue Catalogue(Profiles);
    FSFScopedGameplayServicesForTests Offline(nullptr);
    FTestWorldWrapper TestWorld;
    if (!TestWorld.CreateTestWorld(EWorldType::GameRPC) || !TestWorld.BeginPlayInTestWorld()) return false;
    UWorld* World = TestWorld.GetTestWorld();
    auto* Mode = World->SpawnActor<ASFGameMode>();
    if (!Mode) return false;
    Mode->CapturedSettings = GetDefault<USFCombatPilotSettings>(); Mode->bPilotActive = true;
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Npc = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector(0, 0, 100), FRotator::ZeroRotator, Spawn);
    auto* Controller = World->SpawnActor<APlayerController>();
    auto* Victim = World->SpawnActor<ASFE1Character>(ASFE1Character::StaticClass(), FVector(1500, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!Npc || !Controller || !Victim) return false;
    Controller->Possess(Victim); Victim->CombatSetHealth(100);
    const TArray<FVector2D> Territory{FVector2D(-100,-100), FVector2D(100,-100), FVector2D(100,100), FVector2D(-100,100)};
    auto Reset = [&](const TCHAR* Definition)
    {
        Npc->SetActorLocation(FVector(0, 0, 100)); Npc->SetActorRotation(FRotator::ZeroRotator);
        Npc->ConfigureCreature(Definition); Npc->Initialize(Mode, TEXT("preset_group"), 0, Territory, 100, 1);
    };
    Reset(TEXT("npc/reuse_archer_fixture"));
    auto* Body = Npc->FindComponentByClass<UStaticMeshComponent>();
    TestTrue(TEXT("Configured profile applies its body transform"), Body && Body->GetRelativeScale3D().Equals(FVector(.7,.7,1.3)));
    TestTrue(TEXT("Canonical mesh reference is consumed"), Body && Body->GetStaticMesh() && Body->GetStaticMesh()->GetPathName() == Npc->GetNpcProfile()->Presentation.StaticMeshPath.ToString());
    TestEqual(TEXT("Canonical collision size retained"), Npc->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), 55.f);
    Mode->TryBoarAttack(Npc, Victim);
    TestEqual(TEXT("Authored short reach rejects actual 15m shot"), Victim->CombatHealth(), 100);
    Npc->Tick(0);
    TestFalse(TEXT("Authored short perception cannot acquire 15m target"), Npc->HasThreatFor(Victim));
    Npc->ReceivePackAssist(Victim, TEXT("preset_target")); Npc->Tick(1);
    TestTrue(TEXT("Actual chase uses 1.3m/s preset"), FMath::IsNearlyEqual(Npc->GetActorLocation().X, 130.0, .1));
    Npc->SetActorLocation(FVector(2100, 0, 100)); Npc->Tick(0);
    TestTrue(TEXT("Actual leash uses authored 20m radius"), Npc->IsLeashing());
    Reset(TEXT("npc/bandit_archer")); Npc->Tick(0);
    TestTrue(TEXT("Same target acquired through shipped perception preset"), Npc->HasThreatFor(Victim));
    TestTrue(TEXT("Changing profile restores shipped body transform"), Body->GetRelativeScale3D().Equals(FVector(1.3,.7,.7)));
    Mode->TryBoarAttack(Npc, Victim);
    TestTrue(TEXT("Same 15m shot accepted through shipped reach preset"), Victim->CombatHealth() < 100);
    Reset(TEXT("npc/bandit_archer")); Npc->ReceivePackAssist(Victim, TEXT("preset_target")); Npc->Tick(1);
    TestTrue(TEXT("Same chase uses shipped 3.5m/s preset"), FMath::IsNearlyEqual(Npc->GetActorLocation().X, 350.0, .1));
    Npc->SetActorLocation(FVector(2100, 0, 100)); Npc->Tick(0);
    TestFalse(TEXT("Shipped 60m leash permits same position"), Npc->IsLeashing());
    Victim->CombatSetHealth(100);
    auto* Packmate = World->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), FVector(300, 0, 100), FRotator::ZeroRotator, Spawn);
    if (!Packmate) return false;
    Packmate->Initialize(Mode, TEXT("preset_group"), 1, Territory, 100, 1, true);
    auto AdvanceAssistTime = [&]
    {
        const double Before = World->GetTimeSeconds();
        for (int32 Step = 0; Step < 25; ++Step) World->Tick(LEVELTICK_TimeOnly, .1f);
        TestTrue(TEXT("Actual authority clock advances between both assist delays"),
            World->GetTimeSeconds() - Before > 2 && World->GetTimeSeconds() - Before < 3);
    };
    Reset(TEXT("npc/reuse_archer_fixture")); Npc->ReceivePackAssist(Victim, TEXT("preset_target"));
    AdvanceAssistTime(); Npc->Tick(0);
    TestFalse(TEXT("Actual pack assist waits authored three seconds"), Packmate->HasThreatFor(Victim));
    Reset(TEXT("npc/bandit_archer")); Npc->ReceivePackAssist(Victim, TEXT("preset_target"));
    AdvanceAssistTime(); Npc->Tick(0);
    TestTrue(TEXT("Same packmate assisted after shipped two seconds"), Packmate->HasThreatFor(Victim));
    UStaticMesh* ValidBody = Body->GetStaticMesh();
    const FString ValidDefinition = Npc->GetCreatureId();
    const FVector ValidScale = Body->GetRelativeScale3D();
    const FString BadAssets[] = {TEXT("/Engine/BasicShapes/Missing.Missing"), TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")};
    for (const FString& BadAsset : BadAssets)
    {
        FFileHelper::SaveStringToFile(Source.Replace(TEXT("/Engine/BasicShapes/Cube.Cube"), *BadAsset), *(Scratch / TEXT("bad-asset.json")));
        TestFalse(TEXT("Missing and wrong-type mesh assets reject the catalogue"), SFNpcProfiles::Load(Scratch / TEXT("bad-asset.json"), Scratch, Profiles, Error));
        TestTrue(TEXT("Asset admission error identifies failed reference"), Error.Contains(BadAsset));
        TestEqual(TEXT("Asset rejection preserves valid catalogue"), Profiles.Num(), 7);
        // Simulate an asset-stripped server-admitted descriptor reaching a rendering consumer.
        auto& Alternate = Profiles[TEXT("npc/reuse_archer_fixture")];
        const FSoftObjectPath ValidPath = Alternate.Presentation.StaticMeshPath;
        Alternate.Presentation.StaticMeshPath = FSoftObjectPath(BadAsset);
        AddExpectedError(TEXT("NPC_PRESENTATION_UNAVAILABLE"), EAutomationExpectedErrorFlags::Contains, 1);
        TestFalse(TEXT("Actual ConfigureCreature rejects unusable body asset"), Npc->ConfigureCreature(TEXT("npc/reuse_archer_fixture")));
        TestEqual(TEXT("Failed configuration retains authoritative definition"), Npc->GetCreatureId(), ValidDefinition);
        TestTrue(TEXT("Failed configuration retains visible body and transform"), Body->GetStaticMesh() == ValidBody && Body->GetRelativeScale3D().Equals(ValidScale));
        Alternate.Presentation.StaticMeshPath = ValidPath;
    }
    return true;
}
#endif
