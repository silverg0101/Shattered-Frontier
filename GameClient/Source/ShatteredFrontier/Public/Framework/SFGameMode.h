#pragma once

#include "GameFramework/GameModeBase.h"
#include "Party/SFPartyModel.h"
#include "Math/RandomStream.h"
#include "SFGameMode.generated.h"

class ASFPlayerController;
class ASFConstructionNetworkState;
class ASFE1Character;
class ASFPilotBoar;
class USFCombatPilotSettings;
enum class ESFConstructionNetworkResult : uint8;

// GM-01: the one game mode. Member bodies stay in their feature .cpp: spawn and lifecycle here, construction and
// zone gathering in SFConstructionNetwork.cpp, combat in SFCombatPilot.cpp. All of it is server authority.
UCLASS()
class SHATTEREDFRONTIER_API ASFGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASFGameMode();
    virtual void StartPlay() override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal = TEXT("")) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void Tick(float DeltaSeconds) override;
    // Premade session parties. The controller supplies an intent; identity and target resolution stay on this server.
    void PartyAction(class ASFPlayerController* Actor, uint8 Action, const FString& TargetName);

    // Construction (SFConstructionNetwork.cpp)
    bool IsAdmitted() const;
    bool IsZoneGatheringWorld() const;
    bool IsBuilder(const ASFPlayerController* Controller) const;
    // Zone-world development claim (BLD-02); false in the sandbox. Centre in zone metres, as ClaimTerritory takes it.
    // BLD-02 A2: one claim state per zone build plot (area purpose "claims"), owned through Housing; none in the sandbox.
    bool IsHousingActive() const;
    ASFConstructionNetworkState* FindClaimState(const FString& ClaimId) const;
    /** Server-observed location inside a loaded Housing claim containing built structure cells. Rights are checked by Housing. */
    FString RestHouseClaimAt(const FVector& WorldPosition) const;
    // Housing.ClaimPlot for the controller's character; Claimed when it already owns the plot.
    void ClaimPlot(ASFPlayerController* Controller, const FString& ClaimId, TFunction<void(ESFConstructionNetworkResult)> Done);
    // Housing.Grant of build rights on the controller's claim to another character.
    void GrantBuild(ASFPlayerController* Controller, const FString& ClaimId, const FString& TargetCharacterId, TFunction<void(ESFConstructionNetworkResult)> Done);
    void CommitHousing(ASFPlayerController* Controller, const FString& ClaimId, FIntVector Cell, bool bRemove, int32 Shape, int32 Rotation, TFunction<void(ESFConstructionNetworkResult Result, const FString& Operation)> Done);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void SpawnFiniteDoorObstruction(const FIntVector& Origin, uint8 Yaw);
    void DiscardFiniteDoorObstruction();
#endif

    // Combat (SFCombatPilot.cpp)
    void TryPlayerAttack(ASFE1Character* Attacker, AActor* SelectedTarget = nullptr);
    bool TryPlayerTaunt(ASFE1Character* Taunter, AActor* SelectedTarget);
    void TryBoarAttack(ASFPilotBoar* Boar, ASFE1Character* Victim);
    /** Authority: an NPC's landed melee hit on a player (boar, townsfolk, guard); the caller checks reach and cooldown.
      * A downed player respawns when this world's game mode runs the combat pilot. */
    static void HitPlayer(AActor* Source, ASFE1Character* Victim, int32 Damage, bool bIgnoreArmor = false);
    /** Server-only physical environmental hit from an actual landing. */
    void ApplyFallDamage(ASFE1Character* Victim, float ImpactSpeedMps);
    void RestoreDeadPlayer(ASFE1Character* Player, const struct FSFGameplayResponse& State);
    bool ReleaseSpirit(ASFE1Character* Player);
    bool AcceptResurrection(ASFE1Character* Player, uint32 OfferId, const FString& DeathEventId);
    bool HasPendingDeathReport(const FString& CharacterId) const;
    void BoarDied(ASFPilotBoar* Boar, ASFE1Character* Killer);
    /** Grants the killer's hides for this death, at most once; returns the number granted. */
    int32 RewardKill(const FString& DeathId, const FVector& BoarAt, ASFE1Character* Killer);
    bool IsSafe(const FVector& Location) const;
    bool IsInsideTerritory(const FVector& Location, const TArray<FVector2D>& Polygon) const;
    bool IsCombatActive() const { return bPilotActive; }
    const USFCombatPilotSettings* CombatSettings() const { return CapturedSettings; }
    void RecordBoarEvade(const FString& Group, const FString& CharacterId);

private:
    TMap<FString, FString> PendingGatewayCharacters;
    TMap<FString, FString> PendingGatewayAccounts, PendingGatewayRealms;
    bool UsesGatewayAdmission() const;
    FString TakeGatewayCharacter(const FString& Ticket, FString& Account, FString& Realm);
    FSFPartyModel PartyModel;
    TMap<int32, FString> DurablePartyBySessionId;
    TMap<FString, int32> SessionPartyByDurableId;
    TSet<int32> PendingPartyActions;
    void RestorePartyForPlayer(class ASFPlayerController* Player);
    void RefreshPartyViews();
    class ASFPlayerController* FindPartyPlayer(const FString& Name) const;
    // Opt-in M3 group capture; Unreal CSV supplies frame times, while net totals come from the driver's built-in counters.
    struct FM3ConnectionSample
    {
        int32 Id = 0;
        int32 InBytes = 0, OutBytes = 0;
        int32 LastInBytes = 0, LastOutBytes = 0;
        double JoinedAt = 0;
        double LastAt = 0;
    };
    TMap<TWeakObjectPtr<class UNetConnection>, FM3ConnectionSample> M3Connections;
    TArray<FString> M3FinishedConnections;
    double M3StartedAt = 0;
    uint32 M3DriverInBytes = 0, M3DriverOutBytes = 0;
    int32 M3NextConnectionId = 0;
    bool bM3CaptureFinished = false;
    void RecordM3Connection(class UNetConnection* Connection);
    void FinishM3Connection(class UNetConnection* Connection);
    void FinishM3Capture();

    // Spawn and lifecycle (SFGameMode.cpp)
    uint64 LifecycleAuditSequence = 0;
    TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<APawn>> LifecyclePawns;
    TMap<TWeakObjectPtr<AController>, FString> LifecyclePawnNames;
    void ScheduleLifecycleAudit(AController* Trigger, bool bLeaving);

    // Construction
    void StartConstruction();
    void PostLoginConstruction(APlayerController* NewPlayer);
    void RestartConstruction(AController* NewPlayer);
    int32 SpawnZoneGatheringNodes();
    void RefreshZoneGatheringNodes();
    struct FClaimPlot
    {
        FString ClaimId;
        FVector2D CentreMetres = FVector2D::ZeroVector;
        int32 SideMetres = 20;
        TWeakObjectPtr<ASFConstructionNetworkState> State;
        bool bLoaded = false, bClaimed = false;  // loaded: Housing answered GetClaim (and ListStructures) since start
        FString OwnerId;
        int64 PermissionRevision = 0, StructureRevision = 0;
        TMap<FIntVector, FString> Structures;  // cell -> housing structure id, for removals
    };
    TArray<FClaimPlot> ClaimPlots;
    int32 PrepareClaimPlots();
    void LoadClaims();
    FClaimPlot* FindPlot(const FString& ClaimId);
    FTimerHandle ClaimLoadTimer;
    void PlaceForZoneScenario(APawn* Pawn, bool bPrimary) const;
    TWeakObjectPtr<ASFPlayerController> Builder;
    bool bBuilderAssigned = false;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TWeakObjectPtr<AActor> FiniteDoorObstruction;
    bool bFiniteObstructionSpawned = false;
#endif

    // Combat
    void StartCombat();
    void PostLoginCombat(APlayerController* NewPlayer);
    void RestartCombat(AController* NewPlayer);
    void TickCombat();
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickGreenlandsNetcheck();
#endif
    bool LoadZoneRecord();
    bool RunWorldSlots(const FString& Command, const FString& Group = FString(), int32 Slot = INDEX_NONE);
    void SpawnSlot(const FString& Group, int32 Slot, int32 Cycle, const FString& PointId,
        double X, double Y, double Facing);
    void RespawnPlayer(TWeakObjectPtr<ASFE1Character> Player, int32 RestoredHealth = -1, bool bAtDeathPosition = false);
    FVector ChooseRespawnPoint(const FVector& DeathLocation) const;
    void ReportPlayerDeath(TWeakObjectPtr<ASFE1Character> Player, const struct FSFGameplayRequest& Request);
    struct FBoarGroup { TArray<FVector2D> Polygon; };
    TMap<FString, FBoarGroup> Groups;
    TArray<TArray<FVector2D>> ProtectedAreas;
    TArray<FVector> RespawnPoints;
    TMap<FString, TWeakObjectPtr<ASFPilotBoar>> ActiveBoars;
    TMap<FString, TArray<double>> LeashEvadesByGroupAndCharacter;
    TMap<TWeakObjectPtr<ASFE1Character>, double> PlayerNextSwing;
    TMap<TWeakObjectPtr<ASFE1Character>, FRandomStream> PlayerCombatStreams;
    TMap<TWeakObjectPtr<ASFE1Character>, int32> DodgeScenarioSwings;
    TMap<TWeakObjectPtr<ASFE1Character>, FString> PlayerDeathEvents;
    TMap<TWeakObjectPtr<ASFE1Character>, FVector> PlayerDeathRespawnPoints;
    TSet<TWeakObjectPtr<ASFE1Character>> PlayerRespawnInFlight;
    TSet<FString> PendingDeathReports;
    TSet<FString> RewardedDeaths;
    TSet<FString> CreditedXpDeaths;
    void RetryDefeatReward(const FString& DeathId, const FString& ServiceEventId, const FString& CharacterId,
        const FString& ContainerId, bool bKillerSafe, bool bBoarSafe, TWeakObjectPtr<APlayerController> Controller);
    int32 NextBoarSerial = 0;
    FString SlotStatePath;
    const USFCombatPilotSettings* CapturedSettings = nullptr;
    bool bPilotActive = false;
    bool bCompanionSandbox = false;
    bool bRespawnSandbox = false;
    TArray<FTransform> RespawnSandboxPoints;
    double RespawnSandboxDueAt = -1.0;
    int32 RespawnSandboxCycle = 0;
    bool bSlotBridgeHealthy = false;
    bool bSlotServiceMode = false;
    bool bSlotRequestInFlight = false;
    bool bSlotReadyLogged = false;
    TArray<TPair<FString, int32>> PendingSlotDefeats;
    double NextSlotTick = 0;
    bool bScenario = false;
    bool bScenarioFirstBoarDied = false;
    bool bReconnectDeathTriggered = false;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    void TickGreenlandsDeathNetcheck();
    int32 GreenlandsDeathStage = 0;
    double GreenlandsDeathNextHitAt = 0;
    TWeakObjectPtr<ASFPilotBoar> GreenlandsDeathBoar;
    TWeakObjectPtr<APlayerController> GreenlandsDriverController;
    TWeakObjectPtr<APlayerController> GreenlandsObserverController;
    TWeakObjectPtr<ASFPilotBoar> GreenlandsLead;
    TWeakObjectPtr<ASFPilotBoar> GreenlandsMate;
    FVector GreenlandsLeadAnchor = FVector::ZeroVector;
    FVector GreenlandsMateAnchor = FVector::ZeroVector;
    int32 GreenlandsNetcheckStage = 0;
    int32 GreenlandsLeadSlot = -1;
    int32 GreenlandsMateSlot = -1;
    int32 GreenlandsRegenHealth = 0;
    bool bGreenlandsRegenObserved = false;
    double GreenlandsStageAt = 0;
#endif
};
