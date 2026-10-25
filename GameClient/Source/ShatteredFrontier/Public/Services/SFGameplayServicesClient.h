#pragma once

#include "Engine/DeveloperSettings.h"
#include "Templates/Function.h"
#include "SFGameplayServicesClient.generated.h"

// INT-04 development connection from the UE server to Gameplay.Host (proto/sf/{rewards,progression,housing,crafting}/v1).
// Same shape as FSFInventoryLedgerClient: the gRPC transport lives in the server-only ShatteredFrontierBackend module and
// registers itself here, so client builds carry no backend code.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Gameplay Services (Release Only)"))
class SHATTEREDFRONTIER_API USFGameplayServicesSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Services", meta=(ToolTip="Route pilot rewards, progression, quest, crafting and housing through Gameplay.Host. Off keeps the session-only pilots. Release authors; next world start."))
    bool bEnabled = false;
    UPROPERTY(Config, EditAnywhere, Category="Services", meta=(ToolTip="Loopback host:port of Gameplay.Host (development topology)."))
    FString Endpoint = TEXT("127.0.0.1:50072");
    UPROPERTY(Config, EditAnywhere, Category="Services", meta=(ClampMin="1", ClampMax="30", Units="s", ToolTip="Deadline for each attempt."))
    float TimeoutSeconds = 3;
    UPROPERTY(Config, EditAnywhere, Category="Services", meta=(ClampMin="1", ClampMax="5", ToolTip="Attempts per operation, all with the same operation id."))
    int32 MaxAttempts = 3;
    UPROPERTY(Config, EditAnywhere, Category="Gathering", meta=(ClampMin="1", ClampMax="30", Units="s", ToolTip="Refresh a gathering node when no stage transition is pending, or retry an unavailable snapshot. Release authors; next world start."))
    float GatheringNodeRetrySeconds = 5;
    UPROPERTY(Config, EditAnywhere, Category="Reconnect", meta=(ToolTip="INT-05: restore a dev character's backpack, skills, quest step and position on connect, and drain a disconnected controller's in-flight commits. Combat death recovery also attaches when the combat pilot and Gameplay Services are active. Needs the guarded dev identity. Release authors; next world start."))
    bool bRestoreOnConnect = false;
    UPROPERTY(Config, EditAnywhere, Category="Exchange", meta=(ToolTip="Route the trading UI (direct trades, market listings, wallet) through Gameplay.Host Exchange. Off keeps the fake demo service. Release authors; next world start."))
    bool bExchangeEnabled = true;
    UPROPERTY(Config, EditAnywhere, Category="Group", meta=(ToolTip="Data-center region for premade party records. Release authors; next world start."))
    FString GroupRegionId = TEXT("dc_dev");
    UPROPERTY(Config, EditAnywhere, Category="Exchange", meta=(ClampMin="1.0", ClampMax="10.0", Units="m", ToolTip="Maximum player distance from a settlement market board. Release authors; next world start."))
    float MarketReachM = 3.0f;
    UPROPERTY(Config, EditAnywhere, Category="Exchange", meta=(ClampMin="10", ClampMax="200", ToolTip="Listings per market page. Release authors; next world start."))
    int32 MarketPageSize = 50;
    UPROPERTY(Config, EditAnywhere, Category="Exchange", meta=(ClampMin="1.0", ClampMax="60.0", Units="s", ToolTip="Refresh interval while the market is open. Release authors; next world start."))
    float MarketRefreshSeconds = 5.0f;
    UPROPERTY(Config, EditAnywhere, Category="Exchange", meta=(ClampMin="0", ClampMax="2000", ToolTip="Market fee in basis points (1 bp = 0.01%), computed by the server (the client only previews it). Release authors; next world start."))
    int32 ExchangeFeeBasisPoints = 500;
    UPROPERTY(Config, EditAnywhere, Category="Exchange", meta=(ClampMin="0", ClampMax="1000", ToolTip="Commodity listing fee preview in basis points; Gameplay.Host remains authoritative. Release authors; next world start."))
    int32 ExchangeListingFeeBasisPoints = 100;
    UPROPERTY(Config, EditAnywhere, Category="Reconnect", meta=(ClampMin="5", ClampMax="600", Units="s", ToolTip="Server saves a moved character's position this often (and once at logout)."))
    float PositionSaveIntervalSeconds = 30;
    UPROPERTY(Config, EditAnywhere, Category="Reconnect", meta=(ClampMin="5", ClampMax="120", Units="s", ToolTip="Longest a disconnected controller stays in the world while its in-flight commits resolve."))
    float LinkdeadDrainSeconds = 30;
    bool Validate(FString& Error) const;
};

enum class ESFGameplayRpc : uint8 { GrantDefeatReward, CreateSharedDefeatDrop, ClaimSharedDefeatDrop, ApplyReceipt, GetCharacterState, EndSession, CreateCharacter, LearnRecipe, ClaimPlot, GetClaim, Place, Remove, Craft, ListStructures, Grant, SavePosition, CommitUnstuck, ReportDeath, ClearDeath,
    // Exchange (proto/sf/exchange/v1), carried by the Backend transport over the same Gameplay.Host channel.
    ExchangeWallet, OfferTrade, AcceptTrade, CancelTrade, ListItem, BuyListing, CancelListing, ListListings,
    GetGatheringNode, ListGatheringZoneNodes, HarvestGatheringNode, AdvanceWorldSlots,
    GroupCreate, GroupJoin, GroupLeave, GroupKick, GroupPromote, GroupAssistant, GroupGet };

// Exchange request fields (each RPC reads only its own). Amounts are whole gold, as in the proto.
struct FSFExchangeDelivery { FString EscrowId, DestinationId, ItemId; int64 Quantity = 0; };
struct FSFExchangeRequest
{
    FString TradeId, PartyA, PartyB, OfferOperationId;
    int64 GoldA = 0, GoldB = 0;
    TArray<FSFExchangeDelivery> Deliveries;
    FString ListingId, SellerId, SourceContainerId, EscrowContainerId, ItemId, MarketId;
    int64 Quantity = 0, UnitPrice = 0, Fee = 0, CreatedAtUnixMs = 0;
    FString DestinationContainerId;  // BuyListing
    int64 ExpectedRevision = 0, ExpectedUnitPrice = 0;
};
struct FSFExchangeResponse { int64 Balance = 0, Reserved = 0, Available = 0, WalletRevision = 0, Amount = 0, Fee = 0; FString SubjectId, State; bool bCommitted = false; };
struct FSFMarketListingResponse { FString Id, SellerId, ItemId; int64 Quantity = 0, UnitPrice = 0; };
// Ok; Rejected = a typed gameplay refusal (FAILED_PRECONDITION, PERMISSION_DENIED, ALREADY_EXISTS, INVALID_ARGUMENT, ...);
// Unavailable = UNAVAILABLE / DEADLINE_EXCEEDED / CANCELLED, retried with the same operation id.
enum class ESFGameplayStatus : uint8 { Ok, Rejected, Unavailable };

// One flat request per call; each RPC reads only its own fields (names follow the protos).
struct FSFGameplayRequest
{
    ESFGameplayRpc Rpc = ESFGameplayRpc::GetCharacterState;
    FString OperationId;          // also Rewards.defeat_event_id and Progression.source_receipt_id
    FString CharacterId;          // recipient, actor, owner
    TArray<uint8> AppearancePacket;  // CreateCharacter: CharacterAppearance v2 bytes
    FString ContainerId;
    int64 ExpectedContainerRevision = 0;
    // Rewards
    FString KillerCharacterId;
    bool bKillerProtected = false, bBoarProtected = false;
    FString TableId;
    FString CreatureId;
    TArray<FString> EligibleCharacterIds;
    FString RaidGroupId;
    // Progression
    FString Kind;                 // gather | craft | defeat | quest
    int64 ExpectedProgressionRevision = 0, ExpectedQuestRevision = 0;
    bool bSessionEnd = false, bInRestArea = false;
    FString SkillId; int32 Difficulty = 0; double ActionSeconds = 0; uint64 ContextHash = 0; bool bSuccess = true;
    int32 EnemyLevel = 0; double EnemyTierMultiplier = 0; int32 PartySize = 0; int32 ReferencePartyLevel = 0;
    bool bHasQuest = false;
    FString QuestId, StepId, QuestKind, TargetId, NodeId; int32 Quantity = 1; int64 OccurredAtUnixMs = 0;
    // Housing
    FString ClaimId, StructureId, ContentId, MaterialId, RemovalKind;
    double CenterX = 0, CenterY = 0; int32 SideMetres = 0, PrepaidDays = 0;
    int64 ExpectedPermissionRevision = 0, ExpectedStructureRevision = 0, MaterialQuantity = 0;
    FIntVector Cell = FIntVector::ZeroValue;
    FString GranteeId; int32 Rights = 0;  // Grant: the grantee and its ClaimRight bits (1 = build)
    FString ShapeKey; int32 OrientationIndex = 0;  // Place: BLD-01 geometry identity
    FString ZoneId; FVector Position = FVector::ZeroVector; float Yaw = 0; int64 ExpectedPositionRevision = 0;  // SavePosition
    int32 UnstuckCooldownSeconds = 900;  // CommitUnstuck; zone-server supplied typed tuning
    FString DeathEventId; int64 DeathAtUnixMs = 0; FVector RespawnPosition = FVector::ZeroVector; int32 RespawnDelayMs = 0;
    bool bResurrectAtDeathPosition = false;
    // Crafting
    FString RecipeId; int32 Count = 1;
    TMap<FString, FString> InputGrades;  // base definition -> selected grade definition
    FString LearnSource, TrainerNpc, ScrollContainer;
    FSFExchangeRequest Exchange;
    int32 NodeGeneration = 0;  // HarvestGatheringNode: server-observed generation
    FString SlotCommand, SlotGroupId; int32 SlotIndex = 0, PlayersInArea = 0; bool bHostileSpawning = true;
    TArray<uint8> FollowUpPayload; FString FollowUpDigest; int32 FollowUpVersion = 0;
    FString GroupPartyId, GroupActorId, GroupMemberId, GroupAccountId, GroupRealmId, GroupRegionId;
};

struct FSFGameplayResponse
{
    ESFGameplayStatus Status = ESFGameplayStatus::Unavailable;
    int32 GrpcCode = 14;
    FString Message;
    // Rewards
    bool bGranted = false; FString SourceCode, LedgerOperationId, DefinitionId, ClaimantCharacterId; int64 Quantity = 0;
    int64 ContainerRevision = 0;  // Rewards and Crafting
    // Progression
    bool bProgressionApplied = false, bQuestAccepted = false, bQuestCompleted = false;
    int32 SkillGain = 0, XpGain = 0, Level = 0;
    struct FCombatAttributes
    {
        FString Archetype;
        int32 Level = 0, Strength = 0, Dexterity = 0, Constitution = 0;
        int32 Intelligence = 0, Wisdom = 0, Willpower = 0;
        bool IsValid() const { return Level > 0 && !Archetype.IsEmpty() && Strength >= 10 &&
            Dexterity >= 10 && Constitution >= 10 && Intelligence >= 10 && Wisdom >= 10 && Willpower >= 10; }
    } CombatAttributes;
    int64 ProgressionRevision = 0, QuestRevision = 0;
    double RestedMinutes = 0;
    int64 AppearanceRevision = 0;  // CreateCharacter
    FString ActiveStepId; TArray<FString> RewardBundleIds; TMap<FString, int32> Skills;
    TMap<FString, int64> GrantedItems;  // the quest payout lines committed with this receipt
    TMap<FString, int32> ObjectiveCounts;  // GetCharacterState: the active step's counts by objective key
    FString QuestReason;  // ApplyReceipt: why quest evidence (or an accept/abandon) was not accepted
    struct FQuestState { FString QuestId, State, ActiveStepId; TMap<FString, int32> Counts; int64 Revision = 0; bool bRewardPending = false; };
    TArray<FQuestState> Quests;  // GetCharacterState: every quest row (QST-02)
    TArray<FString> KnownRecipes;
    FString FaceId;  // GetCharacterState: the stored player face; empty keeps the default
    bool bHasPosition = false; FString ZoneId; FVector Position = FVector::ZeroVector; float Yaw = 0; int64 PositionRevision = 0;
    bool bHasDeath = false, bIsDead = false; FString DeathEventId; int64 DeathAtUnixMs = 0, RespawnAtUnixMs = 0;
    FString DeathZoneId; FVector DeathPosition = FVector::ZeroVector, RespawnPosition = FVector::ZeroVector;
    // Housing
    FString State, StructureId, OwnerId; int64 StructureRevision = 0, Revision = 0, PermissionRevision = 0;
    bool bActorPermitted = false;  // GetClaim with server-bound CharacterId
    struct FStructure { FString StructureId, ContentId; FIntVector Cell = FIntVector::ZeroValue; int64 Revision = 0; FString ShapeKey; int32 OrientationIndex = 0; };
    TArray<FStructure> Structures;  // ListStructures; StructureRevision then holds the claim's structure revision
    TArray<uint8> ClaimMask; int32 MaskOriginX = 0, MaskOriginY = 0, MaskWidth = 0, MaskHeight = 0;
    // Crafting
    TMap<FString, int64> Changes;
    int32 QualityScore = 0; FString Grade, DiscoveredRecipeId;
    FSFExchangeResponse Exchange;
    TArray<FSFMarketListingResponse> MarketListings;
    // Gathering node snapshot; the dedicated server projects this into the replicated node state.
    bool bHasNode = false; FString NodeId, NodeDefinitionId, GrowthStage, HarvestStatus, ResourceId;
    int32 NodeGeneration = 0, NodeCharges = 0, NodeCapacity = 0, HarvestQuantity = 0;
    int64 NodeRegrowAtUnixMs = 0, NodeNextTransitionUnixMs = 0;
    FString NodePoolId, NodePointId; FVector NodePositionMetres = FVector::ZeroVector;
    double NodeFacingDegrees = 0; bool bNodeActive = false;
    struct FZoneNode { FString Id, DefinitionId, GrowthStage, PoolId, PointId; int32 Generation = 0, Charges = 0, Capacity = 0;
        int64 RegrowAtUnixMs = 0, NextTransitionUnixMs = 0; FVector PositionMetres = FVector::ZeroVector;
        double FacingDegrees = 0; bool bActive = false; };
    TArray<FZoneNode> ZoneNodes;
    struct FBoarSlot { FString GroupId, PointId; int32 SlotIndex = 0, Cycle = 0; bool bActive = false; double XMetres = 0, YMetres = 0, FacingDegrees = 0; };
    TArray<FBoarSlot> BoarSlots;
    FString GroupPartyId, GroupLeaderId, GroupAssistantId, GroupState;
    FString RaidGroupId;
    TArray<FString> RaidMembers;
    TArray<FString> GroupMembers;
    int32 GroupVersion = 0;
};

using FSFGameplaySend = TFunction<void(const FSFGameplayRequest&, float DeadlineSeconds, TFunction<void(const FSFGameplayResponse&)>)>;

class SHATTEREDFRONTIER_API FSFGameplayServicesClient : public TSharedFromThis<FSFGameplayServicesClient>
{
public:
    static void SetTransportFactory(TFunction<FSFGameplaySend(const FString& Endpoint)> Factory);
    static void SetSendOverrideForTests(FSFGameplaySend Send);
    // Null when disabled, when the backend module is absent (client builds) or settings are invalid.
    static TSharedPtr<FSFGameplayServicesClient> Create(const USFGameplayServicesSettings& Settings);
    // Retries Unavailable with backoff, reusing the request unchanged, up to MaxAttempts. Callback on the game thread.
    void Call(const FSFGameplayRequest& Request, TFunction<void(const FSFGameplayResponse&)> Done);
private:
    FSFGameplayServicesClient(FSFGameplaySend InSend, float InTimeout, int32 InAttempts) : Send(MoveTemp(InSend)), TimeoutSeconds(InTimeout), MaxAttempts(InAttempts) {}
    void Attempt(const FSFGameplayRequest& Request, int32 Number, TFunction<void(const FSFGameplayResponse&)> Done);
    FSFGameplaySend Send;
    float TimeoutSeconds;
    int32 MaxAttempts;
};
