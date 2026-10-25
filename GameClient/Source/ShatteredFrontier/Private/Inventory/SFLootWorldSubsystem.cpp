#include "Inventory/SFLootWorldSubsystem.h"
#include "Inventory/SFLootSourceActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"
#include "Services/SFBackpackComponent.h"

FString USFLootWorldSubsystem::ZoneKey(const UWorld* World)
{
    return World ? FPackageName::GetShortName(World->GetOutermost()->GetName()).ToLower() : FString();
}

void USFLootWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{ Super::Initialize(Collection); ReconcileSeconds = FMath::Clamp(GetDefault<USFGameplayServicesSettings>()->LootReconcileSeconds, 1.f, 60.f); }

bool USFLootWorldSubsystem::RegisterDefeat(AActor* Corpse, const FSFLootDefeatSource& Source)
{
    if (!IsValid(Corpse) || !Corpse->HasAuthority() || !USFBackpackComponent::Services() ||
        Source.bProtected || Source.EventId.IsEmpty() || Source.KillerCharacterId.IsEmpty() || Source.EligibleCharacters.IsEmpty()) { return false; }
    if (Pending.Contains(Source.EventId)) { return true; }
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::CreateCorpseLoot; Request.OperationId = Source.EventId;
    Request.CreatureId = Source.CreatureId; Request.TableId = Source.TableId; Request.KillerCharacterId = Source.KillerCharacterId;
    Request.RaidGroupId = Source.RaidGroupId; Request.EligibleCharacterIds = Source.EligibleCharacters;
    Request.bSharedLoot = Source.bShared; Request.ZoneId = ZoneKey(GetWorld()); Request.Position = Source.Position;
    // Retain the existing body's geometry and transform. No new corpse art.
    if (UStaticMeshComponent* Body = Corpse->FindComponentByClass<UStaticMeshComponent>())
    {
        Request.LootMeshTransform = Body->GetComponentTransform();
        if (Body->GetStaticMesh()) { Request.LootMeshPath = Body->GetStaticMesh()->GetPathName(); }
    }
    if (Request.LootMeshPath.IsEmpty()) { Request.LootMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube"); }
    Pending.Add(Source.EventId, Request);
    SendDefeat(Source.EventId);
    return true; // This subsystem now owns all retries; never also auto-grant.
}

void USFLootWorldSubsystem::SendDefeat(const FString& EventId)
{
    if (InFlight.Contains(EventId) || !Pending.Contains(EventId) || !USFBackpackComponent::Services()) { return; }
    InFlight.Add(EventId);
    USFBackpackComponent::Services()->Call(Pending[EventId], [Weak = TWeakObjectPtr<USFLootWorldSubsystem>(this), EventId](const FSFGameplayResponse& Reply)
    {
        if (USFLootWorldSubsystem* Self = Weak.Get())
        {
            Self->InFlight.Remove(EventId);
            if (Reply.Status == ESFGameplayStatus::Ok)
            {
                for (const FSFLootSourceSnapshot& Snapshot : Reply.LootSources) { Self->Apply(Snapshot); }
                Self->Pending.Remove(EventId);
                UE_LOG(LogTemp, Display, TEXT("LOOT_CORPSE_COMMITTED Event=%s Sources=%d"), *EventId, Reply.LootSources.Num());
            }
            else if (Reply.Status == ESFGameplayStatus::Rejected)
            { UE_LOG(LogTemp, Error, TEXT("LOOT_CORPSE_REJECTED Event=%s Code=%d"), *EventId, Reply.GrpcCode); Self->Pending.Remove(EventId); }
        }
    });
}

void USFLootWorldSubsystem::BindOwner(ASFLootSourceActor* Actor)
{
    const auto& Source = Actor->GetSnapshot();
    if (Source.bGround || Source.bShared) { return; }
    Actor->bOnlyRelevantToOwner = true;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Player = It->Get();
        USFBackpackComponent* Identity = Player ? Player->FindComponentByClass<USFBackpackComponent>() : nullptr;
        if (Identity && Source.EligibleCharacters.Contains(Identity->GetCharacterId()))
        { if (Actor->GetOwner() != Player) { Actor->SetOwner(Player); Actor->ForceNetUpdate(); } return; }
    }
    Actor->SetOwner(nullptr);
}

void USFLootWorldSubsystem::Apply(const FSFLootSourceSnapshot& Snapshot)
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || Snapshot.ZoneId != ZoneKey(GetWorld())) { return; }
    if (const int64* Latest = LatestRevision.Find(Snapshot.Id)) { if (Snapshot.Revision < *Latest) { return; } }
    LatestRevision.Add(Snapshot.Id, Snapshot.Revision);
    ASFLootSourceActor* Actor = Actors.FindRef(Snapshot.Id).Get();
    if (Snapshot.Items.IsEmpty()) { if (Actor) { Actor->Destroy(); } Actors.Remove(Snapshot.Id); return; }
    if (!Actor)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Actor = GetWorld()->SpawnActor<ASFLootSourceActor>(ASFLootSourceActor::StaticClass(), Snapshot.Position, FRotator::ZeroRotator, Params);
        if (!Actor) { return; }
        Actors.Add(Snapshot.Id, Actor);
    }
    Actor->Initialize(Snapshot); BindOwner(Actor);
}

void USFLootWorldSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !USFBackpackComponent::Services()) { return; }
    for (const auto& Pair : Actors) { if (ASFLootSourceActor* Actor = Pair.Value.Get()) { BindOwner(Actor); } }
    const double Now = World->GetTimeSeconds(); if (Now < NextSyncAt) { return; }
    NextSyncAt = Now + ReconcileSeconds;
    TArray<FString> Keys; Pending.GetKeys(Keys); for (const FString& Key : Keys) { SendDefeat(Key); }
    if (bSyncInFlight) { return; }
    bSyncInFlight = true;
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::ListLootSources; Request.ZoneId = ZoneKey(World);
    USFBackpackComponent::Services()->Call(Request, [Weak = TWeakObjectPtr<USFLootWorldSubsystem>(this)](const FSFGameplayResponse& Reply)
    {
        USFLootWorldSubsystem* Self = Weak.Get(); if (!Self) { return; } Self->bSyncInFlight = false;
        if (Reply.Status != ESFGameplayStatus::Ok) { return; }
        for (const FSFLootSourceSnapshot& Snapshot : Reply.LootSources) { Self->Apply(Snapshot); }
    });
}

TStatId USFLootWorldSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(USFLootWorldSubsystem, STATGROUP_Tickables); }
