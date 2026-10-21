#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SFRestAreaVolume.generated.h"

class UBoxComponent;

UENUM()
enum class ESFRestAreaKind : uint8 { Ordinary, Stablekeeper };

/**
 * Placeable rest area (Companion Gameplay Details §3): the server treats a pawn whose location is inside the box as
 * "at a rest area" for companion milestone swaps. Pure geometry, so the verdict is the same on every machine; the
 * size is clamped to published bounds in metres.
 */
UCLASS()
class SHATTEREDFRONTIER_API ASFRestAreaVolume : public AActor
{
    GENERATED_BODY()

public:
    static constexpr float MinHalfExtentMetres = 2.f;
    static constexpr float MaxHalfExtentMetres = 50.f;

    ASFRestAreaVolume();
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditAnywhere, Category = "Rest Area") ESFRestAreaKind Kind = ESFRestAreaKind::Ordinary;
    /** Half size of the box, clamped to 2-50 m per axis. */
    UPROPERTY(EditAnywhere, Category = "Rest Area", meta = (ClampMin = "2", ClampMax = "50", Units = "m"))
    FVector HalfExtentMetres = FVector(8.0, 8.0, 3.0);

    static FVector ClampExtent(const FVector& Metres);
    /** True when Point lies inside the clamped box placed by Transform (yaw/pitch/roll honoured, scale ignored). */
    static bool Contains(const FTransform& Transform, const FVector& HalfExtentMetres, const FVector& Point);
    bool Contains(const FVector& Point) const { return Contains(GetActorTransform(), HalfExtentMetres, Point); }
    /** True when any rest area in World contains Point. */
    static bool IsAtRestArea(const UWorld* World, const FVector& Point);

private:
    UPROPERTY(VisibleAnywhere, Category = "Rest Area") TObjectPtr<UBoxComponent> Box;
};
