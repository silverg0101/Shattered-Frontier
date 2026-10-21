#pragma once

#include "Blueprint/UserWidget.h"
#include "SFInventoryPanel.generated.h"

class UTextBlock;
class USFGatheringPilotComponent;

// Local-only view of the owner's replicated pilot inventory; it sends nothing to the server.
UCLASS()
class SHATTEREDFRONTIER_API USFInventoryPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot);
    // Merges stacks that share a name, drops empty entries and sorts by name.
    static TArray<TPair<FText,int32>> BuildRows(const TArray<TPair<FText,int32>>& Items);
    static FString FormatRows(const TArray<TPair<FText,int32>>& Rows);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void Refresh();

    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Rows;
};
