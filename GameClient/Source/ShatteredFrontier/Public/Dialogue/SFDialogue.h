#pragma once

#include "UI/SFUiWidgets.h"
#include "GameFramework/Actor.h"
#include "SFDialogue.generated.h"

class UImage;
class USkeletalMeshComponent;
class UTextBlock;

// ---- Pure model: one conversation tree per speaker, authored in content/dialogue/<file>.json (CTL-02 NPC dialogue). ----

struct FSFDialogueChoice { FString Label, Next, Action; };  // Action: "" (go to Next), "end" or "accept"
struct FSFDialogueNode { FString Id, Line; TArray<FSFDialogueChoice> Choices; };
// QST-02: the offer names a quest record (content/quests); title, summary, art, objectives and rewards live there.
struct FSFDialogueOffer { FString QuestId, AcceptedNode; };

struct SHATTEREDFRONTIER_API FSFDialogueTree
{
    static constexpr int32 MaxChoices = 4;  // keys 1-4 and the four response rows
    static constexpr int32 MaxLineChars = 100;  // three wrapped lines of 20.6 pt EB Garamond at 320 px
    FString Speaker, Portrait, Start;
    TMap<FString, FSFDialogueNode> Nodes;
    bool bHasOffer = false;
    FSFDialogueOffer Offer;
    /** Parses and validates (start and every target exist, 1-4 choices per node); false with Error naming the first problem. */
    static bool Parse(const FString& Json, FSFDialogueTree& Out, FString& Error);
    /** Reads a file name in content/dialogue (no path separators). */
    static bool Load(const FString& File, FSFDialogueTree& Out, FString& Error);
};

enum class ESFDialogueStep : uint8 { Stay, Closed, Accepted, Declined, Learn };

// Where a conversation stands. Choices and the offer buttons advance it; it never reads the world.
struct SHATTEREDFRONTIER_API FSFDialogueSession
{
    TSharedPtr<const FSFDialogueTree> Tree;
    FString NodeId;
    bool bAccepted = false;
    FString LastAction;

    /** bAlreadyAccepted: the speaker's quest is held or finished, so the conversation opens on its accepted line. */
    void Begin(TSharedPtr<const FSFDialogueTree> InTree, bool bAlreadyAccepted = false)
    {
        Tree = InTree; bAccepted = bAlreadyAccepted && InTree.IsValid() && InTree->bHasOffer;
        NodeId = !InTree.IsValid() ? FString() : bAccepted ? InTree->Offer.AcceptedNode : InTree->Start;
        LastAction.Reset();
    }
    const FSFDialogueNode* Node() const { return Tree.IsValid() ? Tree->Nodes.Find(NodeId) : nullptr; }
    ESFDialogueStep Choose(int32 Index);
    ESFDialogueStep Accept();
    ESFDialogueStep Decline() { return ESFDialogueStep::Declined; }
};

// A speaker placed in a level (or spawned for review): a mannequin that turns to the player and opens its tree on interact.
UCLASS()
class SHATTEREDFRONTIER_API ASFDialogueNpc : public AActor
{
    GENERATED_BODY()
public:
    ASFDialogueNpc();
    UPROPERTY(EditAnywhere, Category="Dialogue", meta=(ToolTip="File name in content/dialogue."))
    FString DialogueFile = TEXT("millwarden_oswin.json");
    UPROPERTY(EditAnywhere, Category="Dialogue", meta=(ClampMin="100.0", ClampMax="1000.0", Units="cm", ToolTip="How close the player must stand to talk."))
    float TalkRangeCm = 350.f;
    // The speaker's head, which the framing camera aims at.
    FVector GetSpeakerAnchor() const;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Body;
};

// The dialogue window (WBP_Dialogue, docs/blueprints/menus.md section 0): a WBP_WindowFrame (emblem icon_window_quests, title = the speaker) with the
// speaker's portrait and line, a separator and the response options (WBP_KitRow rows, keys 1-4 in the cell), and the offered quest beside it in
// the SidePanel (art, summary, objectives, reward item slots, Accept / Decline). A window of USFWindowStack; Esc ends the conversation.
// Local presentation only.
// Binds (required): Choices (VerticalBox; C++ adds the four response rows).
// Optional: Window, Portrait (Image), LineText, OfferColumn (the offer card; collapsed without an offer), OfferArt (Image), OfferTitle, OfferSummary,
// Objective0..2Row / Label / Count (by name), Reward0..3 (WBP_ItemSlot), AcceptButton, DeclineButton (WBP_Button), Tooltip with TipIcon, TipTitle,
// TipRarity, TipType, TipQuantity, TipWeight, TipDescription.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFDialoguePanel : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Dialogue/WBP_Dialogue.WBP_Dialogue_C");

    /** Quest: the offered quest record (null hides the offer column); bOffered false when it is held, finished or locked. */
    void Open(TSharedPtr<const FSFDialogueTree> InTree, const struct FSFQuestDefinition* Quest = nullptr, bool bOffered = true);
    // Keys 1-4 and the response rows; false when the index is not a response.
    bool ChooseIndex(int32 Index);
    // Fired when the conversation ends (Farewell, Decline, Esc) and when the offer is accepted.
    DECLARE_DELEGATE_OneParam(FStepDelegate, ESFDialogueStep);
    FStepDelegate OnStep;
    const FSFDialogueSession& GetSession() const { return Session; }
    int32 ChoiceCount() const { return ChoiceRows.Num(); }
    USFKitRowWidget* GetChoiceRow(int32 Index) const { return ChoiceRows.IsValidIndex(Index) ? ChoiceRows[Index].Get() : nullptr; }

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> Choices;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFWindowFrameWidget> Window;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Portrait;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LineText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> OfferColumn;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> OfferArt;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OfferTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OfferSummary;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> AcceptButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> DeclineButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Tooltip;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> TipIcon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipRarity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipType;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipQuantity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipWeight;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipDescription;

private:
    UFUNCTION() void HandleChoiceClicked(USFSelectableWidget* Source);
    UFUNCTION() void HandleAccept(USFButtonWidget* Source);
    UFUNCTION() void HandleDecline(USFButtonWidget* Source);
    void Apply(ESFDialogueStep Step);
    void Refresh();
    void UpdateTooltip();

    FSFDialogueSession Session;
    TSubclassOf<USFKitRowWidget> RowClass;
    UPROPERTY(Transient) TArray<TObjectPtr<USFKitRowWidget>> ChoiceRows;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> RewardSlots;
    TArray<FString> RewardIds;
    TArray<int32> RewardCounts;
};
