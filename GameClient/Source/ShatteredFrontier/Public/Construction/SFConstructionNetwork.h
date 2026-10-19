#pragma once

#include "Framework/SFE1GameMode.h"
#include "Interaction/SFE1PlayerController.h"
#include "SFConstructionNetwork.generated.h"

class UStaticMeshComponent;
class ASFConstructionNetworkController;

UENUM()
enum class ESFConstructionNetworkResult : uint8
{
    Accepted, InvalidContext, PermissionDenied, Stale, InvalidCell, OutOfReach, Occluded, Occupied, ProjectionFailed
};

// Versioned one-cube development protocol. No material, path, identity or permission payloads.
USTRUCT()
struct FSFConstructionNetworkSnapshot
{
    GENERATED_BODY()
    UPROPERTY() uint64 Revision = 0;
    UPROPERTY() FIntVector Cell = FIntVector::ZeroValue;
};

UCLASS(NotBlueprintable, Transient)
class SHATTEREDFRONTIER_API ASFConstructionNetworkState : public AActor
{
    GENERATED_BODY()
public:
    ASFConstructionNetworkState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    ESFConstructionNetworkResult Place(ASFConstructionNetworkController* Controller, FIntVector Cell, uint64 ExpectedRevision);
    const FSFConstructionNetworkSnapshot& GetSnapshot() const { return Snapshot; }
    bool HasCurrentCollision() const;
    void AdmitPair();
    bool IsPairReady() const { return bPairReady; }
private:
    bool Activate(const FSFConstructionNetworkSnapshot& Candidate);
    UFUNCTION() void OnRep_Snapshot();
    UPROPERTY(ReplicatedUsing=OnRep_Snapshot) FSFConstructionNetworkSnapshot Snapshot;
    UPROPERTY(Replicated) bool bPairReady = false;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Cube;
    bool bProjectionReady = false;
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionNetworkGameMode : public ASFE1GameMode
{
    GENERATED_BODY()
public:
    ASFConstructionNetworkGameMode();
    virtual void StartPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    bool IsAdmitted() const;
    bool IsBuilder(const ASFConstructionNetworkController* Controller) const;
private:
    TWeakObjectPtr<ASFConstructionNetworkController> Builder;
    bool bBuilderAssigned = false;
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionNetworkController : public ASFE1PlayerController
{
    GENERATED_BODY()
public:
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void AssignRole(bool bBuilder);
private:
    void PlaceFromView();
    UFUNCTION(Server, Reliable) void ServerPlace(FIntVector Cell, uint64 ExpectedRevision);
    UFUNCTION(Client, Reliable) void ClientPlacementResult(ESFConstructionNetworkResult Result, uint64 Revision);
    UPROPERTY(Replicated) bool bRoleAssigned = false;
    UPROPERTY(Replicated) bool bBuilderRole = false; // Advisory; permission is derived again on server.
    uint64 LastRequestFrame = MAX_uint64;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickFiniteScenario();
    int32 ScenarioStep = 0;
    double ScenarioStarted = 0;
#endif
};
