#pragma once

#include "GameFramework/Actor.h"
#include "Construction/SFConstructionInstability.h"
#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionDoorSupport.h"
#include "Construction/SFConstructionSaveCodec.h"
#include "SFConstructionSandboxState.generated.h"

class ASFConstructionSandboxController;
class ASFConstructionSandboxPreview;
class ASFConstructionSandboxDoorProjection;
class FSFConstructionReplacementSet;

enum class ESFConstructionDoorAction : uint8 { Place, Toggle, Remove };
struct FSFConstructionDoorIntent
{
    FIntVector OriginCell = FIntVector::ZeroValue;
    int32 YawQuarterTurns = 0; // Place only; other actions must match retained yaw.
    uint64 ExpectedRevision = 0;
    ESFConstructionDoorAction Action = ESFConstructionDoorAction::Place;
};
struct FSFConstructionSandboxDoor
{
    int32 YawQuarterTurns = 0;
    bool bOpen = false;
    ESFConstructionSupportState Support = ESFConstructionSupportState::Unsupported;
    TWeakObjectPtr<ASFConstructionSandboxDoorProjection> Projection;
};
struct FSFConstructionRemovalConsequences
{
    TArray<FIntVector> StructuralCells;
    TArray<FIntVector> DoorOrigins;
};

// Bounded intent only; no geometry/material/path input. Every edit compares the
// assembly revision, so a retry of an accepted edit is stale and cannot duplicate.
struct FSFConstructionSandboxIntent
{
    FIntVector Cell = FIntVector::ZeroValue;
    int32 Shape = 0;
    int32 Rotation = 0;
    uint64 ExpectedRevision = 0;
    bool bRemove = false;
    bool bConfirmSupportLoss = false;
};

struct FSFConstructionSandboxMember
{
    int32 Shape = 0;
    int32 Rotation = 0;
    uint64 Generation = 0;
    TWeakObjectPtr<ASFConstructionSandboxPreview> Projection;
    uint64 ProjectionGeneration = 0; // Derived mesh/collision stamp; occupancy Generation above is independent.
};

// Transient standalone authority checkpoint. No persistence,
// resource economy, authentication, replication or normal E1 mutation route.
UCLASS(NotBlueprintable, Transient)
class SHATTEREDFRONTIER_API ASFConstructionSandboxState : public AActor
{
    GENERATED_BODY()
public:
    ASFConstructionSandboxState();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    FString Evaluate(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, FString* SupportHint = nullptr) const;
    bool Apply(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, FString& Result);
    bool PreviewRemoval(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, TArray<FIntVector>& Affected, FString& Result, FSFConstructionRemovalConsequences* Consequences = nullptr);
    FString EvaluateDoor(const ASFConstructionSandboxController* Controller, const FSFConstructionDoorIntent& Intent) const;
    bool ApplyDoor(ASFConstructionSandboxController* Controller, const FSFConstructionDoorIntent& Intent, FString& Result);
    bool ExportSnapshot(const ASFConstructionSandboxController* Controller, uint64 ExpectedRevision,
        FSFConstructionSandboxSnapshot& OutSnapshot, FString& Result) const;
    bool ImportSnapshot(ASFConstructionSandboxController* Controller, uint64 ExpectedRevision,
        const FSFConstructionSandboxSnapshot& Snapshot, FString& Result);
    int32 GetDoorCount() const { return Doors.Num(); }
    const FSFConstructionSandboxDoor* FindDoor(const FIntVector& Origin) const { return Doors.Find(Origin); }
    bool FindDoorOrigin(const AActor* Projection, FIntVector& OutOrigin) const;
    bool GetDoorDeadline(const FIntVector& Origin, double& Out) const { return DoorInstability.GetDeadline(Origin,Out); }
    void CancelRemoval(const ASFConstructionSandboxController* Controller);
    FString GetInstabilitySummary() const;
    uint64 GetRevision() const { return Revision; }
    int32 GetMemberCount() const { return Members.Num(); }
    bool FindCell(const AActor* Projection, FIntVector& OutCell) const;
    float GetReachCentimetres() const { return ReachCentimetres; }
    static FVector CellCorner(const FIntVector& Cell);
private:
    friend class FSFConstructionReplacementSet;
    bool IsReplacementQueryCurrent(const FSFConstructionReplacementSet& Replacement) const;
    FSFConstructionReplacementSet MakeReplacementSet() const;
    FString ValidateStableSnapshot(const ASFConstructionSandboxController* Controller, uint64 ExpectedRevision) const;
    FString ValidateIntent(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent) const;
    FString ValidateEnvironment() const;
    FString ValidateOwner(const ASFConstructionSandboxController* Controller, uint64 ExpectedRevision) const;
    FString DoorReservations(const FIntVector& Origin, int32 Yaw, bool bOpen, bool bToggle, const ASFConstructionSandboxDoorProjection* OwnOldDoor) const;
    FString StructuralReservations(const FSFConstructionSandboxIntent& Intent) const;
    FString SolveSupport(const TMap<FIntVector, FSFConstructionSandboxMember>& Snapshot, TArray<FSFConstructionSupportResult>& Results) const;
    FString SolveCombined(const TMap<FIntVector, FSFConstructionSandboxMember>& Snapshot,
        const TMap<FIntVector, FSFConstructionSandboxDoor>& DoorSnapshot, TArray<FSFConstructionSupportResult>& Results,
        TArray<FSFConstructionDoorSupportResult>& DoorResults) const;
    FString CheckSupport(const FSFConstructionSandboxIntent& Intent, FString* SupportHint) const;
    bool CommitCandidate(TMap<FIntVector, FSFConstructionSandboxMember> Candidate, const TArray<FIntVector>& ChangedCells, double Now, FString& Result);
    bool CommitCombined(TMap<FIntVector, FSFConstructionSandboxMember> Candidate,
        TMap<FIntVector, FSFConstructionSandboxDoor> CandidateDoors, const TArray<FIntVector>& ChangedCells, double Now, FString& Result);
    void ProcessDeadlines(double Now);
    double AuthorityTime() const;
    TMap<FIntVector, FSFConstructionSandboxMember> Members;
    TMap<FIntVector, FSFConstructionSandboxDoor> Doors;
    uint64 Revision = 0;
    float ReachCentimetres = 0;
    int32 MaximumMembers = 0;
    int32 MaximumDoors = 0;
    double GraceSeconds = 10.0;
    double RetrySeconds = 0.25;
    double NextCollapseRetryAt = 0.0;
    FString LastCollapseFailure;
    uint64 LastCollapseFailureRevision = 0;
    FSFConstructionInstability Instability;
    FSFConstructionInstability DoorInstability;
    TWeakObjectPtr<ASFConstructionSandboxController> ConfirmationController;
    FIntVector ConfirmationCell = FIntVector::ZeroValue;
    uint64 ConfirmationRevision = 0;
    bool bReady = false;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    // Editor automation only; no RPC/config/console clock route. Runtime callers
    // always use world time. Friend tests may drive finite exact boundaries.
    double TestTimeOverride = -1.0;
    friend class FSFConstructionSupportLossTest;
    friend class FSFConstructionDoorAuthorityTest;
    friend class FSFConstructionDoorLifecycleTest;
    friend class FSFConstructionSnapshotRestoreTest;
    friend class FSFConstructionSandboxAuthorityTest;
    int32 TestFailAfterPreparedCount = INDEX_NONE;
#endif
};
