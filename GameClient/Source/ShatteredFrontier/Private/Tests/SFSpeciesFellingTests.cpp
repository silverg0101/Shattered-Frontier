#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Gathering/SFSpeciesFelling.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Every tree and rock a gathering node can show has a felling path: the look profile's harvestable roles (pines, young
// broadleaf, broadleaf/oak, boulders, bedrock, rocks) and each of their dressing meshes take the generic method.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSpeciesFellingPathTest, "ShatteredFrontier.Gathering.SpeciesFellingPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSpeciesFellingPathTest::RunTest(const FString& Parameters)
{
    const auto* Settings=GetDefault<USFSpeciesFellingSettings>();
    TestTrue(TEXT("felling settings are within their bounds"),Settings->Validate());

    // The motion: starts still, ends at the final angle, never runs backwards; the landing hop settles to zero.
    float Previous=-1;
    for (int32 I=0;I<=20;++I)
    {
        const float Angle=SFSpeciesFelling::FallAngleDegrees(I/20.f,Settings->FallFinalDegrees);
        TestTrue(TEXT("fall angle never decreases"),Angle>=Previous); Previous=Angle;
    }
    TestEqual(TEXT("fall starts upright"),SFSpeciesFelling::FallAngleDegrees(0,80),0.f);
    TestEqual(TEXT("fall ends at the final angle"),SFSpeciesFelling::FallAngleDegrees(1,80),80.f);
    TestEqual(TEXT("landing hop has settled"),SFSpeciesFelling::LandingBounceDegrees(0.7f),0.f);

    FString Text; TSharedPtr<FJsonObject> Profile;
    const FString ProfilePath=FPaths::Combine(FPaths::ProjectDir(),TEXT("ArtSource/Greenlands/greenlands-look-profile.json"));
    if (!FFileHelper::LoadFileToString(Text,*ProfilePath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Profile)||!Profile.IsValid())
    { AddError(TEXT("look profile unreadable")); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Layers=nullptr;
    if (!Profile->TryGetArrayField(TEXT("layers"),Layers)) { AddError(TEXT("look profile has no layers")); return false; }

    TMap<FString,TArray<FString>> Have; TMap<FString,FString> Fallback, Harvest;
    for (const TSharedPtr<FJsonValue>& Value:*Layers)
    {
        const TSharedPtr<FJsonObject> Layer=Value->AsObject(); if (!Layer.IsValid()) { continue; }
        const FString Role=Layer->GetStringField(TEXT("role"));
        TArray<FString>& Names=Have.FindOrAdd(Role);
        const TArray<TSharedPtr<FJsonValue>>* HaveField=nullptr;
        if (Layer->TryGetArrayField(TEXT("have"),HaveField)) { for (const auto& Name:*HaveField) { Names.Add(Name->AsString()); } }
        const TSharedPtr<FJsonObject>* Place=nullptr;
        if (Layer->TryGetObjectField(TEXT("place"),Place))
        {
            FString Value2;
            if ((*Place)->TryGetStringField(TEXT("harvest"),Value2)) { Harvest.Add(Role,Value2); }
            if ((*Place)->TryGetStringField(TEXT("fallback"),Value2)) { Fallback.Add(Role,Value2); }
        }
    }
    TestTrue(TEXT("the profile names harvestable roles"),Harvest.Num()>=6);

    FARFilter Filter; Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(TEXT("/Game/Frontier")); Filter.bRecursivePaths=true;
    TArray<FAssetData> Assets;
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssets(Filter,Assets);
    TMap<FString,FAssetData> ByName; for (const FAssetData& Asset:Assets) { ByName.Add(Asset.AssetName.ToString(),Asset); }

    int32 Checked=0;
    for (const TPair<FString,FString>& Role:Harvest)
    {
        const bool bTree=Role.Value==TEXT("wood");
        TestTrue(FString::Printf(TEXT("%s harvest kind %s has a felling path"),*Role.Key,*Role.Value),bTree||Role.Value==TEXT("stone"));
        TArray<FString> Names=Have[Role.Key];
        if (Names.IsEmpty()&&Fallback.Contains(Role.Key)) { Names=Have.FindRef(Fallback[Role.Key]); }
        TestTrue(FString::Printf(TEXT("%s has dressing meshes"),*Role.Key),!Names.IsEmpty());
        for (const FString& Name:Names)
        {
            const FAssetData* Asset=ByName.Find(Name);
            if (!Asset) { AddError(FString::Printf(TEXT("%s (%s): mesh not found under /Game/Frontier"),*Name,*Role.Key)); continue; }
            const UStaticMesh* Mesh=Cast<UStaticMesh>(Asset->GetAsset());
            FSFSpeciesFellingPlan Plan; FString Why;
            if (!SFSpeciesFelling::MakePlan(Mesh,bTree,Plan,&Why)) { AddError(FString::Printf(TEXT("%s (%s): no felling plan: %s"),*Name,*Role.Key,*Why)); continue; }
            TArray<SFSpeciesFelling::FSpot> Spots; SFSpeciesFelling::PlanNotches(*Settings,Plan,0x5EED,Settings->MaxNotches,Spots);
            TestEqual(FString::Printf(TEXT("%s notch count"),*Name),Spots.Num(),Settings->MaxNotches);
            for (const auto& Spot:Spots)
            {
                TestTrue(FString::Printf(TEXT("%s notch is on the mesh"),*Name),FMath::IsFinite(Spot.YawRadians)&&Spot.HeightCm>0&&Spot.HeightCm<=Plan.HeightCm);
            }
            const float Seconds=SFSpeciesFelling::FallSeconds(*Settings,Plan.HeightCm);
            TestTrue(FString::Printf(TEXT("%s fall time is bounded"),*Name),Seconds>=0.3f&&Seconds<=8.f);
            TestTrue(FString::Printf(TEXT("%s has a trunk/footprint radius"),*Name),Plan.TrunkRadiusCm>0);
            ++Checked;
        }
    }
    TestTrue(TEXT("meshes were checked"),Checked>=6);
    return true;
}
#endif
