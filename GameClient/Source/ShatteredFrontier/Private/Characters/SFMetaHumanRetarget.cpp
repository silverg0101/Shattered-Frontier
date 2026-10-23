#include "Characters/SFMetaHumanRetarget.h"

#include "Retargeter/IKRetargeter.h"

void FSFMetaHumanRetargetProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
    FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
    // Game thread: hand the node its source and retargeter, then let it copy the source pose (it needs the game thread).
    const auto* Instance = CastChecked<USFMetaHumanRetargetAnimInstance>(InAnimInstance);
    Node.SourceMeshComponent = Instance->Source;
    Node.IKRetargeterAsset = Instance->Retargeter;
    Node.PreUpdate(InAnimInstance);
}
