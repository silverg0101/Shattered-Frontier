#pragma once

#include "Engine/DeveloperSettings.h"
#include "Templates/Function.h"
#include "Services/SFGameplayServicesClient.h"
#include "SFInventoryLedgerClient.generated.h"

// INT-03 development connection from the UE server to InventoryLedger.Host. The contract is
// proto/sf/inventory/v1/ledger.proto; the gRPC transport lives in the server-only ShatteredFrontierBackend
// module and registers itself here, so client builds carry no backend code.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Inventory Ledger (Release Only)"))
class SHATTEREDFRONTIER_API USFInventoryLedgerSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Ledger", meta=(ToolTip="Persist gathering grants through InventoryLedger. Off keeps the session-only pilot. Release authors; next world start."))
    bool bEnabled = false;
    UPROPERTY(Config, EditAnywhere, Category="Ledger", meta=(ToolTip="Loopback host:port of InventoryLedger.Host (development topology)."))
    FString Endpoint = TEXT("127.0.0.1:50071");
    UPROPERTY(Config, EditAnywhere, Category="Ledger", meta=(ClampMin="1", ClampMax="30", Units="s", ToolTip="Deadline for each attempt."))
    float TimeoutSeconds = 3;
    UPROPERTY(Config, EditAnywhere, Category="Ledger", meta=(ClampMin="1", ClampMax="5", ToolTip="Attempts per operation, all with the same operation id."))
    int32 MaxAttempts = 3;
    bool Validate(FString& Error) const;
};

enum class ESFLedgerRpc : uint8 { EnsureContainer, GetContainer, Grant, Transfer };
// Ok; Stale = ABORTED; Rejected = ALREADY_EXISTS/FAILED_PRECONDITION/NOT_FOUND/INVALID_ARGUMENT/UNAUTHENTICATED;
// Unavailable = UNAVAILABLE or DEADLINE_EXCEEDED (retryable with the same operation id).
enum class ESFLedgerStatus : uint8 { Ok, Stale, Rejected, Unavailable };

struct FSFLedgerRequest
{
    ESFLedgerRpc Rpc = ESFLedgerRpc::GetContainer;
    FString ContainerId;
    // EnsureContainer: character-owned backpack, or the equipment container.
    FString OwnerId;
    int32 Capacity = 0;
    bool bEquipmentContainer = false;
    // Grant.
    FString OperationId;
    int64 ExpectedRevision = 0;
    FString DefinitionId;
    int64 Quantity = 0;
    FString Source;
    FString CausationId;
    // INT-06: complete, immutable ApplyReceiptRequest bytes, captured before Grant.
    TArray<uint8> FollowUpPayload;
    FString FollowUpDigest;
    // Transfer: ContainerId / ExpectedRevision are the source; both containers share one owner.
    FString ToContainerId;
    int64 ToExpectedRevision = 0;
};

struct FSFLedgerResponse
{
    ESFLedgerStatus Status = ESFLedgerStatus::Unavailable;
    int32 GrpcCode = 14;  // grpc::StatusCode, kept for logs and tests
    FString Message;
    // Container view (EnsureContainer/GetContainer), or the grant's revision after and quantity delta.
    int64 Revision = 0;
    TMap<FString, int64> Quantities;
    TMap<FString, int32> DurabilityPercent;
    FString OperationId;
    int64 GrantedDelta = 0;
    int64 ToRevision = 0;  // Transfer: the destination revision after
};

// Sends one attempt and calls back on the game thread. Never blocks the caller.
using FSFLedgerSend = TFunction<void(const FSFLedgerRequest&, float DeadlineSeconds, TFunction<void(const FSFLedgerResponse&)>)>;

class SHATTEREDFRONTIER_API FSFInventoryLedgerClient : public TSharedFromThis<FSFInventoryLedgerClient>
{
public:
    using FFollowUpSerializer = TFunction<bool(const FSFGameplayRequest&, TArray<uint8>&, FString&)>;
    static void SetFollowUpSerializer(FFollowUpSerializer Serializer);
    static bool SerializeFollowUp(const FSFGameplayRequest& Receipt, TArray<uint8>& Payload, FString& Digest);
    // The server-only backend module installs a factory that builds a send function for an endpoint.
    static void SetTransportFactory(TFunction<FSFLedgerSend(const FString& Endpoint)> Factory);
    // Test hook: replaces the transport for clients created afterwards. Pass nullptr to restore.
    static void SetSendOverrideForTests(FSFLedgerSend Send);
    // Null when the backend module is absent (client builds) or settings are invalid.
    static TSharedPtr<FSFInventoryLedgerClient> Create(const USFInventoryLedgerSettings& Settings);

    // Retries Unavailable with backoff, reusing the request unchanged, up to MaxAttempts.
    void Call(const FSFLedgerRequest& Request, TFunction<void(const FSFLedgerResponse&)> Done);

private:
    FSFInventoryLedgerClient(FSFLedgerSend InSend, float InTimeout, int32 InAttempts) : Send(MoveTemp(InSend)), TimeoutSeconds(InTimeout), MaxAttempts(InAttempts) {}
    void Attempt(const FSFLedgerRequest& Request, int32 Number, TFunction<void(const FSFLedgerResponse&)> Done);
    FSFLedgerSend Send;
    float TimeoutSeconds;
    int32 MaxAttempts;
};
