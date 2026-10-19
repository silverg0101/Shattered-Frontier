#pragma once

#include "Framework/SFE1GameMode.h"
#include "Interaction/SFE1PlayerController.h"
#include "GameFramework/HUD.h"
#include "SFConstructionNetwork.generated.h"

class USFGatheringPilotComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;
class ASFConstructionNetworkController;
class USFConstructionSandboxPalette;
enum class ESFConstructionPaletteAction : uint8;

UENUM()
enum class ESFConstructionNetworkResult : uint8
{
    Accepted, InvalidContext, PermissionDenied, Stale, InvalidCell, OutOfReach, Occluded, Occupied, ProjectionFailed, Unsupported, LimitReached, NotFound, Removed, InvalidPiece, DoorBlocked, DoorOpened, DoorClosed, DoorRemoved, InvalidLine
};

USTRUCT()
struct FSFConstructionNetworkMember
{
    GENERATED_BODY()
    UPROPERTY() FIntVector Cell = FIntVector::ZeroValue;
    UPROPERTY() uint8 Shape = 0;
    UPROPERTY() uint8 Rotation = 0;
};

USTRUCT()
struct FSFConstructionNetworkDoor
{
    GENERATED_BODY()
    UPROPERTY() FIntVector Origin = FIntVector::ZeroValue;
    UPROPERTY() uint8 Yaw = 0;
    UPROPERTY() bool bOpen = false;
};

USTRUCT()
struct FSFConstructionNetworkDoorProjection
{
    GENERATED_BODY()
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Boxes;
};

// Canonical pilot development protocol. No material, path, identity or permission payloads.
USTRUCT()
struct FSFConstructionNetworkSnapshot
{
    GENERATED_BODY()
    UPROPERTY() uint64 Revision = 0;
    UPROPERTY() TArray<FSFConstructionNetworkMember> Members;
    UPROPERTY() TArray<FSFConstructionNetworkDoor> Doors;
    int32 Find(const FIntVector& Cell) const { return Members.IndexOfByPredicate([&](const auto& M) { return M.Cell==Cell; }); }
    int32 FindDoor(const FIntVector& Cell) const { return Doors.IndexOfByPredicate([&](const auto& D) { return D.Origin==Cell; }); }
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
    ESFConstructionNetworkResult PlaceLine(ASFConstructionNetworkController* Controller, FIntVector Start, FIntVector End, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation);
    ESFConstructionNetworkResult PlaceDoor(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw);
    ESFConstructionNetworkResult ToggleDoor(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision);
    ESFConstructionNetworkResult RemoveDoor(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision);
    const FSFConstructionNetworkSnapshot& GetSnapshot() const { return Snapshot; }
    bool HasCurrentCollision() const;
    FString GetContentDigest() const;
    void AdmitPair();
    bool IsPairReady() const { return bPairReady; }
    double GetReachCentimetres() const { return ReachCentimetres; }
    int32 GetMaximumMembers() const { return MaximumMembers; }
    bool IsProjectionCurrent() const { return bProjectionReady && ProjectionRevision == Snapshot.Revision; }
    bool GetCellForComponent(const UPrimitiveComponent* Component, FIntVector& OutCell) const;
    bool GetDoorForComponent(const UPrimitiveComponent* Component, FIntVector& OutOrigin) const;
    bool FindDoorAlongRay(const APawn* Pawn, const FVector& Start, const FVector& End, FIntVector& OutOrigin) const;
    bool OwnsDoorProjection(const FSFConstructionNetworkDoor& Door, const FSFConstructionNetworkDoorProjection& Projection) const;
    bool IsSupportAllowedForPreview(FIntVector Cell, uint8 Shape, uint8 Rotation, bool bRemove) const;
    bool IsLineSupportedForPreview(FIntVector Start, FIntVector End, uint8 Shape, uint8 Rotation) const;
    int32 GetMaximumLineCells() const { return MaximumLineCells; }
    int32 GetMaximumDoors() const { return MaximumDoors; }
private:
    ESFConstructionNetworkResult ValidateEditContext(ASFConstructionNetworkController* Controller, uint64 ExpectedRevision, const APawn*& OutPawn, const AActor*& OutFoundation) const;
    ESFConstructionNetworkResult ValidateDoorTarget(ASFConstructionNetworkController* Controller, FIntVector Origin, uint64 ExpectedRevision, int32& OutIndex) const;
    ESFConstructionNetworkResult ValidateMemberCell(const APawn* Pawn, const AActor* Foundation, FIntVector Cell, bool bRemove, uint8 Shape, uint8 Rotation) const;
    bool Activate(const FSFConstructionNetworkSnapshot& Candidate);
    UFUNCTION() void OnRep_Snapshot();
    UPROPERTY(ReplicatedUsing=OnRep_Snapshot) FSFConstructionNetworkSnapshot Snapshot;
    UPROPERTY(Replicated) bool bPairReady = false;
    UPROPERTY(Transient) TMap<FIntVector,TObjectPtr<UProceduralMeshComponent>> Meshes;
    UPROPERTY() TObjectPtr<UStaticMesh> DoorBoxAsset;
    UPROPERTY(Transient) TMap<FIntVector,FSFConstructionNetworkDoorProjection> DoorProjections;
    TMap<FIntVector,FSFConstructionNetworkMember> ProjectedMembers;
    TMap<FIntVector,FSFConstructionNetworkDoor> ProjectedDoors;
    bool bProjectionReady = false;
    bool bConfigurationReady = false;
    UPROPERTY(Replicated) double ReachCentimetres = 0;
    UPROPERTY(Replicated) int32 MaximumMembers = 0;
    UPROPERTY(Replicated) int32 MaximumDoors = 0;
    UPROPERTY(Replicated) int32 MaximumLineCells = 0;
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
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void SpawnFiniteDoorObstruction(const FIntVector& Origin, uint8 Yaw);
    void DiscardFiniteDoorObstruction();
#endif
private:
    TWeakObjectPtr<ASFConstructionNetworkController> Builder;
    bool bBuilderAssigned = false;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TWeakObjectPtr<AActor> FiniteDoorObstruction;
    bool bFiniteObstructionSpawned = false;
#endif
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionNetworkController : public ASFE1PlayerController
{
    GENERATED_BODY()
public:
    ASFConstructionNetworkController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;
    virtual void SetupInputComponent() override;
    virtual void AcknowledgePossession(APawn* P) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void AssignRole(bool bBuilder);
    FString GetBuildStatusText() const;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFGatheringPilotComponent> GatheringPilot;
    UPROPERTY() TObjectPtr<UActorComponent> PilotInventory;
    void InitializeLocalControls();
    void ApplyCursorMode();
    void LayoutPalette();
    void BeginCameraLook();
    void EndCameraLook();
    void ApplicationActivationChanged(bool bActive);
    bool CanUseConstructionControls() const;
    bool GetConstructionRay(FVector& Start, FVector& Direction) const;
    bool PrepareCursorAction();
    void ToggleDemolition();
    void TogglePlacementPlane();
    void LayerDown();
    void LayerUp();
    FString GetPlaneStatusText() const;
    bool GetLayerPoint(const FVector& Start, const FVector& Direction, double RayLength, const FHitResult* Hit, FVector& OutPoint) const;
    void CancelCursorAction();
    void PaletteAction(ESFConstructionPaletteAction Action);
    void PlaceFromView();
    void RemoveFromView();
    void ToggleDoorFromView();
    void RefreshPlacementPreview(bool bDrawOutline);
    void ProcessEdit(FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation);
    void SelectCube();
    void SelectSlab();
    void SelectSlope();
    void SelectDoor();
    void RotatePiece();
    void ToggleLineMode();
    void CancelLine();
    void SelectShape(uint8 Shape);
    UFUNCTION(Server, Reliable) void ServerPlaceLine(FIntVector Start, FIntVector End, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation);
    UFUNCTION(Server, Reliable) void ServerPlace(FIntVector Cell, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation);
    UFUNCTION(Server, Reliable) void ServerRemove(FIntVector Cell, uint64 ExpectedRevision);
    UFUNCTION(Server, Reliable) void ServerPlaceDoor(FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw);
    UFUNCTION(Server, Reliable) void ServerRemoveDoor(FIntVector Origin, uint64 ExpectedRevision);
    UFUNCTION(Server, Reliable) void ServerToggleDoor(FIntVector Origin, uint64 ExpectedRevision);
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
    UPROPERTY(Transient) TObjectPtr<USFConstructionSandboxPalette> Palette;
    bool bCameraLookHeld = false;
    bool bOwnsLookSuppression = false;
    bool bApplicationActive = true;
    bool bDispatchingPaletteAction = false;
    FDelegateHandle ActivationHandle;
    uint64 SampledRevision = MAX_uint64;
    FString CachedPaletteStatus;
    FIntPoint PaletteViewportSize = FIntPoint::ZeroValue;
    float PaletteViewportScale = 0;
    bool bLayerPlacement = false;
    int32 PlacementLayer = 0; // Local selection on the released Z0..19 construction lattice.
    bool bDemolitionMode = false;
    bool bDoorSelected = false;
    bool bLineMode = false;
    bool bHasLineSeed = false;
    FIntVector LineStart = FIntVector::ZeroValue;
    uint64 LineRevision = 0;
    struct FLinePreviewCache
    {
        TWeakObjectPtr<ASFConstructionNetworkState> State;
        uint64 Revision = MAX_uint64;
        FIntVector Start = FIntVector::ZeroValue, End = FIntVector::ZeroValue;
        uint8 Shape = 255, Rotation = 255;
        bool bSupported = false;
    } LineCache;
    bool bHasDoorTarget = false;
    FIntVector TargetDoorOrigin = FIntVector::ZeroValue;
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
    friend class FSFConstructionNetworkCursorTest;
    bool bScenarioCursor = false;
    bool bSyntheticPointerOverPalette = false;
    void TickFiniteScenario();
    int32 ScenarioCursorCheck = 0;
    int32 ScenarioStep = 0;
    double ScenarioStarted = 0;
    FString ScenarioScreenshotPath;
    bool bScenarioLayerReadyReported = false;
    bool bScenarioLayerSeedQueued = false;
    bool bScenarioLayerSeedReported = false;
    bool bScenarioLineSeedQueued = false;
    bool bScenarioLineSeedReported = false;
    bool bScenarioLineReadyReported = false;
    bool bScenarioEmptyReported = false;
    bool bScenarioDoorCameraAimed = false;
    bool bScenarioDoorCapsuleClosedReported = false;
    bool bScenarioDoorCapsuleOpenReported = false;
    bool bScenarioDoorRemoveReported = false;
    bool bScenarioDoorClosedReported = false;
    bool bScenarioDoorOpenReported = false;
    bool bScenarioDoorOpenCaptureStarted = false;
    FString ScenarioDoorOpenPath;
#endif
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionNetworkHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
