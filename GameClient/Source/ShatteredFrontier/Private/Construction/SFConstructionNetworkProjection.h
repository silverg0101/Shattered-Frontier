#pragma once

#include "CoreMinimal.h"

class ASFConstructionNetworkState;
class UProceduralMeshComponent;
struct FSFConstructionNetworkMember;

// Geometry/projection only. Network state owns admission and atomic publication.
namespace SFConstructionNetworkProjection
{
    UProceduralMeshComponent* Stage(ASFConstructionNetworkState* Owner, const FSFConstructionNetworkMember& Member);
    bool Matches(const UProceduralMeshComponent* Mesh, const FSFConstructionNetworkMember& Member);
}
