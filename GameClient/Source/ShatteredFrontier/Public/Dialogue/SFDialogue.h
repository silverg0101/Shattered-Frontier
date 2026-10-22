#pragma once

#include "Blueprint/UserWidget.h"
#include "GameFramework/Actor.h"
#include "SFDialogue.generated.h"

class UBorder;
class UButton;
class UImage;
class USkeletalMeshComponent;
class UTextBlock;
class UVerticalBox;
class USizeBox;

// ---- Pure model: one conversation tree per speaker, authored in content/dialogue/<file>.json (CTL-02 NPC dialogue). ----

struct FSFDialogueChoice { FString Label, Next, Action; };  // Action: "" (go to Next), "end" or "accept"
struct FSFDialogueNode { FString Id, Line; TArray<FSFDialogueChoice> Choices; };
struct FSFDialogueObjective { FString Label; int32 Need = 0; };
struct FSFDialogueOffer
{
    FString Title, Summary, ArtPath, AcceptedNode;
    TArray<FSFDialogueObjective> Objectives;
    int32 RewardXp = 0, RewardCoin = 0;
};

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

enum class ESFDialogueStep : uint8 { Stay, Closed, Accepted, Declined };

// Where a conversation stands. Choices and the offer buttons advance it; it never reads the world.
struct SHATTEREDFRONTIER_API FSFDialogueSession
{
    TSharedPtr<const FSFDialogueTree> Tree;
    FString NodeId;
    bool bAccepted = false;

    void Begin(TSharedPtr<const FSFDialogueTree> InTree) { Tree = InTree; NodeId = InTree.IsValid() ? InTree->Start : FString(); bAccepted = false; }
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

// The v5 dialogue board (10-npc-dialogue): centre window x26-74% y14-78% over the dimmed HUD. Local presentation only.
UCLASS()
class SHATTEREDFRONTIER_API USFDialoguePanel : public UUserWidget
{
    GENERATED_BODY()
public:
    void Open(TSharedPtr<const FSFDialogueTree> InTree);
    // Keys 1-4 and the response rows; false when the index is not a response.
    bool ChooseIndex(int32 Index);
    // Fired when the conversation ends (Farewell, Decline, Esc) and when the offer is accepted.
    DECLARE_DELEGATE_OneParam(FStepDelegate, ESFDialogueStep);
    FStepDelegate OnStep;
    const FSFDialogueSession& GetSession() const { return Session; }

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION() void HandleChoice0() { ChooseIndex(0); }
    UFUNCTION() void HandleChoice1() { ChooseIndex(1); }
    UFUNCTION() void HandleChoice2() { ChooseIndex(2); }
    UFUNCTION() void HandleChoice3() { ChooseIndex(3); }
    UFUNCTION() void HandleAccept();
    UFUNCTION() void HandleDecline();
    void Apply(ESFDialogueStep Step);
    void Refresh();
    void StyleRows();

    FSFDialogueSession Session;
    int32 Selected = 0;
    int32 StyledSelected = -1;
    FVector2D LastMouse = FVector2D(-1.f, -1.f);
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> LineText;
    UPROPERTY(Transient) TObjectPtr<UImage> PortraitImage;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> RowButtons;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RowLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> RowRoots;
    UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> RowPlates;
    UPROPERTY(Transient) TObjectPtr<UWidget> OfferColumn;
    UPROPERTY(Transient) TObjectPtr<UImage> OfferArt;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> OfferTitle;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> OfferSummary;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ObjectiveLabels;
    UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ObjectiveCounts;
    UPROPERTY(Transient) TArray<TObjectPtr<UWidget>> ObjectiveRows;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> XpText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> CoinText;
    UPROPERTY(Transient) TObjectPtr<UButton> AcceptButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> AcceptLabel;
};
