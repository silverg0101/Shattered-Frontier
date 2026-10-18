#pragma once

#include "GameFramework/Actor.h"
#include "Construction/SFConstructionInstability.h"
#include "Construction/SFConstructionSupport.h"
#include "SFConstructionSandboxState.generated.h"

class ASFConstructionSandboxController;
class ASFConstructionSandboxPreview;

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
    bool PreviewRemoval(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, TArray<FIntVector>& Affected, FString& Result);
    void CancelRemoval(const ASFConstructionSandboxController* Controller);
    FString GetInstabilitySummary() const;
    uint64 GetRevision() const { return Revision; }
    int32 GetMemberCount() const { return Members.Num(); }
    bool FindCell(const AActor* Projection, FIntVector& OutCell) const;
    float GetReachCentimetres() const { return ReachCentimetres; }
    static FVector CellCorner(const FIntVector& Cell);
private:
    FString ValidateIntent(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent) const;
    FString ValidateEnvironment() const;
    FString SolveSupport(const TMap<FIntVector, FSFConstructionSandboxMember>& Snapshot, TArray<FSFConstructionSupportResult>& Results) const;
    FString CheckSupport(const FSFConstructionSandboxIntent& Intent, FString* SupportHint) const;
    bool CommitCandidate(TMap<FIntVector, FSFConstructionSandboxMember> Candidate, const TArray<FIntVector>& ChangedCells, double Now, FString& Result);
    void ProcessDeadlines(double Now);
    double AuthorityTime() const;
    TMap<FIntVector, FSFConstructionSandboxMember> Members;
    uint64 Revision = 0;
    float ReachCentimetres = 0;
    int32 MaximumMembers = 0;
    double GraceSeconds = 10.0;
    double RetrySeconds = 0.25;
    double NextCollapseRetryAt = 0.0;
    FString LastCollapseFailure;
    uint64 LastCollapseFailureRevision = 0;
    FSFConstructionInstability Instability;
    TWeakObjectPtr<ASFConstructionSandboxController> ConfirmationController;
    FIntVector ConfirmationCell = FIntVector::ZeroValue;
    uint64 ConfirmationRevision = 0;
    bool bReady = false;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    // Editor automation only; no RPC/config/console clock route. Runtime callers
    // always use world time. Friend tests may drive finite exact boundaries.
    double TestTimeOverride = -1.0;
    friend class FSFConstructionSupportLossTest;
#endif
};
