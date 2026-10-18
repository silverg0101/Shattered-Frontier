#include "Interaction/SFE1InteractionTarget.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ASFE1InteractionTarget::ASFE1InteractionTarget()
{
    bReplicates = true;
    SetReplicateMovement(true);
    FixtureCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("FixtureCollision"));
    SetRootComponent(FixtureCollision);
    // Versioned graybox dimensions in cm. Independent collision also exists on dedicated servers.
    FixtureCollision->SetBoxExtent(FVector(35.0f, 35.0f, 60.0f));
    FixtureCollision->SetCollisionProfileName(TEXT("BlockAll"));
    FixtureMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FixtureMesh"));
    FixtureMesh->SetupAttachment(FixtureCollision);
    FixtureMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.2f));
    FixtureMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (!IsRunningDedicatedServer())
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
        FixtureMesh->SetStaticMesh(Cube.Object);
    }
}

void ASFE1InteractionTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFE1InteractionTarget, bInteractionEnabled);
    DOREPLIFETIME(ASFE1InteractionTarget, InteractionRevision);
}

FVector ASFE1InteractionTarget::GetInteractionAnchor() const
{
    return FixtureCollision->GetComponentLocation();
}

void ASFE1InteractionTarget::SetInteractionEnabled(bool bEnabled)
{
    if (HasAuthority() && bEnabled != bInteractionEnabled)
    {
        bInteractionEnabled = bEnabled;
        ++InteractionRevision;
        ForceNetUpdate();
    }
}
