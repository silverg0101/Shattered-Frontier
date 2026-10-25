#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
// Both public paths are deliberate: this also compiles an unchanged old source consumer.
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCombatNpc.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "UObject/SoftObjectPath.h"
#include <type_traits>

static_assert(std::is_same_v<ASFPilotBoar, ASFCombatNpc>, "Old source type remains the same generic actor");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFNpcRenameCompatibilityTest,
    "ShatteredFrontier.Combat.NpcReuse.LegacyClassAndMapCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFNpcRenameCompatibilityTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Canonical reflected path"), ASFCombatNpc::StaticClass()->GetPathName(),
        FString(TEXT("/Script/ShatteredFrontier.SFCombatNpc")));
    FSoftClassPath LegacyPath(TEXT("/Script/ShatteredFrontier.SFPilotBoar"));
    // Editor-constructed strings have no archive PostLoadPath; apply the native soft-path redirect fixup explicitly.
    TestTrue(TEXT("Old native soft path is redirected"), LegacyPath.FixupCoreRedirects());
    TestEqual(TEXT("Old serialized soft class resolves through redirect"),
        LegacyPath.TryLoadClass<AActor>(), ASFCombatNpc::StaticClass());
    TestEqual(TEXT("Old source StaticClass resolves to canonical class"),
        ASFPilotBoar::StaticClass(), ASFCombatNpc::StaticClass());

    struct FLegacyMap { const TCHAR* Name; const TCHAR* Tag; int32 Count; };
    const FLegacyMap Maps[] = {
        { TEXT("SB_Companions"), TEXT("SF_CompanionSandboxWildlife"), 3 },
        { TEXT("SB_OutOfPlaceNpc"), TEXT("SF_OutOfPlaceNpcSandboxWildlife"), 1 }
    };
    for (const auto& Map : Maps)
    {
        const FString PackagePath = FString(TEXT("/Game/Frontier/Maps/Sandbox/")) + Map.Name;
        TArray<uint8> Bytes;
        const FString File = FPaths::ProjectContentDir() / TEXT("Frontier/Maps/Sandbox") / (FString(Map.Name) + TEXT(".umap"));
        if (!TestTrue(TEXT("Existing shipped map bytes available"), FFileHelper::LoadFileToArray(Bytes, *File))) return false;
        const ANSICHAR OldClass[] = "SFPilotBoar";
        bool bOldImport = false;
        for (int32 I = 0; I + sizeof(OldClass) - 1 <= Bytes.Num(); ++I)
            if (FMemory::Memcmp(Bytes.GetData() + I, OldClass, sizeof(OldClass) - 1) == 0) { bOldImport = true; break; }
        TestTrue(TEXT("Map still contains old serialized class import; no asset resave"), bOldImport);
        UPackage* Package = LoadPackage(nullptr, *PackagePath, LOAD_None);
        UWorld* MapWorld = Package ? UWorld::FindWorldInPackage(Package) : nullptr;
        if (!TestNotNull(*FString::Printf(TEXT("Legacy map loads: %s"), Map.Name), MapWorld)) return false;
        int32 Count = 0;
        for (AActor* Actor : MapWorld->PersistentLevel->Actors)
        {
            ASFCombatNpc* Npc = Cast<ASFCombatNpc>(Actor);
            if (!Npc || !Npc->ActorHasTag(FName(Map.Tag))) continue;
            ++Count;
            TestEqual(TEXT("Serialized actor now has the generic class"), Npc->GetClass(), ASFCombatNpc::StaticClass());
            TestEqual(TEXT("Serialized canonical creature survives"), Npc->GetCreatureId(), FString(TEXT("npc/boar")));
            auto* Body = FindObject<UStaticMeshComponent>(Npc, TEXT("BoarBody"));
            TestNotNull(TEXT("Old serialized body subobject identity survives"), Body);
            if (FCString::Strcmp(Map.Name, TEXT("SB_OutOfPlaceNpc")) == 0)
                TestTrue(TEXT("Special authored tag survives class redirect"), Npc->ActorHasTag(ASFCombatNpc::OutOfPlaceNpcTag));
        }
        TestEqual(TEXT("All old authored NPCs load as generic actors"), Count, Map.Count);
    }

    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    // Old source SpawnActor/Cast calls and new class calls operate on exactly one implementation.
    ASFPilotBoar* Npc = Fixture.GetTestWorld()->SpawnActor<ASFPilotBoar>();
    if (!TestNotNull(TEXT("Old source caller can spawn the new reflected actor"), Npc)) return false;
    TestEqual(TEXT("New spawns use the canonical class"), Npc->GetClass(), ASFCombatNpc::StaticClass());
    TestNotNull(TEXT("Old source Cast remains compatible"), Cast<ASFPilotBoar>(static_cast<AActor*>(Npc)));
    TestTrue(TEXT("Generic spawned actor configures a different canonical species"), Npc->ConfigureCreature(TEXT("npc/wolf")));
    TestEqual(TEXT("Generic runtime profile selected"), Npc->GetCreatureId(), FString(TEXT("npc/wolf")));
    return true;
}
#endif
