#include "Gathering/SFGatheringPilot.h"
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Gathering/SFHarvestPresentation.h"
#include "Gathering/SFHarvestFallingTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
#include "Inventory/SFInventory.h"

#if WITH_EDITOR
// Finishes shaders and streams every texture used by gathering nodes and their falling trees, so a review frame never
// shows proxy materials or low mips. Returns the failure reason, or nullptr when ready.
static const TCHAR* PrepareHarvestReviewResources(UWorld* World,const AActor* Extra=nullptr)
{
    TSet<UMaterialInterface*> Materials;
    const auto Collect=[&Materials](const AActor* Actor)
    {
        TArray<UPrimitiveComponent*> Components; Actor->GetComponents(Components);
        for (const auto* Component:Components)
        {
            TArray<UMaterialInterface*> Used; Component->GetUsedMaterials(Used);
            for (auto* Material:Used) { if (Material) { Materials.Add(Material); } }
        }
    };
    for (TActorIterator<ASFGatheringPilotNode> It(World);It;++It) { Collect(*It); }
    if (Extra) { Collect(Extra); }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TSet<UTexture2D*> Textures;
    for (auto* Material:Materials)
    {
        const auto* Resource=Material->GetMaterial()->GetMaterialResource(GMaxRHIShaderPlatform);
        if (!Resource || !Resource->IsCompilationFinished() || !Resource->GetCompileErrors().IsEmpty()) { return TEXT("ArtShaderNotReady"); }
        TArray<UTexture*> Used; Material->GetUsedTextures(Used);
        for (auto* Texture:Used) { if (auto* TwoD=Cast<UTexture2D>(Texture)) { Textures.Add(TwoD); } }
    }
    if (Textures.IsEmpty()) { return TEXT("NoCompiledArtTextures"); }
    for (auto* Texture:Textures)
    {
        Texture->BlockOnAnyAsyncBuild(); Texture->SetForceMipLevelsToBeResident(120.0f);
        Texture->WaitForStreaming();
    }
    FlushRenderingCommands();
    for (auto* Texture:Textures)
    {
        if (Texture->IsDefaultTexture() || !Texture->IsAsyncCacheComplete() || !Texture->GetResource()
            || Texture->HasPendingInitOrStreaming() || !Texture->IsFullyStreamedIn()) { return TEXT("ArtTextureNotReady"); }
        UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_TEXTURE_READY Asset=%s ResidentMips=%d"),*Texture->GetPathName(),Texture->GetNumResidentMips());
    }
    UE_LOG(LogTemp,Display,TEXT("HARVEST_ART_RESOURCES_READY Materials=%d Textures=%d"),Materials.Num(),Textures.Num());
    return nullptr;
}
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
    auto* Inventory=PC->FindComponentByClass<USFInventoryComponent>();
    if (!Mesh || !Inventory) { return; }
    const auto Quantity=[&]() { return SFInventoryTransactions::GetItemQuantity(Inventory,
        TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre")))); };
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
            if (const TCHAR* Failure=PrepareHarvestReviewResources(GetWorld())) { Fail(Failure); return; }
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
}

struct FSFHarvestFellingReviewState
{
    double Started=0,StableSince=0,RestSince=0;
    FVector LastPosition=FVector::ZeroVector,Base=FVector::ZeroVector,FallDir=FVector::ForwardVector,Side=FVector::RightVector;
    FString Output,Pending;
    TWeakObjectPtr<ACameraActor> Camera;
    uint64 WaitUntilFrame=0;
    int32 Completed=0,NextLean=0,CloseUps=0,Shots=0;
    bool bToolRequested=false,bWaiting=false,bPrepared=false,bNotched=false,bDone=false;
};

// Rendered client of a real dedicated-server felling (tools/Test-HarvestFellingReview.ps1): three owned strikes on the
// map's oak, then timed 1920x1080 frames by lean angle, the supported rest, and stump/log close-ups. Collection only.
void USFGatheringPilotComponent::TickFellingReviewScenario()
{
#if WITH_EDITOR
    auto* PC=Cast<APlayerController>(GetOwner());
    if (GetNetMode()!=NM_Client || !PC || !PC->IsLocalController() || !bEnabled || !bRoleAssigned
        || !PC->GetPawn() || !FParse::Param(FCommandLine::Get(),TEXT("HarvestFellingReview"))) { return; }
    if (!FellingReviewState) { FellingReviewState=MakeShared<FSFHarvestFellingReviewState>(); }
    auto& R=*FellingReviewState; if (R.bDone) { return; }
    const double Now=GetWorld()->GetTimeSeconds();
    const auto Fail=[&](const TCHAR* Reason)
    {
        R.bDone=true; UE_LOG(LogTemp,Error,TEXT("HARVEST_FELLING_REVIEW_FAILED Reason=%s Cuts=%d Shots=%d"),Reason,R.Completed,R.Shots);
        FPlatformMisc::RequestExitWithStatus(false,1);
    };
    if (R.Started==0)
    {
        R.Started=Now;
        if (!FParse::Value(FCommandLine::Get(),TEXT("HarvestFellingReviewOutput="),R.Output)
            || FPaths::IsRelative(R.Output) || !IFileManager::Get().DirectoryExists(*R.Output)) { Fail(TEXT("OutputDirectory")); return; }
    }
    if (Now-R.Started>150) { Fail(TEXT("Timeout")); return; }
    ASFGatheringPilotNode* Wood=nullptr;
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld());It;++It) { if (It->Kind==ESFGatheringPilotKind::Oak) { Wood=*It; break; } }
    if (!Wood || Wood->GetState().Capacity==0) { return; }
    if (!Wood->HarvestVisuals || !Wood->HarvestVisuals->IsReady()) { Fail(TEXT("ActualOakSourceUnavailable")); return; }
    const auto& State=Wood->GetState();
    if (!R.Pending.IsEmpty())
    {
        if (GIsHighResScreenshot || IFileManager::Get().FileSize(*R.Pending)<=0) { return; }
        ++R.Shots; R.Pending.Reset();
    }
    if (GFrameCounter<R.WaitUntilFrame) { return; }
    auto* Tree=State.FallingTree.Get();
    const auto Shot=[&](const FString& Name)
    {
        R.Pending=FPaths::Combine(R.Output,Name+TEXT(".png"));
        if (IFileManager::Get().FileExists(*R.Pending)) { Fail(TEXT("CaptureAlreadyExists")); return; }
        PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1920x1080 filename=%s"),*R.Pending.Replace(TEXT("\\"),TEXT("/"))));
        const FTransform Body=Tree&&Tree->GetFallBody()?Tree->GetFallBody()->GetComponentTransform():FTransform::Identity;
        UE_LOG(LogTemp,Display,TEXT("HARVEST_FELLING_SHOT Name=%s Cuts=%d LeanDeg=%.1f BodyLocation=%s Resting=%d"),*Name,State.Cuts.Num(),
            FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Body.GetUnitAxis(EAxis::Z).Z,-1.0,1.0))),*Body.GetLocation().ToString(),Tree&&Tree->IsResting());
    };
    const auto View=[&](const FVector& Eye,const FVector& Look,float Fov)
    {
        R.Camera->SetActorLocationAndRotation(Eye,(Look-Eye).Rotation()); R.Camera->GetCameraComponent()->SetFieldOfView(Fov);
        PC->SetViewTarget(R.Camera.Get()); R.WaitUntilFrame=GFrameCounter+45;  // temporal history settles on the new view
    };
    if (!R.bPrepared)
    {
        if (!HasRequiredTool(ESFGatheringPilotKind::Oak)) { if (!R.bToolRequested) { R.bToolRequested=TryEquipTool(true); } return; }
        const auto* Character=Cast<ACharacter>(PC->GetPawn()); const FVector Position=PC->GetPawn()->GetActorLocation();
        if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetVelocity().IsNearlyZero(1)
            || FVector::DistSquared(Position,R.LastPosition)>1) { R.StableSince=0; R.LastPosition=Position; return; }
        if (R.StableSince==0) { R.StableSince=Now; }
        if (Now-R.StableSince<1) { return; }
        if (State.Charges!=3 || !State.Cuts.IsEmpty()) { Fail(TEXT("ExpectedFreshThreeChargeOak")); return; }
        if (auto* HUD=PC->GetHUD()) { HUD->bShowHUD=false; }
        for (const TCHAR* Command:{TEXT("r.EyeAdaptationQuality 0"),TEXT("r.DefaultFeature.AutoExposure 0"),TEXT("r.MotionBlurQuality 0"),TEXT("r.ScreenPercentage 100")})
        { GEngine->Exec(GetWorld(),Command); }
        if (const TCHAR* Failure=PrepareHarvestReviewResources(GetWorld())) { Fail(Failure); return; }
        R.Base=Wood->GetActorLocation()-FVector(0,0,70);
        R.Camera=GetWorld()->SpawnActor<ACameraActor>();
        if (!R.Camera.IsValid()) { Fail(TEXT("ReviewCamera")); return; }
        R.bPrepared=true; return;
    }
    if (R.Completed<3)
    {
        if (R.bWaiting)
        {
            if (Feedback.Phase==ESFGatheringPilotPhase::Rejected || Feedback.Phase==ESFGatheringPilotPhase::Canceled) { Fail(TEXT("HarvestRejected")); return; }
            if (Feedback.Phase!=ESFGatheringPilotPhase::Committed || State.Cuts.Num()!=R.Completed+1) { return; }
            ++R.Completed; R.bWaiting=false;
            if (R.Completed==2)
            {
                // Frame the side of the fall: the tree falls away from the notch face (SFHarvestPresentation TreeSplitFor).
                // The mouth faces radially from the trunk axis to the cut (a facet normal of the trunk can be far off).
                const FVector CutPoint=R.Base+Wood->GetActorRotation().RotateVector(FVector(State.Cuts.Last().Point));
                const FVector Radial=FVector(CutPoint.X-R.Base.X,CutPoint.Y-R.Base.Y,0).GetSafeNormal();
                R.FallDir=-Radial;
                R.Side=FVector::CrossProduct(FVector::UpVector,R.FallDir);
                // Like the concept, the camera stands 35 deg toward the notch side so the wedge is seen open, and the
                // review sun comes from behind the camera at 30 deg elevation to light the cut face.
                const FVector SeqDir=(Radial*FMath::Cos(FMath::DegreesToRadians(40.f))-R.Side*FMath::Sin(FMath::DegreesToRadians(40.f))).GetSafeNormal();
                UE_LOG(LogTemp,Display,TEXT("HARVEST_REVIEW_NOTCH_VIEW RadialToViewDeg=%.1f"),
                    FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Radial,SeqDir.GetSafeNormal2D()))));
                for (TActorIterator<ADirectionalLight> It(GetWorld());It;++It)
                {
                    It->GetLightComponent()->SetMobility(EComponentMobility::Movable);
                    It->SetActorRotation((-SeqDir).Rotation()+FRotator(-30,0,0));
                    UE_LOG(LogTemp,Display,TEXT("HARVEST_REVIEW_SUN Rot=%s"),*It->GetActorRotation().ToString());
                }
                const FVector Frame=R.Base+FVector(0,0,450);
                View(Frame+SeqDir*1500,Frame,65); R.bNotched=true;
            }
            return;
        }
        if (R.bNotched) { R.bNotched=false; Shot(TEXT("00-notched")); return; }
        const FVector Eye=PC->GetPawn()->GetPawnViewLocation(); FVector Contact,Normal;
        // Cut ~55 cm above the base (node origin is base+70): the concept stump proportion, room for the hinge tear.
        if (!Wood->HarvestVisuals->TraceSurface(Eye,(Wood->GetActorLocation()-FVector(0,0,15)-Eye).GetSafeNormal(),Contact,Normal)
            || !InteractFromRay(Eye,(Contact-Eye).GetSafeNormal()) || !bWaitingHadSurface) { Fail(TEXT("SurfaceInteraction")); return; }
        R.bWaiting=true; return;
    }
    if (!IsValid(Tree) || !Tree->IsVisualSourceReady() || !Wood->HarvestVisuals->IsTreeDetachedTo(Tree)) { return; }
    static constexpr int32 Leans[]={2,10,22,36,50,64,78};
    const double Lean=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Tree->GetFallBody()->GetUpVector().Z,-1.0,1.0)));
    if (R.NextLean<UE_ARRAY_COUNT(Leans) && !Tree->IsResting())
    {
        if (Lean>=Leans[R.NextLean]) { Shot(FString::Printf(TEXT("%02d-lean-%02d"),R.NextLean+1,Leans[R.NextLean])); ++R.NextLean; }
        return;
    }
    if (!Tree->IsResting()) { return; }
    if (R.RestSince==0) { R.RestSince=Now; return; }
    if (Now-R.RestSince<1.5) { return; }
    const FTransform Rest=Tree->GetFallBody()->GetComponentTransform();
    if (R.CloseUps==0)
    {
        FVector Lowest(0,0,BIG_NUMBER);
        for (const UActorComponent* Component:Tree->GetComponents())
        { if (const auto* Part=Cast<UPrimitiveComponent>(Component); Part && Part->IsVisible()) { Lowest.Z=FMath::Min(Lowest.Z,Part->Bounds.GetBox().Min.Z); } }
        UE_LOG(LogTemp,Display,TEXT("HARVEST_FELLING_REST Transform=%s LowestVisibleZ=%.1f BaseZ=%.1f SkippedLeans=%d"),
            *Rest.ToString(),Lowest.Z,R.Base.Z,int32(UE_ARRAY_COUNT(Leans))-R.NextLean);
        ++R.CloseUps; R.WaitUntilFrame=GFrameCounter+5; Shot(TEXT("08-rest")); return;
    }
    PC->GetPawn()->SetActorHiddenInGame(true);
    if (R.CloseUps==1) { View(R.Base-R.FallDir*260+R.Side*120+FVector(0,0,140),R.Base+FVector(0,0,45),45); ++R.CloseUps; return; }
    if (R.CloseUps==2) { ++R.CloseUps; Shot(TEXT("09-stump-close")); return; }
    if (R.CloseUps==3)
    {
        const FVector Butt=Rest.TransformPosition(FVector(0,0,40));
        const FVector Mid=(Butt+R.Base+FVector(0,0,40))*0.5;   // butt and stump both in frame
        View(Mid+R.Side*500+FVector(0,0,150),Mid,60); ++R.CloseUps; return;
    }
    if (R.CloseUps==4) { ++R.CloseUps; Shot(TEXT("10-log-close")); return; }
    R.bDone=true;
    UE_LOG(LogTemp,Display,TEXT("HARVEST_FELLING_REVIEW_DONE Shots=%d Cuts=%d Charges=%d"),R.Shots,State.Cuts.Num(),State.Charges);
    FPlatformMisc::RequestExitWithStatus(false,0);
#endif
}
#endif
