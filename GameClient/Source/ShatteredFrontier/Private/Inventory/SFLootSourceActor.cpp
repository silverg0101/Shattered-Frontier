#include "Inventory/SFLootSourceActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

ASFLootSourceActor::ASFLootSourceActor()
{
    bReplicates = true;
    SetReplicateMovement(true);
    SourceRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SourceRoot"));
    SetRootComponent(SourceRoot);
    InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
    InteractionBounds->SetupAttachment(SourceRoot);
    InteractionBounds->SetBoxExtent(FVector(25));
    InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(SourceRoot);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // Replicate the immutable descriptor: a renderless authority has no mesh asset NetGUID to send.
    Mesh->SetIsReplicated(false);
}

void ASFLootSourceActor::Initialize(const FSFLootSourceSnapshot& Snapshot)
{
    check(HasAuthority());
    ServerSnapshot = Snapshot; bGround = Snapshot.bGround;
    SetActorLocationAndRotation(Snapshot.Position, Snapshot.MeshRotation);
    MeshView.Path = Snapshot.MeshPath; MeshView.Scale = Snapshot.MeshScale;
    OnRep_MeshView();
    if (Snapshot.bHasMeshBounds)
    {
        HitShape.Center = Snapshot.MeshBoundsOriginCm * Snapshot.MeshScale;
        HitShape.Extent = (Snapshot.MeshBoundsExtentCm * Snapshot.MeshScale.GetAbs()).ComponentMax(FVector(KINDA_SMALL_NUMBER));
    }
    else if (const UStaticMesh* Asset = Mesh->GetStaticMesh())
    {
        HitShape.Center = Asset->GetBounds().Origin * Snapshot.MeshScale;
        HitShape.Extent = (Asset->GetBounds().BoxExtent * Snapshot.MeshScale.GetAbs()).ComponentMax(FVector(KINDA_SMALL_NUMBER));
    }
    else if (!Snapshot.bGround && Snapshot.MeshPath == TEXT("/Engine/BasicShapes/Cube.Cube"))
    {
        // Historical corpse rows predate persisted bounds; this was their shipped body geometry.
        HitShape.Center = FVector::ZeroVector; HitShape.Extent = FVector(50) * Snapshot.MeshScale.GetAbs();
    }
    OnRep_HitShape();
    ForceNetUpdate();
}

void ASFLootSourceActor::OnRep_MeshView()
{
    Mesh->SetRelativeScale3D(MeshView.Scale);
    if (IsRunningDedicatedServer() || GetNetMode() == NM_DedicatedServer) return;
    Mesh->SetStaticMesh(MeshView.Path.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *MeshView.Path));
}

void ASFLootSourceActor::OnRep_HitShape()
{
    InteractionBounds->SetRelativeLocation(HitShape.Center);
    InteractionBounds->SetBoxExtent(HitShape.Extent);
}

bool ASFLootSourceActor::SharesCorpseBody(const ASFLootSourceActor* Other) const
{
    return Other && !bGround && !Other->bGround && !ServerSnapshot.BodyId.IsEmpty() &&
        ServerSnapshot.BodyId == Other->ServerSnapshot.BodyId && ServerSnapshot.ZoneId == Other->ServerSnapshot.ZoneId;
}

void ASFLootSourceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFLootSourceActor, bGround);
    DOREPLIFETIME(ASFLootSourceActor, HitShape);
    DOREPLIFETIME(ASFLootSourceActor, MeshView);
}
