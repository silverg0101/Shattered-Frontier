#pragma once

#include "GameFramework/Actor.h"
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
};

struct FSFConstructionSandboxMember
{
    int32 Shape = 0;
    int32 Rotation = 0;
    uint64 Generation = 0;
    TWeakObjectPtr<ASFConstructionSandboxPreview> Projection;
};

// Transient standalone authority checkpoint. No support solver, persistence,
// resource economy, authentication, replication or normal E1 mutation route.
UCLASS(NotBlueprintable, Transient)
class SHATTEREDFRONTIER_API ASFConstructionSandboxState : public AActor
{
    GENERATED_BODY()
public:
    ASFConstructionSandboxState();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    FString Evaluate(const ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent) const;
    bool Apply(ASFConstructionSandboxController* Controller, const FSFConstructionSandboxIntent& Intent, FString& Result);
    uint64 GetRevision() const { return Revision; }
    int32 GetMemberCount() const { return Members.Num(); }
    bool FindCell(const AActor* Projection, FIntVector& OutCell) const;
    float GetReachCentimetres() const { return ReachCentimetres; }
    static FVector CellCorner(const FIntVector& Cell);
private:
    TMap<FIntVector, FSFConstructionSandboxMember> Members;
    uint64 Revision = 0;
    float ReachCentimetres = 0;
    int32 MaximumMembers = 0;
    bool bReady = false;
};
