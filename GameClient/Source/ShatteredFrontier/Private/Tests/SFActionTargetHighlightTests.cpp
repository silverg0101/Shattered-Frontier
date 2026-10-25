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
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Misc/ScopeExit.h"
#include "MaterialShared.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFActionTargetHighlightTest, "ShatteredFrontier.Loot.SelectionOutlineCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFActionTargetHighlightTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* Player = World->SpawnActor<ASFPlayerController>(); auto* Pawn = World->SpawnActor<ASFE1Character>();
    Pawn->GetCharacterMovement()->SetMovementMode(MOVE_None); Player->Possess(Pawn);
    // This no-net-driver world needs an actual local-player association, as
    // IsLocalController deliberately rejects unowned authority controllers.
    auto* Local = NewObject<ULocalPlayer>(GEngine); Player->Player = Local; Local->PlayerController = Player;
    ON_SCOPE_EXIT { Player->Player = nullptr; Local->PlayerController = nullptr; };
    if (!TestTrue(TEXT("Fixture has a real local controller"), Player->IsLocalController())) return false;
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
    auto* Resource = Outline->GetMaterialResource(GMaxRHIShaderPlatform);
    if (!TestNotNull(TEXT("Actual RHI outline material resource"), Resource)) return false;
    Resource->FinishCompilation(); // This material only, not a global shader/asset flush.
    for (const FString& Error : Resource->GetCompileErrors()) AddError(Error);
    if (!TestTrue(TEXT("Outline shader compilation succeeds with a real shader map"), Resource->IsCompilationFinished() && Resource->GetGameThreadShaderMap() && Resource->GetCompileErrors().IsEmpty())) return false;
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
    // Refresh must release departed scene attachments even when ownership and
    // the selected actor stay unchanged, without resetting surviving FX state.
    auto* Follower = World->SpawnActor<AActor>();
    auto* Attached = NewObject<UStaticMeshComponent>(Follower); Follower->SetRootComponent(Attached); Attached->RegisterComponent();
    Attached->SetStaticMesh(Plain->GetStaticMesh()); Attached->SetCustomDepthStencilValue(12);
    Attached->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_4);
    Follower->AttachToComponent(Static, FAttachmentTransformRules::KeepWorldTransform);
    Selection->Select(Target);
    TestTrue(TEXT("Attached actor joins the selected hierarchy"), Selection->IsMeshHighlighted(Attached));
    TestEqual(TEXT("Dormant restrictive FX bits remain dormant while selected"), Attached->CustomDepthStencilValue, 128);
    Static->SetCustomDepthStencilValue(131);
    Follower->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Skin->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
    Selection->Select(Target);
    TestTrue(TEXT("Detached actor restores exact dormant triple immediately"), !Attached->bRenderCustomDepth && Attached->CustomDepthStencilValue == 12 && Attached->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_4);
    TestFalse(TEXT("Detached component with retained owner stops highlighting"), Skin->bRenderCustomDepth || Selection->IsMeshHighlighted(Skin));
    TestEqual(TEXT("Surviving selected mesh keeps concurrent FX state during refresh"), Static->CustomDepthStencilValue, 131);
    Selection->Select(nullptr);
    TestTrue(TEXT("Restrictive original mask cannot suppress concurrent lower bits"), Static->bRenderCustomDepth && Static->CustomDepthStencilValue == 3 && Static->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_255);
    Selection->Select(Follower); Attached->SetCustomDepthStencilValue(131); Selection->Select(nullptr);
    TestTrue(TEXT("Concurrent activation of dormant restrictive FX remains effective"), Attached->bRenderCustomDepth && Attached->CustomDepthStencilValue == 3 && Attached->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_255);
    Selection->Select(Follower); Attached->SetCustomDepthStencilValue(130);
    Attached->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_2); Attached->SetRenderCustomDepth(false);
    Selection->Select(nullptr);
    TestTrue(TEXT("Concurrent writer's explicit mask and disabled depth are preserved"), !Attached->bRenderCustomDepth && Attached->CustomDepthStencilValue == 2 && Attached->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_2);
    // Restore the original test fixture state for the independent EndPlay case.
    Static->SetCustomDepthStencilValue(12); Static->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_4);
    Skin->AttachToComponent(Static, FAttachmentTransformRules::KeepWorldTransform);
    Selection->Select(Target); Selection->EndPlay(EEndPlayReason::RemovedFromWorld);
    TestTrue(TEXT("EndPlay restores original stencil/write mask and overlay"), Static->CustomDepthStencilValue == 12 && Static->CustomDepthStencilWriteMask == ERendererStencilMask::ERSM_4 && Static->GetOverlayMaterial() == Overlay);
    TestFalse(TEXT("No outline remains after EndPlay"), Selection->HasSelectionOutline());
    const auto* CustomDepth = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
    if (TestNotNull(TEXT("Custom-depth renderer setting available"), CustomDepth)) TestEqual(TEXT("Shipped stencil pass setting enabled"), CustomDepth->GetInt(), 3);
    return true; // Pixel visibility is separately required by the actual rendered pair.
}
#endif
