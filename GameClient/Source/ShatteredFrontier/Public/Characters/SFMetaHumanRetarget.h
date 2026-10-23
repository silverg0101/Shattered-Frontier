#pragma once

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "SFMetaHumanRetarget.generated.h"

class UIKRetargeter;

// Runs Epic's FAnimNode_RetargetPoseFromMesh as the whole graph (no Anim Blueprint): the body copies the source mesh's pose
// through the retargeter every frame.
struct FSFMetaHumanRetargetProxy : public FAnimInstanceProxy
{
    FSFMetaHumanRetargetProxy() = default;
    explicit FSFMetaHumanRetargetProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    FAnimNode_RetargetPoseFromMesh Node;

protected:
    virtual FAnimNode_Base* GetCustomRootNode() override { return &Node; }
    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
};

// The anim instance of a player MetaHuman's body: its pose is the character's (hidden) UE4 mannequin pose, retargeted with
// RTG_SF_MannequinToMetaHuman (tools/Create-PlayerRetarget.py), so every gameplay animation keeps driving the visible body.
UCLASS(Transient, NotBlueprintable)
class SHATTEREDFRONTIER_API USFMetaHumanRetargetAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    void SetSource(USkeletalMeshComponent* InSource, UIKRetargeter* InRetargeter) { Source = InSource; Retargeter = InRetargeter; }

protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override { return new FSFMetaHumanRetargetProxy(this); }
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override { delete static_cast<FSFMetaHumanRetargetProxy*>(InProxy); }

private:
    friend struct FSFMetaHumanRetargetProxy;
    UPROPERTY(Transient) TObjectPtr<UIKRetargeter> Retargeter;
    UPROPERTY(Transient) TWeakObjectPtr<USkeletalMeshComponent> Source;
};
