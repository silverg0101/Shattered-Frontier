#include "Inventory/SFLootComponent.h"
#include "Inventory/SFLootSourceActor.h"
#include "Inventory/SFLootWorldSubsystem.h"
#include "Inventory/SFLootGeometry.h"
#include "Inventory/SFInventory.h"
#include "Inventory/SFInventoryLedgerClient.h"
#include "Gathering/SFStationRecipes.h"
#include "Framework/SFPlayerController.h"
#include "Framework/SFE1Settings.h"
#include "Characters/SFE1Character.h"
#include "Services/SFBackpackComponent.h"
#include "UI/SFInventoryWidget.h"
#include "UI/SFWindowStack.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Tests/SFLootWorkflowNetcheck.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

USFLootComponent::USFLootComponent() { SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick = true; }
USFLootComponent* USFLootComponent::Find(const APlayerController* Controller)
{ return Controller ? Controller->FindComponentByClass<USFLootComponent>() : nullptr; }

bool USFLootComponent::InReach(const APlayerController* Controller, const AActor* Source)
{
    const ASFE1Character* Pawn = Controller ? Cast<ASFE1Character>(Controller->GetPawn()) : nullptr;
    if (!Pawn || Pawn->CombatHealth() <= 0 || !IsValid(Source) || Source->IsActorBeingDestroyed() ||
        Source->GetWorld() != Controller->GetWorld()) { return false; }
    const float Reach = GetDefault<USFE1Settings>()->InteractionRangeMetres * 100.f;
    if (FVector::DistSquared(Pawn->GetActorLocation(), Source->GetActorLocation()) > FMath::Square(Reach)) { return false; }
    FVector Eye; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eye, Rotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(LootVisibility), false, Pawn); FHitResult Hit;
    if (const auto* Loot = Cast<ASFLootSourceActor>(Source))
    {
        for (TActorIterator<ASFLootSourceActor> It(Controller->GetWorld()); It; ++It)
        { if (*It != Source && Loot->SharesCorpseBody(*It)) { Query.AddIgnoredActor(*It); } }
    }
    return !Controller->GetWorld()->LineTraceSingleByChannel(Hit, Eye, Source->GetActorLocation(), ECC_Visibility, Query) || Hit.GetActor() == Source;
}

bool USFLootComponent::TryInteract(AActor* Target)
{
    ASFLootSourceActor* Source = Cast<ASFLootSourceActor>(Target);
    if (!Source) { return false; }
    ServerOpen(Source); return true;
}

void USFLootComponent::ServerOpen_Implementation(ASFLootSourceActor* Source)
{
    auto* Player = Cast<APlayerController>(GetOwner());
    auto Services = USFBackpackComponent::Services();
    USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Player);
#if WITH_EDITOR
    if (FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowNetcheck")))
        UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_OPEN Services=%d Backpack=%d Reach=%d"), Services.IsValid(), Backpack != nullptr, InReach(Player, Source));
#endif
    if (!Services || !Backpack || !InReach(Player, Source)) { return; }
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::OpenLootSource;
    Request.LootSourceId = Source->GetSnapshot().Id; Request.CharacterId = Backpack->GetCharacterId();
    Services->Call(Request, [Weak = TWeakObjectPtr<USFLootComponent>(this), Actor = TWeakObjectPtr<ASFLootSourceActor>(Source)](const FSFGameplayResponse& Reply)
    {
        USFLootComponent* Self = Weak.Get(); auto* Player = Self ? Cast<APlayerController>(Self->GetOwner()) : nullptr;
        if (!Self) { return; }
#if WITH_EDITOR
        if (FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowNetcheck")))
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_OPEN_REPLY Status=%d Sources=%d Reach=%d"), int32(Reply.Status), Reply.LootSources.Num(), InReach(Player, Actor.Get()));
#endif
        if (Reply.Status != ESFGameplayStatus::Ok || Reply.LootSources.Num() != 1 || !InReach(Player, Actor.Get()))
        { if (auto* Controller = Cast<ASFPlayerController>(Player)) { Controller->ClientPilotHit(TEXT("Loot is unavailable. Check your range and eligibility.")); } return; }
        const auto& View = Reply.LootSources[0];
        Self->GetWorld()->GetSubsystem<USFLootWorldSubsystem>()->Apply(View);
        if (View.bGround)
        { if (!View.Items.IsEmpty()) { Self->EnqueueTake(Actor.Get(), View.Items[0]); } }
        else { Self->ExternalSource = Actor.Get(); Self->ClientSnapshot(Actor.Get(), View.Items, true); }
    });
}

void USFLootComponent::RequestTake(const FString& DefinitionId, int32 Quantity, const FString& InstanceId)
{ ServerTake(ExternalSource, DefinitionId, Quantity, InstanceId); }
void USFLootComponent::RequestDrop(const FString& DefinitionId, int32 Quantity, const FString& InstanceId)
{ ServerDrop(DefinitionId, Quantity, InstanceId); }

void USFLootComponent::ServerTake_Implementation(ASFLootSourceActor* Source, const FString& DefinitionId, int32 Quantity, const FString& InstanceId)
{
    // Opening grants no durable permission: every transfer rechecks position and
    // host eligibility. A client cannot transfer from an arbitrary container ID.
    if (Source != ExternalSource || Quantity <= 0 || Quantity > 10000) { return; }
    FSFLootEntry Item; Item.DefinitionId = DefinitionId; Item.Quantity = Quantity; Item.InstanceId = InstanceId;
    EnqueueTake(Source, Item);
}

void USFLootComponent::EnqueueTake(ASFLootSourceActor* Source, const FSFLootEntry& Item)
{
    auto* Player = Cast<APlayerController>(GetOwner());
    USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Player);
    if (!Backpack || !InReach(Player, Source)) { return; }
    FSFBackpackMutation Mutation; Mutation.Label = TEXT("loot-take");
    Mutation.OperationId = TEXT("loot-take:") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Mutation.Run = [Weak = TWeakObjectPtr<USFLootComponent>(this), Actor = TWeakObjectPtr<ASFLootSourceActor>(Source), Item](const FString& Op, int64 Revision, TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        USFLootComponent* Self = Weak.Get(); auto* Player = Self ? Cast<APlayerController>(Self->GetOwner()) : nullptr;
        USFBackpackComponent* Backpack = Player ? USFBackpackComponent::FindOrCreate(Player) : nullptr;
        if (!Self || !Backpack || !InReach(Player, Actor.Get())) { Report(ESFBackpackOutcome::Rejected, Revision); return; }
        USFInventoryComponent* Inventory = Player->FindComponentByClass<USFInventoryComponent>();
        FSFItemQuantity Add; Add.Item = TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Item.DefinitionId))); Add.Quantity = Item.Quantity;
        FSFInventoryChange Change; Change.Inventory = Inventory; Change.AddItems.Add(Add);
        if (SFInventoryTransactions::CanExecuteInventoryTransaction({Change}) != ESFInventoryResult::Success)
        { Report(ESFBackpackOutcome::Rejected, Revision); return; }
        FSFGameplayRequest Q; Q.Rpc = ESFGameplayRpc::TakeLoot; Q.OperationId = Op;
        Q.CharacterId = Backpack->GetCharacterId(); Q.ContainerId = Backpack->GetContainerId(); Q.ExpectedContainerRevision = Revision;
        Q.LootSourceId = Actor->GetSnapshot().Id; Q.LootSourceRevision = Actor->GetSnapshot().Revision;
        Q.LootDefinitionId = Item.DefinitionId; Q.LootQuantity = Item.Quantity; Q.LootInstanceId = Item.InstanceId;
        Self->SendMutation(Q, Actor, MoveTemp(Report));
    };
    Mutation.Finished = [Weak = TWeakObjectPtr<USFBackpackComponent>(Backpack)](ESFBackpackOutcome Outcome)
    { if (Outcome == ESFBackpackOutcome::Committed) { if (auto* B = Weak.Get()) { B->Reconcile(); } } };
    Backpack->Enqueue(MoveTemp(Mutation));
}

void USFLootComponent::ServerDrop_Implementation(const FString& DefinitionId, int32 Quantity, const FString& InstanceId)
{
    auto* Player = Cast<APlayerController>(GetOwner());
    auto* Character = Player ? Cast<ASFE1Character>(Player->GetPawn()) : nullptr;
    USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Player);
    if (!Backpack || !Character || Character->CombatHealth() <= 0 || Quantity <= 0 || Quantity > 10000) { return; }
    FSFBackpackMutation Mutation; Mutation.Label = TEXT("loot-drop"); Mutation.OperationId = TEXT("loot-drop:") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Mutation.Run = [Weak = TWeakObjectPtr<USFLootComponent>(this), DefinitionId, Quantity, InstanceId](const FString& Op, int64 Revision, TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        USFLootComponent* Self = Weak.Get(); auto* Player = Self ? Cast<APlayerController>(Self->GetOwner()) : nullptr;
        auto* Character = Player ? Cast<ASFE1Character>(Player->GetPawn()) : nullptr;
        USFBackpackComponent* Backpack = Player ? USFBackpackComponent::FindOrCreate(Player) : nullptr;
        USFItemDefinition* Item = LoadObject<USFItemDefinition>(nullptr, *SFStationRecipes::ItemAssetPath(DefinitionId));
#if WITH_EDITOR
        if (FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowNetcheck")))
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DROP_PREFLIGHT Character=%d Health=%d Definition=%d Mesh=%d"), Character != nullptr, Character ? Character->CombatHealth() : 0, Item != nullptr, Item && !Item->WorldMesh.IsNull());
#endif
        if (!Self || !Backpack || !Character || Character->CombatHealth() <= 0 || !Item)
        { Report(ESFBackpackOutcome::Rejected, Revision); return; }
        // WorldMesh is optional. Freeze the same canonical model/fallback and its actual bounds before any debit.
        const FSoftObjectPath MeshPath = Item->WorldMesh.IsNull() ? FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")) : Item->WorldMesh.ToSoftObjectPath();
        UStaticMesh* Mesh = Cast<UStaticMesh>(MeshPath.TryLoad());
        if (!Mesh || !SFLootGeometry::ValidGroundBounds(Mesh->GetBounds().Origin, Mesh->GetBounds().BoxExtent, FTransform::Identity))
        { Report(ESFBackpackOutcome::Rejected, Revision); return; }
        FSFGameplayRequest Q; Q.Rpc = ESFGameplayRpc::DropLoot; Q.OperationId = Op;
        Q.CharacterId = Backpack->GetCharacterId(); Q.ContainerId = Backpack->GetContainerId(); Q.ExpectedContainerRevision = Revision;
        Q.LootDefinitionId = DefinitionId; Q.LootQuantity = Quantity; Q.LootInstanceId = InstanceId;
        Q.LootMeshPath = MeshPath.ToString(); Q.ZoneId = USFLootWorldSubsystem::ZoneKey(Self->GetWorld());
        Q.bHasLootMeshBounds = true; Q.LootMeshBoundsOriginCm = Mesh->GetBounds().Origin; Q.LootMeshBoundsExtentCm = Mesh->GetBounds().BoxExtent;
        const FVector Start = Character->GetActorLocation(); FHitResult Floor;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(LootDropFloor), false, Character);
        if (!Self->GetWorld()->LineTraceSingleByChannel(Floor, Start, Start - FVector(0,0,Character->GetSimpleCollisionHalfHeight() * 2), ECC_Visibility, Query))
        {
#if WITH_EDITOR
            if (FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowNetcheck"))) UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_DROP_NO_FLOOR Start=%s"), *Start.ToString());
#endif
            Report(ESFBackpackOutcome::Rejected, Revision); return;
        }
        Q.Position = Floor.ImpactPoint + FVector(0,0,Q.LootMeshBoundsExtentCm.Z - Q.LootMeshBoundsOriginCm.Z);
        Q.LootMeshTransform.SetLocation(Q.Position);
        if (!SFLootGeometry::ValidGroundBounds(Q.LootMeshBoundsOriginCm, Q.LootMeshBoundsExtentCm, Q.LootMeshTransform))
        { Report(ESFBackpackOutcome::Rejected, Revision); return; }
        Self->SendMutation(Q, nullptr, MoveTemp(Report));
    };
    Mutation.Finished = [Weak = TWeakObjectPtr<USFBackpackComponent>(Backpack)](ESFBackpackOutcome Outcome)
    { if (Outcome == ESFBackpackOutcome::Committed) { if (auto* B = Weak.Get()) { B->Reconcile(); } } };
    Backpack->Enqueue(MoveTemp(Mutation));
}

void USFLootComponent::SendMutation(const FSFGameplayRequest& Request, TWeakObjectPtr<ASFLootSourceActor> Source,
    TFunction<void(ESFBackpackOutcome, int64)> Report)
{
    auto Services = USFBackpackComponent::Services();
    if (!Services) { Report(ESFBackpackOutcome::Unavailable, -1); return; }
    Services->Call(Request, [Weak = TWeakObjectPtr<USFLootComponent>(this), Request, Source, Report](const FSFGameplayResponse& Reply)
    {
        USFLootComponent* Self = Weak.Get();
        if (!Self) { Report(ESFBackpackOutcome::Unavailable, -1); return; }
#if WITH_EDITOR
        if (FParse::Param(FCommandLine::Get(), TEXT("LootWorkflowNetcheck")))
            UE_LOG(LogTemp, Display, TEXT("LOOT_WORKFLOW_MUTATION_REPLY Rpc=%d Status=%d Grpc=%d Message=%s"), int32(Request.Rpc), int32(Reply.Status), Reply.GrpcCode, *Reply.Message);
#endif
        if (Reply.Status == ESFGameplayStatus::Unavailable)
        {
            FTimerHandle Retry; Self->GetWorld()->GetTimerManager().SetTimer(Retry,
                FTimerDelegate::CreateWeakLambda(Self, [Weak, Request, Source, Report]() { if (auto* Again = Weak.Get()) { Again->SendMutation(Request, Source, Report); } }),
                Self->GetWorld()->GetSubsystem<USFLootWorldSubsystem>()->GetReconcileSeconds(), false);
            return; // Keep FIFO ownership and the exact operation/payload until its outcome is known.
        }
        if (Reply.Status == ESFGameplayStatus::Ok && Reply.LootSources.Num() == 1)
        {
            const auto& View = Reply.LootSources[0];
            Self->GetWorld()->GetSubsystem<USFLootWorldSubsystem>()->Apply(View);
            if (auto* Backpack = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner())))
            { TMap<FString,int64> Delta; Delta.Add(Request.LootDefinitionId, Request.Rpc == ESFGameplayRpc::TakeLoot ? Request.LootQuantity : -Request.LootQuantity); Backpack->ApplyToBag(Delta); }
            if (Request.Rpc == ESFGameplayRpc::TakeLoot && Source == Self->ExternalSource) { Self->ClientSnapshot(Source.Get(), View.Items, false); }
            UE_LOG(LogTemp, Display, TEXT("LOOT_TRANSFER_COMMITTED Op=%s Item=%s Quantity=%lld"), *Request.OperationId, *Request.LootDefinitionId, Request.LootQuantity);
            Report(ESFBackpackOutcome::Committed, Reply.ContainerRevision);
        }
        else { Report(Reply.GrpcCode == 10 || Reply.Message.Contains(TEXT("Stale")) ? ESFBackpackOutcome::Stale : ESFBackpackOutcome::Rejected, -1); }
    });
}

void USFLootComponent::ClientSnapshot_Implementation(ASFLootSourceActor* Source, const TArray<FSFLootEntry>& Items, bool bOpen)
{
    ExternalSource = Source; ExternalItems = Items;
    auto* Player = Cast<ASFPlayerController>(GetOwner()); if (!Player) { return; }
    if (bOpen)
    {
        if (!ExternalWidget)
        {
            const auto Class = LoadClass<USFInventoryWidget>(nullptr, USFInventoryWidget::AssetPath);
            if (!Class) { return; }
            ExternalWidget = CreateWidget<USFInventoryWidget>(Player, Class);
            ExternalWidget->ConfigureExternal(this); ExternalWidget->AddToViewport(20);
        }
        Player->OpenExternalInventoryPanel(ExternalWidget);
    }
}

void USFLootComponent::ServerCloseExternal_Implementation() { ExternalSource = nullptr; }
void USFLootComponent::ClientCloseExternal_Implementation()
{
    ExternalSource = nullptr; ExternalItems.Reset();
    if (auto* Player = Cast<ASFPlayerController>(GetOwner()))
    { if (ExternalWidget) { Player->GetWindowStack()->Close(ExternalWidget); } }
}

void USFLootComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if WITH_EDITOR
    FSFLootWorkflowNetcheck::Tick(*this);
#endif
    auto* Player = Cast<ASFPlayerController>(GetOwner()); if (!Player) { return; }
    if (Player->IsLocalController() && ExternalWidget && ExternalSource && !Player->GetWindowStack()->IsOpen(ExternalWidget))
    { ExternalSource = nullptr; ExternalItems.Reset(); ServerCloseExternal(); }
    if (!GetOwner()->HasAuthority() || !ExternalSource) { return; }
    if (!InReach(Player, ExternalSource)) { ExternalSource = nullptr; ClientCloseExternal(); return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (bExternalRefreshInFlight || Now < NextExternalRefresh || !USFBackpackComponent::Services()) { return; }
    NextExternalRefresh = Now + GetWorld()->GetSubsystem<USFLootWorldSubsystem>()->GetReconcileSeconds();
    USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Player); if (!Backpack) { return; }
    bExternalRefreshInFlight = true;
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::OpenLootSource; Request.CharacterId = Backpack->GetCharacterId(); Request.LootSourceId = ExternalSource->GetSnapshot().Id;
    USFBackpackComponent::Services()->Call(Request, [Weak = TWeakObjectPtr<USFLootComponent>(this), Actor = TWeakObjectPtr<ASFLootSourceActor>(ExternalSource)](const FSFGameplayResponse& Reply)
    {
        USFLootComponent* Self = Weak.Get(); if (!Self) { return; } Self->bExternalRefreshInFlight = false;
        if (Self->ExternalSource != Actor.Get()) { return; }
        if (Reply.Status == ESFGameplayStatus::Rejected) { Self->ExternalSource = nullptr; Self->ClientCloseExternal(); return; }
        if (Reply.Status != ESFGameplayStatus::Ok || Reply.LootSources.Num() != 1 || !Actor.IsValid()) { return; }
        const auto& View = Reply.LootSources[0];
        if (View.Revision < Actor->GetSnapshot().Revision) { return; }
        Self->GetWorld()->GetSubsystem<USFLootWorldSubsystem>()->Apply(View);
        Self->ClientSnapshot(Actor.Get(), View.Items, false);
    });
}

void USFLootComponent::SetBackpackInstances(const TArray<FSFLedgerInstance>& Instances)
{
    BackpackInstances.Reset();
    for (const auto& Instance : Instances)
    { FSFLootEntry Entry; Entry.DefinitionId = Instance.DefinitionId; Entry.InstanceId = Instance.InstanceId; Entry.Quantity = 1; Entry.DurabilityPercent = Instance.DurabilityPercent; BackpackInstances.Add(Entry); }
}
void USFLootComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME_CONDITION(USFLootComponent, BackpackInstances, COND_OwnerOnly); }
