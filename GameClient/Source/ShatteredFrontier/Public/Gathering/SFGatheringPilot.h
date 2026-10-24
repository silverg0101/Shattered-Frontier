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
struct FSFHarvestFellingReviewState;

class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;
class USFGatheringPilotComponent;
class FSFInventoryLedgerClient;
class USFBackpackComponent;
struct FSFStationRecipe;
struct FSFLedgerResponse;

// Development values: reviewed release authors, next world start, no live/console overrides.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Gathering Pilot (Release Only)"))
class SHATTEREDFRONTIER_API USFGatheringPilotSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Pilot", meta=(ToolTip="GM-01: gathering wherever the world has nodes (zone record, or the sandbox test fixture). Default on; next world start."))
    bool bEnabled = true;
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
    UPROPERTY(Config, EditAnywhere, Category="Refining", meta=(ToolTip="Show the server quality range for selected materials. Release authors; next world start."))
    bool bCraftGradePreviewEnabled = true;
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
enum class ESFGatheringPilotKind : uint8 { Oak, Iron, Sawmill, Forge, Stone, Workbench };  // the sandbox spawns kinds 0-3 by index; append new kinds

// GAT-01 declared appearance per node kind and growth stage (stage names come from the zone record's growth_profile).
// Scale multiplies the mature node (1 = full size, unlisted stage = 1). Release authors; next world start.
USTRUCT()
struct FSFGrowthStageVisual
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Config) ESFGatheringPilotKind Kind = ESFGatheringPilotKind::Oak;
    UPROPERTY(EditAnywhere, Config) FName Stage;
    UPROPERTY(EditAnywhere, Config, meta=(ClampMin="0.1", ClampMax="1.5")) float Scale = 1;
};

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Gathering Growth Appearance (Release Only)"))
class SHATTEREDFRONTIER_API USFGatheringGrowthSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    USFGatheringGrowthSettings();
    UPROPERTY(Config, EditAnywhere, Category="Growth") TArray<FSFGrowthStageVisual> StageVisuals;
    float ScaleFor(ESFGatheringPilotKind Kind, FName Stage) const;
    bool Validate(FString& Error) const;
};

// Server-only growth profile (content: growth_profile in the zone record). DurationFraction is of the grow window.
struct FSFGrowthStageDef { FName Name; float DurationFraction = 0; };

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
    UPROPERTY() FName GrowthStage;  // None = no growth profile; server-owned, replicated with the rest
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
    // The prompt verb for this node: "Chop" (logging), "Mine", "Fish", "Gather" otherwise.
    FString ActionVerb() const;
    bool GetResourceProfile(ESFGatheringSkill& Skill,ESFGatheringMaterialTier& Tier) const;
    FSFGatheringResourceProfile GetAuthoredResourceProfile() const;
    bool IsRefinery() const { return Kind==ESFGatheringPilotKind::Sawmill || Kind==ESFGatheringPilotKind::Forge || Kind==ESFGatheringPilotKind::Workbench; }
    const FSFGatheringPilotNodeState& GetState() const { return State; }
    // Server, before FinishSpawning: after it recovers the node regrows through these stages over GrowFraction of the respawn interval.
    void SetGrowthProfile(const TArray<FSFGrowthStageDef>& Stages, float InGrowFraction);
    // Stage reached ElapsedSeconds into a grow window of GrowWindowSeconds: same rule as BackendServices/GatheringNodes.
    static FName StageAt(const TArray<FSFGrowthStageDef>& Stages, double GrowWindowSeconds, double ElapsedSeconds);
    // Dev capture only (no effect in shipping/test builds): force the stage shown.
    UFUNCTION(BlueprintCallable, Category="Gathering Presentation")
    void DevPreviewGrowthStage(FName Stage);
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
    friend class FSFGatheringPilotLedgerTest;
    friend class FSFGatheringGrowthVisualTest;
    UFUNCTION() void OnRep_State();
    void StartGrowth();
    void AdvanceGrowth();
    void ApplyGrowthVisual();
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
    FTimerHandle GrowthTimer;
    TArray<FSFGrowthStageDef> GrowthStages;
    float GrowFraction = 0.5f;
    double GrowthStartedAt = 0;
    float CapturedRespawnSeconds = 300;
    ESFGatheringPilotKind RecoveryKind = ESFGatheringPilotKind::Oak;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    TOptional<ESFGatheringMaterialTier> TestProfileTier;
#endif
};

UENUM()
enum class ESFGatheringPilotPhase : uint8 { Idle, Harvesting, Committed, Rejected, Canceled, Committing };  // Committing: awaiting the InventoryLedger receipt
UENUM()
enum class ESFGatheringPilotResult : uint8
{
    None, Started, Granted, InvalidContext, InvalidTarget, Stale, Busy, Depleted,
    OutOfReach, Occluded, Moved, Canceled, InventoryFull, Unavailable, KeyConflict, Refined, MissingMaterials, ToolRequired, SkillRequired
};

// Why an equip / unequip request was refused (Ok = applied; Pending = a ledger transfer is queued and the slot changes when it
// commits). Server-side answer; the client only sees the replicated result.
UENUM()
enum class ESFEquipResult : uint8 { Ok, Disabled, InvalidSlot, NotOwned, NotEquippable, WrongSlot, NotEquipped, NoSpace, Pending };

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
    // Cancels the harvest in progress or waiting; true when there was one to cancel.
    bool CancelLocalHarvest();
    void EquipAxe();
    void EquipPickaxe();
    // Equipment (Equipment window, gathering tools). The state is the plugin inventory's replicated EquippedItems; an item's slot is its
    // definition's EquipmentSlot (data). Requests come from the owning client and are validated here on the server: the slot exists, the
    // item is in this player's bag, it is equippable into exactly that slot, and a swap / unequip fits the bag (space and weight).
    static const TArray<FName>& EquipmentSlots();  // Head, Chest, Hands, Legs, Feet, MainHand, OffHand, Neck, Ring, Back
    static FName MainHandSlot() { return FName(TEXT("MainHand")); }
    // Owner (client or server): ask to equip one owned bag item into Slot / return Slot to the bag. False when refused before sending.
    bool RequestEquip(const FSoftObjectPath& Item, FName Slot);
    bool RequestUnequip(FName Slot);
    // Server: validate and apply. Ok or the reason; nothing changes unless Ok. With the ledger on, gear (content/items) first moves
    // between the backpack and equipment containers (Pending), so equipped gear survives a restart.
    ESFEquipResult ApplyEquip(const FSoftObjectPath& Item, FName Slot);
    ESFEquipResult ApplyUnequip(FName Slot);
    // The item in Slot (owner view), empty when the slot is empty.
    FSoftObjectPath GetEquipped(FName Slot) const;
    // Server: makes the equipped gear match the ledger equipment container (definition id -> quantity); tools are left alone.
    void ProjectEquipment(const TMap<FString, int64>& Equipment);
    // Station recipes come from content/crafting; the first is the default. The choice is per player and server-held.
    void SelectStationRecipe(ESFGatheringPilotKind Station, const FString& RecipeId);
    void CycleStationRecipe(ESFGatheringPilotKind Station);
    bool CycleRecipeFromRay(const FVector& Start, const FVector& Direction);  // the station under the cursor
    FString GetStationRecipe(ESFGatheringPilotKind Station) const;
    // Station recipe id ("sawmill" etc.) for a station kind; empty for resources.
    static FString StationName(ESFGatheringPilotKind Kind);
    bool IsEnabled() const { return bEnabled; }
    const FSFGatheringSkillSnapshot& GetSkillSnapshot() const { return Skills; }
    // Server, INT-05 restore: Progression skill values ("skill/<name>", tenths) replace the session values.
    void RestoreSkills(const TMap<FString, int32>& ServiceSkills);
    // Server, INT-05 restore: the Progression character level (derived from lifetime XP by progression-service); owner-replicated for the HUD badge.
    void RestoreLevel(int32 ServiceLevel);
    int32 GetCharacterLevel() const { return CharacterLevel; }
    void RestoreKnownRecipes(const TArray<FString>& Recipes);
    void AddKnownRecipe(const FString& RecipeId);
    bool KnowsRecipe(const FSFStationRecipe& Recipe) const;
    const TArray<FString>& GetKnownRecipes() const { return KnownRecipes; }
    // Read-only views for the gathering HUD (ring, toast, recipe panel).
    const FSFGatheringPilotFeedback& GetFeedback() const { return Feedback; }
    ASFGatheringPilotNode* FindAimedNode(const FVector& Start, const FVector& Direction) const { return FindNode(Start, Direction); }
    bool RequestSkillLock(ESFGatheringSkill Skill,ESFGatheringSkillLock Lock);
    float GetSkillLockCooldownRemaining(ESFGatheringSkill Skill) const;
    bool IsSkillLockReady(ESFGatheringSkill Skill) const;
    bool IsSkillLockChangePending(ESFGatheringSkill Skill) const { return SkillLockPending.IsValidIndex(int32(Skill))&&SkillLockPending[int32(Skill)]; }
    FString GetStatusText(const FVector& Start, const FVector& Direction) const;
    // Read-only view of the owner-replicated inventory: one (display name, quantity) per occupied slot or equipped tool.
    TArray<TPair<FText,int32>> GetInventoryItems() const;
    // Weight in kg (MaxKg 0 = unlimited); false when the pilot has no inventory.
    bool GetInventoryWeight(float& TotalKg, float& MaxKg) const;
    static bool IsPilotRequested();
    void AssignPilotRole(bool bPrimary);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickFiniteNetworkScenario();
    void TickArtReviewScenario();
    void TickFellingNetworkScenario();
    void TickFellingReviewScenario();
    void TickSkillChoicesNetworkScenario();
    int32 SkillChoiceScenarioStep=0;
    double SkillChoiceScenarioStarted=0;
    double SkillChoiceCaptureAt=0;
    TSharedPtr<FSFHarvestArtReviewState> ArtReviewState;
    TSharedPtr<FSFHarvestTreeFallNetworkState> FellingNetworkState;
    TSharedPtr<FSFHarvestFellingReviewState> FellingReviewState;
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
    friend class FSFGatheringPilotLedgerTest;
    friend class FSFEquipmentValidationTest;
    friend class FSFEquipmentLedgerTest;
    UFUNCTION(Server, Reliable) void ServerGatheringAction(ASFGatheringPilotNode* Node, uint32 Revision, uint32 Operation);
    UFUNCTION(Server, Reliable) void ServerGatheringActionAtSurface(ASFGatheringPilotNode* Node, uint32 Revision, uint32 Operation,FVector_NetQuantize10 RequestedPoint);
    void ProcessGatheringAction(ASFGatheringPilotNode* Node,uint32 Revision,uint32 Operation,bool bSurface,const FVector& RequestedPoint);
    bool MatchesRequestBinding(ASFGatheringPilotNode* Node,uint32 Revision,bool bSurface,const FVector& Point) const;
    bool TryBeginGeometryAttempt(double Now);
    UFUNCTION(Server, Reliable) void ServerCancelHarvest(uint32 Operation);
    UFUNCTION(Server, Reliable) void ServerEquip(const FSoftObjectPath& Item, FName Slot);
    UFUNCTION(Server, Reliable) void ServerUnequip(FName Slot);
    int32 FindBagSlot(const FSoftObjectPath& Item) const;
    UFUNCTION(Server, Reliable) void ServerSelectRecipe(ESFGatheringPilotKind Station, const FString& RecipeId);
    const FSFStationRecipe* ActiveStationRecipe(ESFGatheringPilotKind Station) const;
    void BeginCraftCommit();
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
    void CompleteHarvest(bool bRefining);
    void BeginLedgerCommit();
    void OnLedgerGrant(const FSFLedgerResponse& Response);

    // INT-04: the controller backpack is the single ledger writer; set only when USFInventoryLedgerSettings::bEnabled.
    TWeakObjectPtr<USFBackpackComponent> Backpack;
    TSharedPtr<FSFLedgerResponse> LastLedgerResponse;  // the grant reply behind the backpack outcome, for logs
    FString PendingCraftOperation;  // "craft/<guid>", reused on every retry until a terminal outcome
    FString CommittedOperation;     // ledger or craft operation behind the outcome being broadcast
    UPROPERTY(Replicated) TArray<FString> StationRecipes;  // selected recipe id per station kind
    UPROPERTY(Replicated) TArray<FString> KnownRecipes;  // restored owner-only from CharacterState

    FString PendingLedgerOperation;  // "gather/<guid>", reused on every retry until a terminal outcome

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    bool bTestWorldAdmission = false;
#endif
    UPROPERTY(ReplicatedUsing=OnRep_Feedback) FSFGatheringPilotFeedback Feedback;
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotSettings> CapturedSettings;
    UPROPERTY(Transient) TObjectPtr<USFGatheringProgressionSettings> CapturedProgressionSettings;
    UPROPERTY(Replicated) FSFGatheringSkillSnapshot Skills;
    UPROPERTY(Replicated) int32 CharacterLevel = 1;
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
