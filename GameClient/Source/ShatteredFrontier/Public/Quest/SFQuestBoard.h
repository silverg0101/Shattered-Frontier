#pragma once

#include "GameFramework/Actor.h"
#include "UI/SFUiWidgets.h"
#include "SFQuestBoard.generated.h"

class UStaticMeshComponent;
class UTextBlock;
class UWidget;
class APlayerController;
class USFQuestPilotComponent;

/** QST-02 Quest Board: a city's notice board (SM_SilverbrookPropNoticeBoard). A source, not an owner: no quest state lives here;
 *  it lists content quests whose offer names BoardId. Placed from the town layout on server and clients alike (not replicated). */
UCLASS()
class SHATTEREDFRONTIER_API ASFQuestBoard : public AActor
{
    GENERATED_BODY()
public:
    ASFQuestBoard();
    UPROPERTY(EditAnywhere, Category="Quest", meta=(ToolTip="board/<city>, as content/quests offers name it."))
    FString BoardId;
    /** "Silverbrook Notice Board" from board/silverbrook. */
    FString DisplayName() const;
    /** The local player's nearest board within reach and in front of the pawn, or null. */
    static ASFQuestBoard* FindInReach(const APlayerController* PC);
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
};

/** The quest window (WBP_QuestBoard, docs/blueprints/menus.md section 0): a WBP_WindowFrame (emblem icon_window_quests) with the quest list
 *  (WBP_KitRow rows in a WBP_ScrollList) beside the selected quest in the SidePanel (description, objectives with progress, rewards as item
 *  slots and gold) and the buttons in the Footer. Journal mode (J): active quests, Track toggle, hold-to-abandon. Board mode (interact at a
 *  board): every quest posted there, locked ones greyed with the reason, Accept. A window of USFWindowStack (Esc closes it); local presentation,
 *  requests go through the quest log's server RPCs.
 *  Binds (required): QuestList (VerticalBox in a WBP_ScrollList; C++ adds one WBP_KitRow per quest).
 *  Optional: Window (the frame; its title is the journal or the board), Detail (the right-hand column), QuestTitle, Summary, Reason, EmptyText,
 *  Objective0..2Row / Label / Count / Tick (looked up by name), Reward0..3 (WBP_ItemSlot), RewardGold (WBP_GoldDisplay, shown for a gold reward),
 *  NoReward, AcceptButton, TrackButton, AbandonButton (WBP_Button; Abandon is a hold button), Tooltip with TipIcon, TipTitle, TipRarity, TipType,
 *  TipQuantity, TipWeight, TipDescription (the item tooltip over a reward slot). */
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFQuestBoardPanel : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Quest/WBP_QuestBoard.WBP_QuestBoard_C");

    void OpenJournal(USFQuestPilotComponent* InLog);
    void OpenBoard(USFQuestPilotComponent* InLog, const FString& InBoardId, const FString& InTitle);
    void Close();
    bool IsOpen() const { return GetVisibility() != ESlateVisibility::Collapsed; }
    bool IsBoard() const { return !BoardId.IsEmpty(); }
    const FString& GetSelectedQuest() const { return SelectedQuest; }
    void Select(const FString& QuestId) { SelectedQuest = QuestId; Rebuild(); }
    int32 RowCount() const { return RowQuests.Num(); }
    /** Client tick from the quest subsystem: J toggles the journal, walking away closes a board visit, state changes redraw. */
    static void TickClient(APlayerController* Local, USFQuestPilotComponent* Log);
    /** Interact at a board: opens it; true when the key was used. */
    static bool TryOpenBoard(APlayerController* Local, ASFQuestBoard* ExactTarget = nullptr);
    /** Development: open a screen for captures ("journal" or "board"). */
    static void OpenForCapture(APlayerController* Local, const FString& Screen);

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> QuestList;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFWindowFrameWidget> Window;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Detail;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> QuestTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Summary;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Reason;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NoReward;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFGoldDisplayWidget> RewardGold;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> AcceptButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> TrackButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> AbandonButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Tooltip;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> TipIcon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipRarity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipType;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipQuantity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipWeight;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipDescription;

private:
    UFUNCTION() void HandleRowClicked(USFSelectableWidget* Source);
    UFUNCTION() void HandlePrimary(USFButtonWidget* Source);
    UFUNCTION() void HandleTrack(USFButtonWidget* Source);
    UFUNCTION() void HandleAbandon(USFButtonWidget* Source);
    void Show();
    void Rebuild();
    void UpdateTooltip();
    FString StateKey() const;
    struct FRow { FString QuestId, Section, Reason; bool bLocked = false; };
    TArray<FRow> ListRows() const;

    TWeakObjectPtr<USFQuestPilotComponent> Log;
    FString BoardId, SelectedQuest, LastKey;
    TArray<FString> RowQuests;
    TSubclassOf<USFKitRowWidget> RowClass;
    bool bFrozeMovement = false;
    bool bCaptureVisit = false;  // development captures: a board visit that does not end when no board is near
    static constexpr int32 MaxRows = 6;
    static constexpr int32 MaxObjectiveRows = 3;
    static constexpr int32 MaxRewardSlots = 4;
    UPROPERTY(Transient) TArray<TObjectPtr<USFKitRowWidget>> Rows;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> RewardSlots;
    TArray<FString> RewardIds;  // the item id of each shown reward slot (tooltip)
    TArray<int32> RewardCounts;
};
