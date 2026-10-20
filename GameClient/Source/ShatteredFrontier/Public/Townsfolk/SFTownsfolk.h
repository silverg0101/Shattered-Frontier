#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFTownsfolk.generated.h"

class UAnimSequence;
class UMassEntityConfigAsset;
class USkeletalMeshComponent;

// Townsfolk prototype, AI Townsfolk doc section 9 steps 2-3: census residents walking their day on ZoneGraph footways
// across the town, drawn by MassRepresentation, stepping aside for each other and the player.
// Development opt-in (-TownsfolkPrototype) on Greenlands_Silverbrook; client-simulated, never on a dedicated server.

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

    UPROPERTY(Config, EditAnywhere, Category="Prototype", meta=(ClampMin="0.0", ClampMax="2.0", Units="m", ToolTip="Furthest a resident steps aside from the footway to pass someone."))
    float SidestepMaxMetres = 1.5f;

    bool Validate(FString& OutError) const;
};

enum class ESFTownsfolkGoal : uint8 { None, Home, Work, Market };

/** One census resident's day, from silverbrook-census.json. Hours are game hours. */
struct FSFTownsfolkResident
{
    FString Id;
    FVector Home=FVector::ZeroVector, Work=FVector::ZeroVector;
    bool bHasWork=false;
    TArray<FVector> Markets;
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
    void Pose(const FVector& At, float Yaw, bool bWalking, float WalkRate, bool bIndoors);
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY() TObjectPtr<UAnimSequence> Walk;
    UPROPERTY() TObjectPtr<UAnimSequence> Idle;
    int8 Playing=-1;  // 1 walk, 0 idle, -1 nothing yet
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
    bool PlanRoute(const FVector& From, const FVector& To, TArray<FVector>& OutRoute) const;
    /** Records where a resident is shown this frame, for the next frame's sidesteps and the review. */
    void Note(int32 Index, const FVector& At, const FVector& Forward, bool bOutdoors);
    /** How far right of its route (cm) a resident should step this frame to pass the others and the player. */
    float Sidestep(int32 Index, float Side, float DeltaSeconds) const;
    void BeginFrame();
    void NoteFrame(double Milliseconds);
    TArray<FVector> OutdoorPositions() const;

private:
    bool LoadCensus(FString& OutError);
    bool bActive=false;
    TArray<FSFTownsfolkResident> Residents;
    TArray<TArray<FVector>> Routes;  // per resident; Mass fragments must stay trivially copyable
    TArray<FVector> Shown, Facing; TArray<bool> Outdoors;  // last frame's, per resident
    TMap<FIntPoint,TArray<int32>> Cells;  // outdoor residents by 2 m cell, rebuilt each frame
    FVector Player=FVector(TNumericLimits<float>::Max());
    double StartSeconds=0, LastReport=-1, FrameMs=0; int64 Frames=0;
    mutable int32 Routes_Planned=0, Routes_Failed=0;
    int64 ClosePairs=0;  // pairs of outdoor residents under 0.6 m apart, summed over frames since the last report
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

    /** World positions (cm) of the prototype's residents who are outdoors; empty when it isn't running. */
    UFUNCTION(BlueprintCallable, Category="Townsfolk", meta=(WorldContext="WorldContext"))
    static TArray<FVector> OutdoorResidents(UObject* WorldContext);
};
