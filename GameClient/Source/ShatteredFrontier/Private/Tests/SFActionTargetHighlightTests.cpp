#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Interaction/SFActionTargetComponent.h"
#include "Framework/SFPlayerController.h"
#include "Characters/SFE1Character.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFActionTargetHighlightTest, "ShatteredFrontier.Loot.SelectionOutlineCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFActionTargetHighlightTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* Player = World->SpawnActor<ASFPlayerController>(); auto* Pawn = World->SpawnActor<ASFE1Character>();
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); Player->Possess(Pawn);
    if (!Player->PlayerCameraManager)
    { Player->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(); Player->PlayerCameraManager->InitializeFor(Player); }
    auto* Selection = Player->FindComponentByClass<USFActionTargetComponent>();
    auto* Target = World->SpawnActor<AActor>(); auto* Other = World->SpawnActor<AActor>();
    auto* Static = NewObject<UStaticMeshComponent>(Target); Target->SetRootComponent(Static); Static->RegisterComponent();
    auto* Model = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Frontier/Gameplay/LootItems/wolf_pelt/SM_WolfPelt.SM_WolfPelt"));
    if (!Selection || !Model) return false;
    Static->SetStaticMesh(Model); auto* Base = Static->GetMaterial(0); auto* Overlay = UMaterial::GetDefaultMaterial(MD_Surface);
    Static->SetOverlayMaterial(Overlay); Static->SetRenderCustomDepth(true); Static->SetCustomDepthStencilValue(12);
    Static->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_4);
    auto* Skin = NewObject<USkeletalMeshComponent>(Target); Skin->SetupAttachment(Static); Skin->RegisterComponent();
    Skin->SetSkeletalMeshAsset(Pawn->GetMesh()->GetSkeletalMeshAsset());
    if (!TestNotNull(TEXT("Actual skinned target asset available"), Skin->GetSkeletalMeshAsset())) return false;
    auto* Plain = NewObject<UStaticMeshComponent>(Other); Other->SetRootComponent(Plain); Plain->RegisterComponent();
    Plain->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    const bool bNanite = Model->GetNaniteSettings().bEnabled;
    TestTrue(TEXT("Imported target actually uses Nanite"), bNanite);
    TestFalse(TEXT("Ordinary target actually uses the non-Nanite path"), Plain->GetStaticMesh()->GetNaniteSettings().bEnabled);
    Selection->Select(Target);
    TestTrue(TEXT("Shared custom-depth state covers Nanite and skinned components"), Selection->IsMeshHighlighted(Static) && Selection->IsMeshHighlighted(Skin));
    TestEqual(TEXT("Other FX active write-mask bits retained; dormant bits stay off"), Static->CustomDepthStencilValue, 132);
    TestTrue(TEXT("Base material and preexisting overlay untouched"), Static->GetMaterial(0) == Base && Static->GetOverlayMaterial() == Overlay);
    TestEqual(TEXT("Original asset Nanite setting unchanged"), Model->GetNaniteSettings().bEnabled, bNanite);
    auto* Outline = Selection->GetOutlineMaterial();
    if (!TestNotNull(TEXT("Generated common outline material loaded"), Outline)) return false;
    TestEqual(TEXT("Actual material uses the postprocess renderer path"), EMaterialDomain(Outline->GetMaterial()->MaterialDomain), MD_PostProcess);
    FMinimalViewInfo View; View.PostProcessBlendWeight = .25f; View.PostProcessSettings.bOverride_AutoExposureBias = true; View.PostProcessSettings.AutoExposureBias = 3;
    Player->PlayerCameraManager->ApplyCameraModifiers(.016f, View);
    const TArray<FPostProcessSettings>* Blends = nullptr; const TArray<float>* Weights = nullptr;
    Player->PlayerCameraManager->GetCachedPostProcessBlends(Blends, Weights);
    bool bDrawLayer = false;
    for (int32 I = 0; Blends && Weights && I < Blends->Num(); ++I)
        for (const auto& Blend : (*Blends)[I].WeightedBlendables.Array) bDrawLayer |= (*Weights)[I] > 0 && Blend.Object == Outline && Blend.Weight > 0;
    TestTrue(TEXT("Owning camera actually receives outline blendable"), bDrawLayer);
    TestTrue(TEXT("Existing camera postprocess settings retained"), View.PostProcessBlendWeight == .25f && View.PostProcessSettings.AutoExposureBias == 3 && View.PostProcessSettings.bOverride_AutoExposureBias);
    Selection->Select(Other);
    TestTrue(TEXT("Ordinary static mesh uses the same renderer state"), Selection->IsMeshHighlighted(Plain));
    TestTrue(TEXT("Prior exact FX state restored on retarget"), Static->bRenderCustomDepth && Static->CustomDepthStencilValue == 12 && Static->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_4);
    TestFalse(TEXT("Skinned mesh original disabled depth restored"), Skin->bRenderCustomDepth);
    // Another FX writer changes a lower-bit value while our selection is active.
    Plain->SetCustomDepthStencilValue(USFActionTargetComponent::SelectionStencilBit | 3);
    Selection->Select(nullptr);
    TestTrue(TEXT("Concurrent FX lower bits and depth survive release"), Plain->CustomDepthStencilValue == 3 && Plain->bRenderCustomDepth);
    Selection->Select(Target); Selection->EndPlay(EEndPlayReason::RemovedFromWorld);
    TestTrue(TEXT("EndPlay restores original stencil/write mask and overlay"), Static->CustomDepthStencilValue == 12 && Static->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_4 && Static->GetOverlayMaterial() == Overlay);
    TestFalse(TEXT("No outline remains after EndPlay"), Selection->HasSelectionOutline());
    const auto* CustomDepth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
    if (TestNotNull(TEXT("Custom-depth renderer setting available"), CustomDepth)) TestEqual(TEXT("Shipped stencil pass setting enabled"), CustomDepth->GetInt(), 3);
    return true; // Pixel visibility is separately required by the actual rendered pair.
}
#endif
