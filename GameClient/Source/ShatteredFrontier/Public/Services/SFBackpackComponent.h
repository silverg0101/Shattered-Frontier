#pragma once

#include "Components/ActorComponent.h"
#include "SFBackpackComponent.generated.h"

class APlayerController;
class FSFInventoryLedgerClient;
struct FSFLedgerResponse;
class FSFGameplayServicesClient;

enum class ESFBackpackOutcome : uint8 { Committed, Stale, Rejected, Unavailable };

// One queued backpack mutation. Run issues the RPC with the given operation id and expected revision, then reports
// the outcome and the container revision after it (or -1 when the reply carries none, so the next mutation re-reads).
struct FSFBackpackMutation
{
    FString Label;
    FString OperationId;
    bool bRetryStale = true; // INT-06 grants retain their source ID, payload and expected revision.
    TFunction<void(const FString& OperationId, int64 ExpectedRevision, TFunction<void(ESFBackpackOutcome, int64)> Report)> Run;
    TFunction<void(ESFBackpackOutcome)> Finished;
};

// INT-04 decision 4: the single server-side writer of a controller's InventoryLedger backpack. Every mutation (harvest
// Grant, Craft, defeat reward, Place, Remove) is serialised FIFO against the cached container revision. A Stale outcome
// re-reads the container and retries once under the derived id "<op>:r1" (InventoryLedger fingerprints the expected
// revision and a Stale rejection stores no receipt). Exists only when the ledger is enabled.
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API USFBackpackComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    // Server: the controller's backpack, created on first use; null when the ledger is disabled or misconfigured.
    static USFBackpackComponent* FindOrCreate(APlayerController* Controller);
    // Gameplay.Host client shared by every server-side caller; null when services are disabled.
    static TSharedPtr<FSFGameplayServicesClient> Services();
    // Development fixture until DEC-04: a stable dev character name for a connection (see ASFGameMode::InitNewPlayer).
    static bool IsValidDevName(const FString& Name);
    static void AssignDevIdentity(APlayerController* Controller, const FString& Name);
    static void AssignGatewayIdentity(APlayerController* Controller, const FString& CharacterId, const FString& AccountId,
        const FString& RealmId, const FString& ArchetypeId, const TArray<uint8>& AppearancePacket);
    bool HasGatewayCreationSeed() const { return !GatewayArchetypeId.IsEmpty() && !GatewayAppearancePacket.IsEmpty(); }
    const FString& GetGatewayArchetypeId() const { return GatewayArchetypeId; }
    const TArray<uint8>& GetGatewayAppearancePacket() const { return GatewayAppearancePacket; }
    const FString& GetContainerId() { BindIdentity(); return ContainerId; }
    const FString& GetCharacterId() { BindIdentity(); return CharacterId; }
    const FString& GetAccountId() { BindIdentity(); return AccountId; }
    const FString& GetRealmId() { BindIdentity(); return RealmId; }
    TSharedPtr<FSFInventoryLedgerClient> GetLedger() const { return Ledger; }
    void Enqueue(FSFBackpackMutation Mutation);
    // Applies committed ledger deltas to the session bag projection, by definition id. Unknown items are logged.
    void ApplyToBag(const TMap<FString, int64>& Deltas) const;
    bool IsBusy() const { return bBusy; }
    bool IsIdle() const { return !bBusy && Queue.IsEmpty(); }
    bool HasDevIdentity() const { return !DevName.IsEmpty(); }
    bool HasVerifiedIdentity() const { return !VerifiedCharacterId.IsEmpty(); }
    // Queued read: rebuilds the bag projection from the ledger view (after a payout or refund whose items the reply omits,
    // and on an INT-05 restore). Done, when given, reports whether the read succeeded.
    void Reconcile(TFunction<void(bool)> Done = nullptr);
    // Server: moves one ledger item between the backpack and this character's equipment container (equip / unequip), queued
    // like every mutation. The equipment container is created on first use; Reconcile projects it onto the equipped slots.
    void TransferEquipment(const FString& DefinitionId, bool bEquip, TFunction<void(ESFBackpackOutcome)> Finished,
        const FString& InstanceId = FString());
    FString GetEquipmentContainerId() { BindIdentity(); return ContainerId + TEXT("_equipment"); }
#if WITH_DEV_AUTOMATION_TESTS
    // Test hook: a backpack over a stubbed ledger client, without the settings lookup.
    static USFBackpackComponent* CreateForTests(APlayerController* Controller, TSharedPtr<FSFInventoryLedgerClient> InLedger);
#endif
private:
    friend class FSFGatheringPilotLedgerTest;
    bool Initialize(APlayerController* Controller);
    void BindIdentity();
    bool bIdentityUsed = false;
    FString DevName;  // "" uses the connection id
    FString VerifiedCharacterId;
    FString VerifiedAccountId, VerifiedRealmId;
    FString GatewayArchetypeId;
    TArray<uint8> GatewayAppearancePacket;
    void Pump();
    void Execute(int32 Attempt);
    void Refresh(TFunction<void(bool)> Done);
    void EnsureEquipment(TFunction<void(const FSFLedgerResponse&)> Done);
    TSharedPtr<FSFInventoryLedgerClient> Ledger;
    FString ContainerId, CharacterId, AccountId, RealmId;
    int32 Capacity = 0;
    int64 Revision = -1;  // -1: unknown, read (or ensure) before the next mutation
    bool bBusy = false;
    TArray<FSFBackpackMutation> Queue;
};
