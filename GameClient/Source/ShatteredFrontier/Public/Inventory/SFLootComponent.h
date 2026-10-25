#pragma once

#include "Components/ActorComponent.h"
#include "Inventory/SFLootTypes.h"
#include "Services/SFGameplayServicesClient.h"
#include "SFLootComponent.generated.h"

class ASFLootSourceActor;
class USFInventoryWidget;
struct FSFLedgerInstance;
enum class ESFBackpackOutcome : uint8;

UCLASS()
class SHATTEREDFRONTIER_API USFLootComponent : public UActorComponent
{
    GENERATED_BODY()
    friend class FSFLootWorkflowNetcheck;
    friend class FSFLootGroundGeometryTest;
public:
    USFLootComponent();
    static USFLootComponent* Find(const APlayerController* Controller);
    bool TryInteract(AActor* Target);
    void RequestTake(const FString& DefinitionId, int32 Quantity, const FString& InstanceId);
    void RequestDrop(const FString& DefinitionId, int32 Quantity, const FString& InstanceId);
    const TArray<FSFLootEntry>& GetExternalItems() const { return ExternalItems; }
    const TArray<FSFLootEntry>& GetBackpackInstances() const { return BackpackInstances; }
    void SetBackpackInstances(const TArray<FSFLedgerInstance>& Instances);
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    static bool InReach(const APlayerController* Controller, const AActor* Source);
private:
    UFUNCTION(Server, Reliable) void ServerOpen(ASFLootSourceActor* Source);
    UFUNCTION(Server, Reliable) void ServerTake(ASFLootSourceActor* Source, const FString& DefinitionId, int32 Quantity, const FString& InstanceId);
    UFUNCTION(Server, Reliable) void ServerDrop(const FString& DefinitionId, int32 Quantity, const FString& InstanceId);
    UFUNCTION(Client, Reliable) void ClientSnapshot(ASFLootSourceActor* Source, const TArray<FSFLootEntry>& Items, bool bOpen);
    UFUNCTION(Client, Reliable) void ClientCloseExternal();
    UFUNCTION(Server, Reliable) void ServerCloseExternal();
    void EnqueueTake(ASFLootSourceActor* Source, const FSFLootEntry& Item);
    void SendMutation(const FSFGameplayRequest& Request, TWeakObjectPtr<ASFLootSourceActor> Source,
        TFunction<void(ESFBackpackOutcome, int64)> Report);
    UPROPERTY(Transient) TObjectPtr<USFInventoryWidget> ExternalWidget;
    UPROPERTY(Transient) TObjectPtr<ASFLootSourceActor> ExternalSource;
    UPROPERTY(Transient) TArray<FSFLootEntry> ExternalItems;
    UPROPERTY(Replicated) TArray<FSFLootEntry> BackpackInstances;
    double NextExternalRefresh = 0;
    bool bExternalRefreshInFlight = false;
};
