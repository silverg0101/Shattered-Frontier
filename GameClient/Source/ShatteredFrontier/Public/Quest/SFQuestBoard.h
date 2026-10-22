#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "SFQuestBoard.generated.h"

class UBorder;
class UButton;
class UImage;
class UStaticMeshComponent;
class UTextBlock;
class UVerticalBox;
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

/** The quest book (16-quest-journal.png): left page lists quests, right page shows the selected one.
 *  Journal mode (J): active quests, Track toggle, hold-to-abandon. Board mode (interact at a board): every quest posted there,
 *  locked ones greyed with the reason, Accept / Close. Local presentation; requests go through the quest log's server RPCs. */
UCLASS()
class SHATTEREDFRONTIER_API USFQuestBoardPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void OpenJournal(USFQuestPilotComponent* InLog);
    void OpenBoard(USFQuestPilotComponent* InLog, const FString& InBoardId, const FString& InTitle);
    void Close();
    bool IsOpen() const { return GetVisibility() != ESlateVisibility::Collapsed; }
    bool IsBoard() const { return !BoardId.IsEmpty(); }
    const FString& GetSelectedQuest() const { return SelectedQuest; }
    void Select(const FString& QuestId) { SelectedQuest = QuestId; Rebuild(); }
    /** Client tick from the quest subsystem: J toggles the journal, Esc or walking away closes, state changes redraw. */
    static void TickClient(APlayerController* Local, USFQuestPilotComponent* Log);
    /** Interact at a board: opens it; true when the key was used. */
    static bool TryOpenBoard(APlayerController* Local);
    /** Development: open a screen for captures ("journal" or "board"). */
    static void OpenForCapture(APlayerController* Local, const FString& Screen);

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
    UFUNCTION() void HandlePrimary();
    UFUNCTION() void HandleSecondary();
    UFUNCTION() void HandleHoldStart();
    UFUNCTION() void HandleHoldEnd();
    UFUNCTION() void HandleRow0() { SelectRow(0); }
    UFUNCTION() void HandleRow1() { SelectRow(1); }
    UFUNCTION() void HandleRow2() { SelectRow(2); }
    UFUNCTION() void HandleRow3() { SelectRow(3); }
    UFUNCTION() void HandleRow4() { SelectRow(4); }
    UFUNCTION() void HandleRow5() { SelectRow(5); }
    void SelectRow(int32 Index);
    void Show();
    void Rebuild();
    FString StateKey() const;
    struct FRow { FString QuestId, Section, Reason; bool bLocked = false; };
    TArray<FRow> ListRows() const;

    TWeakObjectPtr<USFQuestPilotComponent> Log;
    FString BoardId, SelectedQuest, LastKey;
    TArray<FString> RowQuests;
    double HoldStartedAt = -1.0;
    float HoldFraction = 0.f;
    static constexpr int32 MaxRows = 6;
    static constexpr int32 MaxObjectiveRows = 3;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PageTitle;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> List;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> RowButtons;
    UPROPERTY(Transient) TObjectPtr<UWidget> Detail;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> QuestTitle;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Summary;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> Reason;
    UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> ObjectiveRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ObjectiveLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ObjectiveCounts;
    UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> ObjectiveTicks;
    UPROPERTY(Transient) TArray<TObjectPtr<UImage>> RewardIcons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RewardTexts;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NoReward;
    UPROPERTY(Transient) TObjectPtr<UButton> PrimaryButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PrimaryLabel;
    UPROPERTY(Transient) TObjectPtr<UWidget> PrimaryRoot;
    UPROPERTY(Transient) TObjectPtr<UButton> SecondaryButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> SecondaryLabel;
    UPROPERTY(Transient) TObjectPtr<UWidget> AbandonRoot;
    UPROPERTY(Transient) TObjectPtr<UButton> AbandonButton;
    UPROPERTY(Transient) TObjectPtr<UWidget> HoldHint;
    UPROPERTY(Transient) TObjectPtr<UImage> HoldRing;
    UPROPERTY(Transient) TObjectPtr<UWidget> Emblem;
};
