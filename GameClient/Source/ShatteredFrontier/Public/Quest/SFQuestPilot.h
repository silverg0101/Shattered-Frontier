#pragma once

#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFQuestPilot.generated.h"

class APlayerController;
class UTextBlock;
class UVerticalBox;

// ---- Pure model: mirrors BackendServices/QuestContent (same JSON, same validation and progression rules). ----

struct FSFQuestObjective { FString Key, Kind, TargetId, NodeId; bool bHasNode = false; int32 Count = 0; };
struct FSFQuestStep { FString Id; TArray<FString> Prerequisites; TArray<FSFQuestObjective> Objectives; TArray<FString> Rewards; };

struct SHATTEREDFRONTIER_API FSFQuestDefinition
{
    FString Id; int32 Revision = 0; FString ZoneId; TArray<FSFQuestStep> Steps;
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
    /** Applies one server-observed outcome. Reward bundle ids are appended to NewRewards once per step. */
    ESFQuestApply Apply(const FSFQuestDefinition& Quest, const FString& Kind, const FString& TargetId, const FString& NodeId,
        int32 Quantity, const FString& OperationId, TArray<FString>& NewRewards);
};

// ---- Owner-only replicated view; the client needs no quest content. ----

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
    UPROPERTY() FString Title;
    UPROPERTY() FString Step;
    UPROPERTY() int32 StepNumber = 0;
    UPROPERTY() int32 StepTotal = 0;
    UPROPERTY() bool bComplete = false;
    UPROPERTY() TArray<FSFQuestLine> Lines;
    UPROPERTY() TArray<FString> RewardLines;  // committed completion payout, e.g. "3 Boar Hide"
};

// Release defaults. Pilot stand-in for the reward service: which pilot item the completion bundle pays.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Quest Pilot (Release Only)"))
class SHATTEREDFRONTIER_API USFQuestPilotSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ToolTip="Pilot item definition paid for reward/frontier_founder_completion. Release authors; next world start."))
    FString CompletionRewardItemPath = TEXT("/Game/Frontier/Gameplay/Pilot/DA_BoarHide.DA_BoarHide");
    UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="1", ClampMax="20"))
    int32 CompletionRewardUnits = 3;
};

/** On a player controller. Server owns progress; owning client receives View. */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API USFQuestPilotComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFQuestPilotComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    /** Server only. */
    void Init(TSharedPtr<const FSFQuestDefinition> InQuest);
    bool Observe(const FString& Kind, const FString& TargetId, const FString& NodeId, const FString& OperationId);
    const FSFQuestProgress& GetProgress() const { return Progress; }
    int32 PendingRewardCount() const { return PendingRewards.Num(); }
    const FSFQuestView& GetView() const { return View; }
private:
    void RebuildView();
    void TryDeliverRewards();
    TSharedPtr<const FSFQuestDefinition> Quest;
    FSFQuestProgress Progress;
    TArray<FString> PendingRewards;  // due but not yet in the bag; retried until delivered, then dropped
    UPROPERTY(Replicated) FSFQuestView View;
    // INT-04: with gameplay services on, progress is Progression.ApplyReceipt replies only, one call at a time.
    struct FServiceEvent { FString Kind, TargetId, NodeId, OperationId; bool bRetried = false; };
    void PumpServices();
    TArray<FServiceEvent> ServiceQueue;
    bool bServiceBusy = false;
    int64 ProgressionRevision = 0, QuestRevision = 0, LastOccurredMs = 0;
    TArray<FString> RewardLines;
    // Development fixtures, off unless their command-line flags are given (see Test-QuestPilot.ps1 -Leg Full).
    void TickFullServer();
    void TickFullClient(double Now);
    FString FullTeleportKey;
    FString FullAttemptKey;
    double FullEquipAt = 0, FullStableSince = 0, FullAttemptAt = 0, FullLastSwingAt = 0;
    int32 FullAttempts = 0;
    FVector FullLastPosition = FVector::ZeroVector;
    bool bFullDone = false;
};

// Tracker panel: right 84-99%, top 25-39% (HUD-v1). Title, step, objective rows with counts, tick when done.
UCLASS()
class SHATTEREDFRONTIER_API USFQuestTrackerHud : public UUserWidget
{
    GENERATED_BODY()
public:
    void Present(const FSFQuestView& View);
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StepText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Rows;
    FString LastKey;
};

/** Server (opt-in, Silverbrook, only beside a pilot): loads the quest, attaches the component, routes pilot events.
 *  Client: creates the tracker once the local controller has a quest view. */
UCLASS()
class SHATTEREDFRONTIER_API USFQuestPilotSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    static bool IsRequested();
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USFQuestPilotSubsystem, STATGROUP_Tickables); }
protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
    { return WorldType == EWorldType::Game || WorldType == EWorldType::PIE; }
private:
    void Route(APlayerController* PC, const FString& Kind, const FString& Target, const FString& Node, const FString& Operation);
    TSharedPtr<const FSFQuestDefinition> Quest;
    FDelegateHandle GatheredHandle, PlacedHandle, BoarHandle;
    UPROPERTY(Transient) TObjectPtr<USFQuestTrackerHud> Hud;
};
