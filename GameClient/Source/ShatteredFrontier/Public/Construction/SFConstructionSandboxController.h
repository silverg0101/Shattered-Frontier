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
private:
    void ToggleBuild();
    void SelectCube();
    void SelectSlab();
    void SelectSlope();
    void Rotate();
    void Place();
    void Remove();
    void ConfirmRemoval();
    void CancelRemoval();
    void Commit(bool bRemove);
    TWeakObjectPtr<ASFConstructionSandboxState> State;
    FSFConstructionSandboxIntent PreviewIntent;
    FSFConstructionSandboxIntent PendingRemoval;
    bool bPendingRemoval = false;
    FString RemovalPrompt;
    FIntVector RemovalCell = FIntVector::ZeroValue;
    int32 Shape = 0;
    int32 Rotation = 0;
    bool bBuilding = true;
    bool bHasPreview = false;
    bool bHasRemoval = false;
    FString LastResult;
};
