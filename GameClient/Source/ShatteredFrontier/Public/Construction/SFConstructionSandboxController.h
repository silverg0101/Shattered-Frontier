#pragma once

#include "Interaction/SFE1PlayerController.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Construction/SFConstructionSaveStore.h"
#include "SFConstructionSandboxController.generated.h"

// Input exists only on this opt-in controller; inherited movement/jump/E remain.
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionSandboxController : public ASFE1PlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaSeconds) override;
    bool IsDoorSelected() const { return bDoorSelected; }
    bool IsBuildingEnabled() const { return bBuilding; }
    int32 GetDoorYawQuarterTurns() const { return DoorYaw; }
    const FString& GetLastConstructionResult() const { return LastResult; }
    uint64 GetLastPublishedGeneration() const { return LastPublishedGeneration; }
    uint64 GetLastLoadedGeneration() const { return LastLoadedGeneration; }
    uint64 GetLastLoadedSourceRevision() const { return LastLoadedSourceRevision; }
    bool IsLineMode() const { return bLineMode; }
    bool HasLineSeed() const { return bHasLineSeed; }
    const TArray<FIntVector>& GetLineGhostCells() const { return LineGhostCells; }
    bool IsLinePreviewAllowed() const { return bLinePreviewAllowed; }
    const FString& GetLinePreviewReason() const { return LinePreviewReason; }
private:
    friend class ASFConstructionSandboxState;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    friend class FSFConstructionSnapshotRestoreTest;
    friend class FSFConstructionLineControlsTest;
#endif
    void ToggleBuild();
    void ToggleLine();
    void SelectCube();
    void SelectSlab();
    void SelectSlope();
    void SelectDoor();
    void Interact();
    void SaveDevelopment();
    void LoadDevelopment();
    void Rotate();
    void Place();
    void Remove();
    void ConfirmRemoval();
    void CancelRemoval();
    void Commit(bool bRemove);
    void CommitLine();
    TWeakObjectPtr<ASFConstructionSandboxState> State;
    FSFConstructionSandboxIntent PreviewIntent;
    FSFConstructionDoorIntent DoorPreviewIntent;
    FSFConstructionSandboxIntent PendingRemoval;
    bool bPendingRemoval = false;
    FString RemovalPrompt;
    FIntVector RemovalCell = FIntVector::ZeroValue;
    int32 Shape = 0;
    int32 Rotation = 0;
    bool bBuilding = true;
    bool bHasPreview = false;
    bool bHasRemoval = false;
    bool bDoorSelected = false;
    bool bHasDoorPreview = false;
    bool bHasDoorRemoval = false;
    FIntVector DoorRemovalOrigin = FIntVector::ZeroValue;
    int32 DoorYaw = 0;
    FString LastResult;
    TUniquePtr<FSFConstructionSaveStore> SaveStore;
    uint64 LastPublishedGeneration = 0;
    uint64 LastLoadedGeneration = 0;
    uint64 LastLoadedSourceRevision = 0;
    bool bLineMode = false;
    bool bHasLineSeed = false;
    bool bHasLineEndpoint = false;
    bool bLinePreviewAllowed = false;
    FIntVector LineSeed = FIntVector::ZeroValue;
    uint64 LineSeedRevision = 0;
    FSFConstructionLineIntent LinePreviewIntent;
    TArray<FIntVector> LineGhostCells;
    FString LinePreviewReason;
};
