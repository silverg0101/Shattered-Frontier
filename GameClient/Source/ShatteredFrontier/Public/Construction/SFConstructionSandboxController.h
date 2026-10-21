#pragma once

#include "Framework/SFPlayerController.h"
#include "Construction/SFConstructionSandboxState.h"
#include "Construction/SFConstructionSaveStore.h"
#include "SFConstructionSandboxController.generated.h"

class USFConstructionSandboxPalette;
enum class ESFConstructionPaletteAction : uint8;

// Input exists only on this opt-in controller; inherited movement/jump/E remain.
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFConstructionSandboxController : public ASFPlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void ClientRestart_Implementation(APawn* NewPawn) override;
    bool IsDoorSelected() const { return bDoorSelected; }
    bool IsBuildingEnabled() const { return bBuilding; }
    int32 GetDoorYawQuarterTurns() const { return DoorYaw; }
    const FString& GetLastConstructionResult() const { return LastResult; }
    uint64 GetLastPublishedGeneration() const { return LastPublishedGeneration; }
    uint64 GetLastLoadedGeneration() const { return LastLoadedGeneration; }
    uint64 GetLastLoadedSourceRevision() const { return LastLoadedSourceRevision; }
    bool IsLineMode() const { return bLineMode; }
    bool IsDemolitionMode() const { return bDemolitionMode; }
    bool HasLineSeed() const { return bHasLineSeed; }
    const TArray<FIntVector>& GetLineGhostCells() const { return LineGhostCells; }
    bool IsLinePreviewAllowed() const { return bLinePreviewAllowed; }
    const FString& GetLinePreviewReason() const { return LinePreviewReason; }
protected:
    // GM-01: this standalone fixture keeps its own build keys, palette and cursor.
    virtual bool HostsBuildMode() const override { return false; }
private:
    friend class ASFConstructionSandboxState;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    friend class FSFConstructionSnapshotRestoreTest;
    friend class FSFConstructionLineControlsTest;
    friend class FSFConstructionCursorControlsTest;
#endif
    bool GetConstructionRay(FVector& Start, FVector& Direction) const;
    bool CanUseConstructionControls() const;
    bool CanCommitCursorSample();
    void UpdateConstructionPreview();
    void ApplyCursorMode();
    void ClearCameraSample();
    void BeginCameraLook();
    void EndCameraLook();
    void CommitCapturedRemoval(FSFConstructionSandboxIntent Intent);
    void ApplicationActivationChanged(bool bActive);
    void PaletteAction(ESFConstructionPaletteAction Action);
    void ToggleDemolition();
    void TogglePlacementPlane();
    void LayerDown();
    void LayerUp();
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
    FVector DoorRemovalPoint = FVector::ZeroVector;
    bool bDoorRemovalUsesVirtualPoint = false;
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
    UPROPERTY(Transient) TObjectPtr<USFConstructionSandboxPalette> SandboxPalette;
    FDelegateHandle ActivationHandle;
    bool bCameraLookHeld = false;
    bool bOwnsLookSuppression = false;
    bool bApplicationActive = true;
    bool bDispatchingPaletteAction = false;
    bool bDemolitionMode = false;
    bool bLayerPlacement = false;
    int32 PlacementLayer = 0; // Existing plot lattice Z0..19, not a balance setting.
    bool bHasCursorSample = false;
    FVector SampleRayStart = FVector::ZeroVector;
    FVector SampleRayDirection = FVector::ForwardVector;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Finite dev fixtures supply rays, never operating-system mouse movement.
    bool bScenarioCursor = false;
    bool bSyntheticRayOverride = false;
    bool bSyntheticPointerOverPalette = false;
    FVector SyntheticRayStart = FVector::ZeroVector;
    FVector SyntheticRayDirection = FVector::ForwardVector;
#endif
};
