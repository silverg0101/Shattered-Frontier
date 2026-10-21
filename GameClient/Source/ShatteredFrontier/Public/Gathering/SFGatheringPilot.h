#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Gathering/SFHarvestPresentation.h"
#include "Gathering/SFGatheringProgression.h"
#include "SFGatheringPilot.generated.h"

struct FSFHarvestArtReviewState;
struct FSFHarvestTreeFallNetworkState;

class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class USFGatheringPilotComponent;

// Development values: reviewed release authors, next world start, no live/console overrides.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Gathering Pilot (Release Only)"))
class SHATTEREDFRONTIER_API USFGatheringPilotSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="0.5", ClampMax="30", Units="s"))
    float HarvestSeconds = 3;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="0.1", ClampMax="2", Units="s", ToolTip="Minimum time between localized geometry preflight attempts per controller. Release authors; next world start."))
    float GeometryAttemptCooldownSeconds = 0.5f;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="0.5", ClampMax="5", Units="m"))
    float ReachMetres = 3.5f;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="0.01", ClampMax="0.25", Units="m"))
    float MovementCancelMetres = 0.05f;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="1", ClampMax="12"))
    int32 ChargesPerNode = 3;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="1", ClampMax="86400", Units="s", ToolTip="Session pilot recovery after final depletion. Release authors; next world start. Production regeneration belongs to World."))
    float RespawnSeconds = 300;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="1", ClampMax="10"))
    int32 UnitsPerCharge = 1;
    UPROPERTY(Config, EditAnywhere, Category="Inventory", meta=(ClampMin="1", ClampMax="64"))
    int32 InventorySlots = 8;
    UPROPERTY(Config, EditAnywhere, Category="Inventory", meta=(ClampMin="1", ClampMax="1000", Units="kg"))
    float InventoryWeightKg = 24;
    UPROPERTY(Config, EditAnywhere, Category="Material defaults (Content Generation)", meta=(ClampMin="1", ClampMax="100", ToolTip="Units per stack. Applied when generating new pilot item assets for a compatible release, not at runtime."))
    int32 MaterialStackUnits = 20;
    UPROPERTY(Config, EditAnywhere, Category="Material defaults (Content Generation)", meta=(ClampMin="0.01", ClampMax="50", Units="kg", ToolTip="Weight per unit. Applied only when generating new pilot item assets for a compatible release."))
    float MaterialUnitWeightKg = 1;
    UPROPERTY(Config, EditAnywhere, Category="Refining", meta=(ClampMin="0.5", ClampMax="30", Units="s"))
    float RefineSeconds = 3;
    UPROPERTY(Config, EditAnywhere, Category="Refining", meta=(ClampMin="1", ClampMax="20"))
    int32 TimberInputLogs = 1;
    UPROPERTY(Config, EditAnywhere, Category="Refining", meta=(ClampMin="1", ClampMax="20"))
    int32 TimberOutputUnits = 4;
    UPROPERTY(Config, EditAnywhere, Category="Refining", meta=(ClampMin="1", ClampMax="20"))
    int32 IngotInputOre = 1;
    UPROPERTY(Config, EditAnywhere, Category="Refining", meta=(ClampMin="1", ClampMax="20"))
    int32 IngotOutputUnits = 1;
    UPROPERTY(Config, EditAnywhere, Category="Starter Tools", meta=(ToolTip="Development loadout only: grant one released tier-one axe and pickaxe once per admitted controller. Release authors; next world start."))
    bool bGrantStarterTools = true;
    UPROPERTY(Config, EditAnywhere, Category="Starter Tools (Content Generation)", meta=(ClampMin="0.01", ClampMax="50", Units="kg"))
    float ToolUnitWeightKg = 2;
    bool Validate(FString& Error) const;
};

UENUM(BlueprintType)
enum class ESFGatheringPilotKind : uint8 { Oak, Iron, Sawmill, Forge, Stone };  // Stone last: the sandbox spawns kinds 0-3 by index

class ASFHarvestFallingTree;

USTRUCT()
struct FSFGatheringPilotNodeState
{
    GENERATED_BODY()
    UPROPERTY() uint32 Revision = 0;
    UPROPERTY() uint32 Generation = 0;
    UPROPERTY() int32 Charges = 0;
    UPROPERTY() bool bBusy = false;
    UPROPERTY() int32 Capacity = 0;
    UPROPERTY() double RecoveryAtServerSeconds = 0;
    UPROPERTY() TArray<FSFHarvestVisualCut> Cuts;
    UPROPERTY() TObjectPtr<ASFHarvestFallingTree> FallingTree;
};

// Idempotent public appearance snapshot, never an inventory/reward event.
USTRUCT(BlueprintType)
struct FSFGatheringPilotPresentation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ESFGatheringPilotKind Kind = ESFGatheringPilotKind::Oak;
    UPROPERTY(BlueprintReadOnly) int64 Revision = 0;
    UPROPERTY(BlueprintReadOnly) int32 RemainingCharges = 0;
    UPROPERTY(BlueprintReadOnly) int32 Capacity = 0;
    UPROPERTY(BlueprintReadOnly) bool bBusy = false;
    UPROPERTY(BlueprintReadOnly) bool bRefinery = false;
    UPROPERTY(BlueprintReadOnly) float ExtractedFraction = 0;
    UPROPERTY(BlueprintReadOnly) double RecoveryAtServerSeconds = 0;
};

// Presentation can be replaced by a Blueprint subclass; item grants remain server-owned.
UCLASS(Blueprintable)
class SHATTEREDFRONTIER_API ASFGatheringPilotNode : public AActor
{
    GENERATED_BODY()
public:
    ASFGatheringPilotNode();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    FString GetResourceName() const;
    bool GetResourceProfile(ESFGatheringSkill& Skill,ESFGatheringMaterialTier& Tier) const;
    FSFGatheringResourceProfile GetAuthoredResourceProfile() const;
    bool IsRefinery() const { return Kind==ESFGatheringPilotKind::Sawmill || Kind==ESFGatheringPilotKind::Forge; }
    const FSFGatheringPilotNodeState& GetState() const { return State; }
    UFUNCTION(BlueprintPure, Category="Gathering Presentation")
    FSFGatheringPilotPresentation GetPresentationState() const;
    // Apply current appearance, including late join. May repeat; do not play reward effects here.
    UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category="Gathering Presentation")
    void RefreshResourcePresentation(const FSFGatheringPilotPresentation& Presentation);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_State, Category="Kind")
    ESFGatheringPilotKind Kind = ESFGatheringPilotKind::Oak;
private:
    friend class ASFHarvestFallingTree;
    friend class USFGatheringPilotComponent;
    friend class FSFGatheringPilotAuthorityTest;
    friend class FSFGatheringPilotToolTest;
    friend class FSFGatheringPilotLifecycleTest;
    friend class FSFGatheringPilotArtContactTest;
    friend class FSFHarvestFellingAuthorityTest;
    friend class FSFGatheringPilotSurfaceProtocolTest;
    friend class FSFGatheringPilotProgressionTest;
    UFUNCTION() void OnRep_State();
    void Release(USFGatheringPilotComponent* Interactor);
    void BeginRecovery();
    void Recover();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true")) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess="true")) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UStaticMesh> StationMesh;
    UPROPERTY(ReplicatedUsing=OnRep_State) FSFGatheringPilotNodeState State;
    UPROPERTY() TObjectPtr<USFHarvestPresentation> HarvestVisuals;
    TWeakObjectPtr<USFGatheringPilotComponent> LockedBy;
    FTimerHandle RecoveryTimer;
    float CapturedRespawnSeconds = 300;
    ESFGatheringPilotKind RecoveryKind = ESFGatheringPilotKind::Oak;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    TOptional<ESFGatheringMaterialTier> TestProfileTier;
#endif
};

UENUM()
enum class ESFGatheringPilotPhase : uint8 { Idle, Harvesting, Committed, Rejected, Canceled };
UENUM()
enum class ESFGatheringPilotResult : uint8
{
    None, Started, Granted, InvalidContext, InvalidTarget, Stale, Busy, Depleted,
    OutOfReach, Occluded, Moved, Canceled, InventoryFull, Unavailable, KeyConflict, Refined, MissingMaterials, ToolRequired, SkillRequired
};

USTRUCT()
struct FSFGatheringPilotFeedback
{
    GENERATED_BODY()
    UPROPERTY() uint32 Operation = 0;
    UPROPERTY() ESFGatheringPilotPhase Phase = ESFGatheringPilotPhase::Idle;
    UPROPERTY() ESFGatheringPilotResult Result = ESFGatheringPilotResult::None;
    UPROPERTY() TObjectPtr<ASFGatheringPilotNode> Node = nullptr;
    UPROPERTY() double EndsAtServerSeconds = 0;
    UPROPERTY() ESFGatheringSkill Skill = ESFGatheringSkill::Mining;
    UPROPERTY() int32 RequiredSkill = 0;
    UPROPERTY() int32 CurrentSkill = 0;
    UPROPERTY() bool bSkillGained = false;
};

// Owner controller endpoint; no client-supplied item, yield, character, inventory or time.
UCLASS()
class SHATTEREDFRONTIER_API USFGatheringPilotComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFGatheringPilotComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    bool InteractFromRay(const FVector& Start, const FVector& Direction);
    void CancelLocalHarvest();
    void EquipAxe();
    void EquipPickaxe();
    bool IsEnabled() const { return bEnabled; }
    const FSFGatheringSkillSnapshot& GetSkillSnapshot() const { return Skills; }
    bool RequestSkillLock(ESFGatheringSkill Skill,ESFGatheringSkillLock Lock);
    float GetSkillLockCooldownRemaining(ESFGatheringSkill Skill) const;
    bool IsSkillLockReady(ESFGatheringSkill Skill) const;
    bool IsSkillLockChangePending(ESFGatheringSkill Skill) const { return SkillLockPending.IsValidIndex(int32(Skill))&&SkillLockPending[int32(Skill)]; }
    FString GetStatusText(const FVector& Start, const FVector& Direction) const;
    // Read-only view of the owner-replicated inventory: one (display name, quantity) per occupied slot or equipped tool.
    TArray<TPair<FText,int32>> GetInventoryItems() const;
    static bool IsPilotRequested();
    void AssignPilotRole(bool bPrimary);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickFiniteNetworkScenario();
    void TickArtReviewScenario();
    void TickFellingNetworkScenario();
    void TickSkillChoicesNetworkScenario();
    int32 SkillChoiceScenarioStep=0;
    double SkillChoiceScenarioStarted=0;
    double SkillChoiceCaptureAt=0;
    TSharedPtr<FSFHarvestArtReviewState> ArtReviewState;
    TSharedPtr<FSFHarvestTreeFallNetworkState> FellingNetworkState;
    int32 ScenarioStep = 0;
    double ScenarioStarted = 0;
    double ScenarioReplayedAt = 0;
    double ScenarioStableSince = 0;
    FVector ScenarioLastPosition = FVector::ZeroVector;
    bool bScenarioToolRequested = false;
#endif
private:
    friend class FSFGatheringPilotAuthorityTest;
    friend class FSFGatheringPilotToolTest;
    friend class FSFGatheringPilotLifecycleTest;
    friend class FSFGatheringPilotArtContactTest;
    friend class FSFHarvestFellingAuthorityTest;
    friend class FSFGatheringPilotSurfaceProtocolTest;
    friend class FSFGatheringPilotProgressionTest;
    UFUNCTION(Server, Reliable) void ServerGatheringAction(ASFGatheringPilotNode* Node, uint32 Revision, uint32 Operation);
    UFUNCTION(Server, Reliable) void ServerGatheringActionAtSurface(ASFGatheringPilotNode* Node, uint32 Revision, uint32 Operation,FVector_NetQuantize10 RequestedPoint);
    void ProcessGatheringAction(ASFGatheringPilotNode* Node,uint32 Revision,uint32 Operation,bool bSurface,const FVector& RequestedPoint);
    bool MatchesRequestBinding(ASFGatheringPilotNode* Node,uint32 Revision,bool bSurface,const FVector& Point) const;
    bool TryBeginGeometryAttempt(double Now);
    UFUNCTION(Server, Reliable) void ServerCancelHarvest(uint32 Operation);
    UFUNCTION(Server, Reliable) void ServerChangeSkillLock(ESFGatheringSkill Skill,ESFGatheringSkillLock Lock);
    UFUNCTION(Client, Reliable) void ClientSkillLockTiming(ESFGatheringSkill Skill,float RemainingSeconds);
    UFUNCTION() void OnRep_Feedback();
    UFUNCTION(Client, Reliable) void ClientKeyConflict(uint32 Operation);
    ESFGatheringPilotResult ValidateTarget(const ASFGatheringPilotNode* Node) const;
    bool MeetsSkillRequirement(const ASFGatheringPilotNode* Node) const;
    void Finish(ESFGatheringPilotPhase Phase, ESFGatheringPilotResult Result);
    ASFGatheringPilotNode* FindNode(const FVector& Start, const FVector& Direction) const;
    ESFGatheringPilotResult ApplyInventory(ESFGatheringPilotKind Kind, bool bCommit);
    bool InventoryReady() const;
    bool InitializeStarterTools();
    bool HasRequiredTool(ESFGatheringPilotKind Kind) const;
    bool TryEquipTool(bool bAxe);
    FString GetToolName() const;
    bool IsAuthoritativePilotWorld() const;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    bool bTestWorldAdmission = false;
#endif
    UPROPERTY(ReplicatedUsing=OnRep_Feedback) FSFGatheringPilotFeedback Feedback;
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotSettings> CapturedSettings;
    UPROPERTY(Transient) TObjectPtr<USFGatheringProgressionSettings> CapturedProgressionSettings;
    UPROPERTY(Replicated) FSFGatheringSkillSnapshot Skills;
    UPROPERTY(Replicated) TArray<double> SkillLockReadyAt = {0,0,0,0,0,0};
    TArray<double> LocalSkillLockReadyAt = {0,0,0,0,0,0};
    TArray<bool> SkillLockPending = {false,false,false,false,false,false};
    TArray<bool> LocalSkillLockReady = {true,true,true,true,true,true};
    uint8 SkillLockReadyNotifications=0;
    FSFGatheringProgressionSession ProgressionSession;
    double ActivePracticeInputSeconds = 0;
    double ActivePracticeDurationSeconds = 0;
    uint64 ActivePracticeContext = 0;
    FSFGatheringResourceProfile ActiveResourceProfile;
    TWeakObjectPtr<APawn> HarvestPawn;
    TWeakObjectPtr<ASFGatheringPilotNode> LastNode;
    FVector HarvestStart = FVector::ZeroVector;
    ESFGatheringPilotKind ActiveKind = ESFGatheringPilotKind::Oak;
    uint32 LastRevision = 0;
    uint32 NextLocalOperation = 1;
    uint32 WaitingOperation = 0;
    uint32 WaitingRevision = 0;
    TWeakObjectPtr<ASFGatheringPilotNode> WaitingNode;
    UPROPERTY(Replicated) bool bEnabled = false;
    UPROPERTY(Replicated) bool bPrimaryRole = false;
    UPROPERTY(Replicated) bool bRoleAssigned = false;
    bool bKeyConflict = false;
    bool bStarterToolsGranted = false;
    bool bLastHadSurface=false;
    bool bActiveVisualCut=false;
    bool bWaitingHadSurface=false;
    double NextGeometryAttemptSeconds=0;
    FVector LastRequestedSurface=FVector::ZeroVector;
    FVector WaitingSurface=FVector::ZeroVector;
    FSFHarvestVisualCut ActiveVisualCut;
};
