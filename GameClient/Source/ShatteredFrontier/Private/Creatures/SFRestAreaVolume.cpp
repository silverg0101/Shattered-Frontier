#include "Creatures/SFRestAreaVolume.h"

#include "Components/BoxComponent.h"
#include "EngineUtils.h"

ASFRestAreaVolume::ASFRestAreaVolume()
{
    Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
    Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // display only; the rule is geometry, not overlap events
    Box->SetBoxExtent(HalfExtentMetres * 100.0);
    RootComponent = Box;
}

FVector ASFRestAreaVolume::ClampExtent(const FVector& Metres)
{
    return FVector(FMath::Clamp(Metres.X, MinHalfExtentMetres, MaxHalfExtentMetres), FMath::Clamp(Metres.Y, MinHalfExtentMetres, MaxHalfExtentMetres),
        FMath::Clamp(Metres.Z, MinHalfExtentMetres, MaxHalfExtentMetres));
}

void ASFRestAreaVolume::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    HalfExtentMetres = ClampExtent(HalfExtentMetres);
    Box->SetBoxExtent(HalfExtentMetres * 100.0);
}

bool ASFRestAreaVolume::Contains(const FTransform& Transform, const FVector& HalfExtentMetres, const FVector& Point)
{
    const FVector Extent = ClampExtent(HalfExtentMetres) * 100.0;
    return FBox(-Extent, Extent).IsInside(Transform.InverseTransformPositionNoScale(Point));
}

bool ASFRestAreaVolume::IsAtRestArea(const UWorld* World, const FVector& Point)
{
    if (!World) return false;
    for (TActorIterator<ASFRestAreaVolume> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (It->Contains(Point)) return true;
    }
    return false;
}

bool ASFRestAreaVolume::IsInsideInn(const UWorld* World, const FVector& Point)
{
    if (!World || World->GetNetMode() == NM_Client) return false;
    for (TActorIterator<ASFRestAreaVolume> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (It->Kind == ESFRestAreaKind::Inn && It->Contains(Point)) return true;
    }
    return false;
}
