#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFHarvestFallingTree.h"
#include "Construction/SFConstructionNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/UObjectIterator.h"
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
        case ESFGatheringPilotKind::Stone: return TEXT("/Game/Frontier/Gameplay/Pilot/DA_RoughStone.DA_RoughStone");
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

ASFGatheringPilotNode::ASFGatheringPilotNode()
{
    bReplicates=true;
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
    Label->SetWorldSize(16); Label->SetHorizontalAlignment(EHTA_Center);
    HarvestVisuals=CreateDefaultSubobject<USFHarvestPresentation>(TEXT("HarvestPresentation"));
}

void ASFGatheringPilotNode::BeginPlay()
{
    Super::BeginPlay();
    FString Error; const auto* Settings=GetDefault<USFGatheringPilotSettings>();
    if (HasAuthority() && Settings->Validate(Error))
    {
        State.Capacity=IsRefinery() ? 0 : Settings->ChargesPerNode;
        State.Charges=State.Capacity; CapturedRespawnSeconds=Settings->RespawnSeconds;
    }
    if (!IsRefinery())
    {
        // A zone node stands on a dressing candidate: remove the dressing tree or rock rooted there, so the node's own art
        // and collision are the only ones in place (the server's line-of-sight check and every client alike).
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
    OnRep_State();
}

void ASFGatheringPilotNode::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(RecoveryTimer);
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
    if (!HasAuthority() || IsRefinery() || State.Charges!=0 || State.RecoveryAtServerSeconds>0) { return; }
    RecoveryKind=Kind; State.RecoveryAtServerSeconds=GetWorld()->GetTimeSeconds()+CapturedRespawnSeconds;
    GetWorldTimerManager().SetTimer(RecoveryTimer,this,&ASFGatheringPilotNode::Recover,CapturedRespawnSeconds,false);
}

void ASFGatheringPilotNode::Recover()
{
    if (!HasAuthority() || IsRefinery() || Kind!=RecoveryKind || State.Charges!=0 || State.Capacity<=0
        || State.bBusy || LockedBy.IsValid() || State.RecoveryAtServerSeconds<=0 || State.Revision==MAX_uint32 || State.Generation==MAX_uint32) { return; }
    const double Remaining=State.RecoveryAtServerSeconds-GetWorld()->GetTimeSeconds();
    if (Remaining>0)
    {
        GetWorldTimerManager().SetTimer(RecoveryTimer,this,&ASFGatheringPilotNode::Recover,
            FMath::Max(static_cast<float>(Remaining),UE_SMALL_NUMBER),false);
        return;
    }
    if (IsValid(State.FallingTree)) { State.FallingTree->Destroy(); } State.FallingTree=nullptr;
    State.Charges=State.Capacity; State.RecoveryAtServerSeconds=0; State.Cuts.Reset(); ++State.Revision; ++State.Generation;
    OnRep_State(); ForceNetUpdate();
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
    case ESFGatheringPilotKind::Sawmill: return TEXT("Rough timber (sawmill)");
    case ESFGatheringPilotKind::Forge: return TEXT("Iron ingot (forge)");
    default: return TEXT("Unavailable");
    }
}

void ASFGatheringPilotNode::OnRep_State()
{
    if (HarvestVisuals&&HarvestVisuals->IsReady())
    {
        HarvestVisuals->ApplyState(State.Cuts,State.Charges==0,State.Generation,State.FallingTree);
        Mesh->SetVisibility(false); Label->SetVisibility(false);
    }
    if (IsRefinery())
    {
        Mesh->SetStaticMesh(StationMesh); Mesh->SetRelativeScale3D(FVector(1.4,0.8,0.8));
        const auto* Settings=GetDefault<USFGatheringPilotSettings>();
        Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s | %d %s -> %d units"),*GetResourceName(),
            State.bBusy ? TEXT("In use") : TEXT("E: refine"),Kind==ESFGatheringPilotKind::Sawmill ? Settings->TimberInputLogs : Settings->IngotInputOre,
            Kind==ESFGatheringPilotKind::Sawmill ? TEXT("log") : TEXT("ore"),Kind==ESFGatheringPilotKind::Sawmill ? Settings->TimberOutputUnits : Settings->IngotOutputUnits)));
        Label->SetTextRenderColor(FColor::Yellow);
    }
    else
    {
        Label->SetText(FText::FromString(FString::Printf(TEXT("%s\n%s | %d charges"),*GetResourceName(),
            State.Charges==0 ? TEXT("Depleted") : State.bBusy ? TEXT("In use") : TEXT("E: harvest"),State.Charges)));
        Label->SetTextRenderColor(State.Charges==0 ? FColor::Silver : Kind==ESFGatheringPilotKind::Oak ? FColor::Green : FColor::Cyan);
    }
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
    CapturedProgressionSettings=DuplicateObject<USFGatheringProgressionSettings>(GetDefault<USFGatheringProgressionSettings>(),this);
    FString Error;
    if (!CapturedProgressionSettings->Validate()) { UE_LOG(LogTemp,Error,TEXT("GATHERING_PROGRESSION_CONFIGURATION_FAILED Pilot disabled; release configuration outside declared bounds.")); return; }
    if (!CapturedSettings->Validate(Error)) { UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_CONFIGURATION_FAILED %s"),*Error); return; }
    if (GetOwner()->HasAuthority())
    {
        const auto* Mode=GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>();
        bEnabled=IsPilotRequested() && Mode && Mode->IsAdmitted();
        if (bEnabled && !InitializeStarterTools())
        {
            bEnabled=false;
            UE_LOG(LogTemp,Error,TEXT("GATHERING_PILOT_STARTER_TOOLS_FAILED No partial loadout granted; check released tool assets and bag capacity."));
        }
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
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,Skills,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,SkillLockReadyAt,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bEnabled,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bPrimaryRole,COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFGatheringPilotComponent,bRoleAssigned,COND_OwnerOnly);
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
    if (bEnabled&&PC&&PC->IsLocalController()&&uint8(Skill)<6&&uint8(Lock)<=uint8(ESFGatheringSkillLock::Locked)
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

void USFGatheringPilotComponent::ServerChangeSkillLock_Implementation(ESFGatheringSkill Skill,ESFGatheringSkillLock Lock)
{
    const int32 Index=int32(Skill);
    if (!bEnabled||!IsAuthoritativePilotWorld()||!CapturedProgressionSettings
        ||uint8(Skill)>=6||uint8(Lock)>uint8(ESFGatheringSkillLock::Locked)
        ||!Skills.Locks.IsValidIndex(Index)||!SkillLockReadyAt.IsValidIndex(Index)) { return; }
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
    const auto* Mode=GetWorld()->GetAuthGameMode<ASFConstructionNetworkGameMode>();
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
#if SF_WITH_MINIMAL_INVENTORY
    return GetOwner()->FindComponentByClass<UInventoryComponent>()
        && LoadObject<UInventoryItemDefinition>(nullptr,ItemPath(ESFGatheringPilotKind::Oak))
        && LoadObject<UInventoryItemDefinition>(nullptr,ItemPath(ESFGatheringPilotKind::Iron));
#else
    return false;
#endif
}

bool USFGatheringPilotComponent::InitializeStarterTools()
{
    if (!IsAuthoritativePilotWorld() || !CapturedSettings) { return false; }
    if (bStarterToolsGranted || !CapturedSettings->bGrantStarterTools) { return true; }
#if SF_WITH_MINIMAL_INVENTORY
    auto* Inventory=GetOwner()->FindComponentByClass<UInventoryComponent>();
    FInventoryItemQuantity Axe; Axe.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ToolPath(true))); Axe.Quantity=1;
    FInventoryItemQuantity Pick; Pick.Item=TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(ToolPath(false))); Pick.Quantity=1;
    if (UInventoryTransactionLibrary::ExchangeInventoryItems(Inventory,{}, {Axe,Pick})!=EInventoryTransactionResult::Success) { return false; }
    bStarterToolsGranted=true; GetOwner()->ForceNetUpdate(); return true;
#else
    return false;
#endif
}

bool USFGatheringPilotComponent::HasRequiredTool(ESFGatheringPilotKind Kind) const
{
    if (Kind==ESFGatheringPilotKind::Sawmill || Kind==ESFGatheringPilotKind::Forge) { return true; }
#if SF_WITH_MINIMAL_INVENTORY
    const auto* Inventory=GetOwner()->FindComponentByClass<UInventoryComponent>();
    TSoftObjectPtr<UInventoryItemDefinition> Equipped; int32 Quantity=0;
    // These two released definitions are the pilot's tier-one axe/pickaxe catalogue.
    return Inventory && Inventory->GetEquippedItem(TEXT("tool"),Equipped,Quantity) && Quantity==1
        && Equipped.ToSoftObjectPath()==FSoftObjectPath(ToolPath(Kind==ESFGatheringPilotKind::Oak));
#else
    return false;
#endif
}

bool USFGatheringPilotComponent::TryEquipTool(bool bAxe)
{
    if (!bEnabled) { return false; }
    const auto* PC=Cast<APlayerController>(GetOwner());
    if (!PC || (!PC->HasAuthority() && !PC->IsLocalController()) || (PC->HasAuthority() && !IsAuthoritativePilotWorld())) { return false; }
    if (HasRequiredTool(bAxe ? ESFGatheringPilotKind::Oak : ESFGatheringPilotKind::Iron)) { return true; }
#if SF_WITH_MINIMAL_INVENTORY
    auto* Inventory=GetOwner()->FindComponentByClass<UInventoryComponent>();
    if (!Inventory) { return false; }
    const FSoftObjectPath Desired(ToolPath(bAxe));
    for (int32 I=0; I<FMath::Clamp(Inventory->NumSlots,0,64); ++I)
    {
        TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Quantity=0;
        if (Inventory->GetSlot(I,Item,Quantity) && Quantity>0 && Item.ToSoftObjectPath()==Desired)
        { CancelLocalHarvest(); return Inventory->EquipFromSlot(I); }
    }
#endif
    return false;
}

TArray<TPair<FText,int32>> USFGatheringPilotComponent::GetInventoryItems() const
{
    TArray<TPair<FText,int32>> Items;
#if SF_WITH_MINIMAL_INVENTORY
    const auto* Inventory=GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
    if (!Inventory) { return Items; }
    auto Add=[&Items](const TSoftObjectPtr<UInventoryItemDefinition>& Item,int32 Quantity,const TCHAR* Suffix)
    {
        if (Quantity<=0 || Item.IsNull()) { return; }
        const UInventoryItemDefinition* Definition=Item.LoadSynchronous();
        const FString Name=Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName.ToString() : Item.GetAssetName();
        Items.Emplace(FText::FromString(Name+Suffix),Quantity);
    };
    for (int32 I=0; I<FMath::Clamp(Inventory->NumSlots,0,64); ++I)
    {
        TSoftObjectPtr<UInventoryItemDefinition> Item; int32 Quantity=0;
        if (Inventory->GetSlot(I,Item,Quantity)) { Add(Item,Quantity,TEXT("")); }
    }
    TSoftObjectPtr<UInventoryItemDefinition> Equipped; int32 Quantity=0;
    if (Inventory->GetEquippedItem(TEXT("tool"),Equipped,Quantity)) { Add(Equipped,Quantity,TEXT(" (equipped)")); }
#endif
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
    if (Feedback.Operation==MAX_uint32 || Operation!=Feedback.Operation+1 || Feedback.Phase==ESFGatheringPilotPhase::Harvesting)
    { ClientKeyConflict(Operation); return; }
    Feedback.Operation=Operation; Feedback.Node=Node; Feedback.EndsAtServerSeconds=0; Feedback.bSkillGained=false;
    Feedback.RequiredSkill=0; Feedback.CurrentSkill=0;
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
    Result=ApplyInventory(ActiveKind,true);
    if (Result!=ESFGatheringPilotResult::None) { Finish(ESFGatheringPilotPhase::Rejected,Result); return; }
    // Synchronous game-thread pilot: no fallible reward mutation remains after the inventory commit.
    // Optional cosmetic felling cannot roll back or repeat the granted material.
    const bool bRefining=ActiveKind==ESFGatheringPilotKind::Sawmill || ActiveKind==ESFGatheringPilotKind::Forge;
    if (!bRefining)
    {
        if (CapturedProgressionSettings&&!ActiveResourceProfile.ResourceId.IsNone())
        {
            const int64 GainsBefore=Skills.GainCount;
            ProgressionSession.Practice(Skills,ActiveResourceProfile.Skill,FSFGatheringProgressionSession::Difficulty(ActiveResourceProfile.Tier),ActivePracticeDurationSeconds,
                ActivePracticeContext,ActivePracticeInputSeconds,GetWorld()->GetTimeSeconds(),*CapturedProgressionSettings);
            Feedback.bSkillGained=Skills.GainCount>GainsBefore; Feedback.CurrentSkill=Skills.Get(ActiveResourceProfile.Skill);
        }
        if (bActiveVisualCut) { Feedback.Node->State.Cuts.Add(ActiveVisualCut); }
        --Feedback.Node->State.Charges; Feedback.Node->BeginRecovery();
        if (Feedback.Node->State.Charges==0&&ActiveKind==ESFGatheringPilotKind::Oak&&bActiveVisualCut&&Feedback.Node->HarvestVisuals)
        { Feedback.Node->State.FallingTree=Feedback.Node->HarvestVisuals->BeginTreeFall(Feedback.Node->State.Generation,ActiveVisualCut); }
    }
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
    {
        if (bWaitingHadSurface) { ServerGatheringActionAtSurface(WaitingNode.Get(),WaitingRevision,WaitingOperation,WaitingSurface); }
        else { ServerGatheringAction(WaitingNode.Get(),WaitingRevision,WaitingOperation); }
        return true;
    }
    auto* Node=FindNode(Start,Direction); if (!Node) { return false; }
    if (NextLocalOperation==MAX_uint32) { bKeyConflict=true; return true; }
    WaitingOperation=NextLocalOperation++; WaitingNode=Node; WaitingRevision=Node->GetState().Revision; bKeyConflict=false;
    FVector Normal;
    bWaitingHadSurface=Node->HarvestVisuals&&Node->HarvestVisuals->IsReady()&&Node->HarvestVisuals->TraceSurface(Start,Direction,WaitingSurface,Normal);
    if (bWaitingHadSurface) { ServerGatheringActionAtSurface(Node,WaitingRevision,WaitingOperation,WaitingSurface); }
    else { ServerGatheringAction(Node,WaitingRevision,WaitingOperation); }
    return true;
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
    if (Feedback.bSkillGained&&!bKeyConflict) { Action+=FString::Printf(TEXT(" %s +1 (%d)."),FSFGatheringProgressionSession::SkillName(Feedback.Skill),Feedback.CurrentSkill); }
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
        const auto Required=bPrimaryRole ? ESFGatheringPilotKind::Oak : Mined;
        if (!HasRequiredTool(Required))
        {
            if (!bScenarioToolRequested) { bScenarioToolRequested=TryEquipTool(bPrimaryRole); }
            return;
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
            TArray<UInventoryComponent*> Components; It->GetComponents(Components); VisibleInventories+=Components.Num();
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
#endif
}
#endif
