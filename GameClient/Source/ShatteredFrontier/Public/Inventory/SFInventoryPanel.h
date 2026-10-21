#pragma once

#include "Blueprint/UserWidget.h"
#include "SFInventoryPanel.generated.h"

class UProgressBar;
class UTextBlock;
class USFGatheringPilotComponent;

// Local-only view of the owner's replicated pilot inventory; it sends nothing to the server.
UCLASS()
class SHATTEREDFRONTIER_API USFInventoryPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 GridColumns = 5;
    static constexpr int32 GridRows = 5;

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
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotNames;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> SlotCounts;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WeightText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> WeightPercent;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> WeightBar;
};
