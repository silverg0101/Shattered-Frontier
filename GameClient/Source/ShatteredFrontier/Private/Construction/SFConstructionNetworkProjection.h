#pragma once

#include "CoreMinimal.h"

class ASFConstructionNetworkState;
class UProceduralMeshComponent;
class UStaticMesh;
struct FSFConstructionNetworkMember;
struct FSFConstructionNetworkDoor;
struct FSFConstructionNetworkDoorProjection;

// Geometry/projection only. Network state owns admission and atomic publication.
namespace SFConstructionNetworkProjection
{
    UProceduralMeshComponent* Stage(ASFConstructionNetworkState* Owner, const FSFConstructionNetworkMember& Member);
    bool Matches(const UProceduralMeshComponent* Mesh, const FSFConstructionNetworkMember& Member);
    bool StageDoor(ASFConstructionNetworkState* Owner, UStaticMesh* Asset, const FSFConstructionNetworkDoor& Door, FSFConstructionNetworkDoorProjection& Out);
    bool MatchesDoor(const ASFConstructionNetworkState* Owner, const FSFConstructionNetworkDoor& Door, const FSFConstructionNetworkDoorProjection& Projection);
    void DiscardDoor(FSFConstructionNetworkDoorProjection& Projection);
    bool IsDoorMotionClear(const ASFConstructionNetworkState* Owner, const FSFConstructionNetworkDoor& Door, const FSFConstructionNetworkDoorProjection& Projection);
}
