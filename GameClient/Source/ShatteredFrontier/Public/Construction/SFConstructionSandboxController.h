#pragma once

#include "Interaction/SFE1PlayerController.h"
#include "Construction/SFConstructionSandboxState.h"
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
private:
    friend class ASFConstructionSandboxState;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    friend class FSFConstructionSnapshotRestoreTest;
#endif
    void ToggleBuild();
    void SelectCube();
    void SelectSlab();
    void SelectSlope();
    void SelectDoor();
    void Interact();
    void Rotate();
    void Place();
    void Remove();
    void ConfirmRemoval();
    void CancelRemoval();
    void Commit(bool bRemove);
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
};
