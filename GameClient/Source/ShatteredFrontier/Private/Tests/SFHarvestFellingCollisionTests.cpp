#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFHarvestFellingCollisionTest,
    "ShatteredFrontier.Gathering.FellingCollision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFHarvestFellingCollisionTest::RunTest(const FString& Parameters)
{
    auto* Source=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakV1/Meshes/SM_MightyOak_UpperStructure.SM_MightyOak_UpperStructure"));
    auto* Candidate=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Frontier/Gameplay/Harvest/MightyOakFallV2/SM_MightyOak_FallCollision.SM_MightyOak_FallCollision"));
    if (!TestNotNull(TEXT("Original source"),Source)||!TestNotNull(TEXT("Collision candidate"),Candidate)) { return false; }
    auto* Setup=Candidate->GetBodySetup();
    if (!TestNotNull(TEXT("Baked collision body"),Setup)) { return false; }
    TestTrue(TEXT("Separate collision package owns a separate body"),Setup!=Source->GetBodySetup());
    TestTrue(TEXT("Convex simulation does not use complex-as-simple"),Setup->CollisionTraceFlag!=CTF_UseComplexAsSimple);
    const auto& Hulls=Setup->AggGeom.ConvexElems;
    if (!TestTrue(TEXT("Bounded major-limb convex pieces"),Hulls.Num()>0&&Hulls.Num()<=64)) { return false; }
    FString Json=TEXT("{\n  \"schema\": \"harvest-oak-native-collision-export-v1\",\n  \"units\": \"centimetres\",\n  \"hulls\": [\n");
    for (int32 H=0;H<Hulls.Num();++H)
    {
        const auto& Hull=Hulls[H];
        if (!TestTrue(TEXT("Each source hull has a bounded point count"),Hull.VertexData.Num()>=4&&Hull.VertexData.Num()<=64)) { return false; }
        Json+=TEXT("    {\"vertices_cm\": [");
        for (int32 I=0;I<Hull.VertexData.Num();++I)
        {
            const FVector V=Hull.GetTransform().TransformPosition(Hull.VertexData[I]);
            if (!TestFalse(TEXT("Finite collision position"),V.ContainsNaN())) { return false; }
            Json+=FString::Printf(TEXT("%s[%.9g,%.9g,%.9g]"),I?TEXT(","):TEXT(""),V.X,V.Y,V.Z);
        }
        Json+=TEXT("],\"triangle_indices\":[");
        for (int32 I=0;I<Hull.IndexData.Num();++I)
        { Json+=FString::Printf(TEXT("%s%d"),I?TEXT(","):TEXT(""),Hull.IndexData[I]); }
        Json+=FString::Printf(TEXT("]}%s\n"),H+1<Hulls.Num()?TEXT(","):TEXT(""));
    }
    Json+=TEXT("  ]\n}\n");
    const FString Directory=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("../Artifacts/HarvestArt/mighty-felling-v2-native")));
    IFileManager::Get().MakeDirectory(*Directory,true);
    TestTrue(TEXT("Export actual transformed native hulls for visual review"),FFileHelper::SaveStringToFile(Json,*FPaths::Combine(Directory,TEXT("collision-hulls.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    AddInfo(TEXT("Exports actual baked physics shapes. Convex validity alone does not accept branch fit or falling behavior."));
    return true;
}
#endif
