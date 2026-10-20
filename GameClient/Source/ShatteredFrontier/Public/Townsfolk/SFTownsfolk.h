#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MassEntityTraitBase.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFTownsfolk.generated.h"

class AStaticMeshActor;
class UMassEntityConfigAsset;

// Townsfolk prototype, AI Townsfolk doc section 9 step 2: census residents walking their day on ZoneGraph footways.
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

    UPROPERTY(Config, EditAnywhere, Category="Prototype")
    FString District = TEXT("Riverside");

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
};

UCLASS(meta=(DisplayName="Townsfolk Resident"))
class SHATTEREDFRONTIER_API USFTownsfolkTrait : public UMassEntityTraitBase
{
    GENERATED_BODY()
protected:
    virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
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
    void Show(int32 Index, const FTransform& Transform, bool bVisible);
    void NoteFrame(double Milliseconds);

private:
    bool LoadCensus(FString& OutError);
    bool bActive=false;
    TArray<FSFTownsfolkResident> Residents;
    TArray<TArray<FVector>> Routes;  // per resident; Mass fragments must stay trivially copyable
    double StartSeconds=0, LastReport=-1, FrameMs=0; int64 Frames=0;
    mutable int32 Routes_Planned=0, Routes_Failed=0;
    int32 VisibleNow=0, VisibleCounting=0; FVector Sample=FVector::ZeroVector;
    float StartHour=6, SecondsPerGameHour=150, WalkSpeedCmPerSecond=140;
    UPROPERTY() TObjectPtr<UMassEntityConfigAsset> Config;
    // ponytail: one placeholder actor per resident (50); MassRepresentation's instanced crowds replace them in step 3.
    UPROPERTY() TArray<TObjectPtr<AStaticMeshActor>> Bodies;
};

/** Editor-side helpers for townsfolk_lanes.py. */
UCLASS()
class SHATTEREDFRONTIER_API USFTownsfolkLaneLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Spawns a ZoneShape with the 'Footway' lane profile at world-centimetre points. A point with Z of 0 is traced onto
      * the ground, which needs the level's collision to exist (not in the same tick as loading or building the map). */
    UFUNCTION(BlueprintCallable, Category="Townsfolk", meta=(WorldContext="WorldContext"))
    static AActor* SpawnFootway(UObject* WorldContext, const TArray<FVector>& Points, bool bPolygon, FName Label);

    /** Rebuilds the ZoneGraph data in the editor world; returns the lane count. */
    UFUNCTION(BlueprintCallable, Category="Townsfolk", meta=(WorldContext="WorldContext"))
    static int32 RebuildZoneGraph(UObject* WorldContext);
};
