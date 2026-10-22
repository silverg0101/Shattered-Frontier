#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFTownsfolk.generated.h"

class UAnimSequence;
class USmartObjectDefinition;
class USkeletalMesh;
class UMassEntityConfigAsset;
class USkeletalMeshComponent;
class ASFE1Character;
class AGameModeBase;
class APlayerController;
class USFCrowdControlComponent;
class ASFTownsfolkNpc;
class ASFTownGuard;

// Townsfolk prototype, AI Townsfolk doc section 9 steps 2-3: census residents walking their day on ZoneGraph footways
// across the town, drawn by MassRepresentation, stepping aside for each other and the player.
// On by typed config (bEnabled) in Greenlands_Silverbrook; client-simulated, never on a dedicated server, which
// owns only the replicated town state (step 4).

/** A body for one or more census archetypes: its own skeletal mesh with the walk and idle clips made for that skeleton.
  * Archetypes with no entry keep the UE4 mannequin. Mesh paths are content assets; YawOffset turns the mesh to face +X. */
USTRUCT()
struct FSFTownsfolkLook
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, Category="Look", meta=(ToolTip="Census archetype ids from townsfolk-archetypes.json that wear this body."))
    TArray<FName> Archetypes;
    UPROPERTY(EditAnywhere, Category="Look", meta=(AllowedClasses="/Script/Engine.SkeletalMesh"))
    FSoftObjectPath Mesh;
    UPROPERTY(EditAnywhere, Category="Look", meta=(AllowedClasses="/Script/Engine.AnimSequence"))
    FSoftObjectPath Walk;
    UPROPERTY(EditAnywhere, Category="Look", meta=(AllowedClasses="/Script/Engine.AnimSequence"))
    FSoftObjectPath Idle;
    UPROPERTY(EditAnywhere, Category="Look", meta=(ClampMin="-180.0", ClampMax="180.0", Units="deg"))
    float YawOffset = 0.0f;
    UPROPERTY(EditAnywhere, Category="Look", meta=(ClampMin="50.0", ClampMax="400.0", Units="cm/s", ToolTip="Ground speed the walk clip covers at play rate 1; the play rate scales to the resident's speed."))
    float WalkClipCmPerSecond = 150.0f;
};

/** A Look with its assets loaded. */
struct FSFTownsfolkResolvedLook
{
    USkeletalMesh* Mesh=nullptr; UAnimSequence* Walk=nullptr; UAnimSequence* Idle=nullptr;
    float YawOffset=0, WalkClipCmPerSecond=150;
};

// Release-controlled prototype settings, read at world start; no live overrides.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Townsfolk Prototype"))
class SHATTEREDFRONTIER_API USFTownsfolkSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ToolTip="GM-01: simulate residents on clients in Silverbrook. Default on; next world start."))
    bool bEnabled = true;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="1", ClampMax="300"))
    int32 ResidentCap = 50;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="1.0", ClampMax="240.0", Units="min", ToolTip="Real minutes per game day for the prototype clock; the world clock's day is 60."))
    float GameDayMinutes = 60.0f;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="0.0", ClampMax="23.9", ToolTip="Game hour when the world starts."))
    float StartHour = 6.0f;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="0.5", ClampMax="3.0", Units="m/s", ToolTip="Walking speed at the 60-minute day; a faster prototype clock scales it."))
    float WalkSpeedMetresPerSecond = 1.4f;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ToolTip="Census district to draw residents from; empty takes them evenly from the whole town."))
    FString District;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ToolTip="Bodies by archetype; archetypes not listed keep the UE4 mannequin placeholder."))
    TArray<FSFTownsfolkLook> Looks;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="0.0", ClampMax="2.0", Units="m", ToolTip="Furthest a resident steps aside from the footway to pass someone."))
    float SidestepMaxMetres = 1.5f;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="0.5", ClampMax="2.0", Units="m", ToolTip="Distance inside which a resident steps away from another (the neighbour grid reaches 2 m)."))
    float SidestepRadiusMetres = 1.2f;

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="0.1", ClampMax="2.0", ToolTip="Sideways speed of a full sidestep as a share of walking speed."))
    float SidestepSpeedShare = 0.6f;

    UPROPERTY(Config, EditAnywhere, Category="Smart Objects", meta=(ClampMin="0.2", ClampMax="10.0", Units="s", ToolTip="Real seconds a resident holds a door's slot while going in or coming out."))
    float DoorDwellSeconds = 1.5f;

    UPROPERTY(Config, EditAnywhere, Category="Smart Objects", meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="Share of workers (by a seeded daily roll) who go inside their workplace through its door; the rest stand outside it."))
    float WorkIndoorShare = 0.5f;

    UPROPERTY(Config, EditAnywhere, Category="Smart Objects", meta=(ClampMin="0.0", ClampMax="1.0", ToolTip="Share of market errands (by a seeded daily roll) spent sitting on a bench near the market instead of standing at its stall."))
    float BenchShare = 0.3f;

    UPROPERTY(Config, EditAnywhere, Category="Smart Objects", meta=(ClampMin="5.0", ClampMax="100.0", Units="m", ToolTip="How far from a market a bench may be and still be chosen for its errand."))
    float BenchSearchMetres = 40.0f;

    UPROPERTY(Config, EditAnywhere, Category="Town state", meta=(ClampMin="0.0", ClampMax="24.0", ToolTip="Game hour the server opens the markets; residents skip errands to a closed market."))
    float MarketOpenHour = 7.0f;

    UPROPERTY(Config, EditAnywhere, Category="Town state", meta=(ClampMin="0.0", ClampMax="24.0", ToolTip="Game hour the server closes the markets."))
    float MarketCloseHour = 19.0f;

    UPROPERTY(Config, EditAnywhere, Category="Town state", meta=(ClampMin="10.0", ClampMax="200.0", Units="m", ToolTip="Side of a danger cell; residents avoid work or errands inside a dangerous cell."))
    float DangerCellMetres = 50.0f;

    UPROPERTY(Config, EditAnywhere, Category="Town state", meta=(ClampMin="5.0", ClampMax="1800.0", Units="s", ToolTip="Real seconds a danger cell stays dangerous after the server marks it."))
    float DangerSeconds = 120.0f;

    UPROPERTY(Config, EditAnywhere, Category="Hand-off", meta=(ClampMin="2.0", ClampMax="40.0", Units="m", ToolTip="tune.townsfolk.handoff_tolerance_m: how far a client's seen position may be from the resident's scheduled route before the server drops the request."))
    float HandoffToleranceMetres = 12.0f;

    UPROPERTY(Config, EditAnywhere, Category="Hand-off", meta=(ClampMin="2.0", ClampMax="100.0", Units="m", ToolTip="tune.townsfolk.handoff_release_m: a handed-off resident returns to its schedule only with no player this close."))
    float HandoffReleaseMetres = 30.0f;

    UPROPERTY(Config, EditAnywhere, Category="Hand-off", meta=(ClampMin="2.0", ClampMax="120.0", Units="s", ToolTip="Real seconds without a hit before an attacked resident's hand-off may end."))
    float HandoffQuietSeconds = 10.0f;

    UPROPERTY(Config, EditAnywhere, Category="Hand-off", meta=(ClampMin="1", ClampMax="64", ToolTip="Most residents handed off at once in the zone."))
    int32 MaxHandoffs = 16;

    UPROPERTY(Config, EditAnywhere, Category="Hand-off", meta=(ClampMin="1", ClampMax="500", ToolTip="Health of a handed-off resident (pilot scale: player melee does 20)."))
    int32 ResidentHealth = 40;

    UPROPERTY(Config, EditAnywhere, Category="Hand-off", meta=(ClampMin="1.0", ClampMax="30.0", Units="s", ToolTip="Real seconds an attacked resident runs from the attacker after each hit (section 5.4)."))
    float FleeSeconds = 6.0f;

    UPROPERTY(Config, EditAnywhere, Category="Hand-off", meta=(ClampMin="1.0", ClampMax="10.0", Units="m/s"))
    float FleeSpeedMetresPerSecond = 4.5f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="0", ClampMax="100", Units="%", ToolTip="tune.townsfolk.flee_health_pct: an attacked resident fights back above this share of its health and flees at or below it."))
    int32 FleeHealthPct = 35;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="1.0", ClampMax="30.0", Units="s", ToolTip="Real seconds a resident fights back after each hit before it breaks off and flees."))
    float FightBackSeconds = 6.0f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="1", ClampMax="100", ToolTip="The townsfolk combat profile's bare-fist damage (pilot scale: player health 80)."))
    int32 ResidentMeleeDamage = 4;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="0.5", ClampMax="5.0", Units="s"))
    float ResidentMeleeCooldownSeconds = 1.6f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="30.0", ClampMax="3600.0", Units="s", ToolTip="tune.ai.guard_grudge_s: how long a witnessed crime keeps the player an offender in the zone."))
    float GuardGrudgeSeconds = 600.0f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="5.0", ClampMax="60.0", Units="m", ToolTip="A guard witnesses a crime, or spots an offender, this close and in line of sight."))
    float GuardSightMetres = 30.0f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="10.0", ClampMax="300.0", Units="m", ToolTip="An unwitnessed victim runs to report to the nearest guard this close; a crime further from every guard goes unreported."))
    float ReportMaxMetres = 80.0f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="1.0", ClampMax="10.0", Units="m", ToolTip="How close a reporting resident gets to the guard before the report counts."))
    float ReportReachMetres = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="1", ClampMax="100"))
    int32 GuardMeleeDamage = 18;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="0.5", ClampMax="5.0", Units="s"))
    float GuardMeleeCooldownSeconds = 1.4f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="0.5", ClampMax="3.0", Units="m/s"))
    float GuardPatrolSpeedMetresPerSecond = 1.4f;

    UPROPERTY(Config, EditAnywhere, Category="Crime and guards", meta=(ClampMin="1.0", ClampMax="10.0", Units="m/s"))
    float GuardChaseSpeedMetresPerSecond = 5.0f;

    bool Validate(FString& OutError) const;
};

enum class ESFTownsfolkGoal : uint8 { None, Home, Work, Market };

/** Kinds of Smart Object residents claim: a building door (enter or leave), a market stall (stand), a bench (sit). */
enum class ESFTownsfolkSpot : uint8 { Door, Stall, Bench };

/** One Smart Object from silverbrook-smartobjects.json; At is its origin in cm, Facing the way it faces in degrees. */
struct FSFTownsfolkSpot
{
    FSmartObjectHandle Handle;
    FVector At=FVector::ZeroVector;
    float Facing=0;
};

/** One census resident's day, from silverbrook-census.json. Hours are game hours. */
struct FSFTownsfolkResident
{
    FString Id;
    FName Archetype;
    FVector Home=FVector::ZeroVector, Work=FVector::ZeroVector;
    FString HomeLabel, WorkLabel;  // the buildings whose doors they use
    bool bHasWork=false;
    TArray<FVector> Markets;
    TArray<FString> MarketLabels;  // parallel to Markets: the stall each is
    TArray<int32> MarketBits;  // parallel to Markets: its index in the census markets, the bit in ASFTownState's open mask
    float Wake=6, WorkStart=8, WorkEnd=16, Sleep=22, ErrandChance=0.5f;
    uint32 Seed=0;
    /** An errand to a market closed in OpenMarkets stays home. */
    ESFTownsfolkGoal GoalAt(float Hour, int32 Day, int32& OutMarket, uint32 OpenMarkets=~0u) const;
};

USTRUCT()
struct FSFTownDangerCell
{
    GENERATED_BODY()
    UPROPERTY() FIntPoint Cell=FIntPoint::ZeroValue;
    UPROPERTY() double Until=0;  // server world seconds
};

/** A player who committed a witnessed crime (doc section 5.5): its player state, or the pawn when it has none. */
USTRUCT()
struct FSFTownOffender
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<AActor> Who;
    UPROPERTY() double Until=0;  // server world seconds
};

/** Crime rules (doc section 5.5), kept free of actors so tests can drive them. */
namespace SFTownCrime
{
    /** Offenders are keyed by player state so the status survives a respawned pawn. */
    SHATTEREDFRONTIER_API const AActor* OffenderKey(const AActor* Attacker);
    SHATTEREDFRONTIER_API bool IsOffender(const TArray<FSFTownOffender>& Offenders, const AActor* Key, double Now);
    SHATTEREDFRONTIER_API void Mark(TArray<FSFTownOffender>& Offenders, const AActor* Key, double Until);
    /** An attacked resident fights back above FleePct of its health and flees at or below it. */
    SHATTEREDFRONTIER_API bool FightsBack(int32 Health, int32 MaxHealth, int32 FleePct);
}

/** Server-owned town state (AI Townsfolk doc section 5.1), replicated to every client: the town clock, which markets are
  * open and the danger cells. A few bytes per change and nothing per resident; clients derive the hour from the
  * GameState's synced server time, so the clock costs no traffic once it starts. Spawned by USFTownsfolkSubsystem on the
  * authority of Greenlands_Silverbrook. */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFTownState : public AActor
{
    GENERATED_BODY()
public:
    ASFTownState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    /** Authority: starts the clock and market hours from validated settings. */
    void Start(const USFTownsfolkSettings& Settings);
    float Hour(int32& OutDay) const;
    float DayMinutes() const { return SecondsPerGameHour*24.0f/60.0f; }
    uint32 OpenMask() const { return uint32(OpenMarkets); }
    bool IsDanger(const FVector& At) const;
    int32 DangerCount() const;
    /** Authority: the cell around At (cm) is dangerous for the settings' DangerSeconds. */
    void MarkDanger(const FVector& At);
    /** The world's town state, or null. */
    static ASFTownState* Find(const UWorld* World);
    /** Any machine: is the attacker (or its player) an offender in this zone? */
    bool IsOffender(const AActor* Attacker) const;
    /** Authority: the attacker is an offender for GuardGrudgeSeconds; Why is logged (witnessed, reported). */
    void MarkOffender(const AActor* Attacker, const TCHAR* Why);
    /** Authority: Attacker hit Victim. A guard who sees either makes the attacker an offender now. Otherwise returns the
      * nearest guard within ReportMaxMetres for the victim to run and tell, or null: the crime goes unreported. */
    ASFTownGuard* ReportCrime(const AActor* Attacker, const AActor* Victim);
private:
    double ServerSeconds() const;
    UPROPERTY(Replicated) float StartHour=6;
    UPROPERTY(Replicated) double StartServerSeconds=0;
    UPROPERTY(Replicated) float SecondsPerGameHour=150;
    UPROPERTY(Replicated) float DangerCellCm=5000;
    UPROPERTY(Replicated) int32 OpenMarkets=0;  // bit per census market; all open or all closed by the hours today
    UPROPERTY(Replicated) TArray<FSFTownDangerCell> Danger;
    UPROPERTY(Replicated) TArray<FSFTownOffender> Offenders;  // zone memory, never persisted (NPC Behavior section 6.3)
    float OpenHour=7, CloseHour=19, DangerSeconds=120;
};

/** A handed-off resident (doc section 2): a server-authoritative actor replicated to every client while gameplay needs that
  * resident (an attack, or a quest step). Each client hides its own Mass copy while this exists and resumes it from here
  * when the server releases it. An attacked resident fights back, then flees or runs to tell a guard (section 5.5). Moves kinematically
  * like the combat pilot's boar. */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFTownsfolkNpc : public ACharacter
{
    GENERATED_BODY()
public:
    ASFTownsfolkNpc();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    /** Authority, before BeginPlay. */
    void Init(const FString& Id, FName InArchetype, int32 Serial, int32 Health, bool bQuest);
    /** Authority: takes the hit, reports the crime, then fights back or flees; at 0 health it is gone until the next dawn. */
    void ReceiveHit(const AActor* Attacker, int32 Damage);
    const FString& Id() const { return ResidentId; }
    int32 Serial() const { return HandoffSerial; }
    int32 Health() const { return CurrentHealth; }
    bool IsFighting() const { return GetWorld()->GetTimeSeconds()<FightUntil; }
    bool IsReporting() const { return ReportTo.IsValid(); }
    int32 QuestHolds=0;  // authority: quest players holding it (they share one resident); released at 0
    FName ArchetypeName() const { return Archetype; }
    double LastActiveSeconds=0;  // authority: last hit or hand-off
    int32 DeathDay=-1;  // authority: game day it died on
    UPROPERTY(VisibleAnywhere) TObjectPtr<USFCrowdControlComponent> CrowdControl;
private:
    UPROPERTY(Replicated) FString ResidentId;
    UPROPERTY(Replicated) FName Archetype;
    UPROPERTY(Replicated) int32 HandoffSerial=0;
    UPROPERTY(Replicated) int32 CurrentHealth=0;
    FVector FleeFrom=FVector::ZeroVector; double FleeUntil=0, FightUntil=0, NextSwingAt=0; int32 MaxHealth=0;
    TWeakObjectPtr<AActor> Foe;  // whom it fights back
    TWeakObjectPtr<ASFTownGuard> ReportTo; TWeakObjectPtr<const AActor> CrimeBy;  // an unwitnessed crime it runs to report
    void Step(const FVector& Direction, float SpeedCm, float DeltaSeconds);
    FVector LastAt=FVector::ZeroVector; int8 Playing=-1; bool bLookApplied=false;
    UPROPERTY() TObjectPtr<UAnimSequence> Walk;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
};

/** A town guard (doc section 5.5): a server NPC replicated to every client. It walks its patrol loop, witnesses crimes
  * it sees, and chases and strikes any offender it sees until the offender is down, out of reach or forgiven.
  * ponytail: guards can't be attacked yet and the spawner hands in the patrol; lane-generated loops come with the
  * Silverbrook wiring. */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFTownGuard : public ACharacter
{
    GENERATED_BODY()
public:
    ASFTownGuard();
    virtual void Tick(float DeltaSeconds) override;
    /** Authority: the loop it walks (cm). */
    void SetPatrol(const TArray<FVector>& Points) { Patrol=Points; NextPoint=0; }
    /** Is Who within GuardSightMetres and in line of sight? */
    bool Sees(const AActor* Who) const;
    ASFE1Character* Engaged() const { return Target.Get(); }
    int32 Hits=0;  // authority: strikes landed, for the dev scenario
private:
    TArray<FVector> Patrol; int32 NextPoint=0;
    TWeakObjectPtr<ASFE1Character> Target; double NextSwingAt=0;
    FVector LastAt=FVector::ZeroVector; int8 Playing=-1; bool bLookApplied=false;
    UPROPERTY() TObjectPtr<UAnimSequence> Walk;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
};

/** Added by the server to each player controller: the client's requests to attack a resident (doc section 5.3). */
UCLASS()
class SHATTEREDFRONTIER_API USFTownsfolkHandoffComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFTownsfolkHandoffComponent();
    /** The client sees resident ResidentId at SeenAt (cm) and swings at it; the server checks it against the schedule. */
    UFUNCTION(Server, Reliable) void ServerAttackResident(const FString& ResidentId, FVector_NetQuantize SeenAt);
    UFUNCTION(Server, Reliable) void ServerAttackNpc(ASFTownsfolkNpc* Npc);
    UFUNCTION(Server, Reliable) void ServerAttackSelectedNpc(ASFTownsfolkNpc* Npc);
};

/** The SmartObject subsystem on NM_Client, where the engine creates none (it holds server-authoritative Smart Objects).
  * Client-simulated residents (doc section 5.2) create their ambient door, stall and bench objects here and claim them
  * locally; those claims are cosmetic and never gameplay. Server and standalone worlds keep the engine's own subsystem. */
UCLASS()
class SHATTEREDFRONTIER_API USFClientSmartObjectSubsystem : public USmartObjectSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
};

USTRUCT()
struct FSFTownsfolkResidentFragment : public FMassFragment
{
    GENERATED_BODY()
    int32 Resident=INDEX_NONE;
    ESFTownsfolkGoal Goal=ESFTownsfolkGoal::None;
    int32 Market=INDEX_NONE;
    int32 Next=0;  // next point of this resident's route (USFTownsfolkSubsystem::Route)
    bool bInside=true;
    ESFTownsfolkGoal InsideGoal=ESFTownsfolkGoal::Home;  // which building (home or work) they are in while bInside
    uint8 Phase=0;  // 0 on the move or standing, 1 inside waiting for the door, 2 coming out of it, 3 going in
    bool bEnter=false;  // the route ends at a door they go through
    bool bFace=false;  // they stand facing FaceYaw at the end of the route (a stall or bench)
    float Wait=0;  // real seconds left in a door phase
    float FaceYaw=0;
    FVector OnRoute=FVector::ZeroVector;  // where the route has got to; the shown transform adds the sidestep
    FVector Forward=FVector::ForwardVector;
    float Side=0;  // cm to the right of the route
};

UCLASS(meta=(DisplayName="Townsfolk Resident"))
class SHATTEREDFRONTIER_API USFTownsfolkTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()
protected:
    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

/** A resident's body, spawned by MassRepresentation within 150 m: the UE4 mannequin, walking or standing.
  * ponytail: placeholder look and no far instances; MetaHuman Crowds kits replace it (doc section 9 step 3). */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API ASFTownsfolkBody : public AActor
{
    GENERATED_BODY()
public:
    ASFTownsfolkBody();
    /** Look null is the mannequin; a body recycled between archetypes changes mesh and clips when the look changes. */
    void Pose(const FVector& At, float Yaw, bool bWalking, float WalkSpeedCm, bool bIndoors, const FSFTownsfolkResolvedLook* Look);
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UAnimSequence> Walk;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    int8 Playing=-1;  // 1 walk, 0 idle, -1 nothing yet
    const FSFTownsfolkResolvedLook* Applied=nullptr;
    UPROPERTY() TObjectPtr<USkeletalMesh> DefaultMesh;
};

/** Picks each resident's goal from the clock, plans a footway route when it changes, and walks it. */
UCLASS()
class SHATTEREDFRONTIER_API USFTownsfolkProcessor : public UMassProcessor
{
    GENERATED_BODY()
public:
    USFTownsfolkProcessor();
protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
    FMassEntityQuery EntityQuery;
};

UCLASS()
class SHATTEREDFRONTIER_API USFTownsfolkSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;

    bool IsActive() const { return bActive; }
    /** Residents wait for the replicated town state; the clock and walking speed come from it. */
    bool HasState() const { return State!=nullptr; }
    void SetState(ASFTownState* InState) { State=InState; }
    ASFTownState* TownState() const { return State; }
    float GameHour(int32& OutDay) const { return State->Hour(OutDay); }
    float WalkSpeedCm() const { return WalkSpeedMetres*100.0f*FMath::Max(1.0f,60.0f/State->DayMinutes()); }
    /** The resident's goal with the town state applied: closed markets and danger cells send them home. */
    ESFTownsfolkGoal GoalFor(int32 Resident, float Hour, int32 Day, int32& OutMarket) const;
    const FSFTownsfolkResident& Resident(int32 Index) const { return Residents[Index]; }
    TArray<FVector>& Route(int32 Index) { return Routes[Index]; }
    const FSFTownsfolkResolvedLook* LookFor(FName Archetype) const { return Looks.Find(Archetype); }
    /** bExact walks all the way to To; otherwise the route stops at most 4 m off the footway. */
    bool PlanRoute(const FVector& From, const FVector& To, TArray<FVector>& OutRoute, bool bExact=false) const;
    /** Where a resident goes for a goal, with seeded choices: through a door, to a stall slot or a bench slot (claimed here,
      * held until ReleaseStand). Sets the fragment's bEnter, bFace and FaceYaw. Falls back to the census point (bScatter). */
    void PlanStop(int32 Resident, ESFTownsfolkGoal Goal, int32 Market, int32 Day, FSFTownsfolkResidentFragment& W, FVector& OutTo, bool& bOutExact, bool& bOutScatter);
    const FSFTownsfolkSpot* DoorFor(int32 Resident, ESFTownsfolkGoal Building) const;
    /** Claims a free slot of the door; OutAt in cm and OutYaw the slot's facing (toward the door). */
    bool ClaimDoor(int32 Resident, const FSFTownsfolkSpot& Door, FVector& OutAt, float& OutYaw);
    void ReleaseDoor(int32 Resident);
    void ReleaseStand(int32 Resident);
    float DoorDwell() const { return DoorDwellSeconds; }
    /** Records where a resident is shown this frame, for the next frame's sidesteps and the review. */
    void Note(int32 Index, const FVector& At, const FVector& Forward, bool bOutdoors);
    /** How far right of its route (cm) a resident should step this frame to pass the others and the player. */
    float Sidestep(int32 Index, float Side, float DeltaSeconds) const;
    void BeginFrame(float DeltaSeconds);
    void NoteFrame(double Milliseconds);
    TArray<FVector> OutdoorPositions() const;

    // Hand-off (doc sections 2 and 5.3).
    /** Client: the local player swung (the pilot attack key). Hits a handed-off resident in reach, or asks the server to
      * hand off the outdoor resident in front; false when no resident is in reach, so the swing goes to the combat pilot. */
    bool LocalAttack(ASFE1Character* Attacker);
    /** Server: a client's attack on a resident it sees at SeenAt; dropped and logged unless it fits the schedule. */
    void HandleAttackRequest(APlayerController* PC, const FString& ResidentId, const FVector& SeenAt);
    void HandleNpcAttack(APlayerController* PC, ASFTownsfolkNpc* Npc, bool bSelected = false);
    /** Server, quest hook: hands off the resident of Archetype (census role) whose scheduled stop is nearest Near, held
      * until ReleaseQuestHandoff. Null when none is free. */
    ASFTownsfolkNpc* RequestQuestHandoff(FName Archetype, const FVector& Near);
    void ReleaseQuestHandoff(ASFTownsfolkNpc* Npc);
    /** Every machine: a hand-off actor began or ended play here. */
    void NoteNpc(ASFTownsfolkNpc* Npc, bool bPresent);
    /** Server: the interaction is over and no player is near (an attack), or the dead resident's dawn has come. */
    bool ReleaseDue(const ASFTownsfolkNpc& Npc) const;
    bool IsHandedOff(int32 Resident) const { return Npcs.Contains(Resident); }
    /** Processor: where a released resident picks up its schedule; true once per release. */
    bool TakeResume(int32 Resident, FVector& OutAt) { return Resume.RemoveAndCopyValue(Resident,OutAt); }

private:
    bool LoadCensus(FString& OutError);
    void OnPostLogin(AGameModeBase* Mode, APlayerController* PC);
    /** Server: is Seen within the hand-off tolerance of the resident's route from its last stop to its current one? */
    bool OnSchedule(int32 Resident, const FVector& Seen) const;
    FVector StopFor(int32 Resident, ESFTownsfolkGoal Goal, int32 Market) const;
    ASFTownsfolkNpc* SpawnNpc(int32 Resident, const FVector& At, bool bQuest);
    void TickScenario();
    void TickQuestScenario();
    /** Dev map TownsfolkGuards with -TownsfolkCrimeScenario: town state, a guard, three residents and their attackers. */
    void StartCrimeScenario(UWorld& World);
    void TickCrimeScenario();
    TArray<TWeakObjectPtr<AActor>> CrimeCast;  // guard, residents A B C, the player, fixture attackers F1 F2
    double CrimeScenarioStart=0; FTimerHandle CrimeScenarioTimer; uint8 CrimeScenarioPhase=0;
    TMap<int32,TWeakObjectPtr<ASFTownsfolkNpc>> Npcs;  // handed-off residents, by index
    TMap<int32,FVector> Resume;
    TMap<FString,int32> Ids;
    TMap<TWeakObjectPtr<APlayerController>,double> NextSwing;
    TArray<int32> Still;  // frames each resident has stood still, for the scenario's pick
    int32 HandoffSerial=0, ScenarioTarget=INDEX_NONE, LastReturned=INDEX_NONE;
    FVector ScenarioHome=FVector::ZeroVector;
    TArray<FVector> ScenarioPath; int32 ScenarioPathNext=0, ScenarioPathFor=INDEX_NONE; double ScenarioPathAt=0;
    double ScenarioStart=-1, LastReturnedAt=0; bool bScenario=false, bScenarioSent=false, bCensus=false;
    bool bQuestScenario=false; uint8 QuestScenarioPhase=0; FVector QuestScenarioHome=FVector::ZeroVector; double QuestScenarioAt=0;
    FDelegateHandle PostLoginHandle;
    bool LoadLooks(FString& OutError);
    /** Creates the Smart Objects from silverbrook-smartobjects.json once the SmartObject subsystem is running. */
    void EnsureSpots();
    bool Claim(int32 Resident, ESFTownsfolkSpot Kind, const FSFTownsfolkSpot& Spot, FVector& OutAt, float& OutYaw);
    USmartObjectDefinition* MakeDefinition(const TArray<FVector>& Offsets, float YawDegrees);
    TMap<FName,FSFTownsfolkResolvedLook> Looks;
    bool bSpotsTried=false, bSpots=false;
    UPROPERTY() TArray<TObjectPtr<USmartObjectDefinition>> SpotDefinitions;  // door, stall, bench
    TMap<FString,FSFTownsfolkSpot> Doors, Stalls;  // by building / market label
    TArray<FSFTownsfolkSpot> Benches;
    TArray<FSmartObjectClaimHandle> DoorHolds, StandHolds;  // per resident: at most one door and one stall or bench
    int32 DoorClaims=0, StallClaims=0, BenchClaims=0, ClaimFailures=0;  // since the last report
    float DoorDwellSeconds=1.5f, WorkIndoorShare=0.5f, BenchShare=0.3f, BenchSearchCm=4000, SidestepRadiusCm=120, SidestepSpeedShare=0.6f;
    UPROPERTY() TArray<TObjectPtr<UObject>> LookAssets;
    bool bActive=false;
    TArray<FSFTownsfolkResident> Residents;
    TArray<TArray<FVector>> Routes;  // per resident; Mass fragments must stay trivially copyable
    TArray<FVector> Shown, Facing; TArray<bool> Outdoors;  // last frame's, per resident
    TMap<FIntPoint,TArray<int32>> Cells;  // outdoor residents by 2 m cell, rebuilt each frame
    FVector Player=FVector(TNumericLimits<float>::Max());
    double LastReport=-1, FrameMs=0; int64 Frames=0;
    mutable int32 Routes_Planned=0, Routes_Failed=0;
    int64 ClosePairs=0;  // pairs of outdoor residents under 0.6 m apart, summed over frames since the last report
    double ClosePairSeconds=0;  // the same, each frame weighted by its length: comparable across frame rates
    float LastDelta=0.016f;
    double LastSample=0;  // real seconds of the last TOWNSFOLK_STATE line
    float WalkSpeedMetres=1.4f, SidestepMaxCm=150;
    UPROPERTY() TObjectPtr<ASFTownState> State;
    UPROPERTY() TObjectPtr<UMassEntityConfigAsset> Config;
};

/** Helpers for townsfolk_lanes.py (editor) and review_townsfolk_in_game.py. */
UCLASS()
class SHATTEREDFRONTIER_API USFTownsfolkLaneLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Spawns a ZoneShape with the 'Footway' lane profile at world-centimetre points. A point with Z of 0 is traced onto
      * the ground, which needs the level's collision to exist (not in the same tick as loading or building the map).
      * A polygon's arms point at Centre, the junction centre, so each lines up with the street it joins. */
    UFUNCTION(BlueprintCallable, Category="Townsfolk", meta=(WorldContext="WorldContext"))
    static AActor* SpawnFootway(UObject* WorldContext, const TArray<FVector>& Points, bool bPolygon, FName Label, FVector Centre);

    /** Rebuilds the ZoneGraph data in the editor world; returns the lane count. */
    UFUNCTION(BlueprintCallable, Category="Townsfolk", meta=(WorldContext="WorldContext"))
    static int32 RebuildZoneGraph(UObject* WorldContext);

    /** Lanes reachable over any lane link from the lane nearest At (cm); with the network's total in OutTotal. */
    UFUNCTION(BlueprintCallable, Category="Townsfolk", meta=(WorldContext="WorldContext"))
    static int32 ReachableLanes(UObject* WorldContext, FVector At, int32& OutTotal);

    /** World positions (cm) of the prototype's residents who are outdoors; empty when it isn't running. */
    UFUNCTION(BlueprintCallable, Category="Townsfolk", meta=(WorldContext="WorldContext"))
    static TArray<FVector> OutdoorResidents(UObject* WorldContext);
};
