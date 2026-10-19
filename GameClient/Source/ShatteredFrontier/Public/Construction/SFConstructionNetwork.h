#pragma once

#include "Framework/SFE1GameMode.h"
#include "Interaction/SFE1PlayerController.h"
#include "GameFramework/HUD.h"
#include "SFConstructionNetwork.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class ASFConstructionNetworkController;

UENUM()
enum class ESFConstructionNetworkResult : uint8
{
    Accepted, InvalidContext, PermissionDenied, Stale, InvalidCell, OutOfReach, Occluded, Occupied, ProjectionFailed, Unsupported, LimitReached, NotFound, Removed
};

// Canonical-cube development protocol. No material, path, identity or permission payloads.
USTRUCT()
struct FSFConstructionNetworkSnapshot
{
    GENERATED_BODY()
    UPROPERTY() uint64 Revision = 0;
    UPROPERTY() TArray<FIntVector> Cells;
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FSFConstructionNetworkSnapshot> : TStructOpsTypeTraitsBase2<FSFConstructionNetworkSnapshot>
{
    enum { WithNetSerializer = true };
};

UCLASS(NotBlueprintable, Transient)
class SHATTEREDFRONTIER_API ASFConstructionNetworkState : public AActor
{
    GENERATED_BODY()
public:
    ASFConstructionNetworkState();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    ESFConstructionNetworkResult Edit(ASFConstructionNetworkController* Controller, FIntVector Cell, uint64 ExpectedRevision, bool bRemove);
    const FSFConstructionNetworkSnapshot& GetSnapshot() const { return Snapshot; }
    bool HasCurrentCollision() const;
    FString GetContentDigest() const;
    void AdmitPair();
    bool IsPairReady() const { return bPairReady; }
    double GetReachCentimetres() const { return ReachCentimetres; }
    int32 GetMaximumMembers() const { return MaximumMembers; }
    bool IsProjectionCurrent() const { return bProjectionReady && ProjectionRevision == Snapshot.Revision; }
private:
    bool Activate(const FSFConstructionNetworkSnapshot& Candidate);
    UFUNCTION() void OnRep_Snapshot();
    UPROPERTY(ReplicatedUsing=OnRep_Snapshot) FSFConstructionNetworkSnapshot Snapshot;
    UPROPERTY(Replicated) bool bPairReady = false;
    UPROPERTY() TObjectPtr<UStaticMesh> CubeAsset;
    UPROPERTY(Transient) TMap<FIntVector,TObjectPtr<UStaticMeshComponent>> Cubes;
    bool bProjectionReady = false;
    bool bConfigurationReady = false;
    UPROPERTY(Replicated) double ReachCentimetres = 0;
    UPROPERTY(Replicated) int32 MaximumMembers = 0;
    uint64 ProjectionRevision = 0;
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
    virtual void AcknowledgePossession(APawn* P) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void AssignRole(bool bBuilder);
    FString GetBuildStatusText() const;
private:
    void PlaceFromView();
    void RemoveFromView();
    void RefreshPlacementPreview(bool bDrawOutline);
    void ProcessEdit(FIntVector Cell, uint64 ExpectedRevision, bool bRemove);
    UFUNCTION(Server, Reliable) void ServerPlace(FIntVector Cell, uint64 ExpectedRevision);
    UFUNCTION(Server, Reliable) void ServerRemove(FIntVector Cell, uint64 ExpectedRevision);
    UFUNCTION(Client, Reliable) void ClientPlacementResult(ESFConstructionNetworkResult Result, uint64 Revision);
    UPROPERTY(Replicated) bool bRoleAssigned = false;
    UPROPERTY(Replicated) bool bBuilderRole = false; // Advisory; permission is derived again on server.
    uint64 LastRequestFrame = MAX_uint64;
    bool bHasPlacementPreview = false;
    FIntVector PreviewCell = FIntVector::ZeroValue;
    bool bHasRemovalPreview = false;
    FIntVector RemovalCell = FIntVector::ZeroValue;
    FString PreviewStatus = TEXT("Connecting to construction state...");
    FString LastPlacementStatus = TEXT("No placement requested.");
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickFiniteScenario();
    int32 ScenarioStep = 0;
    double ScenarioStarted = 0;
    FString ScenarioScreenshotPath;
#endif
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionNetworkHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
