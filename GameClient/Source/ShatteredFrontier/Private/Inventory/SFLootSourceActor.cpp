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
        HitShape.Center = Asset->GetBounds().Origin * Snapshot.MeshScale;
        HitShape.Extent = (Asset->GetBounds().BoxExtent * Snapshot.MeshScale.GetAbs()).ComponentMax(FVector(KINDA_SMALL_NUMBER));
    }
    OnRep_HitShape();
    ForceNetUpdate();
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
}
