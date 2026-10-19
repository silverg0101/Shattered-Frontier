#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "SFGatheringPilot.generated.h"

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
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="0.5", ClampMax="5", Units="m"))
    float ReachMetres = 3.5f;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="0.01", ClampMax="0.25", Units="m"))
    float MovementCancelMetres = 0.05f;
    UPROPERTY(Config, EditAnywhere, Category="Harvest", meta=(ClampMin="1", ClampMax="12"))
    int32 ChargesPerNode = 3;
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
enum class ESFGatheringPilotKind : uint8 { Oak, Iron, Sawmill, Forge };

USTRUCT()
struct FSFGatheringPilotNodeState
{
    GENERATED_BODY()
    UPROPERTY() uint32 Revision = 0;
    UPROPERTY() int32 Charges = 0;
    UPROPERTY() bool bBusy = false;
};

// Presentation can be replaced by a Blueprint subclass; item grants remain server-owned.
UCLASS(Blueprintable)
class SHATTEREDFRONTIER_API ASFGatheringPilotNode : public AActor
{
    GENERATED_BODY()
public:
    ASFGatheringPilotNode();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    FString GetResourceName() const;
    bool IsRefinery() const { return Kind==ESFGatheringPilotKind::Sawmill || Kind==ESFGatheringPilotKind::Forge; }
    const FSFGatheringPilotNodeState& GetState() const { return State; }
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_State, Category="Kind")
    ESFGatheringPilotKind Kind = ESFGatheringPilotKind::Oak;
private:
    friend class USFGatheringPilotComponent;
    friend class FSFGatheringPilotAuthorityTest;
    friend class FSFGatheringPilotToolTest;
    UFUNCTION() void OnRep_State();
    void Release(USFGatheringPilotComponent* Interactor);
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY() TObjectPtr<UStaticMesh> StationMesh;
    UPROPERTY(ReplicatedUsing=OnRep_State) FSFGatheringPilotNodeState State;
    TWeakObjectPtr<USFGatheringPilotComponent> LockedBy;
};

UENUM()
enum class ESFGatheringPilotPhase : uint8 { Idle, Harvesting, Committed, Rejected, Canceled };
UENUM()
enum class ESFGatheringPilotResult : uint8
{
    None, Started, Granted, InvalidContext, InvalidTarget, Stale, Busy, Depleted,
    OutOfReach, Occluded, Moved, Canceled, InventoryFull, Unavailable, KeyConflict, Refined, MissingMaterials, ToolRequired
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
    FString GetStatusText(const FVector& Start, const FVector& Direction) const;
    static bool IsPilotRequested();
    void AssignPilotRole(bool bPrimary);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickFiniteNetworkScenario();
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
    UFUNCTION(Server, Reliable) void ServerGatheringAction(ASFGatheringPilotNode* Node, uint32 Revision, uint32 Operation);
    UFUNCTION(Server, Reliable) void ServerCancelHarvest(uint32 Operation);
    UFUNCTION() void OnRep_Feedback();
    UFUNCTION(Client, Reliable) void ClientKeyConflict(uint32 Operation);
    ESFGatheringPilotResult ValidateTarget(const ASFGatheringPilotNode* Node) const;
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
};
