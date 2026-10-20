#pragma once

#include "GameFramework/Character.h"
#include "SFVoidreignCharacter.generated.h"

class UAnimSequence;
class UBlendSpace;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** One live material-on-plane effect; Progress is driven 0 -> 1 -> 0 over its duration (or pulsed while persistent). */
USTRUCT()
struct FSFVoidreignVfx
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Component;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
    float Age = 0.f;
    float Duration = 1.f;
    bool bPersistent = false;
};

/**
 * Voidreign mount presentation: rider attached at the RiderSeat bone with a speed-blended seat pose,
 * plus basic-attack and Rift Recall playback (horse, rider and material VFX in sync).
 * Client-side review aid only. Real Rift Recall displacement, damage and validation belong to the
 * server per the Companion Abilities and Animation design; nothing here is gameplay authority.
 */
UCLASS()
class SHATTEREDFRONTIER_API ASFVoidreignCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASFVoidreignCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Voidreign") void PlayBasicAttack();
    UFUNCTION(BlueprintCallable, Category = "Voidreign") void PlayRiftRecall();
    UFUNCTION(BlueprintPure, Category = "Voidreign") bool IsPerformingAction() const { return Phase != EPhase::None; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voidreign|Rider") TObjectPtr<USkeletalMeshComponent> RiderMesh;
    /** Rider seat blend space on the rider skeleton, X = mount ground speed (cm/s). */
    UPROPERTY(EditAnywhere, Category = "Voidreign|Rider") TObjectPtr<UBlendSpace> RiderLocomotion;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Rider") bool bShowRider = true;

    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> MountAttack;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RiderAttack;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RiftWindup;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RiftChargeRun;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RiftImpact;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RecallEnter;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RecallExit;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RiderBrace;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Clips") TObjectPtr<UAnimSequence> RiderRecallHold;

    UPROPERTY(EditAnywhere, Category = "Voidreign|VFX") TObjectPtr<UStaticMesh> VfxPlane;
    UPROPERTY(EditAnywhere, Category = "Voidreign|VFX") TObjectPtr<UMaterialInterface> VfxRiftOpen;
    UPROPERTY(EditAnywhere, Category = "Voidreign|VFX") TObjectPtr<UMaterialInterface> VfxRiftTrail;
    UPROPERTY(EditAnywhere, Category = "Voidreign|VFX") TObjectPtr<UMaterialInterface> VfxRiftAnchor;
    UPROPERTY(EditAnywhere, Category = "Voidreign|VFX") TObjectPtr<UMaterialInterface> VfxRiftCollapse;
    UPROPERTY(EditAnywhere, Category = "Voidreign|VFX") TObjectPtr<UMaterialInterface> VfxSpark;

    /** Presentation charge length; the design caps the rift anchor at 15 m. */
    UPROPERTY(EditAnywhere, Category = "Voidreign|Rift Recall", meta = (ClampMin = "300", ClampMax = "1500", Units = "cm"))
    float ChargeDistance = 1200.f;
    UPROPERTY(EditAnywhere, Category = "Voidreign|Rift Recall", meta = (ClampMin = "400", ClampMax = "1500", Units = "cm/s"))
    float ChargeSpeed = 1000.f;
    /** Pause between impact and the recall (the design's return window is player-timed). */
    UPROPERTY(EditAnywhere, Category = "Voidreign|Rift Recall", meta = (ClampMin = "0.2", ClampMax = "3", Units = "s"))
    float RecallDelay = 0.8f;

    /** Play mode review: alternate basic attack and Rift Recall every N seconds (0 = off). */
    UPROPERTY(EditAnywhere, Category = "Voidreign|Demo", meta = (ClampMin = "0", ClampMax = "60", Units = "s"))
    float DemoAbilityInterval = 0.f;

private:
    enum class EPhase : uint8 { None, Attack, Windup, Charge, Impact, Hold, RecallEnter, RecallExit };

    void EnterPhase(EPhase NewPhase);
    void EndAction();
    void PlayMount(UAnimSequence* Clip, bool bLoop);
    void PlayRider(UAnimSequence* Clip);
    void SpawnVfx(UMaterialInterface* Material, const FVector& Location, float SizeMetres, float Duration, bool bPersistent, bool bUpright = false);
    void TickVfx(float DeltaSeconds);
    float ClipLength(const UAnimSequence* Clip) const;

    EPhase Phase = EPhase::None;
    float PhaseTime = 0.f;
    float ChargeTravelled = 0.f;
    float TrailTimer = 0.f;
    float DemoTimer = 0.f;
    bool bDemoNextIsRift = false;
    bool bHitSpawned = false;
    FTransform Anchor;

    UPROPERTY() TArray<FSFVoidreignVfx> Vfx;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> AnchorVfx;
};
