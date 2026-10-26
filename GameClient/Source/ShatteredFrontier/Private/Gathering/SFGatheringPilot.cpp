#include "Gathering/SFGatheringPilot.h"
#include "Characters/SFE1Character.h"
#include "Gathering/SFHarvestFallingTree.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Engine/NetConnection.h"
#include "Misc/Guid.h"
#include "Construction/SFConstructionNetwork.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Construction/SFConstructionSandboxDoorProjection.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/DateTime.h"
#include "Containers/Ticker.h"
#include "Net/UnrealNetwork.h"
#include "Quest/SFQuestPilotEvents.h"
#include "Quest/SFQuestPilot.h"
#include "Gathering/SFStationRecipes.h"
#include "Inventory/SFGear.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFCharacterSession.h"
#include "Services/SFGameplayServicesClient.h"
#include "UObject/ConstructorHelpers.h"
#include "Inventory/SFInventory.h"

namespace
{
    FString ChooseInputGrade(USFInventoryComponent* Inventory, const FSFItemStack& Input)
    {
        static const TCHAR* Grades[] = {TEXT("crude"), TEXT(""), TEXT("fine"), TEXT("superior"), TEXT("masterwork")};
        for (const TCHAR* Grade : Grades)
        {
            const FString Id = *Grade ? Input.DefinitionId + TEXT("/") + Grade : Input.DefinitionId;
            const auto Asset = TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Id)));
            if (Inventory && SFInventoryTransactions::GetItemQuantity(Inventory, Asset) >= Input.Quantity) { return Id; }
        }
        return Input.DefinitionId;
    }
    // The ledger id of a gear item (content/items), or "" for tools, other items and an empty path.
    FString GearId(const FSoftObjectPath& Item)
    {
        const USFItemDefinition* Definition = Item.IsNull() ? nullptr : TSoftObjectPtr<USFItemDefinition>(Item).LoadSynchronous();
        return Definition && SFGear::Find(Definition->UniqueId) ? Definition->UniqueId : FString();
    }
    const TCHAR* ItemPath(ESFGatheringPilotKind Kind)
    {
        switch (Kind)
        {
        case ESFGatheringPilotKind::Oak: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog");
        case ESFGatheringPilotKind::Iron: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre");
        case ESFGatheringPilotKind::Sawmill: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_RoughTimber.DA_RoughTimber");
        case ESFGatheringPilotKind::Forge: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronIngot.DA_IronIngot");
        case ESFGatheringPilotKind::Stone: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_RoughStone.DA_RoughStone");
        default: return TEXT("");
        }
    }
    // InventoryLedger definition ids for the raw materials: the content ids recipes and quests use (INT-04).
    const TCHAR* LedgerItemId(ESFGatheringPilotKind Kind)
    {
        switch (Kind)
        {
        case ESFGatheringPilotKind::Oak: return TEXT("item/oak_log");
        case ESFGatheringPilotKind::Iron: return TEXT("item/iron_ore");
        case ESFGatheringPilotKind::Stone: return TEXT("item/rough_stone");
        default: return TEXT("");
        }
    }
    const TCHAR* ToolPath(bool bAxe)
    {
        return bAxe ? TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronAxe.DA_IronAxe")
            : TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronPickaxe.DA_IronPickaxe");
    }
    const TCHAR* ResultText(ESFGatheringPilotResult Result)
    {
        switch (Result)
        {
        case ESFGatheringPilotResult::Started: return TEXT("Working. Stay still; E or Escape cancels.");
        case ESFGatheringPilotResult::Refined: return TEXT("Refinement committed to your inventory.");
        case ESFGatheringPilotResult::ToolRequired: return TEXT("Required tool not equipped: 5 iron axe for oak, 6 iron pickaxe for ore.");
        case ESFGatheringPilotResult::SkillRequired: return TEXT("Your gathering skill does not meet this material's requirement.");
        case ESFGatheringPilotResult::MissingMaterials: return TEXT("Required raw materials are missing. Nothing consumed.");
        case ESFGatheringPilotResult::Granted: return TEXT("Harvest committed to your inventory.");
        case ESFGatheringPilotResult::Stale: return TEXT("Kind changed. Target it again.");
        case ESFGatheringPilotResult::Busy: return TEXT("Another player is harvesting this node.");
        case ESFGatheringPilotResult::Depleted: return TEXT("This node is depleted.");
        case ESFGatheringPilotResult::OutOfReach: return TEXT("Move closer to harvest.");
        case ESFGatheringPilotResult::Occluded: return TEXT("The resource is blocked from view.");
        case ESFGatheringPilotResult::Moved: return TEXT("Harvest canceled: you moved. No charge consumed.");
        case ESFGatheringPilotResult::Canceled: return TEXT("Harvest canceled. No charge consumed.");
        case ESFGatheringPilotResult::InventoryFull: return TEXT("Inventory is full or overweight. No charge consumed.");
        case ESFGatheringPilotResult::Unavailable: return TEXT("Inventory unavailable. No charge consumed.");
        case ESFGatheringPilotResult::KeyConflict: return TEXT("Request identity conflict. No new action applied.");
        case ESFGatheringPilotResult::None: return TEXT("Aim at an oak or iron node and press E.");
        default: return TEXT("Harvest rejected. No charge consumed.");
        }
    }
}

bool USFGatheringPilotSettings::Validate(FString& Error) const
{
    const bool bValid=FMath::IsFinite(HarvestSeconds) && HarvestSeconds>=0.5f && HarvestSeconds<=30
        && FMath::IsFinite(GeometryAttemptCooldownSeconds) && GeometryAttemptCooldownSeconds>=0.1f && GeometryAttemptCooldownSeconds<=2
        && FMath::IsFinite(ReachMetres) && ReachMetres>=0.5f && ReachMetres<=5
        && FMath::IsFinite(MovementCancelMetres) && MovementCancelMetres>=0.01f && MovementCancelMetres<=0.25f
        && ChargesPerNode>=1 && ChargesPerNode<=12 && UnitsPerCharge>=1 && UnitsPerCharge<=10
        && ChargesPerNode*UnitsPerCharge<=30 && InventorySlots>=1 && InventorySlots<=64
        && FMath::IsFinite(RespawnSeconds) && RespawnSeconds>=1 && RespawnSeconds<=86400
        && FMath::IsFinite(InventoryWeightKg) && InventoryWeightKg>=1 && InventoryWeightKg<=1000
        && MaterialStackUnits>=1 && MaterialStackUnits<=100 && FMath::IsFinite(MaterialUnitWeightKg)
        && MaterialUnitWeightKg>=0.01f && MaterialUnitWeightKg<=50
        && FMath::IsFinite(RefineSeconds) && RefineSeconds>=0.5f && RefineSeconds<=30
        && TimberInputLogs>=1 && TimberInputLogs<=20 && TimberOutputUnits>=1 && TimberOutputUnits<=20
        && IngotInputOre>=1 && IngotInputOre<=20 && IngotOutputUnits>=1 && IngotOutputUnits<=20
        && FMath::IsFinite(ToolUnitWeightKg) && ToolUnitWeightKg>=0.01f && ToolUnitWeightKg<=50;
    Error=bValid ? TEXT("") : TEXT("Gathering pilot configuration outside declared bounds (including <=30 units per node).");
    return bValid;
}

USFGatheringGrowthSettings::USFGatheringGrowthSettings()
{
    // Shipped defaults: sapling-sized when young, 70 percent when growing. Oak and the rocks share the table shape.
    for (const ESFGatheringPilotKind Kind : {ESFGatheringPilotKind::Oak, ESFGatheringPilotKind::Iron, ESFGatheringPilotKind::Stone})
    {
        StageVisuals.Add({Kind,TEXT("young"),0.45f}); StageVisuals.Add({Kind,TEXT("growing"),0.7f}); StageVisuals.Add({Kind,TEXT("mature"),1.0f});
    }
    StageRules.Add({TEXT("young"),0.5f,0.3f,false});
    StageRules.Add({TEXT("growing"),0.5f,0.5f});
    StageRules.Add({TEXT("mature"),0.f,1.f});
}

float USFGatheringGrowthSettings::ScaleFor(ESFGatheringPilotKind Kind, FName Stage) const
{
    for (const FSFGrowthStageVisual& V : StageVisuals) { if (V.Kind==Kind && V.Stage==Stage) { return V.Scale; } }
    return 1;
}

bool USFGatheringGrowthSettings::Validate(FString& Error) const
{
    bool bValid=FMath::IsFinite(GrowFraction) && GrowFraction>=0.01f && GrowFraction<=0.99f
        && StageRules.Num()>=2 && StageRules.Num()<=4;
    double Sum=0; float PreviousYield=0;
    const int32 Charges=GetDefault<USFGatheringPilotSettings>()->ChargesPerNode;
    for (int32 I=0; I<StageRules.Num(); ++I)
    {
        const auto& Rule=StageRules[I];
        const double EarliestAllowed=1.f-GrowFraction+Sum*GrowFraction;
        const int32 StageCharges=FMath::IsFinite(Rule.YieldFraction)
            ? FMath::Max(1,FMath::RoundToInt(Charges*FMath::Clamp(Rule.YieldFraction,0.f,1.f))) : 0;
        bValid&=!Rule.Name.IsNone() && FMath::IsFinite(Rule.DurationFraction) && FMath::IsFinite(Rule.YieldFraction)
            && Rule.DurationFraction>=(I==StageRules.Num()-1 ? 0.f : 0.0001f)
            && Rule.DurationFraction<=(I==StageRules.Num()-1 ? 0.f : 1.f)
            && Rule.YieldFraction>PreviousYield && Rule.YieldFraction<=1.f
            && Rule.YieldFraction<=EarliestAllowed+0.0001f
            && Charges>=1 && double(StageCharges)/Charges<=EarliestAllowed+0.0001;
        PreviousYield=Rule.YieldFraction; Sum+=Rule.DurationFraction;
        for (int32 J=0; J<I; ++J) { bValid&=Rule.Name!=StageRules[J].Name; }
    }
    bValid&=FMath::IsNearlyEqual(Sum,1.0,0.0001) && FMath::IsNearlyEqual(PreviousYield,1.f);
    for (int32 I=0; I<StageVisuals.Num(); ++I)
    {
        const FSFGrowthStageVisual& V=StageVisuals[I];
        bValid&=!V.Stage.IsNone() && FMath::IsFinite(V.Scale) && V.Scale>=0.1f && V.Scale<=1.5f;
        for (int32 J=0; J<I; ++J) { bValid&=!(StageVisuals[J].Kind==V.Kind && StageVisuals[J].Stage==V.Stage); }
    }
    Error=bValid ? TEXT("") : TEXT("Gathering growth settings outside declared stage, timing, yield or appearance bounds.");
    return bValid;
}

namespace
{
// Debug text labels only in dev scenarios (-UiEvidence*, *Scenario); players see the prop and the HUD prompt instead.
bool ShowStationLabels() { return FCString::Stristr(FCommandLine::Get(),TEXT("UiEvidence")) || FCString::Stristr(FCommandLine::Get(),TEXT("Scenario")); }

// Placed props stand in for the stations until dedicated workbench/forge/sawmill meshes exist under /Game/Frontier/Props.
UStaticMesh* StationPropMesh(ESFGatheringPilotKind Kind)
{
    const TCHAR* Path=Kind==ESFGatheringPilotKind::Workbench ? TEXT("/Game/Frontier/Environment/Greenlands/Silverbrook/Meshes/SM_SilverbrookPropBench.SM_SilverbrookPropBench")
        : Kind==ESFGatheringPilotKind::Sawmill ? TEXT("/Game/Frontier/Environment/Greenlands/Meshes/SM_LogStack.SM_LogStack")
        : TEXT("/Game/Frontier/Environment/Greenlands/Arrival/Meshes/SM_PH_wooden_crate_01_00.SM_PH_wooden_crate_01_00");
    return LoadObject<UStaticMesh>(nullptr,Path);
}
}

ASFGatheringPilotNode::ASFGatheringPilotNode()
{
    bReplicates=true;
    SetReplicateMovement(true);
    // A zone node replaces the dressing tree or rock on its spot, so it must stay visible as far as that dressing would.
    // ponytail: fixed 500 m; World cell relevancy replaces it.
    SetNetCullDistanceSquared(FMath::Square(50000.0));
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh")); SetRootComponent(Mesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    StationMesh=Cube.Object;
    Mesh->SetStaticMesh(Shape.Object); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetRelativeScale3D(FVector(0.7,0.7,1.4));
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("ResourceLabel")); Label->SetupAttachment(Mesh);
    Label->SetRelativeLocation(FVector(0,-60,90)); Label->SetRelativeRotation(FRotator(0,-90,0));
    Label->SetWorldSize(16); Label->SetHorizontalAlignment(EHTA_Center); Label->SetVisibility(ShowStationLabels());
    HarvestVisuals=CreateDefaultSubobject<USFHarvestPresentation>(TEXT("HarvestPresentation"));
}

void ASFGatheringPilotNode::BeginPlay()
{
    Super::BeginPlay();
    FString Error; const auto* Settings=GetDefault<USFGatheringPilotSettings>();
    if (HasAuthority() && Settings->Validate(Error))
    {
        if (GrowthStages.IsEmpty())
        {
            const auto* Growth=GetDefault<USFGatheringGrowthSettings>();
            if (Growth->Validate(Error))
            {
                GrowFraction=Growth->GrowFraction;
                for (const auto& Rule : Growth->StageRules) { GrowthStages.Add({Rule.Name,Rule.DurationFraction,Rule.YieldFraction,Rule.bBlocking}); }
            }
        }
        State.Capacity=IsRefinery() ? 0 : Settings->ChargesPerNode;
        State.Charges=State.Capacity; CapturedRespawnSeconds=Settings->RespawnSeconds;
        if (!GrowthStages.IsEmpty()) { State.GrowthStage=GrowthStages.Last().Name; State.bGrowthBlocking=GrowthStages.Last().bBlocking; }  // a fresh node starts mature
        bUsesServices=!IsRefinery() && GetDefault<USFGameplayServicesSettings>()->bEnabled;
        if (bUsesServices)
        {
            State.Charges=0; State.GrowthStage=NAME_None; State.bGrowthBlocking=false; State.bPlacementActive=false;
            ServiceClient=USFBackpackComponent::Services();
            RefreshServiceState();
        }
    }
    OnRep_State();
}

void ASFGatheringPilotNode::OnRep_ReplicatedMovement()
{
    Super::OnRep_ReplicatedMovement();
    ApplyGrowthVisual();
    if (State.bPlacementActive) { ClearDressingAtPlacement(); }
}

void ASFGatheringPilotNode::ClearDressingAtPlacement()
{
    if (IsRefinery() || !State.bPlacementActive ||
        (ClearedDressingAt.IsSet() && ClearedDressingAt->Equals(GetActorLocation(),1))) { return; }
    // Dressing instances are local map data, so each client clears the point it actually receives.
    ClearedDressingAt=GetActorLocation();
    const FVector At=GetActorLocation()-FVector(0,0,70); const FSphere Near(At,400);
    for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
    {
        if (It->GetWorld()!=GetWorld() || !It->Bounds.GetSphere().Intersects(Near)) { continue; }
        TArray<int32> Rooted;
        for (const int32 I : It->GetInstancesOverlappingSphere(At,100,true))
        {
            FTransform T; It->GetInstanceTransform(I,T,true);
            if (FVector::Dist2D(T.GetLocation(),At)<100) { Rooted.Add(I); }
        }
        if (!Rooted.IsEmpty()) { It->RemoveInstances(Rooted); }
    }
}

void ASFGatheringPilotNode::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(RecoveryTimer); GetWorldTimerManager().ClearTimer(GrowthTimer);
    GetWorldTimerManager().ClearTimer(ServiceRefreshTimer); GetWorldTimerManager().ClearTimer(ServiceGrowthTimer);
    if (HasAuthority()&&IsValid(State.FallingTree)) { State.FallingTree->Destroy(); State.FallingTree=nullptr; }
    Super::EndPlay(Reason);
}

FSFGatheringPilotPresentation ASFGatheringPilotNode::GetPresentationState() const
{
    FSFGatheringPilotPresentation View;
    View.Kind=Kind; View.Revision=State.Revision; View.RemainingCharges=State.Charges;
    View.Capacity=State.Capacity; View.bBusy=State.bBusy; View.bRefinery=IsRefinery();
    View.ExtractedFraction=View.bRefinery || State.Capacity<=0 ? 0
        : FMath::Clamp(1.0f-static_cast<float>(State.Charges)/State.Capacity,0.0f,1.0f);
    View.RecoveryAtServerSeconds=State.RecoveryAtServerSeconds;
    return View;
}

void ASFGatheringPilotNode::BeginRecovery()
{
    if (bUsesServices) { return; }
    if (!HasAuthority() || IsRefinery() || State.Charges!=0 || State.RecoveryAtServerSeconds>0) { return; }
    GetWorldTimerManager().ClearTimer(GrowthTimer);
    RecoveryKind=Kind; State.GrowthStage=NAME_None; State.bGrowthBlocking=false;
    State.RecoveryAtServerSeconds=GetWorld()->GetTimeSeconds()+CapturedRespawnSeconds;
    const float Delay=GrowthStages.IsEmpty() ? CapturedRespawnSeconds : CapturedRespawnSeconds*(1.f-GrowFraction);
    GetWorldTimerManager().SetTimer(RecoveryTimer,this,&ASFGatheringPilotNode::Recover,Delay,false);
}

void ASFGatheringPilotNode::SetGrowthProfile(const TArray<FSFGrowthStageDef>& Stages, float InGrowFraction)
{
    const int32 Capacity=GetDefault<USFGatheringPilotSettings>()->ChargesPerNode;
    bool bValid=Stages.Num()>=2 && Stages.Num()<=4 && FMath::IsFinite(InGrowFraction)
        && InGrowFraction>=0.01f && InGrowFraction<=0.99f && Capacity>=1;
    double Start=0; float PreviousYield=0;
    for (int32 I=0; I<Stages.Num(); ++I)
    {
        const auto& Stage=Stages[I];
        const double AvailableFraction=1.0-InGrowFraction+Start*InGrowFraction;
        const int32 Limit=FMath::IsFinite(Stage.YieldFraction)
            ? FMath::Max(1,FMath::RoundToInt(Capacity*FMath::Clamp(Stage.YieldFraction,0.f,1.f))) : 0;
        bValid&=!Stage.Name.IsNone() && FMath::IsFinite(Stage.DurationFraction) && FMath::IsFinite(Stage.YieldFraction)
            && Stage.DurationFraction>=(I==Stages.Num()-1 ? 0.f : 0.0001f)
            && Stage.DurationFraction<=(I==Stages.Num()-1 ? 0.f : 1.f)
            && Stage.YieldFraction>PreviousYield && Stage.YieldFraction<=1.f
            && Capacity>0 && double(Limit)/Capacity<=AvailableFraction+0.0001;
        for (int32 J=0; J<I; ++J) { bValid&=Stage.Name!=Stages[J].Name; }
        PreviousYield=Stage.YieldFraction; Start+=Stage.DurationFraction;
    }
    bValid&=FMath::IsNearlyEqual(Start,1.0,0.0001) && FMath::IsNearlyEqual(PreviousYield,1.f);
    if (!bValid) { UE_LOG(LogTemp,Error,TEXT("GATHERING_GROWTH_PROFILE_INVALID Kind=%d"),int32(Kind)); return; }
    GrowthStages=Stages; GrowFraction=InGrowFraction;
}

FName ASFGatheringPilotNode::StageAt(const TArray<FSFGrowthStageDef>& Stages, double GrowWindowSeconds, double ElapsedSeconds)
{
    double Start=0; FName Out;
    for (int32 I=0; I<Stages.Num(); ++I)
    {
        if (I>0 && ElapsedSeconds<Start*GrowWindowSeconds) { break; }
        Out=Stages[I].Name; Start+=Stages[I].DurationFraction;
    }
    return Out;
}

void ASFGatheringPilotNode::StartGrowth()
{
    if (bUsesServices) { return; }
    if (!HasAuthority() || GrowthStages.IsEmpty()) { return; }
    GrowthStartedAt=GetWorld()->GetTimeSeconds(); AdvanceGrowth();
}

bool ASFGatheringPilotNode::CanAdvanceToStage(FName Stage) const
{
    for (const FSFGrowthStageDef& Rule : GrowthStages)
    { if (Rule.Name==Stage && !Rule.bBlocking) { return true; } }
    const auto* Settings=GetDefault<USFGatheringGrowthSettings>();
    const float CurrentScale=State.GrowthStage.IsNone() ? 0.f : Settings->ScaleFor(Kind,State.GrowthStage);
    const float NextScale=Settings->ScaleFor(Kind,Stage);
    if ((NextScale<=CurrentScale && State.bGrowthBlocking) || !Mesh || !Mesh->GetStaticMesh()) { return true; }
    FTransform NextTransform=Mesh->GetComponentTransform();
    NextTransform.SetScale3D(FVector(0.7,0.7,1.4)*NextScale);
    const FBoxSphereBounds Bounds=Mesh->CalcBounds(NextTransform);
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It; ++It)
    {
        if (*It!=this && It->State.Charges>0 && It->Mesh &&
            Bounds.GetBox().Intersect(It->Mesh->Bounds.GetBox())) { return false; }
    }
    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(GatheringGrowthClearance),false,this);
    GetWorld()->OverlapMultiByObjectType(Overlaps,Bounds.Origin,FQuat::Identity,Objects,
        FCollisionShape::MakeBox(Bounds.BoxExtent),Query);
    for (const FOverlapResult& Hit : Overlaps)
    {
        const AActor* Other=Hit.GetActor();
        const UPrimitiveComponent* Component=Hit.GetComponent();
        if (!Other || Other==this || !Component || Component->GetCollisionResponseToChannel(ECC_Pawn)!=ECR_Block) { continue; }
        if (Other->IsA<ASFGatheringPilotNode>()) { continue; } // handled above, including nonblocking nodes
        if (Other->IsA<APawn>() || Other->IsA<ASFConstructionNetworkState>() ||
            Other->IsA<ASFConstructionSandboxState>() || Other->IsA<ASFConstructionSandboxDoorProjection>() ||
            Other->IsA<AStaticMeshActor>() || Component->GetCollisionObjectType()==ECC_WorldDynamic)
        { return false; }
    }
    return true;
}

void ASFGatheringPilotNode::AdvanceGrowth()
{
    if (!HasAuthority() || bUsesServices || GrowthStages.IsEmpty() || State.RecoveryAtServerSeconds>0) { return; }
    const double Window=CapturedRespawnSeconds*GrowFraction, Elapsed=GetWorld()->GetTimeSeconds()-GrowthStartedAt;
    int32 StageIndex=0; double StageStart=0;
    for (int32 I=1; I<GrowthStages.Num(); ++I)
    {
        StageStart+=GrowthStages[I-1].DurationFraction;
        if (Elapsed<StageStart*Window) { break; }
        StageIndex=I;
    }
    const FName NextStage=GrowthStages[StageIndex].Name;
    if (State.GrowthStage!=NextStage)
    {
        if (!CanAdvanceToStage(NextStage))
        {
            GrowthTimer=GetWorldTimerManager().SetTimerForNextTick(this,&ASFGatheringPilotNode::AdvanceGrowth);
            return;
        }
        State.GrowthStage=NextStage;
        State.bGrowthBlocking=GrowthStages[StageIndex].bBlocking;
        const int32 Limit=FMath::Max(1,FMath::RoundToInt(State.Capacity*GrowthStages[StageIndex].YieldFraction));
        State.Charges=FMath::Max(0,Limit-HarvestedThisGrowth);
        ++State.Revision;
    }
    double Boundary=0; bool bFound=false;  // next stage start after Elapsed, if any
    for (int32 I=0; I<GrowthStages.Num()-1; ++I)
    {
        Boundary+=GrowthStages[I].DurationFraction;
        if (Elapsed<Boundary*Window) { bFound=true; break; }
    }
    if (bFound) { GetWorldTimerManager().SetTimer(GrowthTimer,this,&ASFGatheringPilotNode::AdvanceGrowth,FMath::Max(float(Boundary*Window-Elapsed),0.01f),false); }
    OnRep_State(); ForceNetUpdate();
}

void ASFGatheringPilotNode::DevPreviewGrowthStage(FName Stage)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!HasAuthority()) { return; }
    GetWorldTimerManager().ClearTimer(GrowthTimer); State.GrowthStage=Stage; OnRep_State(); ForceNetUpdate();
#endif
}

void ASFGatheringPilotNode::ApplyGrowthVisual()
{
    if (IsRefinery()) { return; }
    const float Scale=GetDefault<USFGatheringGrowthSettings>()->ScaleFor(Kind,State.GrowthStage);
    Mesh->SetRelativeScale3D(FVector(0.7,0.7,1.4)*Scale);
    if (HarvestVisuals) { HarvestVisuals->SetGrowthScale(Scale); }
}

void ASFGatheringPilotNode::Recover()
{
    if (bUsesServices) { return; }
    if (!HasAuthority() || IsRefinery() || Kind!=RecoveryKind || State.Charges!=0 || State.Capacity<=0
        || State.bBusy || LockedBy.IsValid() || State.RecoveryAtServerSeconds<=0 || State.Revision==MAX_uint32 || State.Generation==MAX_uint32) { return; }
    const double Remaining=State.RecoveryAtServerSeconds-GetWorld()->GetTimeSeconds()
        -(GrowthStages.IsEmpty() ? 0.0 : CapturedRespawnSeconds*GrowFraction);
    if (Remaining>0)
    {
        GetWorldTimerManager().SetTimer(RecoveryTimer,this,&ASFGatheringPilotNode::Recover,
            FMath::Max(static_cast<float>(Remaining),UE_SMALL_NUMBER),false);
        return;
    }
    if (IsValid(State.FallingTree)) { State.FallingTree->Destroy(); } State.FallingTree=nullptr;
    State.Charges=GrowthStages.IsEmpty() ? State.Capacity : 0;
    HarvestedThisGrowth=0; State.GrowthStage=NAME_None; State.bGrowthBlocking=false;
    State.RecoveryAtServerSeconds=0; State.Cuts.Reset(); ++State.Generation;
    if (GrowthStages.IsEmpty()) { ++State.Revision; }
    StartGrowth();  // no profile: no-op
    OnRep_State(); ForceNetUpdate();
}

bool ASFGatheringPilotNode::ApplyServiceSnapshot(const FSFGameplayResponse& Snapshot)
{
    if (!HasAuthority() || !bUsesServices || !Snapshot.bHasNode || Snapshot.NodeId!=ServiceNodeId ||
        Snapshot.NodeGeneration<1 || Snapshot.NodeCharges<0 || Snapshot.NodeCapacity<1 || Snapshot.NodeCharges>Snapshot.NodeCapacity ||
        Snapshot.NodePoolId.IsEmpty() || Snapshot.NodePointId.IsEmpty() || Snapshot.bNodeActive!=(Snapshot.NodeCharges>0) ||
        Snapshot.NodePositionMetres.ContainsNaN() || !FMath::IsFinite(Snapshot.NodeFacingDegrees))
    { return false; }
    const TCHAR* ExpectedDefinition=Kind==ESFGatheringPilotKind::Oak ? TEXT("gathering_node/oak_tree")
        : Kind==ESFGatheringPilotKind::Iron ? TEXT("gathering_node/iron_vein")
        : Kind==ESFGatheringPilotKind::Stone ? TEXT("gathering_node/stone_outcrop") : TEXT("");
    if (Snapshot.NodeDefinitionId!=ExpectedDefinition) { return false; }
    if (Snapshot.NodeGeneration<int32(State.Generation)) { return true; }  // a replayed receipt cannot rewind a newer host snapshot
    const bool bMoved=ServicePointId!=Snapshot.NodePointId;
    if (bMoved)
    {
        ServicePointId=Snapshot.NodePointId;
        SetActorLocationAndRotation(Snapshot.NodePositionMetres*100.0+FVector(0,0,70),
            FRotator(0,Snapshot.NodeFacingDegrees,0),false,nullptr,ETeleportType::TeleportPhysics);
    }
    const FName HostStage(*Snapshot.GrowthStage);
    const uint32 PreviousGeneration=State.Generation;
    const int32 PreviousCharges=State.Charges, PreviousCapacity=State.Capacity;
    const FName PreviousStage=State.GrowthStage;
    if (State.Generation!=uint32(Snapshot.NodeGeneration))
    { State.Cuts.Reset(); if (IsValid(State.FallingTree)) { State.FallingTree->Destroy(); } State.FallingTree=nullptr; }
    State.Generation=uint32(Snapshot.NodeGeneration); State.Charges=Snapshot.NodeCharges; State.Capacity=Snapshot.NodeCapacity;
    const bool bWasActive=State.bPlacementActive;
    State.bPlacementActive=Snapshot.bNodeActive;
    PendingServiceStage=HostStage;
    if (HostStage.IsNone() || CanAdvanceToStage(HostStage))
    {
        State.GrowthStage=HostStage; State.bGrowthBlocking=!HostStage.IsNone();
        for (const FSFGrowthStageDef& Rule : GrowthStages)
        { if (Rule.Name==HostStage) { State.bGrowthBlocking=Rule.bBlocking; break; } }
        PendingServiceStage=NAME_None; GetWorldTimerManager().ClearTimer(ServiceGrowthTimer);
    }
    else if (!GetWorldTimerManager().IsTimerActive(ServiceGrowthTimer))
    { ServiceGrowthTimer=GetWorldTimerManager().SetTimerForNextTick(this,&ASFGatheringPilotNode::RetryServiceGrowth); }
    bServiceStateReady=true;
    const bool bChanged=PreviousGeneration!=State.Generation || PreviousCharges!=State.Charges ||
        PreviousCapacity!=State.Capacity || PreviousStage!=State.GrowthStage || bMoved || bWasActive!=State.bPlacementActive;
    const double NowMs=(FDateTime::UtcNow()-FDateTime::FromUnixTimestamp(0)).GetTotalMilliseconds();
    State.RecoveryAtServerSeconds=Snapshot.NodeRegrowAtUnixMs>0
        ? GetWorld()->GetTimeSeconds()+FMath::Max(0.0,(double(Snapshot.NodeRegrowAtUnixMs)-NowMs)/1000.0) : 0;
    GetWorldTimerManager().ClearTimer(ServiceRefreshTimer);
    if (Snapshot.NodeNextTransitionUnixMs>0)
    {
        const float Delay=float(FMath::Max(0.05,(double(Snapshot.NodeNextTransitionUnixMs)-NowMs)/1000.0+0.05));
        GetWorldTimerManager().SetTimer(ServiceRefreshTimer,this,&ASFGatheringPilotNode::RefreshServiceState,Delay,false);
    }
    else
    {
        GetWorldTimerManager().SetTimer(ServiceRefreshTimer,this,&ASFGatheringPilotNode::RefreshServiceState,
            GetDefault<USFGameplayServicesSettings>()->GatheringNodeRetrySeconds,false);
    }
    if (bChanged)
    {
        UE_LOG(LogTemp,Display,TEXT("GATHERING_NODE_HOST_STATE Id=%s Generation=%u Stage=%s Charges=%d Point=%s Active=%d NextTransition=%lld"),
            *ServiceNodeId,State.Generation,*State.GrowthStage.ToString(),State.Charges,*ServicePointId,State.bPlacementActive,Snapshot.NodeNextTransitionUnixMs);
        ++State.Revision; OnRep_State(); ForceNetUpdate();
    }
    return true;
}

void ASFGatheringPilotNode::ApplyZoneNode(const FSFGameplayResponse::FZoneNode& Node)
{
    // A list started before a harvest must not rewind a same-generation receipt or later GetNode read.
    if (bServiceStateReady && Node.Generation<=int32(State.Generation)) { return; }
    FSFGameplayResponse Snapshot;
    Snapshot.bHasNode=true; Snapshot.NodeId=Node.Id; Snapshot.NodeDefinitionId=Node.DefinitionId;
    Snapshot.NodeGeneration=Node.Generation; Snapshot.NodeCharges=Node.Charges; Snapshot.NodeCapacity=Node.Capacity;
    Snapshot.GrowthStage=Node.GrowthStage; Snapshot.NodeRegrowAtUnixMs=Node.RegrowAtUnixMs;
    Snapshot.NodeNextTransitionUnixMs=Node.NextTransitionUnixMs; Snapshot.NodePoolId=Node.PoolId;
    Snapshot.NodePointId=Node.PointId; Snapshot.NodePositionMetres=Node.PositionMetres;
    Snapshot.NodeFacingDegrees=Node.FacingDegrees; Snapshot.bNodeActive=Node.bActive;
    ++ServiceSnapshotSerial;
    ApplyServiceSnapshot(Snapshot);
}

void ASFGatheringPilotNode::RetryServiceGrowth()
{
    if (!HasAuthority() || !bUsesServices || PendingServiceStage.IsNone()) { return; }
    if (!CanAdvanceToStage(PendingServiceStage))
    { ServiceGrowthTimer=GetWorldTimerManager().SetTimerForNextTick(this,&ASFGatheringPilotNode::RetryServiceGrowth); return; }
    State.GrowthStage=PendingServiceStage; PendingServiceStage=NAME_None;
    State.bGrowthBlocking=true;
    for (const FSFGrowthStageDef& Rule : GrowthStages)
    { if (Rule.Name==State.GrowthStage) { State.bGrowthBlocking=Rule.bBlocking; break; } }
    ++State.Revision; OnRep_State(); ForceNetUpdate();
}

void ASFGatheringPilotNode::RefreshServiceState()
{
    if (!HasAuthority() || !bUsesServices) { return; }
    if (!ServiceClient.IsValid() || ServiceNodeId.IsEmpty())
    {
        UE_LOG(LogTemp,Error,TEXT("GATHERING_NODE_SERVICE_UNAVAILABLE Id=%s"),*ServiceNodeId);
    }
    else
    {
        FSFGameplayRequest Request; Request.Rpc=ESFGameplayRpc::GetGatheringNode; Request.NodeId=ServiceNodeId;
        TWeakObjectPtr<ASFGatheringPilotNode> Weak(this);
        const uint64 Serial=++ServiceSnapshotSerial;
        ServiceClient->Call(Request,[Weak,Serial](const FSFGameplayResponse& Response)
        {
            if (ASFGatheringPilotNode* Self=Weak.Get())
            {
                if (Serial!=Self->ServiceSnapshotSerial) { return; }
                if (Response.Status==ESFGameplayStatus::Ok && Self->ApplyServiceSnapshot(Response)) { return; }
                UE_LOG(LogTemp,Warning,TEXT("GATHERING_NODE_SNAPSHOT_FAILED Id=%s Code=%d"),*Self->ServiceNodeId,Response.GrpcCode);
                Self->State.Charges=0; Self->bServiceStateReady=false; ++Self->State.Revision; Self->OnRep_State(); Self->ForceNetUpdate();
                Self->GetWorldTimerManager().SetTimer(Self->ServiceRefreshTimer,Self,&ASFGatheringPilotNode::RefreshServiceState,
                    GetDefault<USFGameplayServicesSettings>()->GatheringNodeRetrySeconds,false);
            }
        });
        return;
    }
    GetWorldTimerManager().SetTimer(ServiceRefreshTimer,this,&ASFGatheringPilotNode::RefreshServiceState,
        GetDefault<USFGameplayServicesSettings>()->GatheringNodeRetrySeconds,false);
}

void ASFGatheringPilotNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFGatheringPilotNode,State); DOREPLIFETIME(ASFGatheringPilotNode,Kind);
}

bool ASFGatheringPilotNode::GetResourceProfile(ESFGatheringSkill& Skill,ESFGatheringMaterialTier& Tier) const
{
    // These released item identities are common beginner materials. New tiers need
    // their own published resource/items and visuals, never an arbitrary retag.
    const auto Profile=GetAuthoredResourceProfile();
    if (Profile.ResourceId.IsNone()) { return false; }
    Skill=Profile.Skill; Tier=Profile.Tier; return true;
}

FSFGatheringResourceProfile ASFGatheringPilotNode::GetAuthoredResourceProfile() const
{
    FSFGatheringResourceProfile Profile;
    if (Kind==ESFGatheringPilotKind::Oak) { Profile.ResourceId=TEXT("resource/oak_log"); Profile.Skill=ESFGatheringSkill::Logging; }
    else if (Kind==ESFGatheringPilotKind::Iron) { Profile.ResourceId=TEXT("resource/iron_ore"); Profile.Skill=ESFGatheringSkill::Mining; }
    else if (Kind==ESFGatheringPilotKind::Stone) { Profile.ResourceId=TEXT("resource/rough_stone"); Profile.Skill=ESFGatheringSkill::Mining; }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (TestProfileTier.IsSet()) { Profile.Tier=TestProfileTier.GetValue(); }
#endif
    return Profile;
}

FString ASFGatheringPilotNode::GetResourceName() const
{
    switch (Kind)
    {
    case ESFGatheringPilotKind::Oak: return TEXT("Oak logs");
    case ESFGatheringPilotKind::Iron: return TEXT("Iron ore");
    case ESFGatheringPilotKind::Stone: return TEXT("Rough stone");
    case ESFGatheringPilotKind::Sawmill: return TEXT("Sawmill");
    case ESFGatheringPilotKind::Forge: return TEXT("Forge");
    case ESFGatheringPilotKind::Workbench: return TEXT("Workbench");
    default: return TEXT("Unavailable");
    }
}

void ASFGatheringPilotNode::OnRep_State()
{
    SetActorHiddenInGame(!State.bPlacementActive);
    ClearDressingAtPlacement();
    if (!IsRefinery())
    {
        // Sprouts remain traceable for harvest while pawns can walk through them.
        Mesh->SetCollisionEnabled(State.bPlacementActive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Mesh->SetCollisionResponseToChannel(ECC_Pawn,State.bGrowthBlocking ? ECR_Block : ECR_Ignore);
    }
    if (HarvestVisuals&&HarvestVisuals->IsReady())
    {
        HarvestVisuals->ApplyState(State.Cuts,State.Charges==0,State.Generation,State.FallingTree);
        Mesh->SetVisibility(false); if (Label) { Label->SetVisibility(false); }
    }
    ApplyGrowthVisual();
    if (IsRefinery())
    {
        if (UStaticMesh* Prop=StationPropMesh(Kind)) { Mesh->SetStaticMesh(Prop); Mesh->SetRelativeScale3D(FVector(1.0)); }
        else { Mesh->SetStaticMesh(StationMesh); Mesh->SetRelativeScale3D(FVector(1.4,0.8,0.8)); }
    }
    // A map-placed node on a dedicated server has no label: servers do not load text render components.
    if (!Label) {}
    else if (IsRefinery())
    {
        // Recipes are content (content/crafting); a station with several lists them and C chooses.
        TArray<FString> Recipes;
        for (const FSFStationRecipe* Recipe : SFStationRecipes::ForStation(USFGatheringPilotComponent::StationName(Kind))) { Recipes.Add(Recipe->Describe()); }
        Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s | %s"),*GetResourceName(),
            State.bBusy ? TEXT("In use") : Recipes.Num()>1 ? TEXT("E: craft, C: recipe") : TEXT("E: craft"),*FString::Join(Recipes,TEXT(" / ")))));
        Label->SetTextRenderColor(FColor::Yellow);
    }
    else
    {
        Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s | %d charges"),*GetResourceName(),
            State.Charges==0 ? TEXT("Depleted") : State.bBusy ? TEXT("In use") : TEXT("E: harvest"),State.Charges)));
        Label->SetTextRenderColor(State.Charges==0 ? FColor::Silver : Kind==ESFGatheringPilotKind::Oak ? FColor::Green : FColor::Cyan);
    }
    if (Label&&!ShowStationLabels()) { Label->SetVisibility(false); }
    if (HasActorBegunPlay() && GetNetMode()!=NM_DedicatedServer) { RefreshResourcePresentation(GetPresentationState()); }
}

void ASFGatheringPilotNode::Release(USFGatheringPilotComponent* Interactor)
{
    if (HasAuthority() && LockedBy.Get()==Interactor)
    { LockedBy.Reset(); State.bBusy=false; OnRep_State(); ForceNetUpdate(); }
}

USFGatheringPilotComponent::USFGatheringPilotComponent()
{
    SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=true;
}

bool USFGatheringPilotComponent::IsPilotRequested()
{
    return GetDefault<USFGatheringPilotSettings>()->bEnabled;  // GM-01: typed config, no launch flag
}

void USFGatheringPilotComponent::BeginPlay()
{
    Super::BeginPlay();
    CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),this);
    CapturedProgressionSettings=DuplicateObject<USFGatheringProgressionSettings>(GetDefault<USFGatheringProgressionSettings>(),this);
    FString Error;
    if (!CapturedProgressionSettings->Validate()) { UE_LOG(LogTemp,Error,TEXT("GATHERING_PROGRESSION_CONFIGURATION_FAILED Pilot disabled; release configuration outside declared bounds.")); return; }
    if (!CapturedSettings->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_CONFIGURATION_FAILED %s"),*Error); return; }
    if (GetOwner()->HasAuthority())
    {
        const auto* Mode=GetWorld()->GetAuthGameMode<ASFGameMode>();
        bEnabled=IsPilotRequested() && Mode && Mode->IsAdmitted();
        if (bEnabled && !InitializeStarterTools())
        {
            bEnabled=false;
            UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_STARTER_TOOLS_FAILED No partial loadout granted; check released tool assets and bag capacity."));
        }
        const auto* LedgerSettings=GetDefault<USFInventoryLedgerSettings>();
        if (bEnabled && LedgerSettings->bEnabled)
        {
            Backpack=USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetOwner()));
            if (!Backpack.IsValid()) { bEnabled=false; }  // fail closed: an enabled ledger never falls back to session-only grants
        }
        CaptureSkillLockSupport();
    }
}

void USFGatheringPilotComponent::CaptureSkillLockSupport()
{
    // Configuration chooses the adapter at admission, never connection health or restore readiness.
    bDurableSkillLocksMode=bEnabled && GetDefault<USFGameplayServicesSettings>()->bEnabled;
    bSessionSkillLocksSupported=bEnabled && IsAuthoritativePilotWorld()
        && !GetDefault<USFInventoryLedgerSettings>()->bEnabled
        && !bDurableSkillLocksMode;
}

void USFGatheringPilotComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner()->HasAuthority() && IsValid(Feedback.Node)) { Feedback.Node->Release(this); }
    Super::EndPlay(Reason);
}

void USFGatheringPilotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,Feedback,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,Skills,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,CharacterLevel,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,RestedMinutes,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,SkillLockReadyAt,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bEnabled,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bSessionSkillLocksSupported,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bPrimaryRole,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bRoleAssigned,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,StationRecipes,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,KnownRecipes,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,LastEquipResult,COND_OwnerOnly);
}

float USFGatheringPilotComponent::GetSkillLockCooldownRemaining(ESFGatheringSkill Skill) const
{
    const int32 Index=int32(Skill);
    if (!GetWorld()||!SkillLockReadyAt.IsValidIndex(Index)) { return 0; }
    if (!GetOwner()->HasAuthority())
    { return float(FMath::Max(0.0,LocalSkillLockReadyAt[Index]-GetWorld()->GetTimeSeconds())); }
    const auto* GS=GetWorld()->GetGameState();
    const double Now=GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    return float(FMath::Max(0.0,SkillLockReadyAt[Index]-Now));
}

bool USFGatheringPilotComponent::IsSkillLockReady(ESFGatheringSkill Skill) const
{
    if (uint8(Skill)>=6||!GetOwner()) { return false; }
    return GetOwner()->HasAuthority() ? GetSkillLockCooldownRemaining(Skill)<=0 : LocalSkillLockReady[int32(Skill)];
}

bool USFGatheringPilotComponent::RequestSkillLock(ESFGatheringSkill Skill,ESFGatheringSkillLock Lock)
{
    const auto* PC=Cast<APlayerController>(GetOwner());
    if (SupportsSkillLocks()&&PC&&PC->IsLocalController()&&uint8(Skill)<6&&uint8(Lock)<=uint8(ESFGatheringSkillLock::Locked)
        &&!IsSkillLockChangePending(Skill)&&IsSkillLockReady(Skill)&&GetSkillLockCooldownRemaining(Skill)<=0)
    { SkillLockPending[int32(Skill)]=true; ServerChangeSkillLock(Skill,Lock); return true; }
    return false;
}

void USFGatheringPilotComponent::ClientSkillLockTiming_Implementation(ESFGatheringSkill Skill,float RemainingSeconds)
{
    if (uint8(Skill)>=6||!GetWorld()||!FMath::IsFinite(RemainingSeconds)||RemainingSeconds<0||RemainingSeconds>60) { return; }
    // Receipt-relative remaining time avoids jumps in the client's estimated server clock.
    // The server still makes every admission decision; this countdown is advisory.
    LocalSkillLockReadyAt[int32(Skill)]=GetWorld()->GetTimeSeconds()+RemainingSeconds;
    LocalSkillLockReady[int32(Skill)]=RemainingSeconds==0;
    SkillLockPending[int32(Skill)]=false;
}

namespace
{
bool ParseServiceSkillLock(const FString& Name, ESFGatheringSkillLock& Out)
{
    if (Name==TEXT("up")) { Out=ESFGatheringSkillLock::Up; return true; }
    if (Name==TEXT("down")) { Out=ESFGatheringSkillLock::Down; return true; }
    if (Name==TEXT("locked")) { Out=ESFGatheringSkillLock::Locked; return true; }
    return false;
}

int64 CurrentUnixMs()
{
    const FDateTime Now=FDateTime::UtcNow();
    return Now.ToUnixTimestamp()*1000+Now.GetMillisecond();
}
}

void USFGatheringPilotComponent::RestoreSkillLocks(const TMap<FString,FString>& ServiceLocks,
    const TMap<FString,int64>& ReadyAtUnixMs, bool bAuthoritative)
{
    if (!GetOwner()||!GetOwner()->HasAuthority()||!bDurableSkillLocksMode||!GetWorld()) { return; }
    // A new host must send the complete six-skill snapshot. Missing or malformed data never enables controls.
    TArray<ESFGatheringSkillLock> Parsed;
    TArray<double> Deadlines;
    const int64 NowMs=CurrentUnixMs();
    const double WorldNow=GetWorld()->GetTimeSeconds();
    for (int32 Index=0; Index<6 && bAuthoritative; ++Index)
    {
        const FString Id=TEXT("skill/")+FString(FSFGatheringProgressionSession::SkillName(ESFGatheringSkill(Index))).ToLower();
        const FString* Name=ServiceLocks.Find(Id);
        const int64* Ready=ReadyAtUnixMs.Find(Id);
        ESFGatheringSkillLock Lock;
        if (!Name||!Ready||!ParseServiceSkillLock(*Name,Lock)||*Ready<0||*Ready>NowMs+60000)
        { bAuthoritative=false; break; }
        Parsed.Add(Lock);
        Deadlines.Add(WorldNow+FMath::Max(0.0,double(*Ready-NowMs)/1000.0));
    }
    bSessionSkillLocksSupported=bAuthoritative && Parsed.Num()==6;
    if (bSessionSkillLocksSupported)
    {
        Skills.Locks=MoveTemp(Parsed);
        SkillLockReadyAt=MoveTemp(Deadlines);
        for (int32 Index=0; Index<6; ++Index)
        {
            if (!ServerSkillLockPending[Index])
            { ClientSkillLockTiming(ESFGatheringSkill(Index),float(FMath::Max(0.0,SkillLockReadyAt[Index]-WorldNow))); }
        }
    }
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::ApplyDurableSkillLockAck(ESFGatheringSkill Skill,
    ESFGatheringSkillLock Lock, int64 ReadyAtUnixMs)
{
    const int32 Index=int32(Skill);
    if (!GetOwner()||!GetOwner()->HasAuthority()||!SupportsSkillLocks()||!bDurableSkillLocksMode
        ||!Skills.Locks.IsValidIndex(Index)||!ServerSkillLockPending.IsValidIndex(Index)
        ||!ServerSkillLockPending[Index]
        ||ReadyAtUnixMs<0||ReadyAtUnixMs>CurrentUnixMs()+60000)
    { RejectDurableSkillLock(Skill); return; }
    const double Remaining=FMath::Max(0.0,double(ReadyAtUnixMs-CurrentUnixMs())/1000.0);
    Skills.Locks[Index]=Lock;
    SkillLockReadyAt[Index]=GetWorld()->GetTimeSeconds()+Remaining;
    ServerSkillLockPending[Index]=false;
    SkillLockReadyNotifications|=uint8(1<<Index);
    ClientSkillLockTiming(Skill,float(Remaining));
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::RejectDurableSkillLock(ESFGatheringSkill Skill)
{
    const int32 Index=int32(Skill);
    if (!ServerSkillLockPending.IsValidIndex(Index)) { return; }
    ServerSkillLockPending[Index]=false;
    ClientSkillLockTiming(Skill,GetSkillLockCooldownRemaining(Skill));
}

void USFGatheringPilotComponent::ServerChangeSkillLock_Implementation(ESFGatheringSkill Skill,ESFGatheringSkillLock Lock)
{
    const int32 Index=int32(Skill);
    if (uint8(Skill)>=6||uint8(Lock)>uint8(ESFGatheringSkillLock::Locked)) { return; }
    if (!SupportsSkillLocks()||!IsAuthoritativePilotWorld()
        ||!Skills.Locks.IsValidIndex(Index)||!SkillLockReadyAt.IsValidIndex(Index))
    { ClientSkillLockTiming(Skill,0); return; }
    if (bDurableSkillLocksMode)
    {
        auto* PC=Cast<APlayerController>(GetOwner());
        auto* Pack=PC ? PC->FindComponentByClass<USFBackpackComponent>() : nullptr;
        auto* Session=PC ? PC->FindComponentByClass<USFCharacterSessionComponent>() : nullptr;
        auto* Quest=PC ? PC->FindComponentByClass<USFQuestPilotComponent>() : nullptr;
        if (!PC||!Pack||!Session||!Quest||!USFBackpackComponent::Services()
            ||(!Pack->HasVerifiedIdentity()&&!Pack->HasDevIdentity())
            ||Session->RestoredCharacterId().IsEmpty()||Session->RestoredCharacterId()!=Pack->GetCharacterId()
            ||USFCharacterSessionComponent::IsRestoring(PC)||USFCharacterSessionComponent::IsLinkdead(PC)
            ||ServerSkillLockPending[Index])
        { RejectDurableSkillLock(Skill); return; }
        if (Skills.Locks[Index]==Lock||GetSkillLockCooldownRemaining(Skill)>0)
        { ClientSkillLockTiming(Skill,GetSkillLockCooldownRemaining(Skill)); return; }
        ServerSkillLockPending[Index]=true;
        if (!Quest->QueueGatheringSkillLock(Skill,Lock)) { RejectDurableSkillLock(Skill); }
        return;
    }
    if (!CapturedProgressionSettings) { ClientSkillLockTiming(Skill,0); return; }
    const double Now=GetWorld()->GetTimeSeconds();
    #if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FParse::Param(FCommandLine::Get(),TEXT("GatheringSkillChoicesTrace")))
    { UE_LOG(LogTemp,Display,TEXT("GATHERING_SKILL_CHOICE_REQUEST Skill=%d Requested=%d Current=%d Now=%.3f ReadyAt=%.3f"),Index,int32(Lock),int32(Skills.Locks[Index]),Now,SkillLockReadyAt[Index]); }
    #endif
    if (Now<SkillLockReadyAt[Index]||Skills.Locks[Index]==Lock)
    { ClientSkillLockTiming(Skill,float(FMath::Max(0.0,SkillLockReadyAt[Index]-Now))); return; }
    // Preferences change future gain admission only. Values and practice remain server-owned.
    Skills.Locks[Index]=Lock;
    SkillLockReadyAt[Index]=Now+CapturedProgressionSettings->SkillLockChangeCooldownSeconds;
    SkillLockReadyNotifications|=uint8(1<<Index);
    ClientSkillLockTiming(Skill,CapturedProgressionSettings->SkillLockChangeCooldownSeconds);
    GetOwner()->ForceNetUpdate();
}

bool USFGatheringPilotComponent::IsAuthoritativePilotWorld() const
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) { return false; }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (bTestWorldAdmission && GetWorld()->WorldType==EWorldType::GameRPC && GetNetMode()==NM_Standalone) { return true; }
#endif
    const auto* Mode=GetWorld()->GetAuthGameMode<ASFGameMode>();
    return IsPilotRequested() && Mode && Mode->IsAdmitted();
}

bool USFGatheringPilotComponent::MeetsSkillRequirement(const ASFGatheringPilotNode* Node) const
{
    ESFGatheringSkill Skill; ESFGatheringMaterialTier Tier;
    return !Node->GetResourceProfile(Skill,Tier)||Skills.Get(Skill)>=FSFGatheringProgressionSession::RequiredSkill(Tier);
}

ESFGatheringPilotResult USFGatheringPilotComponent::ValidateTarget(const ASFGatheringPilotNode* Node) const
{
    const auto* PC=Cast<APlayerController>(GetOwner()); const APawn* Pawn=PC ? PC->GetPawn() : nullptr;
    if (!bEnabled || !CapturedSettings || !Pawn || Pawn->GetController()!=PC || Pawn->IsActorBeingDestroyed())
    { return ESFGatheringPilotResult::InvalidContext; }
    if (!IsValid(Node) || Node->IsActorBeingDestroyed() || Node->GetWorld()!=GetWorld()) { return ESFGatheringPilotResult::InvalidTarget; }
    if (Pawn->GetActorLocation().ContainsNaN() || Node->GetActorLocation().ContainsNaN() || Pawn->GetPawnViewLocation().ContainsNaN())
    { return ESFGatheringPilotResult::InvalidContext; }
    const double Reach=CapturedSettings->ReachMetres*100.0;
    if (FVector::DistSquared(Pawn->GetActorLocation(),Node->GetActorLocation())>Reach*Reach) { return ESFGatheringPilotResult::OutOfReach; }
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GatheringPilotLOS),false,Pawn); FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit,Pawn->GetPawnViewLocation(),Node->GetActorLocation(),ECC_Visibility,Params)
        && Hit.GetActor()!=Node) { return ESFGatheringPilotResult::Occluded; }
    return ESFGatheringPilotResult::None;
}

bool USFGatheringPilotComponent::InventoryReady() const
{
    return GetOwner()->FindComponentByClass<USFInventoryComponent>()
        && LoadObject<USFItemDefinition>(nullptr,ItemPath(ESFGatheringPilotKind::Oak))
        && LoadObject<USFItemDefinition>(nullptr,ItemPath(ESFGatheringPilotKind::Iron));
}

bool USFGatheringPilotComponent::InitializeStarterTools()
{
    if (!IsAuthoritativePilotWorld() || !CapturedSettings) { return false; }
    if (bStarterToolsGranted || !CapturedSettings->bGrantStarterTools) { return true; }
    auto* Inventory=GetOwner()->FindComponentByClass<USFInventoryComponent>();
    FSFItemQuantity Axe; Axe.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ToolPath(true))); Axe.Quantity=1;
    FSFItemQuantity Pick; Pick.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ToolPath(false))); Pick.Quantity=1;
    if (SFInventoryTransactions::ExchangeInventoryItems(Inventory,{}, {Axe,Pick})!=ESFInventoryResult::Success) { return false; }
    bStarterToolsGranted=true; GetOwner()->ForceNetUpdate(); return true;
}

bool USFGatheringPilotComponent::HasRequiredTool(ESFGatheringPilotKind Kind) const
{
    if (Kind==ESFGatheringPilotKind::Sawmill || Kind==ESFGatheringPilotKind::Forge || Kind==ESFGatheringPilotKind::Workbench) { return true; }
    const auto* Inventory=GetOwner()->FindComponentByClass<USFInventoryComponent>();
    TSoftObjectPtr<USFItemDefinition> Equipped; int32 Quantity=0;
    // These two released definitions are the pilot's tier-one axe/pickaxe catalogue.
    return Inventory && Inventory->GetEquippedItem(MainHandSlot(),Equipped,Quantity) && Quantity==1
        && Equipped.ToSoftObjectPath()==FSoftObjectPath(ToolPath(Kind==ESFGatheringPilotKind::Oak));
}

bool USFGatheringPilotComponent::TryEquipTool(bool bAxe)
{
    if (!bEnabled) { return false; }
    if (HasRequiredTool(bAxe ? ESFGatheringPilotKind::Oak : ESFGatheringPilotKind::Iron)) { return true; }
    // Tools are main-hand items: the same validated request the Equipment window sends.
    return RequestEquip(FSoftObjectPath(ToolPath(bAxe)),MainHandSlot());
}

const TArray<FName>& USFGatheringPilotComponent::EquipmentSlots()
{
    static const TArray<FName> Slots={TEXT("Head"),TEXT("Chest"),TEXT("Hands"),TEXT("Legs"),TEXT("Feet"),TEXT("MainHand"),TEXT("OffHand"),TEXT("Neck"),TEXT("Ring"),TEXT("Ring2"),TEXT("Back")};
    return Slots;
}

int32 USFGatheringPilotComponent::FindBagSlot(const FSoftObjectPath& Item) const
{
    const auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    for (int32 I=0; Inventory && !Item.IsNull() && I<FMath::Clamp(Inventory->NumSlots,0,64); ++I)
    {
        TSoftObjectPtr<USFItemDefinition> Held; int32 Quantity=0;
        if (Inventory->GetSlot(I,Held,Quantity) && Quantity>0 && Held.ToSoftObjectPath()==Item) { return I; }
    }
    return INDEX_NONE;
}

FSoftObjectPath USFGatheringPilotComponent::GetEquipped(FName Slot) const
{
    const auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    TSoftObjectPtr<USFItemDefinition> Equipped; int32 Quantity=0;
    if (Inventory && Inventory->GetEquippedItem(Slot,Equipped,Quantity)) { return Equipped.ToSoftObjectPath(); }
    return FSoftObjectPath();
}

bool USFGatheringPilotComponent::RequestEquip(const FSoftObjectPath& Item, FName Slot, const FString& InstanceId)
{
    const auto* PC=Cast<APlayerController>(GetOwner());
    if (!bEnabled || !PC || !EquipmentSlots().Contains(Slot) || FindBagSlot(Item)==INDEX_NONE) { return false; }
    if (Slot==MainHandSlot()) { CancelLocalHarvest(); }  // a tool change ends the channel (the server revalidates the tool anyway)
    if (PC->HasAuthority()) { const ESFEquipResult R=ApplyEquip(Item,Slot,InstanceId); return R==ESFEquipResult::Ok || R==ESFEquipResult::Pending; }
    if (!PC->IsLocalController()) { return false; }
    ServerEquip(Item,Slot,InstanceId);
    return true;
}

bool USFGatheringPilotComponent::RequestUnequip(FName Slot)
{
    const auto* PC=Cast<APlayerController>(GetOwner());
    if (!bEnabled || !PC || GetEquipped(Slot).IsNull()) { return false; }
    if (Slot==MainHandSlot()) { CancelLocalHarvest(); }
    if (PC->HasAuthority()) { const ESFEquipResult R=ApplyUnequip(Slot); return R==ESFEquipResult::Ok || R==ESFEquipResult::Pending; }
    if (!PC->IsLocalController()) { return false; }
    ServerUnequip(Slot);
    return true;
}

void USFGatheringPilotComponent::ServerEquip_Implementation(const FSoftObjectPath& Item, FName Slot, const FString& InstanceId) { ApplyEquip(Item,Slot,InstanceId); }
void USFGatheringPilotComponent::ServerUnequip_Implementation(FName Slot) { ApplyUnequip(Slot); }

ESFEquipResult USFGatheringPilotComponent::ApplyEquip(const FSoftObjectPath& Item, FName Slot, const FString& InstanceId)
{
    ESFEquipResult Result=ESFEquipResult::Disabled;
    const APlayerController* Controller=Cast<APlayerController>(GetOwner());
    const ASFE1Character* Pawn=Controller ? Cast<ASFE1Character>(Controller->GetPawn()) : nullptr;
    auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    const int32 BagSlot=FindBagSlot(Item);  // ownership: the item must be in this player's own bag
    const USFItemDefinition* Definition=BagSlot!=INDEX_NONE ? TSoftObjectPtr<USFItemDefinition>(Item).LoadSynchronous() : nullptr;
    if (!bEnabled || !Inventory || !IsAuthoritativePilotWorld() || (Pawn && Pawn->CombatHealth()<=0)) { Result=ESFEquipResult::Disabled; }
    else if (!EquipmentSlots().Contains(Slot)) { Result=ESFEquipResult::InvalidSlot; }
    else if (BagSlot==INDEX_NONE) { Result=ESFEquipResult::NotOwned; }
    else if (!Definition || Definition->UseType!=ESFItemUseType::Equip) { Result=ESFEquipResult::NotEquippable; }
    else if (Definition->EquipmentSlot!=Slot && !(SFGear::Category(Definition)==ESFEquipmentCategory::Ring && Slot==TEXT("Ring2"))) { Result=ESFEquipResult::WrongSlot; }
    else if (SFGear::Category(Definition)==ESFEquipmentCategory::Shield && Slot!=TEXT("OffHand")) { Result=ESFEquipResult::WrongSlot; }
    else if (Slot==TEXT("OffHand") && !GetEquipped(TEXT("MainHand")).IsNull())
    {
        const USFItemDefinition* Main=TSoftObjectPtr<USFItemDefinition>(GetEquipped(TEXT("MainHand"))).LoadSynchronous();
        const ESFEquipmentCategory Kind=SFGear::Category(Main);
        if (Kind==ESFEquipmentCategory::TwoHand || Kind==ESFEquipmentCategory::Bow || Kind==ESFEquipmentCategory::Staff)
        { Result=ESFEquipResult::OffHandBlocked; }
    }
    if (Result==ESFEquipResult::OffHandBlocked) { LastEquipResult=Result; GetOwner()->ForceNetUpdate(); UE_LOG(LogTemp,Display,TEXT("EQUIP_APPLIED Slot=%s Item=%s Result=%d Reason=%s"),*Slot.ToString(),*Item.GetAssetName(),int32(Result),*StaticEnum<ESFEquipResult>()->GetNameStringByValue(int64(Result))); return Result; }
    if (Result==ESFEquipResult::Disabled && bEnabled && Inventory && IsAuthoritativePilotWorld() && (!Pawn || Pawn->CombatHealth()>0)
        && EquipmentSlots().Contains(Slot) && BagSlot!=INDEX_NONE && Definition && Definition->UseType==ESFItemUseType::Equip
        && (Definition->EquipmentSlot==Slot || (SFGear::Category(Definition)==ESFEquipmentCategory::Ring && Slot==TEXT("Ring2"))))
    {
        const ESFEquipmentCategory Kind=SFGear::Category(Definition);
        const bool bTwoHand=Kind==ESFEquipmentCategory::TwoHand || Kind==ESFEquipmentCategory::Bow || Kind==ESFEquipmentCategory::Staff;
        const FString OffGear=bTwoHand ? GearId(GetEquipped(TEXT("OffHand"))) : FString();
        if (!Inventory->EquipFromSlot(BagSlot,Slot,false)) { Result=ESFEquipResult::NoSpace; }
        else if (USFBackpackComponent* Pack=(GearId(Item).IsEmpty() && GearId(GetEquipped(Slot)).IsEmpty() && OffGear.IsEmpty()) ? nullptr : USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetOwner())))
        {
            TWeakObjectPtr<USFGatheringPilotComponent> Weak(this);
            const auto Resync=[Weak]() { if (auto* Self=Weak.Get()) { Self->LastEquipResult=ESFEquipResult::LedgerRejected; if (auto* P=USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner()))) { P->Reconcile(); } } };
            const auto Apply=[Weak,Item,Slot,Resync](ESFBackpackOutcome Outcome)
            {
                USFGatheringPilotComponent* Self=Weak.Get();
                auto* Inv=Self ? Self->GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
                const int32 Bag=Inv && Outcome==ESFBackpackOutcome::Committed ? Self->FindBagSlot(Item) : INDEX_NONE;
                if (Bag!=INDEX_NONE && Inv->EquipFromSlot(Bag,Slot))
                {
                    Self->LastEquipResult=ESFEquipResult::Ok;
                    Self->GetOwner()->ForceNetUpdate();
                    if (auto* P=USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner()))) { P->Reconcile(); }
                    UE_LOG(LogTemp,Display,TEXT("EQUIP_COMMITTED Item=%s"),*Item.GetAssetName()); return;
                }
                Resync();
            };
            const FString NewGear=GearId(Item), OldGear=GearId(GetEquipped(Slot));
            const auto EquipNew=[Weak,NewGear,InstanceId,Apply]()
            {
                USFBackpackComponent* P=Weak.IsValid() ? USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Weak->GetOwner())) : nullptr;
                if (NewGear.IsEmpty() || !P) { Apply(P ? ESFBackpackOutcome::Committed : ESFBackpackOutcome::Unavailable); }
                else { P->TransferEquipment(NewGear,true,Apply,InstanceId); }
            };
            const auto ClearMain=[Pack,Inventory,Slot,OldGear,EquipNew,Resync]()
            {
                if (OldGear.IsEmpty()) { EquipNew(); return; }
                const FSFEquippedSlot* Worn=Inventory->EquippedItems.FindByPredicate([Slot](const FSFEquippedSlot& S) { return S.SlotName==Slot; });
                Pack->TransferEquipment(OldGear,false,[EquipNew,Resync](ESFBackpackOutcome Outcome) { if (Outcome==ESFBackpackOutcome::Committed) { EquipNew(); } else { Resync(); } }, Worn ? Worn->InstanceId : FString());
            };
            if (OffGear.IsEmpty()) { ClearMain(); }
            else
            {
                const FSFEquippedSlot* Off=Inventory->EquippedItems.FindByPredicate([](const FSFEquippedSlot& S) { return S.SlotName==TEXT("OffHand"); });
                Pack->TransferEquipment(OffGear,false,[ClearMain,Resync](ESFBackpackOutcome Outcome) { if (Outcome==ESFBackpackOutcome::Committed) { ClearMain(); } else { Resync(); } }, Off ? Off->InstanceId : FString());
            }
            Result=ESFEquipResult::Pending;
        }
        else { Result=Inventory->EquipFromSlot(BagSlot,Slot) ? ESFEquipResult::Ok : ESFEquipResult::NoSpace; }
    }
    LastEquipResult=Result;
    GetOwner()->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("EQUIP_APPLIED Slot=%s Item=%s Result=%d Reason=%s"),*Slot.ToString(),*Item.GetAssetName(),int32(Result),*StaticEnum<ESFEquipResult>()->GetNameStringByValue(int64(Result)));
    return Result;
}

ESFEquipResult USFGatheringPilotComponent::ApplyUnequip(FName Slot)
{
    ESFEquipResult Result=ESFEquipResult::Disabled;
    const APlayerController* Controller=Cast<APlayerController>(GetOwner());
    const ASFE1Character* Pawn=Controller ? Cast<ASFE1Character>(Controller->GetPawn()) : nullptr;
    auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    if (!bEnabled || !Inventory || !IsAuthoritativePilotWorld() || (Pawn && Pawn->CombatHealth()<=0)) { Result=ESFEquipResult::Disabled; }
    else if (!EquipmentSlots().Contains(Slot)) { Result=ESFEquipResult::InvalidSlot; }
    else if (GetEquipped(Slot).IsNull()) { Result=ESFEquipResult::NotEquipped; }
    else if (USFBackpackComponent* Pack=GearId(GetEquipped(Slot)).IsEmpty() ? nullptr : USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetOwner())))
    {
        // ponytail: bag space is checked when the slot empties, after the ledger moved it; a full bag ends in a re-read.
        const FSFEquippedSlot* Worn=Inventory->EquippedItems.FindByPredicate([Slot](const FSFEquippedSlot& S) { return S.SlotName==Slot; });
        Pack->TransferEquipment(GearId(GetEquipped(Slot)),false,[Weak=TWeakObjectPtr<USFGatheringPilotComponent>(this),Slot](ESFBackpackOutcome Outcome)
        {
            USFGatheringPilotComponent* Self=Weak.Get();
            auto* Inv=Self ? Self->GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
            if (Inv && Outcome==ESFBackpackOutcome::Committed && Inv->UnequipSlot(Slot))
            { Self->LastEquipResult=ESFEquipResult::Ok; Self->GetOwner()->ForceNetUpdate(); if (auto* P=USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner()))) { P->Reconcile(); } return; }
            if (Self) { Self->LastEquipResult=ESFEquipResult::LedgerRejected; }
            if (auto* P=Self ? USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner())) : nullptr) { P->Reconcile(); }
        }, Worn ? Worn->InstanceId : FString());
        Result=ESFEquipResult::Pending;
    }
    else { Result=Inventory->UnequipSlot(Slot) ? ESFEquipResult::Ok : ESFEquipResult::NoSpace; }
    LastEquipResult=Result;
    GetOwner()->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("UNEQUIP_APPLIED Slot=%s Result=%d"),*Slot.ToString(),int32(Result));
    return Result;
}

void USFGatheringPilotComponent::ProjectEquipment(const TMap<FString,int64>& Equipment, const TArray<FSFLedgerInstance>& Instances)
{
    auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    if (!Inventory) { return; }
    // The backpack projection already counts unequipped gear in the bag, so gear moves in and out of the slots here.
    const auto Change=[Inventory](const FString& Id, bool bAdd)
    {
        FSFInventoryChange Delta; Delta.Inventory=Inventory;
        FSFItemQuantity One; One.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Id))); One.Quantity=1;
        (bAdd ? Delta.AddItems : Delta.RemoveItems).Add(One);
        return SFInventoryTransactions::ExecuteInventoryTransaction({Delta})==ESFInventoryResult::Success;
    };
    int32 Mismatches=0;
    TMap<FString,int64> Remaining=Equipment;
    for (const FName& Slot : EquipmentSlots())
    {
        const FString Gear=GearId(GetEquipped(Slot));
        if (Gear.IsEmpty()) { continue; }
        if (Remaining.FindRef(Gear)>0) { --Remaining.FindOrAdd(Gear); continue; }
        if (!(Inventory->UnequipSlot(Slot) && Change(Gear,false))) { ++Mismatches; }
    }
      for (const TPair<FString,int64>& Held : Remaining)
    {
        const FSFGearStats* Stats=SFGear::Find(Held.Key);
        if (Held.Value<=0 || !Stats) { continue; }
        for (int64 Copy=0; Copy<Held.Value; ++Copy)
        {
            const FName Target=Stats->Category==ESFEquipmentCategory::Ring && !GetEquipped(TEXT("Ring")).IsNull() ? FName(TEXT("Ring2")) : Stats->Slot;
            const int32 Bag=Change(Held.Key,true) ? FindBagSlot(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Held.Key))) : INDEX_NONE;
            if (Bag==INDEX_NONE || !Inventory->EquipFromSlot(Bag,Target))
            { if (Bag!=INDEX_NONE) { Change(Held.Key,false); } ++Mismatches; }
        }
      }
      for (FSFEquippedSlot& Slot : Inventory->EquippedItems)
      {
          const USFItemDefinition* Definition = Slot.Item.LoadSynchronous();
          if (Definition)
          {
              const FSFLedgerInstance* Current = Instances.FindByPredicate([Definition,&Slot](const FSFLedgerInstance& Copy)
                  { return Copy.DefinitionId == Definition->UniqueId && (Slot.InstanceId.IsEmpty() || Copy.InstanceId == Slot.InstanceId); });
              if (!Current) { Current = Instances.FindByPredicate([Definition](const FSFLedgerInstance& Copy) { return Copy.DefinitionId == Definition->UniqueId; }); }
              Slot.InstanceId = Current ? Current->InstanceId : FString();
              Slot.DurabilityPercent = FMath::Clamp(Current ? Current->DurabilityPercent : 100, 0, 100);
          }
      }
    GetOwner()->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("EQUIPMENT_PROJECTED Items=%d Mismatches=%d"),Equipment.Num(),Mismatches);
}

bool USFGatheringPilotComponent::GetInventoryWeight(float& TotalKg, float& MaxKg) const
{
    const auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    if (Inventory) { TotalKg=Inventory->GetTotalWeight(); MaxKg=Inventory->MaxWeight; return true; }
    return false;
}

TArray<TPair<FText,int32>> USFGatheringPilotComponent::GetInventoryItems() const
{
    TArray<TPair<FText,int32>> Items;
    const auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    if (!Inventory) { return Items; }
    auto Add=[&Items](const TSoftObjectPtr<USFItemDefinition>& Item,int32 Quantity,const TCHAR* Suffix)
    {
        if (Quantity<=0 || Item.IsNull()) { return; }
        const USFItemDefinition* Definition=Item.LoadSynchronous();
        const FString Name=Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName.ToString() : Item.GetAssetName();
        Items.Emplace(FText::FromString(Name+Suffix),Quantity);
    };
    for (int32 I=0; I<FMath::Clamp(Inventory->NumSlots,0,64); ++I)
    {
        TSoftObjectPtr<USFItemDefinition> Item; int32 Quantity=0;
        if (Inventory->GetSlot(I,Item,Quantity)) { Add(Item,Quantity,TEXT("")); }
    }
    for (const FName Slot : EquipmentSlots())
    {
        TSoftObjectPtr<USFItemDefinition> Equipped; int32 Quantity=0;
        if (Inventory->GetEquippedItem(Slot,Equipped,Quantity)) { Add(Equipped,Quantity,TEXT(" (equipped)")); }
    }
    return Items;
}

void USFGatheringPilotComponent::EquipAxe() { TryEquipTool(true); }
void USFGatheringPilotComponent::EquipPickaxe() { TryEquipTool(false); }

FString USFGatheringPilotComponent::GetToolName() const
{
    return HasRequiredTool(ESFGatheringPilotKind::Oak) ? TEXT("Iron axe")
        : HasRequiredTool(ESFGatheringPilotKind::Iron) ? TEXT("Iron pickaxe") : TEXT("None equipped");
}

ESFGatheringPilotResult USFGatheringPilotComponent::ApplyInventory(ESFGatheringPilotKind Kind, bool bCommit)
{
    auto* Inventory=GetOwner()->FindComponentByClass<USFInventoryComponent>();
    FSFInventoryChange Change; Change.Inventory=Inventory;
    FSFItemQuantity Reward; Reward.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ItemPath(Kind)));
    Reward.Quantity=CapturedSettings->UnitsPerCharge;
    if (Kind==ESFGatheringPilotKind::Sawmill || Kind==ESFGatheringPilotKind::Forge || Kind==ESFGatheringPilotKind::Workbench)
    {
        // The player's selected station recipe, from the same content the crafting service validates.
        const FSFStationRecipe* Recipe=ActiveStationRecipe(Kind);
        if (!Recipe) { return ESFGatheringPilotResult::Unavailable; }
        const auto Stack=[Inventory](const FSFItemStack& Item, bool bInput)
        {
            FSFItemQuantity Out; Out.Item=TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(bInput ? ChooseInputGrade(Inventory, Item) : Item.DefinitionId)));
            Out.Quantity=int32(Item.Quantity); return Out;
        };
        for (const FSFItemStack& Input : Recipe->Inputs) { Change.RemoveItems.Add(Stack(Input, true)); }
        for (const FSFItemStack& Output : Recipe->Outputs) { Change.AddItems.Add(Stack(Output, false)); }
    }
    else { Change.AddItems.Add(Reward); }
    const auto Result=bCommit ? SFInventoryTransactions::ExecuteInventoryTransaction({Change})
        : SFInventoryTransactions::CanExecuteInventoryTransaction({Change});
    switch (Result)
    {
    case ESFInventoryResult::Success: return ESFGatheringPilotResult::None;
    case ESFInventoryResult::InsufficientSpace:
    case ESFInventoryResult::WeightLimitExceeded: return ESFGatheringPilotResult::InventoryFull;
    case ESFInventoryResult::InsufficientItems: return ESFGatheringPilotResult::MissingMaterials;
    default: return ESFGatheringPilotResult::Unavailable;
    }
}

void USFGatheringPilotComponent::ServerGatheringActionAtSurface_Implementation(ASFGatheringPilotNode* Node,uint32 Revision,uint32 Operation,FVector_NetQuantize10 RequestedPoint)
{
    ProcessGatheringAction(Node,Revision,Operation,true,RequestedPoint);
}

void USFGatheringPilotComponent::ServerGatheringAction_Implementation(ASFGatheringPilotNode* Node, uint32 Revision, uint32 Operation)
{
    ProcessGatheringAction(Node,Revision,Operation,false,FVector::ZeroVector);
}

bool USFGatheringPilotComponent::MatchesRequestBinding(ASFGatheringPilotNode* Node,uint32 Revision,bool bSurface,const FVector& Point) const
{
    return LastNode.Get()==Node && LastRevision==Revision && bLastHadSurface==bSurface
        && (!bSurface || LastRequestedSurface.Equals(Point,0));
}

bool USFGatheringPilotComponent::TryBeginGeometryAttempt(double Now)
{
    if (!CapturedSettings || !FMath::IsFinite(Now) || Now<NextGeometryAttemptSeconds) { return false; }
    NextGeometryAttemptSeconds=Now+CapturedSettings->GeometryAttemptCooldownSeconds;
    return true;
}

void USFGatheringPilotComponent::ProcessGatheringAction(ASFGatheringPilotNode* Node,uint32 Revision,uint32 Operation,bool bSurface,const FVector& RequestedPoint)
{
    if (!IsAuthoritativePilotWorld() || !bEnabled) { return; }
    FVector_NetQuantize10 Point=FVector::ZeroVector;
    const bool bValidPoint=!bSurface || FSFHarvestVisualCut::CanonicalizePoint(RequestedPoint,Point);
    if (Operation==Feedback.Operation)
    {
        if (!bValidPoint || !MatchesRequestBinding(Node,Revision,bSurface,Point)) { ClientKeyConflict(Operation); }
        else { GetOwner()->ForceNetUpdate(); } // Replay returns the current original outcome, never a second grant.
        return;
    }
    if (Feedback.Operation==MAX_uint32 || Operation!=Feedback.Operation+1 || Feedback.Phase==ESFGatheringPilotPhase::Harvesting
        || Feedback.Phase==ESFGatheringPilotPhase::Committing)
    { ClientKeyConflict(Operation); return; }
    Feedback.Operation=Operation; Feedback.Node=Node; Feedback.EndsAtServerSeconds=0; Feedback.bSkillGained=false;
    Feedback.RequiredSkill=0; Feedback.CurrentSkill=0;
    if (USFCharacterSessionComponent::IsRestoring(GetOwner())) { Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::Unavailable); return; }
    ESFGatheringSkill ResourceSkill; ESFGatheringMaterialTier ResourceTier;
    if (IsValid(Node)&&Node->GetResourceProfile(ResourceSkill,ResourceTier))
    {
        Feedback.Skill=ResourceSkill; Feedback.CurrentSkill=Skills.Get(ResourceSkill);
        Feedback.RequiredSkill=FSFGatheringProgressionSession::RequiredSkill(ResourceTier);
    }
    LastNode=Node; LastRevision=Revision;
    bLastHadSurface=bSurface; LastRequestedSurface=Point; bActiveVisualCut=false;
    ESFGatheringPilotResult Result=bValidPoint ? ValidateTarget(Node) : ESFGatheringPilotResult::InvalidTarget;
    if (Result==ESFGatheringPilotResult::None && Node->State.Revision!=Revision) { Result=ESFGatheringPilotResult::Stale; }
    if (Result==ESFGatheringPilotResult::None && Node->bUsesServices && !Node->bServiceStateReady)
    { Result=ESFGatheringPilotResult::Unavailable; }
    if (Result==ESFGatheringPilotResult::None && Node->bUsesServices && !Backpack.IsValid())
    { Result=ESFGatheringPilotResult::Unavailable; }
    if (Result==ESFGatheringPilotResult::None && !Node->IsRefinery() && Node->State.Charges<=0) { Result=ESFGatheringPilotResult::Depleted; }
    if (Result==ESFGatheringPilotResult::None && Node->State.bBusy) { Result=ESFGatheringPilotResult::Busy; }
    if (Result==ESFGatheringPilotResult::None && !MeetsSkillRequirement(Node)) { Result=ESFGatheringPilotResult::SkillRequired; }
    if (Result==ESFGatheringPilotResult::None && !HasRequiredTool(Node->Kind)) { Result=ESFGatheringPilotResult::ToolRequired; }
    if (Result==ESFGatheringPilotResult::None && !InventoryReady()) { Result=ESFGatheringPilotResult::Unavailable; }
    if (Result==ESFGatheringPilotResult::None) { Result=ApplyInventory(Node->Kind,false); }
    if (Result==ESFGatheringPilotResult::None && bSurface && !TryBeginGeometryAttempt(GetWorld()->GetTimeSeconds()))
    { Result=ESFGatheringPilotResult::Unavailable; }
    if (Result==ESFGatheringPilotResult::None&&bSurface)
    {
        const auto* PC=Cast<APlayerController>(GetOwner()); FVector Eye; FRotator Rotation;
        PC->GetPawn()->GetActorEyesViewPoint(Eye,Rotation);
        if (!Node->HarvestVisuals||!Node->HarvestVisuals->IsReady()||!Node->HarvestVisuals->PrepareCut(Eye,Point,PC->GetPawn(),CapturedSettings->ReachMetres*100.0,Node->Mesh,ActiveVisualCut))
        { Result=ESFGatheringPilotResult::InvalidTarget; }
        else { bActiveVisualCut=true; }
    }
    if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Rejected,Result); return; }
    ActiveKind=Node->Kind;
    ActivePracticeInputSeconds=GetWorld()->GetTimeSeconds();
    ActivePracticeDurationSeconds=CapturedSettings->HarvestSeconds;
    ActiveResourceProfile=Node->GetAuthoredResourceProfile();
    // Stable content + authored position across renewal. Production World cell IDs
    // and character identity are intentionally unavailable in this session adapter.
    ActivePracticeContext=HashCombine(GetTypeHash(Node->GetAuthoredResourceProfile().ResourceId),GetTypeHash(Node->GetActorLocation()));
    Node->LockedBy=this; Node->State.bBusy=true; Node->OnRep_State(); Node->ForceNetUpdate();
    const auto* PC=Cast<APlayerController>(GetOwner()); HarvestPawn=PC->GetPawn(); HarvestStart=HarvestPawn->GetActorLocation();
    Feedback.Phase=ESFGatheringPilotPhase::Harvesting; Feedback.Result=ESFGatheringPilotResult::Started;
    Feedback.EndsAtServerSeconds=GetWorld()->GetTimeSeconds()+(Node->IsRefinery() ? CapturedSettings->RefineSeconds : CapturedSettings->HarvestSeconds);
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::Finish(ESFGatheringPilotPhase Phase, ESFGatheringPilotResult Result)
{
    if (IsValid(Feedback.Node)) { Feedback.Node->Release(this); }
    Feedback.Phase=Phase; Feedback.Result=Result; Feedback.EndsAtServerSeconds=0; HarvestPawn.Reset();
    UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_OUTCOME Operation=%u Result=%d"),Feedback.Operation,int32(Result));
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::ServerCancelHarvest_Implementation(uint32 Operation)
{
    if (Operation==Feedback.Operation && Feedback.Phase==ESFGatheringPilotPhase::Harvesting)
    { Finish(ESFGatheringPilotPhase::Canceled,ESFGatheringPilotResult::Canceled); }
}

void USFGatheringPilotComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    if (GetOwner()->HasAuthority()&&bEnabled&&SkillLockReadyNotifications)
    {
        const double Now=GetWorld()->GetTimeSeconds();
        for (int32 Index=0;Index<6;++Index)
        {
            if ((SkillLockReadyNotifications&(1<<Index))&&Now>=SkillLockReadyAt[Index])
            { SkillLockReadyNotifications&=uint8(~(1<<Index)); ClientSkillLockTiming(ESFGatheringSkill(Index),0); }
        }
    }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TickFiniteNetworkScenario();
    TickArtReviewScenario();
    TickFellingNetworkScenario();
    TickFellingReviewScenario();
    TickSkillChoicesNetworkScenario();
#endif
    if (!GetOwner()->HasAuthority() || !bEnabled || Feedback.Phase!=ESFGatheringPilotPhase::Harvesting) { return; }
    if (!IsAuthoritativePilotWorld()) { Finish(ESFGatheringPilotPhase::Canceled,ESFGatheringPilotResult::InvalidContext); return; }
    auto Result=ValidateTarget(Feedback.Node);
    const auto* PC=Cast<APlayerController>(GetOwner()); const APawn* Pawn=PC ? PC->GetPawn() : nullptr;
    if (Result==ESFGatheringPilotResult::None && (HarvestPawn.Get()!=Pawn || FVector::DistSquared(HarvestStart,Pawn->GetActorLocation())
        >FMath::Square(CapturedSettings->MovementCancelMetres*100.0))) { Result=ESFGatheringPilotResult::Moved; }
    if (Result==ESFGatheringPilotResult::None && (Feedback.Node->Kind!=ActiveKind || Feedback.Node->LockedBy.Get()!=this || Feedback.Node->State.Revision!=LastRevision
        || (!Feedback.Node->IsRefinery() && Feedback.Node->State.Charges<=0) || Feedback.Node->State.Revision==MAX_uint32)) { Result=ESFGatheringPilotResult::Stale; }
    if (Result==ESFGatheringPilotResult::None && !HasRequiredTool(ActiveKind)) { Result=ESFGatheringPilotResult::ToolRequired; }
    if (Result==ESFGatheringPilotResult::None && !MeetsSkillRequirement(Feedback.Node)) { Result=ESFGatheringPilotResult::SkillRequired; }
    if (Result==ESFGatheringPilotResult::None&&!Feedback.Node->IsRefinery())
    {
        const auto Current=Feedback.Node->GetAuthoredResourceProfile();
        if (Current.ResourceId!=ActiveResourceProfile.ResourceId||Current.Skill!=ActiveResourceProfile.Skill
            ||Current.Tier!=ActiveResourceProfile.Tier||Current.ToolTier!=ActiveResourceProfile.ToolTier) { Result=ESFGatheringPilotResult::Stale; }
    }
    if (Result==ESFGatheringPilotResult::None && bActiveVisualCut)
    {
        FVector Eye; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eye,Rotation);
        if (!Feedback.Node->HarvestVisuals || !Feedback.Node->HarvestVisuals->ValidateContact(Eye,Pawn,
            CapturedSettings->ReachMetres*100.0,Feedback.Node->Mesh,ActiveVisualCut)) { Result=ESFGatheringPilotResult::InvalidTarget; }
    }
    if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Canceled,Result); return; }
    if (GetWorld()->GetTimeSeconds()<Feedback.EndsAtServerSeconds) { return; }
    const bool bRefining=ActiveKind==ESFGatheringPilotKind::Sawmill || ActiveKind==ESFGatheringPilotKind::Forge || ActiveKind==ESFGatheringPilotKind::Workbench;
    if (Backpack.IsValid() && bRefining && USFBackpackComponent::Services())
    {
        // INT-04: the crafting service converts the backpack items; the station stays locked until its receipt.
        Result=ApplyInventory(ActiveKind,false);
        if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Rejected,Result); return; }
        Feedback.Phase=ESFGatheringPilotPhase::Committing; GetOwner()->ForceNetUpdate();
        BeginCraftCommit();
        return;
    }
    if (Backpack.IsValid() && !bRefining)
    {
        // The ledger is the truth for raw materials: check the projection still fits, then hold the node lock
        // until the receipt (or a terminal failure) arrives. Charges are consumed only after the receipt.
        Result=ApplyInventory(ActiveKind,false);
        if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Rejected,Result); return; }
        Feedback.Phase=ESFGatheringPilotPhase::Committing; GetOwner()->ForceNetUpdate();
        BeginLedgerCommit();
        return;
    }
    Result=ApplyInventory(ActiveKind,true);
    if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Rejected,Result); return; }
    CompleteHarvest(bRefining);
}

void USFGatheringPilotComponent::CompleteHarvest(bool bRefining)
{
    // Synchronous game-thread pilot: no fallible reward mutation remains after the inventory commit.
    // Optional cosmetic felling cannot roll back or repeat the granted material.
    if (!bRefining)
    {
        if (!Backpack.IsValid() && CapturedProgressionSettings&&!ActiveResourceProfile.ResourceId.IsNone())
        {
            const int64 GainsBefore=Skills.GainCount;
            ProgressionSession.Practice(Skills,ActiveResourceProfile.Skill,FSFGatheringProgressionSession::Difficulty(ActiveResourceProfile.Tier),ActivePracticeDurationSeconds,
                ActivePracticeContext,ActivePracticeInputSeconds,GetWorld()->GetTimeSeconds(),*CapturedProgressionSettings);
            Feedback.bSkillGained=Skills.GainCount>GainsBefore; Feedback.CurrentSkill=Skills.Get(ActiveResourceProfile.Skill);
        }
        if (bActiveVisualCut) { Feedback.Node->State.Cuts.Add(ActiveVisualCut); }
        if (!bServiceHarvestApplied) { --Feedback.Node->State.Charges; ++Feedback.Node->HarvestedThisGrowth; Feedback.Node->BeginRecovery(); }
        if (Feedback.Node->State.Charges==0&&ActiveKind==ESFGatheringPilotKind::Oak&&bActiveVisualCut&&Feedback.Node->HarvestVisuals)
        { Feedback.Node->State.FallingTree=Feedback.Node->HarvestVisuals->BeginTreeFall(Feedback.Node->State.Generation,ActiveVisualCut); }
    }
    ++Feedback.Node->State.Revision;
    bServiceHarvestApplied=false;
    const FSFStationRecipe* Recipe=bRefining ? ActiveStationRecipe(ActiveKind) : nullptr;
    if (bRefining || !Backpack.IsValid())
    {
        SFQuestPilotEvents::OnGathered().Broadcast(Cast<APlayerController>(GetOwner()),ActiveKind,
            Recipe ? Recipe->Outputs[0].DefinitionId : FString(LedgerItemId(ActiveKind)),CommittedOperation);
    }
    CommittedOperation.Reset();
    Finish(ESFGatheringPilotPhase::Committed,bRefining ? ESFGatheringPilotResult::Refined : ESFGatheringPilotResult::Granted);
}

namespace
{
    ESFBackpackOutcome LedgerOutcome(ESFLedgerStatus Status)
    {
        switch (Status)
        {
        case ESFLedgerStatus::Ok: return ESFBackpackOutcome::Committed;
        case ESFLedgerStatus::Stale: return ESFBackpackOutcome::Stale;
        case ESFLedgerStatus::Rejected: return ESFBackpackOutcome::Rejected;
        default: return ESFBackpackOutcome::Unavailable;
        }
    }
}

void USFGatheringPilotComponent::BeginLedgerCommit()
{
    if (PendingLedgerOperation.IsEmpty())
    { PendingLedgerOperation=TEXT("gather/")+FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower); }
    USFBackpackComponent* Pack=Backpack.Get();
    if (!Pack) { OnLedgerGrant(FSFLedgerResponse()); return; }
    LastLedgerResponse.Reset();
    const FString Operation=PendingLedgerOperation, Item=LedgerItemId(ActiveKind);
    const int64 Quantity=CapturedSettings->UnitsPerCharge;
    const FString Causation=FString::Printf(TEXT("%s:%u"),*Pack->GetContainerId(),Feedback.Operation);
    FSFGameplayRequest FollowUp; FollowUp.Rpc=ESFGameplayRpc::ApplyReceipt;
    FollowUp.OperationId=Operation; FollowUp.Kind=TEXT("gather"); FollowUp.CharacterId=Pack->GetCharacterId();
    const FString NodeId=ActiveKind==ESFGatheringPilotKind::Oak ? TEXT("gathering_node/oak_tree") : TEXT("gathering_node/iron_vein");
    USFQuestPilotComponent* Quest=Cast<APlayerController>(GetOwner())->FindComponentByClass<USFQuestPilotComponent>();
    TArray<uint8> IntentBytes; FString IntentDigest;
    if (!Quest || !Quest->CaptureGatherReceipt(FollowUp,Item,NodeId))
    {
        UE_LOG(LogTemp,Error,TEXT("GATHERING_FOLLOW_UP_CAPTURE_FAILED Op=%s"),*Operation);
        OnLedgerGrant(FSFLedgerResponse()); return;
    }
    // Capture the accepted harvest, not the quest adapter's placeholder difficulty.
    FollowUp.SkillId=TEXT("skill/")+FString(FSFGatheringProgressionSession::SkillName(ActiveResourceProfile.Skill)).ToLower();
    FollowUp.Difficulty=FSFGatheringProgressionSession::Difficulty(ActiveResourceProfile.Tier);
    FollowUp.ActionSeconds=ActivePracticeDurationSeconds;
    FollowUp.ContextHash=ActivePracticeContext;
    if (!FSFInventoryLedgerClient::SerializeFollowUp(FollowUp,IntentBytes,IntentDigest))
    {
        UE_LOG(LogTemp,Error,TEXT("GATHERING_FOLLOW_UP_CAPTURE_FAILED Op=%s"),*Operation);
        OnLedgerGrant(FSFLedgerResponse()); return;
    }
    if (IsValid(Feedback.Node) && Feedback.Node->bUsesServices)
    {
        PendingHarvestRequest=FSFGameplayRequest();
        PendingHarvestRequest.Rpc=ESFGameplayRpc::HarvestGatheringNode;
        PendingHarvestRequest.OperationId=Operation; PendingHarvestRequest.CharacterId=Pack->GetCharacterId();
        PendingHarvestRequest.NodeId=Feedback.Node->ServiceNodeId;
        PendingHarvestRequest.NodeGeneration=int32(Feedback.Node->State.Generation);
        PendingHarvestRequest.ContainerId=Pack->GetContainerId();
        PendingHarvestRequest.FollowUpPayload=MoveTemp(IntentBytes);
        PendingHarvestRequest.FollowUpDigest=MoveTemp(IntentDigest);
        PendingHarvestRequest.FollowUpVersion=1;
        BeginNodeHarvest();
        return;
    }
    // The backpack serialises the local-mode Grant behind any other writer (INT-04).
    TWeakObjectPtr<USFGatheringPilotComponent> Weak(this);
    FSFBackpackMutation Mutation; Mutation.Label=TEXT("gather"); Mutation.OperationId=Operation; Mutation.bRetryStale=false;
    Mutation.Run=[Weak,Item,Quantity,Causation,IntentBytes,IntentDigest](const FString& Op,int64 Expected,TFunction<void(ESFBackpackOutcome,int64)> Report)
    {
        USFGatheringPilotComponent* Self=Weak.Get(); USFBackpackComponent* Owner=Self ? Self->Backpack.Get() : nullptr;
        if (!Owner) { Report(ESFBackpackOutcome::Unavailable,-1); return; }
        FSFLedgerRequest Grant; Grant.Rpc=ESFLedgerRpc::Grant; Grant.OperationId=Op; Grant.ContainerId=Owner->GetContainerId();
        Grant.ExpectedRevision=Expected; Grant.DefinitionId=Item; Grant.Quantity=Quantity;
        Grant.Source=TEXT("gathering"); Grant.CausationId=Causation;
        Grant.FollowUpPayload=IntentBytes; Grant.FollowUpDigest=IntentDigest;
        Owner->GetLedger()->Call(Grant,[Weak,Report](const FSFLedgerResponse& Response)
        {
            if (USFGatheringPilotComponent* Again=Weak.Get()) { Again->LastLedgerResponse=MakeShared<FSFLedgerResponse>(Response); }
            Report(LedgerOutcome(Response.Status),Response.Revision);
        });
    };
    Mutation.Finished=[Weak,Operation](ESFBackpackOutcome Outcome)
    {
        USFGatheringPilotComponent* Self=Weak.Get();
        // A disconnected owner's outcome is still resolved by the ledger; only the reply is dropped.
        if (!Self || Self->Feedback.Phase!=ESFGatheringPilotPhase::Committing || Self->PendingLedgerOperation!=Operation)
        { UE_LOG(LogTemp,Display,TEXT("GATHERING_LEDGER_REPLY_DROPPED Op=%s Code=%d"),*Operation,Self && Self->LastLedgerResponse ? Self->LastLedgerResponse->GrpcCode : -1); return; }
        FSFLedgerResponse Response=Self->LastLedgerResponse ? *Self->LastLedgerResponse : FSFLedgerResponse();
        Response.Status=Outcome==ESFBackpackOutcome::Committed ? ESFLedgerStatus::Ok : Outcome==ESFBackpackOutcome::Stale ? ESFLedgerStatus::Stale
            : Outcome==ESFBackpackOutcome::Rejected ? ESFLedgerStatus::Rejected : ESFLedgerStatus::Unavailable;
        Self->OnLedgerGrant(Response);
    };
    Pack->Enqueue(MoveTemp(Mutation));
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // INT-05 reconnect runner: drop this player's connection once, with the ledger grant in flight (Committing).
    static bool bDropped=false;
    auto* Controller=Cast<APlayerController>(GetOwner());
    if (!bDropped && (!IsValid(Feedback.Node) || !Feedback.Node->bUsesServices) &&
        FParse::Param(FCommandLine::Get(),TEXT("SFDropDuringCommit")) && Controller && Controller->GetNetConnection())
    {
        bDropped=true;
        UE_LOG(LogTemp,Display,TEXT("RECONNECT_DROP_DURING_COMMIT Op=%s Container=%s"),*Operation,*Pack->GetContainerId());
        Controller->GetNetConnection()->Close();
    }
#endif
}

void USFGatheringPilotComponent::OnLedgerGrant(const FSFLedgerResponse& Response)
{
    const FString Operation=PendingLedgerOperation;
    const FString Container=Backpack.IsValid() ? Backpack->GetContainerId() : FString();
    // Node state is logged before any charge is consumed: a failed commit must show it unchanged.
    UE_LOG(LogTemp,Display,TEXT("GATHERING_LEDGER_OUTCOME Op=%s Container=%s Code=%d Revision=%lld Delta=%lld NodeRevision=%d Charges=%d"),*Operation,*Container,
        Response.GrpcCode,Response.Revision,Response.GrantedDelta,IsValid(Feedback.Node) ? int32(Feedback.Node->State.Revision) : -1,IsValid(Feedback.Node) ? Feedback.Node->State.Charges : -1);
    if (Response.Status==ESFLedgerStatus::Stale) { PendingLedgerOperation.Reset(); Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::Stale); return; }
    if (Response.Status!=ESFLedgerStatus::Ok)
    {
        PendingLedgerOperation.Reset();
        // ponytail: an exhausted retry may still have committed; the backpack re-reads before its next mutation.
        // Reconciling that quantity into the session inventory belongs with the GatheringNodes integration slice.
        Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::Unavailable);
        return;
    }
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("GatheringLedgerExitAfterCommit")))
    {
        UE_LOG(LogTemp,Display,TEXT("GATHERING_LEDGER_EXIT_AFTER_COMMIT Op=%s"),*Operation); GLog->Flush();
        FPlatformMisc::RequestExitWithStatus(true,0);
        return;
    }
#endif
    if (ApplyInventory(ActiveKind,true)!=ESFGatheringPilotResult::None)
    { UE_LOG(LogTemp,Error,TEXT("GATHERING_LEDGER_PROJECTION_MISMATCH Op=%s Container=%s"),*Operation,*Container); }
    CommittedOperation=Operation;
    PendingLedgerOperation.Reset();
    if (!IsValid(Feedback.Node))
    {
        Finish(ESFGatheringPilotPhase::Committed,ESFGatheringPilotResult::Granted); return;
    }
    CompleteHarvest(false);
}

void USFGatheringPilotComponent::BeginNodeHarvest()
{
    ASFGatheringPilotNode* Node=Feedback.Node;
    if (!IsValid(Node) || !Node->ServiceClient.IsValid() || PendingHarvestRequest.OperationId.IsEmpty())
    { Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::Unavailable); return; }
    TWeakObjectPtr<USFGatheringPilotComponent> Weak(this);
    Node->ServiceClient->Call(PendingHarvestRequest,[Weak](const FSFGameplayResponse& Reply)
    { if (USFGatheringPilotComponent* Self=Weak.Get()) { Self->OnNodeHarvest(Reply); } });
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    static bool bDropped=false;
    auto* Controller=Cast<APlayerController>(GetOwner());
    if (!bDropped && FParse::Param(FCommandLine::Get(),TEXT("SFDropDuringCommit")) && Controller && Controller->GetNetConnection())
    {
        bDropped=true;
        UE_LOG(LogTemp,Display,TEXT("RECONNECT_DROP_DURING_COMMIT Op=%s Container=%s"),*PendingHarvestRequest.OperationId,*PendingHarvestRequest.ContainerId);
        Controller->GetNetConnection()->Close();
    }
#endif
}

void USFGatheringPilotComponent::OnNodeHarvest(const FSFGameplayResponse& Response)
{
    if (Feedback.Phase!=ESFGatheringPilotPhase::Committing || !IsValid(Feedback.Node)) { return; }
    if (Response.Status==ESFGameplayStatus::Unavailable ||
        (Response.Status==ESFGameplayStatus::Ok && Response.HarvestStatus==TEXT("pending")))
    {
        // The host owns the durable intent; replay its exact request until the grant resolves.
        TWeakObjectPtr<USFGatheringPilotComponent> Weak(this);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
        { if (USFGatheringPilotComponent* Self=Weak.Get()) { Self->BeginNodeHarvest(); } return false; }),
            GetDefault<USFGameplayServicesSettings>()->GatheringNodeRetrySeconds);
        return;
    }
    if (Response.GrpcCode==12)
    {
        UE_LOG(LogTemp,Error,TEXT("GATHERING_HOST_CAPABILITY_REQUIRED Op=%s RPC=HarvestWithIntent Code=12; upgrade Gameplay.Host"),
            *PendingLedgerOperation);
        PendingLedgerOperation.Reset(); PendingHarvestRequest=FSFGameplayRequest();
        Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::Unavailable); return;
    }
    ASFGatheringPilotNode* Node=Feedback.Node;
    ++Node->ServiceSnapshotSerial; // A GetNode requested before this committed harvest cannot rewind its charges or stage.
    if (Response.Status!=ESFGameplayStatus::Ok || !Node->ApplyServiceSnapshot(Response) ||
        Response.HarvestStatus!=TEXT("granted") || Response.ResourceId!=LedgerItemId(ActiveKind) ||
        Response.HarvestQuantity!=CapturedSettings->UnitsPerCharge)
    {
        UE_LOG(LogTemp,Error,TEXT("GATHERING_NODE_COMMIT_MISMATCH Op=%s Status=%s Code=%d"),*PendingLedgerOperation,*Response.HarvestStatus,Response.GrpcCode);
        PendingLedgerOperation.Reset(); PendingHarvestRequest=FSFGameplayRequest();
        Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::Unavailable); return;
    }
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(),TEXT("GatheringLedgerExitAfterCommit")))
    {
        UE_LOG(LogTemp,Display,TEXT("GATHERING_LEDGER_EXIT_AFTER_COMMIT Op=%s Node=%s Charges=%d"),
            *PendingLedgerOperation,*Node->ServiceNodeId,Node->State.Charges);
        GLog->Flush(); FPlatformMisc::RequestExitWithStatus(true,0);
        return;
    }
#endif
    const FString Operation=PendingLedgerOperation; PendingLedgerOperation.Reset(); PendingHarvestRequest=FSFGameplayRequest();
    if (ApplyInventory(ActiveKind,true)!=ESFGatheringPilotResult::None)
    { UE_LOG(LogTemp,Error,TEXT("GATHERING_LEDGER_PROJECTION_MISMATCH Op=%s"),*Operation); }
    // Harvest granted through the host, so refresh the backpack writer's revision before its next queued mutation.
    if (USFBackpackComponent* Pack=Backpack.Get()) { Pack->Reconcile(); }
    bServiceHarvestApplied=true; CommittedOperation=Operation;
    CompleteHarvest(false);
    Node->RefreshServiceState();
}

void USFGatheringPilotComponent::BeginCraftCommit()
{
    if (PendingCraftOperation.IsEmpty())
    { PendingCraftOperation=TEXT("craft/")+FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower); }
    USFBackpackComponent* Pack=Backpack.Get();
    const FSFStationRecipe* Recipe=ActiveStationRecipe(ActiveKind);
    if (!Pack || !Recipe || !USFBackpackComponent::Services())
    { PendingCraftOperation.Reset(); Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::Unavailable); return; }
    if (!KnowsRecipe(*Recipe))
    { PendingCraftOperation.Reset(); Finish(ESFGatheringPilotPhase::Rejected,ESFGatheringPilotResult::InvalidContext); return; }
    const FString Operation=PendingCraftOperation, RecipeId=Recipe->Id;
    TMap<FString,FString> InputGrades;
    auto* Bag = GetOwner()->FindComponentByClass<USFInventoryComponent>();
    for (const FSFItemStack& Input : Recipe->Inputs)
    { InputGrades.Add(Input.DefinitionId, ChooseInputGrade(Bag, Input)); }
    TSharedRef<TMap<FString,int64>> Changes=MakeShared<TMap<FString,int64>>();
    TSharedRef<FString> Discovered=MakeShared<FString>();
    TWeakObjectPtr<USFGatheringPilotComponent> Weak(this);
    FSFBackpackMutation Mutation; Mutation.Label=TEXT("craft"); Mutation.OperationId=Operation;
    Mutation.Run=[Weak,RecipeId,InputGrades,Changes,Discovered](const FString& Op,int64 Expected,TFunction<void(ESFBackpackOutcome,int64)> Report)
    {
        USFGatheringPilotComponent* Self=Weak.Get(); USFBackpackComponent* Owner=Self ? Self->Backpack.Get() : nullptr;
        if (!Owner) { Report(ESFBackpackOutcome::Unavailable,-1); return; }
        FSFGameplayRequest Craft; Craft.Rpc=ESFGameplayRpc::Craft; Craft.OperationId=Op; Craft.CharacterId=Owner->GetCharacterId();
        Craft.ContainerId=Owner->GetContainerId(); Craft.RecipeId=RecipeId; Craft.ExpectedContainerRevision=Expected; Craft.Count=1;
        Craft.InputGrades=InputGrades;
        USFBackpackComponent::Services()->Call(Craft,[Changes,Discovered,Report](const FSFGameplayResponse& Response)
        {
            *Changes=Response.Changes;
            *Discovered=Response.DiscoveredRecipeId;
            // FAILED_PRECONDITION covers a stale container revision: the backpack re-reads and retries once.
            Report(Response.Status==ESFGameplayStatus::Ok ? ESFBackpackOutcome::Committed : Response.Status==ESFGameplayStatus::Unavailable ? ESFBackpackOutcome::Unavailable
                : Response.GrpcCode==9 ? ESFBackpackOutcome::Stale : ESFBackpackOutcome::Rejected, Response.Status==ESFGameplayStatus::Ok ? Response.ContainerRevision : -1);
        });
    };
    Mutation.Finished=[Weak,Operation,RecipeId,Changes,Discovered](ESFBackpackOutcome Outcome)
    {
        USFGatheringPilotComponent* Self=Weak.Get();
        if (!Self || Self->Feedback.Phase!=ESFGatheringPilotPhase::Committing || Self->PendingCraftOperation!=Operation)
        { UE_LOG(LogTemp,Display,TEXT("CRAFTING_SERVICE_REPLY_DROPPED Op=%s"),*Operation); return; }
        Self->PendingCraftOperation.Reset();
        UE_LOG(LogTemp,Display,TEXT("CRAFTING_SERVICE_OUTCOME Op=%s Recipe=%s Outcome=%d Changes=%d StationRevision=%d"),*Operation,*RecipeId,int32(Outcome),Changes->Num(),
            IsValid(Self->Feedback.Node) ? int32(Self->Feedback.Node->State.Revision) : -1);
        if (Outcome!=ESFBackpackOutcome::Committed)
        { Self->Finish(ESFGatheringPilotPhase::Rejected,Outcome==ESFBackpackOutcome::Unavailable ? ESFGatheringPilotResult::Unavailable : ESFGatheringPilotResult::MissingMaterials); return; }
        // The receipt is the truth: project exactly its item changes into the session bag.
        if (USFBackpackComponent* Owner=Self->Backpack.Get()) { Owner->ApplyToBag(*Changes); }
        if (!Discovered->IsEmpty()) { Self->AddKnownRecipe(*Discovered); }
        Self->CommittedOperation=Operation;
        if (!IsValid(Self->Feedback.Node)) { Self->Finish(ESFGatheringPilotPhase::Committed,ESFGatheringPilotResult::Refined); return; }
        Self->CompleteHarvest(true);
    };
    Pack->Enqueue(MoveTemp(Mutation));
}

FString USFGatheringPilotComponent::StationName(ESFGatheringPilotKind Kind)
{
    switch (Kind)
    {
    case ESFGatheringPilotKind::Sawmill: return TEXT("sawmill");
    case ESFGatheringPilotKind::Forge: return TEXT("forge");
    case ESFGatheringPilotKind::Workbench: return TEXT("workbench");
    default: return FString();
    }
}

const FSFStationRecipe* USFGatheringPilotComponent::ActiveStationRecipe(ESFGatheringPilotKind Station) const
{
    const TArray<const FSFStationRecipe*> Recipes=SFStationRecipes::ForStation(StationName(Station));
    if (Recipes.IsEmpty()) { return nullptr; }
    const FString Selected=StationRecipes.IsValidIndex(int32(Station)) ? StationRecipes[int32(Station)] : FString();
    const FSFStationRecipe* const* Match=Recipes.FindByPredicate([&](const FSFStationRecipe* Recipe) { return Recipe->Id==Selected; });
    if (Match && KnowsRecipe(**Match)) { return *Match; }
    const FSFStationRecipe* const* FirstKnown = Recipes.FindByPredicate([this](const FSFStationRecipe* R) { return KnowsRecipe(*R); });
    return FirstKnown ? *FirstKnown : nullptr;
}

FString USFGatheringPilotComponent::GetStationRecipe(ESFGatheringPilotKind Station) const
{
    const FSFStationRecipe* Recipe=ActiveStationRecipe(Station);
    return Recipe ? Recipe->Id : FString();
}

void USFGatheringPilotComponent::SelectStationRecipe(ESFGatheringPilotKind Station, const FString& RecipeId)
{
    if (!bEnabled) { return; }
    if (GetOwner()->HasAuthority()) { ServerSelectRecipe_Implementation(Station,RecipeId); } else { ServerSelectRecipe(Station,RecipeId); }
}

void USFGatheringPilotComponent::CycleStationRecipe(ESFGatheringPilotKind Station)
{
    const TArray<const FSFStationRecipe*> Recipes=SFStationRecipes::ForStation(StationName(Station)).FilterByPredicate(
        [this](const FSFStationRecipe* R) { return KnowsRecipe(*R); });
    if (Recipes.Num()<2) { return; }
    const int32 Current=Recipes.IndexOfByKey(ActiveStationRecipe(Station));
    SelectStationRecipe(Station,Recipes[(Current+1)%Recipes.Num()]->Id);
}

bool USFGatheringPilotComponent::KnowsRecipe(const FSFStationRecipe& Recipe) const
{ return Recipe.bDefaultKnown || KnownRecipes.Contains(Recipe.Id); }

void USFGatheringPilotComponent::RestoreKnownRecipes(const TArray<FString>& Recipes)
{
    if (!GetOwner()->HasAuthority()) { return; }
    KnownRecipes.Reset();
    for (const FString& Id : Recipes) { if (SFStationRecipes::Find(Id)) { KnownRecipes.AddUnique(Id); } }
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::AddKnownRecipe(const FString& RecipeId)
{
    if (GetOwner()->HasAuthority() && SFStationRecipes::Find(RecipeId))
    { KnownRecipes.AddUnique(RecipeId); GetOwner()->ForceNetUpdate(); }
}

bool USFGatheringPilotComponent::CycleRecipeFromRay(const FVector& Start, const FVector& Direction)
{
    ASFGatheringPilotNode* Node=FindNode(Start,Direction);
    if (!Node || !Node->IsRefinery()) { return false; }
    CycleStationRecipe(Node->Kind);
    return true;
}

void USFGatheringPilotComponent::RestoreLevel(int32 ServiceLevel)
{
    CharacterLevel=FMath::Max(1,ServiceLevel);
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::RestoreRestedMinutes(double ServiceMinutes)
{
    RestedMinutes = FMath::IsFinite(ServiceMinutes) ? FMath::Max(0.0, ServiceMinutes) : 0.0;
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::RestoreSkills(const TMap<FString,int32>& ServiceSkills)
{
    for (int32 Index=0; Index<Skills.Values.Num(); ++Index)
    {
        const FString Key=TEXT("skill/")+FString(FSFGatheringProgressionSession::SkillName(ESFGatheringSkill(Index))).ToLower();
        if (const int32* Value=ServiceSkills.Find(Key)) { Skills.Values[Index]=FMath::Clamp(*Value,0,900); }
    }
    GetOwner()->ForceNetUpdate();
}

void USFGatheringPilotComponent::ServerSelectRecipe_Implementation(ESFGatheringPilotKind Station, const FString& RecipeId)
{
    // Only a recipe this station ships, and never mid-craft: the commit uses the recipe chosen when it started.
    if (!IsAuthoritativePilotWorld() || !bEnabled || Feedback.Phase==ESFGatheringPilotPhase::Harvesting || Feedback.Phase==ESFGatheringPilotPhase::Committing) { return; }
    const FSFStationRecipe* Recipe=SFStationRecipes::Find(RecipeId);
    if (!Recipe || Recipe->Station!=StationName(Station) || !KnowsRecipe(*Recipe)) { return; }
    StationRecipes.SetNum(int32(ESFGatheringPilotKind::Workbench)+1);
    StationRecipes[int32(Station)]=RecipeId;
    GetOwner()->ForceNetUpdate();
    UE_LOG(LogTemp,Display,TEXT("GATHERING_RECIPE_SELECTED Station=%s Recipe=%s"),*StationName(Station),*RecipeId);
}


ASFGatheringPilotNode* USFGatheringPilotComponent::FindNode(const FVector& Start, const FVector& Direction) const
{
    if (!bEnabled || Start.ContainsNaN() || Direction.ContainsNaN() || !Direction.IsNormalized()) { return nullptr; }
    const auto* PC=Cast<APlayerController>(GetOwner());
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GatheringPilotCursor),false,PC ? PC->GetPawn() : nullptr); FHitResult Hit;
    // Read-only cursor acquisition; authoritative reach is checked from the pawn again.
    GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+Direction*10000.0,ECC_Visibility,Params);
    return Cast<ASFGatheringPilotNode>(Hit.GetActor());
}

bool USFGatheringPilotComponent::InteractFromRay(const FVector& Start, const FVector& Direction)
{
    return InteractWithTarget(FindNode(Start, Direction), Start, Direction);
}

bool USFGatheringPilotComponent::InteractWithTarget(ASFGatheringPilotNode* Node, const FVector& Start, const FVector& Direction, const FHitResult* Contact)
{
    if (!bEnabled) { return false; }
    if (Feedback.Phase==ESFGatheringPilotPhase::Harvesting) { CancelLocalHarvest(); return true; }
    if (Feedback.Phase==ESFGatheringPilotPhase::Committing) { return true; }  // the server is saving; nothing to cancel
    if (WaitingOperation!=0)
    {
        if (WaitingNode != Node) { return true; }
        if (bWaitingHadSurface) { ServerGatheringActionAtSurface(WaitingNode.Get(),WaitingRevision,WaitingOperation,WaitingSurface); }
        else { ServerGatheringAction(WaitingNode.Get(),WaitingRevision,WaitingOperation); }
        return true;
    }
    if (!IsValid(Node) || Node->GetWorld() != GetWorld()) { return false; }
    if (NextLocalOperation==MAX_uint32) { bKeyConflict=true; return true; }
    WaitingOperation=NextLocalOperation++; WaitingNode=Node; WaitingRevision=Node->GetState().Revision; bKeyConflict=false;
    FVector Normal;
    const FVector ContactDirection = Contact && Contact->GetActor() == Node ? (Contact->ImpactPoint - Start).GetSafeNormal() : Direction;
    bWaitingHadSurface=Node->HarvestVisuals&&Node->HarvestVisuals->IsReady()&&Node->HarvestVisuals->TraceSurface(Start,ContactDirection,WaitingSurface,Normal);
    if (bWaitingHadSurface) { ServerGatheringActionAtSurface(Node,WaitingRevision,WaitingOperation,WaitingSurface); }
    else { ServerGatheringAction(Node,WaitingRevision,WaitingOperation); }
    return true;
}

bool USFGatheringPilotComponent::CancelLocalHarvest()
{
    if (!bEnabled) { return false; }
    if (WaitingOperation!=0) { ServerCancelHarvest(WaitingOperation); return true; }
    if (Feedback.Phase==ESFGatheringPilotPhase::Harvesting) { ServerCancelHarvest(Feedback.Operation); return true; }
    return false;
}

void USFGatheringPilotComponent::OnRep_Feedback()
{ if (WaitingOperation==Feedback.Operation) { WaitingOperation=0; WaitingNode.Reset(); } }

void USFGatheringPilotComponent::ClientKeyConflict_Implementation(uint32 Operation)
{ if (Operation==WaitingOperation) { WaitingOperation=0; WaitingNode.Reset(); } bKeyConflict=true; }

FString USFGatheringPilotComponent::GetStatusText(const FVector& Start, const FVector& Direction) const
{
    if (!bEnabled) { return FString(); }
    FString InventoryText=TEXT("Inventory adapter unavailable on this platform.");
    if (auto* Inventory=GetOwner()->FindComponentByClass<USFInventoryComponent>())
    {
        const int64 Wood=SFInventoryTransactions::GetItemQuantity(Inventory,TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Oak))));
        const int64 Ore=SFInventoryTransactions::GetItemQuantity(Inventory,TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Iron))));
        const int64 Timber=SFInventoryTransactions::GetItemQuantity(Inventory,TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Sawmill))));
        const int64 Ingots=SFInventoryTransactions::GetItemQuantity(Inventory,TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Forge))));
        InventoryText=FString::Printf(TEXT("Your bag: Logs %lld | Ore %lld | Timber %lld | Ingots %lld | %.1f / %.1f kg"),Wood,Ore,Timber,Ingots,Inventory->GetTotalWeight(),Inventory->MaxWeight);
    }
    FString Action=ResultText(bKeyConflict ? ESFGatheringPilotResult::KeyConflict : Feedback.Result);
    if (Feedback.bSkillGained&&!bKeyConflict) { Action+=FString::Printf(TEXT(" %s +1 (%d)."),FSFGatheringProgressionSession::SkillName(Feedback.Skill),Feedback.CurrentSkill); }
    if (WaitingOperation!=0) { Action=TEXT("Pending: waiting for server. E retries the same request."); }
    else if (Feedback.Phase==ESFGatheringPilotPhase::Committing) { Action=TEXT("Saving to your inventory..."); }
    else if (Feedback.Phase==ESFGatheringPilotPhase::Harvesting)
    {
        const auto* GS=GetWorld()->GetGameState(); const double Now=GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
        Action=FString::Printf(TEXT("Working on %s: %.1f s remaining | Stay still | E / Escape cancel"),
            IsValid(Feedback.Node) ? *Feedback.Node->GetResourceName() : TEXT("resource"),FMath::Max(0.0,Feedback.EndsAtServerSeconds-Now));
    }
    else if (const auto* Node=FindNode(Start,Direction))
    {
        const auto Result=ValidateTarget(Node);
        Action+=FString::Printf(TEXT(" | %s: %s"),*Node->GetResourceName(),
            Result!=ESFGatheringPilotResult::None ? ResultText(Result) : !Node->IsRefinery() && Node->GetState().Charges==0 ? TEXT("Depleted")
            : Node->GetState().bBusy ? TEXT("In use") : Node->IsRefinery() ? TEXT("E: refine") : TEXT("E: harvest"));
        if (!Node->IsRefinery())
        {
            ESFGatheringSkill Skill; ESFGatheringMaterialTier Tier;
            if (Node->GetResourceProfile(Skill,Tier))
            {
                Action+=FString::Printf(TEXT(" | %s %d / required %d | tier-one %s%s"),FSFGatheringProgressionSession::SkillName(Skill),Skills.Get(Skill),
                    FSFGatheringProgressionSession::RequiredSkill(Tier),Node->Kind==ESFGatheringPilotKind::Oak ? TEXT("axe") : TEXT("pickaxe"),
                    MeetsSkillRequirement(Node) ? TEXT("") : TEXT(" | Skill too low"));
            }
            Action+=FString::Printf(TEXT(" (%d charges)"),Node->GetState().Charges);
        }
    }
    return FString::Printf(TEXT("GATHERING PILOT | Session-only | Logging %d | Mining %d\n"),Skills.Get(ESFGatheringSkill::Logging),Skills.Get(ESFGatheringSkill::Mining))
        +InventoryText+TEXT("\nTool: ")+GetToolName()+TEXT(" | 5: axe | 6: pickaxe | M: skills\n")+Action;
}

void USFGatheringPilotComponent::AssignPilotRole(bool bPrimary)
{
    if (GetOwner()->HasAuthority()) { bPrimaryRole=bPrimary; bRoleAssigned=true; }
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void USFGatheringPilotComponent::TickFiniteNetworkScenario()
{
    const auto* PC=Cast<APlayerController>(GetOwner());
    const bool bRefiningScenario=FParse::Param(FCommandLine::Get(),TEXT("GatheringRefiningScenario"));
    if (GetNetMode()!=NM_Client || !PC || !PC->IsLocalController() || !bEnabled || !bRoleAssigned || !PC->GetPawn()
        || (!FParse::Param(FCommandLine::Get(),TEXT("GatheringPilotScenario")) && !bRefiningScenario)) { return; }
    ASFGatheringPilotNode* Wood=nullptr; ASFGatheringPilotNode* Ore=nullptr;
    ASFGatheringPilotNode* Sawmill=nullptr; ASFGatheringPilotNode* Forge=nullptr;
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It; ++It)
    {
        switch (It->Kind)
        {
        // A zone world has many nodes: every client and the server use the one nearest the world origin.
        case ESFGatheringPilotKind::Oak: if (!Wood || It->GetActorLocation().Size2D()<Wood->GetActorLocation().Size2D()) { Wood=*It; } break;
        case ESFGatheringPilotKind::Iron: case ESFGatheringPilotKind::Stone: break;  // chosen below: the rock nearest the chosen oak
        case ESFGatheringPilotKind::Sawmill: Sawmill=*It; break;
        case ESFGatheringPilotKind::Forge: Forge=*It; break;
        }
    }
    for (TActorIterator<ASFGatheringPilotNode> It(GetWorld()); It && Wood; ++It)
    {
        if ((It->Kind==ESFGatheringPilotKind::Iron || It->Kind==ESFGatheringPilotKind::Stone) && (!Ore || FVector::Dist2D(It->GetActorLocation(),Wood->GetActorLocation())<FVector::Dist2D(Ore->GetActorLocation(),Wood->GetActorLocation())))
        { Ore=*It; }
    }
    if (bRefiningScenario && (!Sawmill || !Forge)) { return; }
    const ESFGatheringPilotKind Mined=Ore ? Ore->Kind : ESFGatheringPilotKind::Iron;  // iron ore in the sandbox, rough stone in a zone
    if (!Wood || !Ore || Wood->GetState().Charges<0 || Ore->GetState().Charges<0) { return; }
    const double Now=GetWorld()->GetTimeSeconds();
    if (ScenarioStarted==0) { ScenarioStarted=Now; }
    if (Now-ScenarioStarted>40)
    { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED Timeout=1 Step=%d Phase=%d Result=%d Primary=%d Pawn=%s Wood=%s Ore=%s"),ScenarioStep,int32(Feedback.Phase),int32(Feedback.Result),
        bPrimaryRole,*PC->GetPawn()->GetActorLocation().ToCompactString(),*Wood->GetActorLocation().ToCompactString(),*Ore->GetActorLocation().ToCompactString()); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    auto* Inventory=GetOwner()->FindComponentByClass<USFInventoryComponent>();
    if (!Inventory) { return; }
    const auto Quantity=[&](ESFGatheringPilotKind Kind)
    { return SFInventoryTransactions::GetItemQuantity(Inventory,TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(ItemPath(Kind)))); };
    const auto Interact=[&](ASFGatheringPilotNode* Node)
    {
        const FVector Start=PC->GetPawn()->GetPawnViewLocation();
        return InteractFromRay(Start,(Node->GetActorLocation()-Start).GetSafeNormal());
    };
    if (ScenarioStep==0)
    {
        const auto Required=bPrimaryRole ? ESFGatheringPilotKind::Oak : Mined;
        if (!HasRequiredTool(Required))
        {
            if (!bScenarioToolRequested) { bScenarioToolRequested=TryEquipTool(bPrimaryRole); }
            return;
        }
        if (bScenarioToolRequested)  // the client's equip request came back through replication (tools/Test-GatheringPilot.ps1 checks this)
        {
            UE_LOG(LogTemp,Display,TEXT("EQUIP_REPLICATED Slot=%s Item=%s"),*MainHandSlot().ToString(),*GetEquipped(MainHandSlot()).GetAssetName());
            bScenarioToolRequested=false;
        }
        const auto* Character=Cast<ACharacter>(PC->GetPawn());
        const FVector Position=PC->GetPawn()->GetActorLocation();
        if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetVelocity().IsNearlyZero(1.0)
            || FVector::DistSquared(Position,ScenarioLastPosition)>1.0)
        { ScenarioStableSince=0; ScenarioLastPosition=Position; return; }
        if (ScenarioStableSince==0) { ScenarioStableSince=Now; }
        if (Now-ScenarioStableSince<1.0) { return; }
        // Wait for initial replicated charges; default-constructed snapshots contain zero.
        if (Wood->GetState().Charges==0 || Ore->GetState().Charges==0) { return; }
        if (bPrimaryRole) { if (Interact(Wood)) { ScenarioStep=1; } }
        else if (Wood->GetState().Revision==1)
        {
            if (Quantity(ESFGatheringPilotKind::Oak)!=0 || Quantity(Mined)!=0 || Skills.PracticeCount!=0 || Skills.GainCount!=0)
            { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED ForeignGrant=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_OBSERVER_PRIVATE_EMPTY Wood=0 Ore=0 Practice=0 Gains=0"));
            if (Interact(Ore)) { ScenarioStep=1; }
        }
    }
    else if (ScenarioStep==1 && Feedback.Phase==ESFGatheringPilotPhase::Committed
        && Skills.PracticeCount==1
        && Quantity(bPrimaryRole ? ESFGatheringPilotKind::Oak : Mined)==CapturedSettings->UnitsPerCharge)
    {
        if (bPrimaryRole)
        {
            if (bWaitingHadSurface) { ServerGatheringActionAtSurface(Wood,0,Feedback.Operation,WaitingSurface); }
            else { ServerGatheringAction(Wood,0,Feedback.Operation); }
            ScenarioReplayedAt=Now; ScenarioStep=2;
            UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_PRIMARY_REPLAY Operation=%u"),Feedback.Operation);
        }
        else { ScenarioStep=2; }
    }
    else if (ScenarioStep==2 && Wood->GetState().Revision==1 && Ore->GetState().Revision==1 && (!bPrimaryRole || Now-ScenarioReplayedAt>=1))
    {
        int32 VisibleInventories=0;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            TArray<USFInventoryComponent*> Components; It->GetComponents(Components); VisibleInventories+=Components.Num();
            TArray<USFGatheringPilotComponent*> Endpoints; It->GetComponents(Endpoints);
            for (const auto* Endpoint:Endpoints)
            {
                if (Endpoint!=this&&(Endpoint->Skills.PracticeCount!=0||Endpoint->Skills.GainCount!=0
                    ||Endpoint->Skills.Get(ESFGatheringSkill::Mining)!=0||Endpoint->Skills.Get(ESFGatheringSkill::Logging)!=0))
                { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED ForeignProgression=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            }
        }
        const int64 WoodCount=Quantity(ESFGatheringPilotKind::Oak), OreCount=Quantity(Mined);
        if (VisibleInventories!=1 || WoodCount!=(bPrimaryRole ? CapturedSettings->UnitsPerCharge : 0)
            || OreCount!=(bPrimaryRole ? 0 : CapturedSettings->UnitsPerCharge)||Skills.PracticeCount!=1||Skills.GainCount>1
            ||Skills.Get(bPrimaryRole ? ESFGatheringSkill::Mining : ESFGatheringSkill::Logging)!=0
            ||Skills.Get(bPrimaryRole ? ESFGatheringSkill::Logging : ESFGatheringSkill::Mining)!=Skills.GainCount)
        { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED Inventories=%d Wood=%lld Ore=%lld"),VisibleInventories,WoodCount,OreCount); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_CLIENT_DONE Primary=%d Inventories=1 Wood=%lld Ore=%lld WoodRevision=1 OreRevision=1"),bPrimaryRole,WoodCount,OreCount);
        UE_LOG(LogTemp,Display,TEXT("GATHERING_PROGRESSION_CLIENT_DONE Primary=%d Practice=1 Gains=%lld Logging=%d Mining=%d"),bPrimaryRole,Skills.GainCount,Skills.Get(ESFGatheringSkill::Logging),Skills.Get(ESFGatheringSkill::Mining));
        if (bRefiningScenario)
        { if (Interact(bPrimaryRole ? Sawmill : Forge)) { ScenarioStep=3; } }
        else { ScenarioStep=5; FPlatformMisc::RequestExitWithStatus(false,0); }
    }
    else if (ScenarioStep==3 && Feedback.Phase==ESFGatheringPilotPhase::Committed && Feedback.Result==ESFGatheringPilotResult::Refined
        && Quantity(bPrimaryRole ? ESFGatheringPilotKind::Sawmill : ESFGatheringPilotKind::Forge)==(bPrimaryRole ? CapturedSettings->TimberOutputUnits : CapturedSettings->IngotOutputUnits))
    {
        ServerGatheringAction(bPrimaryRole ? Sawmill : Forge,0,Feedback.Operation);
        ScenarioReplayedAt=Now; ScenarioStep=4;
    }
    else if (ScenarioStep==4 && Sawmill->GetState().Revision==1 && Forge->GetState().Revision==1 && Now-ScenarioReplayedAt>=1)
    {
        const int64 Timber=Quantity(ESFGatheringPilotKind::Sawmill), Ingots=Quantity(ESFGatheringPilotKind::Forge);
        if (Skills.PracticeCount!=1||Quantity(ESFGatheringPilotKind::Oak)!=0 || Quantity(Mined)!=0
            || Timber!=(bPrimaryRole ? CapturedSettings->TimberOutputUnits : 0) || Ingots!=(bPrimaryRole ? 0 : CapturedSettings->IngotOutputUnits))
        { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED RecipeOutputs=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("REFINING_PILOT_CLIENT_DONE Primary=%d Wood=0 Ore=0 Timber=%lld Ingots=%lld SawmillRevision=1 ForgeRevision=1"),bPrimaryRole,Timber,Ingots);
        ScenarioStep=5; FPlatformMisc::RequestExitWithStatus(false,0);
    }
}
#endif

FString ASFGatheringPilotNode::ActionVerb() const
{
    ESFGatheringSkill Skill; ESFGatheringMaterialTier Tier;
    if (!GetResourceProfile(Skill, Tier)) { return TEXT("Gather"); }
    switch (Skill)
    {
    case ESFGatheringSkill::Logging: return TEXT("Chop");
    case ESFGatheringSkill::Mining: return TEXT("Mine");
    case ESFGatheringSkill::Fishing: return TEXT("Fish");
    default: return TEXT("Gather");
    }
}
