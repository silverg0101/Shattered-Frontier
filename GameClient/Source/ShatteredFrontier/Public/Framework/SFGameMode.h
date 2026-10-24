#pragma once

#include "GameFramework/GameModeBase.h"
#include "Party/SFPartyModel.h"
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
    void TryBoarAttack(ASFPilotBoar* Boar, ASFE1Character* Victim);
    /** Authority: an NPC's landed melee hit on a player (boar, townsfolk, guard); the caller checks reach and cooldown.
      * A downed player respawns when this world's game mode runs the combat pilot. */
    static void HitPlayer(AActor* Source, ASFE1Character* Victim, int32 Damage);
    void RestoreDeadPlayer(ASFE1Character* Player, const struct FSFGameplayResponse& State);
    bool HasPendingDeathReport(const FString& CharacterId) const;
    void BoarDied(ASFPilotBoar* Boar, ASFE1Character* Killer);
    /** Grants the killer's hides for this death, at most once; returns the number granted. */
    int32 RewardKill(const FString& DeathId, const FVector& BoarAt, ASFE1Character* Killer);
    bool IsSafe(const FVector& Location) const;
    bool IsInsideTerritory(const FVector& Location, const TArray<FVector2D>& Polygon) const;
    bool IsCombatActive() const { return bPilotActive; }
    const USFCombatPilotSettings* CombatSettings() const { return CapturedSettings; }

private:
    FSFPartyModel PartyModel;
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
    bool LoadZoneRecord();
    bool RunWorldSlots(const FString& Command, const FString& Group = FString(), int32 Slot = INDEX_NONE);
    void SpawnSlot(const FString& Group, int32 Slot, int32 Cycle, const FString& PointId,
        double X, double Y, double Facing);
    void RespawnPlayer(TWeakObjectPtr<ASFE1Character> Player);
    FVector ChooseRespawnPoint(const FVector& DeathLocation) const;
    void ReportPlayerDeath(TWeakObjectPtr<ASFE1Character> Player, const struct FSFGameplayRequest& Request);
    struct FBoarGroup { TArray<FVector2D> Polygon; };
    TMap<FString, FBoarGroup> Groups;
    TArray<TArray<FVector2D>> ProtectedAreas;
    TArray<FVector> RespawnPoints;
    TMap<FString, TWeakObjectPtr<ASFPilotBoar>> ActiveBoars;
    TMap<TWeakObjectPtr<ASFE1Character>, double> PlayerNextSwing;
    TMap<TWeakObjectPtr<ASFE1Character>, FString> PlayerDeathEvents;
    TMap<TWeakObjectPtr<ASFE1Character>, FVector> PlayerDeathRespawnPoints;
    TSet<TWeakObjectPtr<ASFE1Character>> PlayerRespawnInFlight;
    TSet<FString> PendingDeathReports;
    TSet<FString> RewardedDeaths;
    void RetryDefeatReward(const FString& DeathId, const FString& ServiceEventId, const FString& CharacterId,
        const FString& ContainerId, bool bKillerSafe, bool bBoarSafe, TWeakObjectPtr<APlayerController> Controller);
    int32 NextBoarSerial = 0;
    FString SlotStatePath;
    const USFCombatPilotSettings* CapturedSettings = nullptr;
    bool bPilotActive = false;
    bool bCompanionSandbox = false;
    bool bSlotBridgeHealthy = false;
    bool bSlotServiceMode = false;
    bool bSlotRequestInFlight = false;
    bool bSlotReadyLogged = false;
    TArray<TPair<FString, int32>> PendingSlotDefeats;
    double NextSlotTick = 0;
    bool bScenario = false;
    bool bScenarioFirstBoarDied = false;
    bool bReconnectDeathTriggered = false;
};
