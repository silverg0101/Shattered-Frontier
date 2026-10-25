#include "Inventory/SFLootSourceActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

ASFLootSourceActor::ASFLootSourceActor()
{
    bReplicates = true;
    SetReplicateMovement(true);
    InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
    SetRootComponent(InteractionBounds);
    InteractionBounds->SetBoxExtent(FVector(25));
    InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(InteractionBounds);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetIsReplicated(true);
}

void ASFLootSourceActor::Initialize(const FSFLootSourceSnapshot& Snapshot)
{
    check(HasAuthority());
    ServerSnapshot = Snapshot; bGround = Snapshot.bGround;
    SetActorLocationAndRotation(Snapshot.Position, Snapshot.MeshRotation);
    Mesh->SetRelativeScale3D(Snapshot.MeshScale);
    if (UStaticMesh* Asset = LoadObject<UStaticMesh>(nullptr, *Snapshot.MeshPath))
    {
        Mesh->SetStaticMesh(Asset);
        const FVector Extent = Asset->GetBounds().BoxExtent * Snapshot.MeshScale.GetAbs();
        InteractionBounds->SetBoxExtent(Extent.ComponentMax(FVector(8)));
    }
    ForceNetUpdate();
}

void ASFLootSourceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFLootSourceActor, bGround);
}
