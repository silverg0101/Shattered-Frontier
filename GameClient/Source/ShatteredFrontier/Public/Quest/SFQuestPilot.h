#pragma once

#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DeveloperSettings.h"
#include "Services/SFGameplayServicesClient.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFQuestPilot.generated.h"

class APlayerController;
class UTextBlock;
class UVerticalBox;

// ---- Pure model: mirrors BackendServices/QuestContent (same JSON, same validation and progression rules). ----

struct FSFQuestObjective { FString Key, Kind, TargetId, NodeId; bool bHasNode = false; int32 Count = 0; };
struct FSFQuestRewardItem { FString DefinitionId; int64 Quantity = 0; };
struct FSFQuestStep { FString Id; TArray<FString> Prerequisites; TArray<FSFQuestObjective> Objectives; TArray<FString> Rewards; TArray<FSFQuestRewardItem> RewardItems; };
// QST-02 offer block: where a quest is posted (boards) and who gives it (giver_npc), and what it needs first.
struct FSFQuestOffer { FString Title, Summary, Art, GiverNpc; TArray<FString> Boards, RequiresCompleted; };

struct SHATTEREDFRONTIER_API FSFQuestDefinition
{
    FString Id; int32 Revision = 0; FString ZoneId; TArray<FSFQuestStep> Steps;
    bool bHasOffer = false; FSFQuestOffer Offer;
    FString Title() const;
    /** Every reward line of the quest, in step order ("4 Rough Timber"). */
    TArray<FString> RewardLines() const;
    /** Parses and validates; false with Error naming the first problem. */
    static bool Parse(const FString& Json, FSFQuestDefinition& Out, FString& Error);
    static bool Load(const FString& Path, FSFQuestDefinition& Out, FString& Error);
};

enum class ESFQuestApply : uint8 { Accepted, Duplicate, Mismatch, Completed };

struct SHATTEREDFRONTIER_API FSFQuestProgress
{
    int32 StepIndex = 0;
    TMap<FString,int32> Counts;
    TSet<FString> Operations;
    TSet<FString> GrantedSteps;
    bool IsComplete(const FSFQuestDefinition& Quest) const { return StepIndex >= Quest.Steps.Num(); }
    /** The active step's unfilled objective this outcome counts for, or null. */
    const FSFQuestObjective* Wants(const FSFQuestDefinition& Quest, const FString& Kind, const FString& TargetId, const FString& NodeId) const;
    /** Applies one server-observed outcome. Reward bundle ids are appended to NewRewards once per step. */
    ESFQuestApply Apply(const FSFQuestDefinition& Quest, const FString& Kind, const FString& TargetId, const FString& NodeId,
        int32 Quantity, const FString& OperationId, TArray<FString>& NewRewards);
};

enum class ESFQuestOfferState : uint8 { Available, Active, Completed, Locked };

/** Quest JSON files under content/quests, loaded once per process on server and client. */
struct SHATTEREDFRONTIER_API FSFQuestCatalog
{
    TArray<TSharedPtr<const FSFQuestDefinition>> Quests;  // file name order
    TSharedPtr<const FSFQuestDefinition> Find(const FString& Id) const;
    static const FSFQuestCatalog& Get();
    /** "Gather Oak Logs", "Talk to Millwarden Oswin": kind verb + the item or NPC display name. */
    static FString ObjectiveLabel(const FSFQuestObjective& Objective);
    /** Board/NPC filter (Quest Model section 3.2): Locked with Reason "Finish <title> first" when a prerequisite is not completed. */
    ESFQuestOfferState OfferState(const FSFQuestDefinition& Quest, const TSet<FString>& Active, const TSet<FString>& Completed, FString* Reason = nullptr) const;
    /** Quests posted on BoardId, content order, at most Max. */
    TArray<TSharedPtr<const FSFQuestDefinition>> ForBoard(const FString& BoardId, int32 Max) const;
    /** The quest NpcId (npc/<dialogue stem>) gives, or null. */
    TSharedPtr<const FSFQuestDefinition> ForGiver(const FString& NpcId) const;
};

// ---- Owner-only replicated view; the client renders labels without the server's progress model. ----

USTRUCT()
struct FSFQuestLine
{
    GENERATED_BODY()
    UPROPERTY() FString Label;
    UPROPERTY() int32 Count = 0;
    UPROPERTY() int32 Target = 0;
};

USTRUCT()
struct FSFQuestView
{
    GENERATED_BODY()
    UPROPERTY() FString QuestId;
    UPROPERTY() FString Title;
    UPROPERTY() FString Step;
    UPROPERTY() int32 StepNumber = 0;
    UPROPERTY() int32 StepTotal = 0;
    UPROPERTY() bool bComplete = false;
    UPROPERTY() bool bRewardWaiting = false;  // completed, but the reward is not in the backpack yet (full backpack, host down)
    UPROPERTY() TArray<FSFQuestLine> Lines;
    UPROPERTY() TArray<FString> RewardLines;  // committed completion payout, e.g. "3 Boar Hide"
};

// Release defaults (QST-02 typed configuration; release authors, next world start).
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Quests (Release Only)"))
class SHATTEREDFRONTIER_API USFQuestPilotSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Quest", meta=(ToolTip="GM-01: quests run in their zone (Silverbrook). Default on; next world start."))
    bool bEnabled = true;
    UPROPERTY(Config, EditAnywhere, Category="Quest", meta=(ClampMin="1.0", ClampMax="10.0", Units="m", ToolTip="How close the player walks to a talk_resident objective's resident to count it."))
    float TalkRangeMetres = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Quest", meta=(ClampMin="1.0", ClampMax="10.0", Units="m", ToolTip="Server distance check for opening a quest board and accepting from it."))
    float BoardReachM = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Quest", meta=(ClampMin="1.0", ClampMax="10.0", Units="m", ToolTip="Server distance check for accepting a quest from its giver NPC and for talk_npc objectives."))
    float OfferReachM = 4.0f;
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="1.0", ClampMax="60.0", Units="s", ToolTip="How often a waiting reward (full backpack, host down) is checked again."))
    float RewardRetrySeconds = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Quest", meta=(ClampMin="1", ClampMax="30", ToolTip="A board lists at most this many quests, content order."))
    int32 MaxQuestsPerBoard = 12;
    float BoardReachCm() const { return FMath::Clamp(BoardReachM, 1.f, 10.f) * 100.f; }
    float OfferReachCm() const { return FMath::Clamp(OfferReachM, 1.f, 10.f) * 100.f; }
};

/** On a player controller: the character's quest log. Server owns progress; the owning client receives Views. */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API USFQuestPilotComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFQuestPilotComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** Server only: a committed outcome; counts for every active quest whose current step wants it. */
    bool Observe(const FString& Kind, const FString& TargetId, const FString& NodeId, const FString& OperationId);
    // INT-06: fill the gather grant's follow-up receipt (progression always; quest evidence for the first quest that wants it).
    bool CaptureGatherReceipt(struct FSFGameplayRequest& Receipt, const FString& TargetId, const FString& NodeId);
    // Kind of the first unfinished objective of the first active quest ("gather", "craft", ...), or empty when none.
    FString ActiveObjectiveKind() const;
    const TArray<FSFQuestView>& GetViews() const { return Views; }
    const FSFQuestView* FindView(const FString& QuestId) const { return Views.FindByPredicate([&](const FSFQuestView& V) { return V.QuestId == QuestId; }); }
    /** Active and completed quest ids, from the replicated state (valid on server and owning client). */
    TSet<FString> ActiveIds() const;
    TSet<FString> CompletedIds() const { return TSet<FString>(Completed); }
    /** Server, INT-05 restore and refresh: the Progression quest rows replace session progress. */
    void Restore(const TArray<FSFGameplayResponse::FQuestState>& Rows, int64 InProgressionRevision);
    bool HasPendingServiceWork() const { return bServiceBusy || !ServiceQueue.IsEmpty(); }

    // Client requests. SourceId is the board ("board/<city>") or the giver NPC ("npc/<dialogue stem>") the player stands at.
    UFUNCTION(Server, Reliable) void ServerAcceptQuest(const FString& QuestId, const FString& SourceId);
    UFUNCTION(Server, Reliable) void ServerAbandonQuest(const FString& QuestId);
    /** The player opened NpcId's dialogue: counts a talk_npc objective (the turn-in) when the server sees the NPC within reach. */
    UFUNCTION(Server, Reliable) void ServerTalkedTo(const FString& NpcId);
    /** Server, testable: why an accept from SourceId is refused ("" = allowed); checks the source offers it, reach, state and prerequisites. */
    FString CheckAccept(const FString& QuestId, const FString& SourceId) const;
    /** Last accept/abandon result the server sent ("accepted", "abandoned" or a refusal reason), for the panels. */
    const FString& GetLastResult() const { return LastResult; }
    // Client-local tracking (journal Track toggle); the tracker shows up to three tracked quests.
    bool IsTracked(const FString& QuestId) const { return !Untracked.Contains(QuestId); }
    void SetTracked(const FString& QuestId, bool bTracked) { if (bTracked) { Untracked.Remove(QuestId); } else { Untracked.Add(QuestId); } }
    static constexpr int32 MaxTracked = 3;

private:
    UFUNCTION(Client, Reliable) void ClientQuestResult(const FString& QuestId, const FString& Result);
    struct FEntry
    {
        TSharedPtr<const FSFQuestDefinition> Quest;
        FSFQuestProgress Progress;
        int64 Revision = 0;
        bool bRewardWaiting = false;
        TArray<FString> RewardLines;
    };
    FEntry* Find(const FString& QuestId) { return Entries.FindByPredicate([&](const FEntry& E) { return E.Quest->Id == QuestId; }); }
    const FEntry* Find(const FString& QuestId) const { return Entries.FindByPredicate([&](const FEntry& E) { return E.Quest->Id == QuestId; }); }
    void Accept(const FString& QuestId, const FString& SourceId);
    void Abandon(const FString& QuestId);
    void AddEntry(TSharedPtr<const FSFQuestDefinition> Quest, int64 Revision);
    void RebuildView();
    void TryDeliverRewards();
    void RequestRefresh(float DelaySeconds);
    void Refresh();
    /** talk_resident: hold the active step's resident through the townsfolk hand-off; count it when the player reaches it. */
    void TickTalk();
    TWeakObjectPtr<class ASFTownsfolkNpc> Talker;
    double NextTalkRequest = 0;
    TArray<FEntry> Entries;                 // server: active quests and those completed this session (kept for the tracker)
    TArray<TPair<FString, FSFQuestRewardItem>> PendingRewards;  // without services: due but not yet in the bag
    UPROPERTY(Replicated) TArray<FSFQuestView> Views;
    UPROPERTY(Replicated) TArray<FString> Completed;
    // INT-04: with gameplay services on, progress is Progression.ApplyReceipt replies only, one call at a time.
    struct FServiceEvent { FString ReceiptKind, QuestKind, QuestId, TargetId, NodeId, OperationId; bool bRetried = false; };
    void PumpServices();
    TArray<FServiceEvent> ServiceQueue;
    bool bServiceBusy = false, bRefreshBusy = false, bRestored = false;
    int64 ProgressionRevision = 0, LastOccurredMs = 0;
    double NextRefreshAt = 0;
    int32 RefreshesDue = 0;
    FString LastResult;
    TSet<FString> Untracked;
    // Development fixtures, off unless their command-line flags are given (see Test-QuestPilot.ps1 -Leg Full).
    void TickDevAccept();
    bool bDevAcceptDone = false;
    void TickBoardScenario(double Now);
    void TickFullServer();
    void TickFullClient(double Now);
    FString FullTeleportKey;
    FString FullAttemptKey;
    double FullEquipAt = 0, FullStableSince = 0, FullAttemptAt = 0, FullLastSwingAt = 0;
    int32 FullAttempts = 0;
    FVector FullLastPosition = FVector::ZeroVector;
    bool bFullDone = false;
    friend class FSFQuestLogTest;
};

/** Server (Silverbrook and the QuestBoard dev map): attaches the quest log, routes pilot events.
 *  Both sides: places the layout's quest boards and quest givers. Client: the tracker, the journal (J) and the board panel. */
UCLASS()
class SHATTEREDFRONTIER_API USFQuestPilotSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    static bool IsRequested();
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    // Dialogue shows an offered quest on the tracker while its board is open; RestoreTracker returns to the log.
    void PresentOffer(const FSFQuestView& View) { OfferView = View; bOfferShown = true; }
    void RestoreTracker() { bOfferShown = false; }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USFQuestPilotSubsystem, STATGROUP_Tickables); }
    /** Maps where the quest log runs. */
    static bool IsQuestMap(const UWorld* World);
    /** The local player's quest book (journal and board), created on first use. */
    UPROPERTY(Transient) TObjectPtr<class USFQuestBoardPanel> Panel;
protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
    { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
private:
    void Route(APlayerController* PC, const FString& Kind, const FString& Target, const FString& Node, const FString& Operation);
    void PlaceFromLayout(UWorld& World);
    bool bActive = false;
    FDelegateHandle GatheredHandle, PlacedHandle, BoarHandle;
    FSFQuestView OfferView;
    bool bOfferShown = false;
};
