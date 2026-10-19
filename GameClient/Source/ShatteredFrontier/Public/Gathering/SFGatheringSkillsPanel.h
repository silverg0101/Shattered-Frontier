#pragma once

#include "Blueprint/UserWidget.h"
#include "Gathering/SFGatheringProgression.h"
#include "SFGatheringSkillsPanel.generated.h"

class UButton;
class UTextBlock;
class USFGatheringPilotComponent;

UCLASS()
class SHATTEREDFRONTIER_API USFGatheringSkillsPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION() void TrainLogging();
    UFUNCTION() void HoldLogging();
    UFUNCTION() void AllowLoggingDecrease();
    UFUNCTION() void TrainMining();
    UFUNCTION() void HoldMining();
    UFUNCTION() void AllowMiningDecrease();
    void SetLock(ESFGatheringSkill Skill, ESFGatheringSkillLock Lock);
    void Refresh();

    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LoggingStatus;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> MiningStatus;
    UPROPERTY(Transient) TObjectPtr<UButton> LoggingButtons[3];
    UPROPERTY(Transient) TObjectPtr<UButton> MiningButtons[3];
};
