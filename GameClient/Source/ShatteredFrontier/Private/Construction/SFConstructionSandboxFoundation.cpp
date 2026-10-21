#include "Construction/SFConstructionSandboxFoundation.h"
#include "Construction/SFConstructionSandbox.h"
#include "Construction/SFConstructionGeometry.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool SFConstructionSandboxFoundation::HasKnownPlane(const UWorld* World)
{
    if (!World || World->GetNetMode() != NM_Standalone
        || !World->GetAuthGameMode<ASFConstructionSandboxGameMode>()) { return false; }
    return MatchesKnownPlaneGeometry(World);
}

bool SFConstructionSandboxFoundation::MatchesKnownPlaneGeometry(const UWorld* World, const AActor** OutFoundation, const FVector& AnchorOffsetCm)
{
    if (OutFoundation) { *OutFoundation = nullptr; }
    if (!World) { return false; }
    const AActor* Foundation = nullptr;
    int32 Found = 0;
    for (TActorIterator<AStaticMeshActor> It(const_cast<UWorld*>(World)); It; ++It)
    {
        if (!It->ActorHasTag(TEXT("SFConstructionSandboxFoundationV1"))) { continue; }
        const auto* Mesh = It->GetStaticMeshComponent();
        if (It->IsActorBeingDestroyed() || !Mesh || Mesh != It->GetRootComponent() || !Mesh->GetStaticMesh()
            || Mesh->GetStaticMesh()->GetPathName() != TEXT("/Engine/BasicShapes/Cube.Cube")
            || !It->GetActorLocation().Equals(FVector(0,0,-50)+AnchorOffsetCm)
            || !It->GetActorScale3D().Equals(FVector(20,20,1))
            || !It->GetActorQuat().Equals(FQuat::Identity)
            || !Mesh->GetComponentTransform().Equals(It->GetActorTransform())
            || !Mesh->IsCollisionEnabled()
            || Mesh->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block
            || Mesh->GetCollisionResponseToChannel(ECC_Visibility) != ECR_Block) { return false; }
        ++Found;
        Foundation = *It;
    }
    if (Found == 1 && OutFoundation) { *OutFoundation = Foundation; }
    return Found == 1;
}

bool SFConstructionSandboxFoundation::Anchors(const UWorld* World, const FIntVector& Cell, int32 Shape, int32 Rotation)
{
    return HasKnownPlane(World) && TouchesPlane(Cell, Shape, Rotation);
}

bool SFConstructionSandboxFoundation::TouchesPlane(const FIntVector& Cell, int32 Shape, int32 Rotation)
{
    // The verified cube's top is Z=0; the plot is [-1000,1000]cm on X/Y.
    if (Cell.Z != 0 || Cell.X < -20 || Cell.X >= 20 || Cell.Y < -20 || Cell.Y >= 20) { return false; }
    FSFConstructionSolid Solid;
    if (!SFConstructionGeometry::Build(Shape, Rotation, Solid)) { return false; }
    for (const auto& Face : Solid.Faces)
    {
        bool bBottom = true;
        for (int32 Index : Face) { bBottom &= Solid.Vertices[Index].Z == 0; }
        if (!bBottom) { continue; }
        FVector TwiceArea = FVector::ZeroVector;
        for (int32 I = 1; I + 1 < Face.Num(); ++I)
        {
            TwiceArea += FVector::CrossProduct(Solid.Vertices[Face[I]] - Solid.Vertices[Face[0]],
                Solid.Vertices[Face[I+1]] - Solid.Vertices[Face[0]]);
        }
        if (TwiceArea.Z < 0) { return true; }
    }
    return false;
}
