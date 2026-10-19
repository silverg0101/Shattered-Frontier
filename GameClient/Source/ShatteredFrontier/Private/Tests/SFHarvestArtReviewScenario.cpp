#include "Gathering/SFGatheringPilot.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Gathering/SFHarvestPresentation.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/PackageName.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "RenderingThread.h"
#include "RHI.h"
#endif
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"
#endif

struct FSFHarvestArtReviewState
{
    double Started=0,StableSince=0,ReplayAt=0;
    FVector LastPosition=FVector::ZeroVector;
    FString Output,Pending;
    int32 Completed=0,Captures=0;
    uint32 LastMeshHash=0;
    uint64 ResourceReadyFrame=0;
    bool bResourcesPrepared=false;
    bool bToolRequested=false,bInitialCaptured=false,bWaitingCommit=false,bReplayed=false,bDone=false;
};

void USFGatheringPilotComponent::TickArtReviewScenario()
{
#if SF_WITH_MINIMAL_INVENTORY
    auto* PC=Cast<APlayerController>(GetOwner());
    if (GetNetMode()!=NM_Client || !PC || !PC->IsLocalController() || !bEnabled || !bRoleAssigned
        || !PC->GetPawn() || !FParse::Param(FCommandLine::Get(),TEXT("HarvestArtReview"))) { return; }
    if (!ArtReviewState) { ArtReviewState=MakeShared<FSFHarvestArtReviewState>(); }
    auto& Review=*ArtReviewState; if (Review.bDone) { return; }
    const double Now=GetWorld()->GetTimeSeconds();
    const auto Fail=[&](const TCHAR* Reason)
    {
        Review.bDone=true; UE_LOG(LogTemp,Error,TEXT("HARVEST_ART_FAILED Reason=%s Cuts=%d Captures=%d"),Reason,Review.Completed,Review.Captures);
        FPlatformMisc::RequestExitWithStatus(false,1);
    };
    if (Review.Started==0)
    {
        Review.Started=Now;
        if (!FParse::Value(FCommandLine::Get(),TEXT("HarvestArtReviewOutput="),Review.Output)
            || FPaths::IsRelative(Review.Output) || !IFileManager::Get().DirectoryExists(*Review.Output)) { Fail(TEXT("OutputDirectory")); return; }
    }
    if (Now-Review.Started>75) { Fail(TEXT("Timeout")); return; }
    ASFGatheringPilotNode* Ore=nullptr;
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld());It;++It) { if (It->Kind==ESFGatheringPilotKind::Iron) { Ore=*It; break; } }
    if (!Ore || Ore->GetState().Capacity==0) { return; }
    if (!Ore->HarvestVisuals || !Ore->HarvestVisuals->IsReady()) { Fail(TEXT("ImportedRockUnavailable")); return; }
    auto* Mesh=Ore->FindComponentByClass<UProceduralMeshComponent>();
    auto* Inventory=PC->FindComponentByClass<UInventoryComponent>();
    if (!Mesh || !Inventory) { return; }
    const auto Quantity=[&]() { return UInventoryTransactionLibrary::GetItemQuantity(Inventory,
        TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre")))); };
    FVector Eye; FRotator EyeRotation; PC->GetPawn()->GetActorEyesViewPoint(Eye,EyeRotation);
    const FVector Aim=Ore->GetActorLocation()+FVector((Review.Completed-1)*18,0,0);
    PC->SetControlRotation((Aim-Eye).Rotation());
    if (!Review.Pending.IsEmpty())
    {
        if (FScreenshotRequest::IsScreenshotRequested() || IFileManager::Get().FileSize(*Review.Pending)<=0) { return; }
        ++Review.Captures; UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_CAPTURE File=%s"),*Review.Pending); Review.Pending.Reset();
    }
    const auto Capture=[&]()
    {
        uint32 Hash=0; int32 Triangles=0,Vertices=0,Debris=0;
        for (int32 SectionIndex=0;SectionIndex<Mesh->GetNumSections();++SectionIndex)
        {
            const auto* Section=Mesh->GetProcMeshSection(SectionIndex); if (!Section) { continue; }
            Vertices+=Section->ProcVertexBuffer.Num(); Triangles+=Section->ProcIndexBuffer.Num()/3;
            for (const auto& Vertex:Section->ProcVertexBuffer) { Hash=HashCombineFast(Hash,GetTypeHash(Vertex.Position)); }
        }
        TArray<UStaticMeshComponent*> Parts; Ore->GetComponents(Parts);
        for (const auto* Part:Parts)
        { if (Part!=Ore->Mesh && !Part->ComponentHasTag(TEXT("HarvestSourceComparison")) && Part->GetStaticMesh() && Part->IsVisible() && Part->GetCollisionEnabled()==ECollisionEnabled::NoCollision) { ++Debris; } }
        if (Triangles<=0 || (Review.Completed>0 && (Hash==Review.LastMeshHash || Debris==0))) { Fail(TEXT("NoDeformationOrDebris")); return; }
        Review.LastMeshHash=Hash;
        UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_STATE Cuts=%d Ore=%lld Revision=%u Charges=%d MeshHash=%u Vertices=%d Triangles=%d Debris=%d"),
            Ore->GetState().Cuts.Num(),Quantity(),Ore->GetState().Revision,Ore->GetState().Charges,Hash,Vertices,Triangles,Debris);
        Review.Pending=FPaths::Combine(Review.Output,FString::Printf(TEXT("rock-cut-%d.png"),Review.Completed));
        if (IFileManager::Get().FileExists(*Review.Pending)) { Fail(TEXT("CaptureAlreadyExists")); return; }
        FScreenshotRequest::RequestScreenshot(Review.Pending,false,false);
    };
    if (!Review.bInitialCaptured)
    {
        if (!HasRequiredTool(ESFGatheringPilotKind::Iron))
        { if (!Review.bToolRequested) { Review.bToolRequested=TryEquipTool(false); } return; }
        const auto* Character=Cast<ACharacter>(PC->GetPawn()); const FVector Position=PC->GetPawn()->GetActorLocation();
        if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetVelocity().IsNearlyZero(1)
            || FVector::DistSquared(Position,Review.LastPosition)>1)
        { Review.StableSince=0; Review.LastPosition=Position; return; }
        if (Review.StableSince==0) { Review.StableSince=Now; }
        if (Now-Review.StableSince<3) { return; }
        if (!Review.bResourcesPrepared)
        {
#if WITH_EDITOR
            // Fixed studio lighting is confined to this offscreen review client.
            // The larger oak canopy otherwise hides the rock in this grey fixture.
            for (TActorIterator<ADirectionalLight> It(GetWorld());It;++It)
            { It->GetLightComponent()->SetIntensity(3.0f); }
            auto* FillActor=GetWorld()->SpawnActor<AActor>();
            auto* Fill=NewObject<UDirectionalLightComponent>(FillActor,TEXT("HarvestReviewFill"));
            FillActor->SetRootComponent(Fill); Fill->SetMobility(EComponentMobility::Movable);
            Fill->SetIntensity(1.5f); Fill->SetCastShadows(false); Fill->SetLightColor(FLinearColor(0.8f,0.9f,1.0f));
            Fill->RegisterComponent(); Fill->SetWorldRotation((Aim-Eye).Rotation());
            if (auto* HUD=PC->GetHUD()) { HUD->bShowHUD=false; }
            TSet<UMaterialInterface*> Materials;
            for (TActorIterator<ASFGatheringPilotNode> It(GetWorld());It;++It)
            {
                TArray<UPrimitiveComponent*> Components; It->GetComponents(Components);
                for (const auto* Component:Components)
                {
                    TArray<UMaterialInterface*> Used; Component->GetUsedMaterials(Used);
                    for (auto* Material:Used) { if (Material) { Materials.Add(Material); } }
                }
            }
            FAssetCompilingManager::Get().FinishAllCompilation();
            TSet<UTexture2D*> Textures;
            for (auto* Material:Materials)
            {
                const auto* Resource=Material->GetMaterial()->GetMaterialResource(GMaxRHIShaderPlatform);
                if (!Resource || !Resource->IsCompilationFinished() || !Resource->GetCompileErrors().IsEmpty())
                { Fail(TEXT("ArtShaderNotReady")); return; }
                TArray<UTexture*> Used; Material->GetUsedTextures(Used);
                for (auto* Texture:Used) { if (auto* TwoD=Cast<UTexture2D>(Texture)) { Textures.Add(TwoD); } }
            }
            if (Textures.IsEmpty()) { Fail(TEXT("NoCompiledArtTextures")); return; }
            for (auto* Texture:Textures)
            {
                Texture->BlockOnAnyAsyncBuild(); Texture->SetForceMipLevelsToBeResident(120.0f);
                Texture->WaitForStreaming();
            }
            FlushRenderingCommands();
            for (auto* Texture:Textures)
            {
                if (Texture->IsDefaultTexture() || !Texture->IsAsyncCacheComplete() || !Texture->GetResource()
                    || Texture->HasPendingInitOrStreaming() || !Texture->IsFullyStreamedIn())
                { Fail(TEXT("ArtTextureNotReady")); return; }
                UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_TEXTURE_READY Asset=%s ResidentMips=%d"),
                    *Texture->GetPathName(),Texture->GetNumResidentMips());
            }
            UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_RESOURCES_READY Materials=%d Textures=%d"),Materials.Num(),Textures.Num());
#endif
            Review.bResourcesPrepared=true; Review.ResourceReadyFrame=GFrameCounter; return;
        }
        if (GFrameCounter<Review.ResourceReadyFrame+2) { return; }
        if (Ore->GetState().Capacity!=3 || Ore->GetState().Charges!=3 || !Ore->GetState().Cuts.IsEmpty() || Quantity()!=0)
        { Fail(TEXT("ExpectedShippedDefaults")); return; }
        if (FParse::Param(FCommandLine::Get(),TEXT("HarvestArtUnlit")))
        {
            if (!GEngine || !GEngine->GameViewport) { Fail(TEXT("DiagnosticViewportMissing")); return; }
            GEngine->GameViewport->SetViewMode(VMI_Unlit);
            UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_UNLIT_DIAGNOSTIC"));
        }
        if (FParse::Param(FCommandLine::Get(),TEXT("HarvestArtCompareNative")))
        {
            auto* Comparison=NewObject<UStaticMeshComponent>(Ore,TEXT("HarvestSourceComparison"));
            const FString ComparisonRoot=FPackageName::DoesPackageExist(TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/SM_HarvestRock_Intact"))
                ?TEXT("/Game/Frontier/Gameplay/Harvest/RockV2/Meshes/"):TEXT("/Game/Frontier/Gameplay/Harvest/V1/Rock/Meshes/");
            Comparison->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,*(ComparisonRoot+TEXT("SM_HarvestRock_Intact.SM_HarvestRock_Intact"))));
            Comparison->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Comparison->ComponentTags.Add(TEXT("HarvestSourceComparison"));
            Comparison->RegisterComponent();
            FTransform Transform=Mesh->GetComponentTransform();
            Transform.AddToTranslation(PC->GetControlRotation().RotateVector(FVector(0,-190,0)));
            Comparison->SetWorldTransform(Transform);
            UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_NATIVE_COMPARISON Side=CameraLeft"));
        }
        Review.bInitialCaptured=true; Capture(); return;
    }
    if (Review.bWaitingCommit)
    {
        if (Feedback.Phase==ESFGatheringPilotPhase::Rejected || Feedback.Phase==ESFGatheringPilotPhase::Canceled)
        { Fail(TEXT("HarvestRejected")); return; }
        if (Feedback.Phase!=ESFGatheringPilotPhase::Committed || Ore->GetState().Cuts.Num()!=Review.Completed+1 || Quantity()!=Review.Completed+1) { return; }
        ++Review.Completed; Review.bWaitingCommit=false; Capture(); return;
    }
    if (Review.Completed<3)
    {
        FVector Contact,Normal;
        if (!Ore->HarvestVisuals->TraceSurface(Eye,(Aim-Eye).GetSafeNormal(),Contact,Normal)
            || !InteractFromRay(Eye,(Contact-Eye).GetSafeNormal()) || !bWaitingHadSurface)
        { Fail(TEXT("SurfaceInteraction")); return; }
        Review.bWaitingCommit=true; return;
    }
    if (!Review.bReplayed)
    {
        ServerGatheringActionAtSurface(Ore,WaitingRevision,Feedback.Operation,WaitingSurface);
        Review.bReplayed=true; Review.ReplayAt=Now; return;
    }
    if (Now-Review.ReplayAt<1) { return; }
    if (Quantity()!=3 || Ore->GetState().Cuts.Num()!=3 || Ore->GetState().Revision!=3 || Ore->GetState().Charges!=0
        || Review.Captures!=4 || Feedback.Phase!=ESFGatheringPilotPhase::Committed || bKeyConflict)
    { Fail(TEXT("FinalOrReplayInvariant")); return; }
    Review.bDone=true; UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_DONE Ore=3 Cuts=3 Revision=3 Charges=0 Captures=4 ReplayUnchanged=1"));
    FPlatformMisc::RequestExitWithStatus(false,0);
#endif
}
#endif
