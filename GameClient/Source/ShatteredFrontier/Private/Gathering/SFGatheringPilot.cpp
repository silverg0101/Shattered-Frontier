#include "Gathering/SFGatheringPilot.h"
#include "Construction/SFConstructionNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
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
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"
#endif

namespace
{
    const TCHAR* ItemPath(ESFGatheringPilotKind Kind)
    {
        switch (Kind)
        {
        case ESFGatheringPilotKind::Oak: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_OakLog.DA_OakLog");
        case ESFGatheringPilotKind::Iron: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronOre.DA_IronOre");
        case ESFGatheringPilotKind::Sawmill: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_RoughTimber.DA_RoughTimber");
        case ESFGatheringPilotKind::Forge: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_IronIngot.DA_IronIngot");
        default: return TEXT("");
        }
    }
    const TCHAR* ResultText(ESFGatheringPilotResult Result)
    {
        switch (Result)
        {
        case ESFGatheringPilotResult::Started: return TEXT("Working. Stay still; E or Escape cancels.");
        case ESFGatheringPilotResult::Refined: return TEXT("Refinement committed to your inventory.");
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
        && FMath::IsFinite(ReachMetres) && ReachMetres>=0.5f && ReachMetres<=5
        && FMath::IsFinite(MovementCancelMetres) && MovementCancelMetres>=0.01f && MovementCancelMetres<=0.25f
        && ChargesPerNode>=1 && ChargesPerNode<=12 && UnitsPerCharge>=1 && UnitsPerCharge<=10
        && ChargesPerNode*UnitsPerCharge<=30 && InventorySlots>=1 && InventorySlots<=64
        && FMath::IsFinite(InventoryWeightKg) && InventoryWeightKg>=1 && InventoryWeightKg<=1000
        && MaterialStackUnits>=1 && MaterialStackUnits<=100 && FMath::IsFinite(MaterialUnitWeightKg)
        && MaterialUnitWeightKg>=0.01f && MaterialUnitWeightKg<=50
        && FMath::IsFinite(RefineSeconds) && RefineSeconds>=0.5f && RefineSeconds<=30
        && TimberInputLogs>=1 && TimberInputLogs<=20 && TimberOutputUnits>=1 && TimberOutputUnits<=20
        && IngotInputOre>=1 && IngotInputOre<=20 && IngotOutputUnits>=1 && IngotOutputUnits<=20;
    Error=bValid ? TEXT("") : TEXT("Gathering pilot configuration outside declared bounds (including <=30 units per node).");
    return bValid;
}

ASFGatheringPilotNode::ASFGatheringPilotNode()
{
    bReplicates=true;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ResourceMesh")); SetRootComponent(Mesh);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    StationMesh=Cube.Object;
    Mesh->SetStaticMesh(Shape.Object); Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetRelativeScale3D(FVector(0.7,0.7,1.4));
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("ResourceLabel")); Label->SetupAttachment(Mesh);
    Label->SetRelativeLocation(FVector(0,-60,90)); Label->SetRelativeRotation(FRotator(0,-90,0));
    Label->SetWorldSize(16); Label->SetHorizontalAlignment(EHTA_Center);
}

void ASFGatheringPilotNode::BeginPlay()
{
    Super::BeginPlay();
    FString Error; const auto* Settings=GetDefault<USFGatheringPilotSettings>();
    if (HasAuthority() && Settings->Validate(Error)) { State.Charges=IsRefinery() ? 0 : Settings->ChargesPerNode; }
    OnRep_State();
}

void ASFGatheringPilotNode::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFGatheringPilotNode,State); DOREPLIFETIME(ASFGatheringPilotNode,Kind);
}

FString ASFGatheringPilotNode::GetResourceName() const
{
    switch (Kind)
    {
    case ESFGatheringPilotKind::Oak: return TEXT("Oak logs");
    case ESFGatheringPilotKind::Iron: return TEXT("Iron ore");
    case ESFGatheringPilotKind::Sawmill: return TEXT("Rough timber (sawmill)");
    case ESFGatheringPilotKind::Forge: return TEXT("Iron ingot (forge)");
    default: return TEXT("Unavailable");
    }
}

void ASFGatheringPilotNode::OnRep_State()
{
    if (IsRefinery())
    {
        Mesh->SetStaticMesh(StationMesh); Mesh->SetRelativeScale3D(FVector(1.4,0.8,0.8));
        const auto* Settings=GetDefault<USFGatheringPilotSettings>();
        Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s | %d %s -> %d units"),*GetResourceName(),
            State.bBusy ? TEXT("In use") : TEXT("E: refine"),Kind==ESFGatheringPilotKind::Sawmill ? Settings->TimberInputLogs : Settings->IngotInputOre,
            Kind==ESFGatheringPilotKind::Sawmill ? TEXT("log") : TEXT("ore"),Kind==ESFGatheringPilotKind::Sawmill ? Settings->TimberOutputUnits : Settings->IngotOutputUnits)));
        Label->SetTextRenderColor(FColor::Yellow); return;
    }
    Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s | %d charges"),*GetResourceName(),
        State.Charges==0 ? TEXT("Depleted") : State.bBusy ? TEXT("In use") : TEXT("E: harvest"),State.Charges)));
    Label->SetTextRenderColor(State.Charges==0 ? FColor::Silver : Kind==ESFGatheringPilotKind::Oak ? FColor::Green : FColor::Cyan);
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
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return FParse::Param(FCommandLine::Get(),TEXT("GatheringPilot"));
#else
    return false;
#endif
}

void USFGatheringPilotComponent::BeginPlay()
{
    Super::BeginPlay();
    CapturedSettings=DuplicateObject<USFGatheringPilotSettings>(GetDefault<USFGatheringPilotSettings>(),this);
    FString Error;
    if (!CapturedSettings->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_CONFIGURATION_FAILED %s"),*Error); return; }
    if (GetOwner()->HasAuthority())
    {
        const auto* Mode=GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>();
        bEnabled=IsPilotRequested() && Mode && Mode->IsAdmitted();
    }
#if SF_WITH_MINIMAL_INVENTORY
    if (auto* Inventory=GetOwner()->FindComponentByClass<UInventoryComponent>())
    { Inventory->SetBuiltInInteractionEnabled(false); Inventory->SetComponentTickEnabled(false); }
#endif
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
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bEnabled,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bPrimaryRole,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bRoleAssigned,COND_OwnerOnly);
}

bool USFGatheringPilotComponent::IsAuthoritativePilotWorld() const
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) { return false; }
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (bTestWorldAdmission && GetWorld()->WorldType==EWorldType::GameRPC && GetNetMode()==NM_Standalone) { return true; }
#endif
    const auto* Mode=GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>();
    return IsPilotRequested() && Mode && Mode->IsAdmitted();
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
#if SF_WITH_MINIMAL_INVENTORY
    return GetOwner()->FindComponentByClass<UInventoryComponent>()
        && LoadObject<UInventoryItemDefinition>(nullptr,ItemPath(ESFGatheringPilotKind::Oak))
        && LoadObject<UInventoryItemDefinition>(nullptr,ItemPath(ESFGatheringPilotKind::Iron));
#else
    return false;
#endif
}

ESFGatheringPilotResult USFGatheringPilotComponent::ApplyInventory(ESFGatheringPilotKind Kind, bool bCommit)
{
#if SF_WITH_MINIMAL_INVENTORY
    auto* Inventory=GetOwner()->FindComponentByClass<UInventoryComponent>();
    FInventoryTransactionChange Change; Change.Inventory=Inventory;
    FInventoryItemQuantity Reward; Reward.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ItemPath(Kind)));
    Reward.Quantity=CapturedSettings->UnitsPerCharge;
    if (Kind==ESFGatheringPilotKind::Sawmill || Kind==ESFGatheringPilotKind::Forge)
    {
        FInventoryItemQuantity Cost;
        Cost.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ItemPath(Kind==ESFGatheringPilotKind::Sawmill ? ESFGatheringPilotKind::Oak : ESFGatheringPilotKind::Iron)));
        Cost.Quantity=Kind==ESFGatheringPilotKind::Sawmill ? CapturedSettings->TimberInputLogs : CapturedSettings->IngotInputOre;
        Reward.Quantity=Kind==ESFGatheringPilotKind::Sawmill ? CapturedSettings->TimberOutputUnits : CapturedSettings->IngotOutputUnits;
        Change.RemoveItems.Add(Cost);
    }
    Change.AddItems.Add(Reward);
    const auto Result=bCommit ? UInventoryTransactionLibrary::ExecuteInventoryTransaction({Change})
        : UInventoryTransactionLibrary::CanExecuteInventoryTransaction({Change});
    switch (Result)
    {
    case EInventoryTransactionResult::Success: return ESFGatheringPilotResult::None;
    case EInventoryTransactionResult::InsufficientSpace:
    case EInventoryTransactionResult::WeightLimitExceeded: return ESFGatheringPilotResult::InventoryFull;
    case EInventoryTransactionResult::InsufficientItems: return ESFGatheringPilotResult::MissingMaterials;
    default: return ESFGatheringPilotResult::Unavailable;
    }
#else
    return ESFGatheringPilotResult::Unavailable;
#endif
}

void USFGatheringPilotComponent::ServerGatheringAction_Implementation(ASFGatheringPilotNode* Node, uint32 Revision, uint32 Operation)
{
    if (!IsAuthoritativePilotWorld() || !bEnabled) { return; }
    if (Operation==Feedback.Operation)
    {
        if (LastNode.Get()!=Node || LastRevision!=Revision) { ClientKeyConflict(Operation); }
        else { GetOwner()->ForceNetUpdate(); } // Replay returns the current original outcome, never a second grant.
        return;
    }
    if (Feedback.Operation==MAX_uint32 || Operation!=Feedback.Operation+1 || Feedback.Phase==ESFGatheringPilotPhase::Harvesting)
    { ClientKeyConflict(Operation); return; }
    Feedback.Operation=Operation; Feedback.Node=Node; Feedback.EndsAtServerSeconds=0;
    LastNode=Node; LastRevision=Revision;
    ESFGatheringPilotResult Result=ValidateTarget(Node);
    if (Result==ESFGatheringPilotResult::None && Node->State.Revision!=Revision) { Result=ESFGatheringPilotResult::Stale; }
    if (Result==ESFGatheringPilotResult::None && !Node->IsRefinery() && Node->State.Charges<=0) { Result=ESFGatheringPilotResult::Depleted; }
    if (Result==ESFGatheringPilotResult::None && Node->State.bBusy) { Result=ESFGatheringPilotResult::Busy; }
    if (Result==ESFGatheringPilotResult::None && !InventoryReady()) { Result=ESFGatheringPilotResult::Unavailable; }
    if (Result==ESFGatheringPilotResult::None) { Result=ApplyInventory(Node->Kind,false); }
    if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Rejected,Result); return; }
    ActiveKind=Node->Kind;
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
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TickFiniteNetworkScenario();
#endif
    if (!GetOwner()->HasAuthority() || !bEnabled || Feedback.Phase!=ESFGatheringPilotPhase::Harvesting) { return; }
    if (!IsAuthoritativePilotWorld()) { Finish(ESFGatheringPilotPhase::Canceled,ESFGatheringPilotResult::InvalidContext); return; }
    auto Result=ValidateTarget(Feedback.Node);
    const auto* PC=Cast<APlayerController>(GetOwner()); const APawn* Pawn=PC ? PC->GetPawn() : nullptr;
    if (Result==ESFGatheringPilotResult::None && (HarvestPawn.Get()!=Pawn || FVector::DistSquared(HarvestStart,Pawn->GetActorLocation())
        >FMath::Square(CapturedSettings->MovementCancelMetres*100.0))) { Result=ESFGatheringPilotResult::Moved; }
    if (Result==ESFGatheringPilotResult::None && (Feedback.Node->Kind!=ActiveKind || Feedback.Node->LockedBy.Get()!=this || Feedback.Node->State.Revision!=LastRevision
        || (!Feedback.Node->IsRefinery() && Feedback.Node->State.Charges<=0) || Feedback.Node->State.Revision==MAX_uint32)) { Result=ESFGatheringPilotResult::Stale; }
    if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Canceled,Result); return; }
    if (GetWorld()->GetTimeSeconds()<Feedback.EndsAtServerSeconds) { return; }
    Result=ApplyInventory(ActiveKind,true);
    if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Rejected,Result); return; }
    // Synchronous game-thread pilot: no fallible step remains after the inventory commit.
    const bool bRefining=ActiveKind==ESFGatheringPilotKind::Sawmill || ActiveKind==ESFGatheringPilotKind::Forge;
    if (!bRefining) { --Feedback.Node->State.Charges; }
    ++Feedback.Node->State.Revision;
    Finish(ESFGatheringPilotPhase::Committed,bRefining ? ESFGatheringPilotResult::Refined : ESFGatheringPilotResult::Granted);
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
    if (!bEnabled) { return false; }
    if (Feedback.Phase==ESFGatheringPilotPhase::Harvesting) { CancelLocalHarvest(); return true; }
    if (WaitingOperation!=0)
    { ServerGatheringAction(WaitingNode.Get(),WaitingRevision,WaitingOperation); return true; }
    auto* Node=FindNode(Start,Direction); if (!Node) { return false; }
    if (NextLocalOperation==MAX_uint32) { bKeyConflict=true; return true; }
    WaitingOperation=NextLocalOperation++; WaitingNode=Node; WaitingRevision=Node->GetState().Revision; bKeyConflict=false;
    ServerGatheringAction(Node,WaitingRevision,WaitingOperation); return true;
}

void USFGatheringPilotComponent::CancelLocalHarvest()
{
    if (!bEnabled) { return; }
    if (WaitingOperation!=0) { ServerCancelHarvest(WaitingOperation); }
    else if (Feedback.Phase==ESFGatheringPilotPhase::Harvesting) { ServerCancelHarvest(Feedback.Operation); }
}

void USFGatheringPilotComponent::OnRep_Feedback()
{ if (WaitingOperation==Feedback.Operation) { WaitingOperation=0; WaitingNode.Reset(); } }

void USFGatheringPilotComponent::ClientKeyConflict_Implementation(uint32 Operation)
{ if (Operation==WaitingOperation) { WaitingOperation=0; WaitingNode.Reset(); } bKeyConflict=true; }

FString USFGatheringPilotComponent::GetStatusText(const FVector& Start, const FVector& Direction) const
{
    if (!bEnabled) { return FString(); }
    FString InventoryText=TEXT("Inventory adapter unavailable on this platform.");
#if SF_WITH_MINIMAL_INVENTORY
    if (auto* Inventory=GetOwner()->FindComponentByClass<UInventoryComponent>())
    {
        const int64 Wood=UInventoryTransactionLibrary::GetItemQuantity(Inventory,TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Oak))));
        const int64 Ore=UInventoryTransactionLibrary::GetItemQuantity(Inventory,TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Iron))));
        const int64 Timber=UInventoryTransactionLibrary::GetItemQuantity(Inventory,TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Sawmill))));
        const int64 Ingots=UInventoryTransactionLibrary::GetItemQuantity(Inventory,TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ItemPath(ESFGatheringPilotKind::Forge))));
        InventoryText=FString::Printf(TEXT("Your bag: Logs %lld | Ore %lld | Timber %lld | Ingots %lld | %.1f / %.1f kg"),Wood,Ore,Timber,Ingots,Inventory->GetTotalWeight(),Inventory->MaxWeight);
    }
#endif
    FString Action=ResultText(bKeyConflict ? ESFGatheringPilotResult::KeyConflict : Feedback.Result);
    if (WaitingOperation!=0) { Action=TEXT("Pending: waiting for server. E retries the same request."); }
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
        if (!Node->IsRefinery()) { Action+=FString::Printf(TEXT(" (%d charges)"),Node->GetState().Charges); }
    }
    return TEXT("GATHERING PILOT | Session-only inventory; practice tools\n")+InventoryText+TEXT("\n")+Action;
}

void USFGatheringPilotComponent::AssignPilotRole(bool bPrimary)
{
    if (GetOwner()->HasAuthority()) { bPrimaryRole=bPrimary; bRoleAssigned=true; }
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void USFGatheringPilotComponent::TickFiniteNetworkScenario()
{
#if SF_WITH_MINIMAL_INVENTORY
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
        case ESFGatheringPilotKind::Oak: Wood=*It; break;
        case ESFGatheringPilotKind::Iron: Ore=*It; break;
        case ESFGatheringPilotKind::Sawmill: Sawmill=*It; break;
        case ESFGatheringPilotKind::Forge: Forge=*It; break;
        }
    }
    if (bRefiningScenario && (!Sawmill || !Forge)) { return; }
    if (!Wood || !Ore || Wood->GetState().Charges<0 || Ore->GetState().Charges<0) { return; }
    const double Now=GetWorld()->GetTimeSeconds();
    if (ScenarioStarted==0) { ScenarioStarted=Now; }
    if (Now-ScenarioStarted>40)
    { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED Timeout=1 Step=%d Phase=%d Result=%d"),ScenarioStep,int32(Feedback.Phase),int32(Feedback.Result)); FPlatformMisc::RequestExitWithStatus(false,1); return; }
    auto* Inventory=GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (!Inventory) { return; }
    const auto Quantity=[&](ESFGatheringPilotKind Kind)
    { return UInventoryTransactionLibrary::GetItemQuantity(Inventory,TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ItemPath(Kind)))); };
    const auto Interact=[&](ASFGatheringPilotNode* Node)
    {
        const FVector Start=PC->GetPawn()->GetPawnViewLocation();
        return InteractFromRay(Start,(Node->GetActorLocation()-Start).GetSafeNormal());
    };
    if (ScenarioStep==0)
    {
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
            if (Quantity(ESFGatheringPilotKind::Oak)!=0 || Quantity(ESFGatheringPilotKind::Iron)!=0)
            { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED ForeignGrant=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
            UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_OBSERVER_PRIVATE_EMPTY Wood=0 Ore=0"));
            if (Interact(Ore)) { ScenarioStep=1; }
        }
    }
    else if (ScenarioStep==1 && Feedback.Phase==ESFGatheringPilotPhase::Committed
        && Quantity(bPrimaryRole ? ESFGatheringPilotKind::Oak : ESFGatheringPilotKind::Iron)==CapturedSettings->UnitsPerCharge)
    {
        if (bPrimaryRole)
        {
            ServerGatheringAction(Wood,0,Feedback.Operation); ScenarioReplayedAt=Now; ScenarioStep=2;
            UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_PRIMARY_REPLAY Operation=%u"),Feedback.Operation);
        }
        else { ScenarioStep=2; }
    }
    else if (ScenarioStep==2 && Wood->GetState().Revision==1 && Ore->GetState().Revision==1 && (!bPrimaryRole || Now-ScenarioReplayedAt>=1))
    {
        int32 VisibleInventories=0;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        { TArray<UInventoryComponent*> Components; It->GetComponents(Components); VisibleInventories+=Components.Num(); }
        const int64 WoodCount=Quantity(ESFGatheringPilotKind::Oak), OreCount=Quantity(ESFGatheringPilotKind::Iron);
        if (VisibleInventories!=1 || WoodCount!=(bPrimaryRole ? CapturedSettings->UnitsPerCharge : 0)
            || OreCount!=(bPrimaryRole ? 0 : CapturedSettings->UnitsPerCharge))
        { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED Inventories=%d Wood=%lld Ore=%lld"),VisibleInventories,WoodCount,OreCount); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("GATHERING_PILOT_CLIENT_DONE Primary=%d Inventories=1 Wood=%lld Ore=%lld WoodRevision=1 OreRevision=1"),bPrimaryRole,WoodCount,OreCount);
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
        if (Quantity(ESFGatheringPilotKind::Oak)!=0 || Quantity(ESFGatheringPilotKind::Iron)!=0
            || Timber!=(bPrimaryRole ? CapturedSettings->TimberOutputUnits : 0) || Ingots!=(bPrimaryRole ? 0 : CapturedSettings->IngotOutputUnits))
        { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_SCENARIO_FAILED RecipeOutputs=1")); FPlatformMisc::RequestExitWithStatus(false,1); return; }
        UE_LOG(LogTemp,Display,TEXT("REFINING_PILOT_CLIENT_DONE Primary=%d Wood=0 Ore=0 Timber=%lld Ingots=%lld SawmillRevision=1 ForgeRevision=1"),bPrimaryRole,Timber,Ingots);
        ScenarioStep=5; FPlatformMisc::RequestExitWithStatus(false,0);
    }
#endif
}
#endif
