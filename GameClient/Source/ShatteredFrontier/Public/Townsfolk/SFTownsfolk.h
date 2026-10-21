#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "SmartObjectRuntime.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFTownsfolk.generated.h"

class UAnimSequence;
class USmartObjectDefinition;
class USkeletalMesh;
class UMassEntityConfigAsset;
class USkeletalMeshComponent;

// Townsfolk prototype, AI Townsfolk doc section 9 steps 2-3: census residents walking their day on ZoneGraph footways
// across the town, drawn by MassRepresentation, stepping aside for each other and the player.
// Development opt-in (-TownsfolkPrototype) on Greenlands_Silverbrook; client-simulated, never on a dedicated server.

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
    float Wake=6, WorkStart=8, WorkEnd=16, Sleep=22, ErrandChance=0.5f;
    uint32 Seed=0;
    ESFTownsfolkGoal GoalAt(float Hour, int32 Day, int32& OutMarket) const;
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

    bool IsActive() const { return bActive; }
    float GameHour(int32& OutDay) const;
    float WalkSpeedCm() const { return WalkSpeedCmPerSecond; }
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

private:
    bool LoadCensus(FString& OutError);
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
    double StartSeconds=0, LastReport=-1, FrameMs=0; int64 Frames=0;
    mutable int32 Routes_Planned=0, Routes_Failed=0;
    int64 ClosePairs=0;  // pairs of outdoor residents under 0.6 m apart, summed over frames since the last report
    double ClosePairSeconds=0;  // the same, each frame weighted by its length: comparable across frame rates
    float LastDelta=0.016f;
    float StartHour=6, SecondsPerGameHour=150, WalkSpeedCmPerSecond=140, SidestepMaxCm=150;
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
