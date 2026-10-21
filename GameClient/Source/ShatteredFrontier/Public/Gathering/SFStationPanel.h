#pragma once

#include "Blueprint/UserWidget.h"
#include "Framework/SFHudStyle.h"
#include "SFStationPanel.generated.h"

class UBorder;
class UImage;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class USFGatheringPilotComponent;
class ASFGatheringPilotNode;

// v5 station panel (docs/Game-Design/concept-art/UI/v5/crafting-station.png): the aimed station's recipes with the
// selected one in moss, its output, have / need rows, craft time, the Craft action with the interact key, and the
// running craft as the queue. Read-only presentation of replicated pilot state; crafting stays on the interact key.
UCLASS()
class SHATTEREDFRONTIER_API USFStationPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    // Null hides the panel. Rebuilds the recipe list only when the station or its selected recipe changes.
    void Present(USFGatheringPilotComponent* Pilot, const ASFGatheringPilotNode* Station);
    // item/rough_timber -> rough_timber (the T_Icon_ stem).
    static FString IconStem(const FString& DefinitionId);

protected:
    virtual void NativeOnInitialized() override;

private:
    void ShowCraftKey(const FString& Key);
    void Rebuild(USFGatheringPilotComponent* Pilot, const ASFGatheringPilotNode* Station, const FString& RecipeId);

    UPROPERTY(Transient) TObjectPtr<UTextBlock> Title;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> RecipeList;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> OutputName;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> OutputText;
    UPROPERTY(Transient) TObjectPtr<UImage> OutputIcon;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Ingredients;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CraftTime;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CraftKey;
    UPROPERTY(Transient) TObjectPtr<UImage> CraftCap;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> QueueTitle;
    UPROPERTY(Transient) TObjectPtr<UBorder> QueueRow;
    UPROPERTY(Transient) TObjectPtr<UImage> QueueIcon;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> QueueName;
    UPROPERTY(Transient) TObjectPtr<UProgressBar> QueueBar;
    SFHudStyle::FSFFramedBar QueueFramed;  // widgets owned by WidgetTree; Root null when the kit is missing
    UPROPERTY(Transient) TObjectPtr<UTextBlock> QueuePercent;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> QueueEmpty;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> HaveTexts;
    UPROPERTY(Transient) TArray<TObjectPtr<UImage>> Glyphs;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> Underlines;
    TArray<FString> NeedNames;
    TArray<int32> NeedAmounts;
    FString ShownKey;  // station kind + recipe id the list was built for
    uint32 QueueOperation = 0;
    double QueueTotalSeconds = 0;
};
