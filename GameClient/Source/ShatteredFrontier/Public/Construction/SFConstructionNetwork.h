#pragma once

#include "Framework/SFGameMode.h"
#include "GameFramework/HUD.h"
#include "SFConstructionNetwork.generated.h"

class USFGatheringPilotComponent;
class USFQuestPilotComponent;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class UStaticMesh;
class ASFPlayerController;
class USFConstructionSandboxPalette;
class USFGatheringSkillsPanel;
enum class ESFConstructionPaletteAction : uint8;

UENUM()
enum class ESFConstructionNetworkResult : uint8
{
    Accepted, InvalidContext, PermissionDenied, Stale, InvalidCell, OutOfReach, Occluded, Occupied, ProjectionFailed, Unsupported, LimitReached, NotFound, Removed, InvalidPiece, DoorBlocked, DoorOpened, DoorClosed, DoorRemoved, InvalidLine, ServiceUnavailable, Claimed, Granted
};

// A Housing structure mapped onto the catalog; out-of-range shape or rotation rejects the restore.
struct FSFConstructionNetworkRestoredPiece { FIntVector Cell = FIntVector::ZeroValue; int32 Shape = INDEX_NONE; int32 Rotation = INDEX_NONE; };

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
    // bCommit=false runs every admission check without changing anything (a housing commit then follows).
    ESFConstructionNetworkResult Edit(ASFPlayerController* Controller, FIntVector Cell, uint64 ExpectedRevision, bool bRemove, uint8 Shape, uint8 Rotation, bool bCommit=true);
    ESFConstructionNetworkResult PlaceLine(ASFPlayerController* Controller, FIntVector Start, FIntVector End, uint64 ExpectedRevision, uint8 Shape, uint8 Rotation);
    ESFConstructionNetworkResult PlaceDoor(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision, uint8 Yaw);
    ESFConstructionNetworkResult ToggleDoor(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision);
    ESFConstructionNetworkResult RemoveDoor(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision);
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
    // BLD-02: the claim's world anchor. Cells stay claim-local; this actor sits at the anchor, so every projected
    // component translates with it, and world queries add GetAnchorOffsetCm().
    FIntVector GetAnchorCell() const { return AnchorCell; }
    FVector GetAnchorOffsetCm() const;
    bool HasExpectedTransform() const;
    // Server only, before the first edit; an anchor never changes while the claim has members.
    bool ConfigureAnchor(const FIntVector& Cell);
    // BLD-02 A2: the Housing claim this state projects ("" in the sandbox). Set with the anchor at spawn.
    const FString& GetClaimId() const { return ClaimId; }
    void SetClaimId(const FString& InClaimId) { ClaimId=InClaimId; }
    // A7: rebuild an empty claim from its Housing structures. Every cell must lie inside the claim extent, or nothing
    // is projected (hard reject). Pieces come back as identity cubes until piece content is persisted (BLD-01).
    bool RestoreFromHousing(const TArray<FSFConstructionNetworkRestoredPiece>& Pieces);
private:
    ESFConstructionNetworkResult ValidateEditContext(ASFPlayerController* Controller, uint64 ExpectedRevision, const APawn*& OutPawn, const AActor*& OutFoundation) const;
    ESFConstructionNetworkResult ValidateDoorTarget(ASFPlayerController* Controller, FIntVector Origin, uint64 ExpectedRevision, int32& OutIndex) const;
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
    UPROPERTY(Replicated) FIntVector AnchorCell = FIntVector::ZeroValue;
    UPROPERTY(Replicated) FString ClaimId;
    uint64 ProjectionRevision = 0;
};

UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionNetworkHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
