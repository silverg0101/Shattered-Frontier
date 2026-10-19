#pragma once

#include "Framework/SFE1GameMode.h"
#include "Interaction/SFE1PlayerController.h"
#include "GameFramework/HUD.h"
#include "SFConstructionNetwork.generated.h"

class UStaticMeshComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class ASFConstructionNetworkController;

UENUM()
enum class ESFConstructionNetworkResult : uint8
{
    Accepted, InvalidContext, PermissionDenied, Stale, InvalidCell, OutOfReach, Occluded, Occupied, ProjectionFailed, Unsupported, LimitReached, NotFound, Removed, InvalidPiece
};

USTRUCT()
struct FSFConstructionNetworkMember
{
    GENERATED_BODY()
    UPROPERTY() FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY() uint8 Shape = 0;
    UPROPERTY() uint8 Rotation = 0;
};

// Canonical pilot development protocol. No material, path, identity or permission payloads.
USTRUCT()
struct FSFConstructionNetworkSnapshot
{
    GENERATED_BODY()
    UPROPERTY() uint64 Revision = 0;
    UPROPERTY() TArray<FSFConstructionNetworkMember> Members;
    int32 Find(const FIntVector& Cell) const { return Members.IndexOfByPredicate([&](const auto& M) { return M.Cell==Cell; }); }
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
    ESFConstructionNetworkResult Edit(ASFConstructionNetworkController* Controller, FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation);
    const FSFConstructionNetworkSnapshot& GetSnapshot() const { return Snapshot; }
    bool HasCurrentCollision() const;
    FString GetContentDigest() const;
    void AdmitPair();
    bool IsPairReady() const { return bPairReady; }
    double GetReachCentimetres() const { return ReachCentimetres; }
    int32 GetMaximumMembers() const { return MaximumMembers; }
    bool IsProjectionCurrent() const { return bProjectionReady && ProjectionRevision == Snapshot.Revision; }
    bool GetCellForComponent(const UPrimitiveComponent* Component, FIntVector& OutCell) const;
    bool IsSupportAllowedForPreview(FIntVector Cell, uint8 Shape, uint8 Rotation, bool bRemove) const;
private:
    bool Activate(const FSFConstructionNetworkSnapshot& Candidate);
    UFUNCTION() void OnRep_Snapshot();
    UPROPERTY(ReplicatedUsing=OnRep_Snapshot) FSFConstructionNetworkSnapshot Snapshot;
    UPROPERTY(Replicated) bool bPairReady = false;
    UPROPERTY(Transient) TMap<FIntVector,TObjectPtr<UProceduralMeshComponent>> Meshes;
    TMap<FIntVector,FSFConstructionNetworkMember> ProjectedMembers;
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
    ASFConstructionNetworkController();
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
    void ProcessEdit(FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation);
    void SelectCube();
    void SelectSlab();
    void SelectSlope();
    void RotatePiece();
    void SelectShape(uint8 Shape);
    UFUNCTION(Server, Reliable) void ServerPlace(FIntVector Cell, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation);
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
    uint8 SelectedShape = 0;
    uint8 SelectedRotation = 0;
    uint8 SelectedYaw = 0;
    struct FSupportCache
    {
        TWeakObjectPtr<ASFConstructionNetworkState> State;
        uint64 Revision = MAX_uint64;
        FIntVector PlacementCell = FIntVector::ZeroValue;
        FIntVector RemovalCell = FIntVector::ZeroValue;
        uint8 Shape = 255, Rotation = 255;
        bool bHasRemoval = false;
        bool bPlacementAllowed = false, bRemovalAllowed = false;
    } SupportCache;
    FString RemovalStatus = TEXT("Aim at a placed piece to remove it.");
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickFiniteScenario();
    int32 ScenarioStep = 0;
    double ScenarioStarted = 0;
    FString ScenarioScreenshotPath;
    bool bScenarioEmptyReported = false;
#endif
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionNetworkHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
