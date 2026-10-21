#pragma once

#include "Engine/DeveloperSettings.h"
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

/** Rift Recall tuning (`tune.combat.companion_rift_*`, Companion Abilities and Animation §5). Release-authored
 *  defaults; window and distance are captured when a Rift Charge starts, the lockout when a recall starts. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Voidreign Rift Recall"))
class SHATTEREDFRONTIER_API USFVoidreignSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Rift Recall", meta=(DisplayName="Rift return window", ClampMin="1500", ClampMax="4000", Delta="250", Units="ms"))
    int32 RiftRecallWindowMs = 3000;
    UPROPERTY(Config, EditAnywhere, Category="Rift Recall", meta=(DisplayName="Rift return distance", ClampMin="8.0", ClampMax="15.0", Delta="0.5", Units="m"))
    float RiftAnchorMaxMetres = 15.f;
    UPROPERTY(Config, EditAnywhere, Category="Rift Recall", meta=(DisplayName="Rider pause after rift return", ClampMin="250", ClampMax="1000", Delta="50", Units="ms"))
    int32 RiftRiderLockoutMs = 500;
    bool Validate(FString& Error) const;
};

/** Why a recall request was refused (or None). Failure keeps the anchor until the window expires. */
enum class ESFRiftRecallReject : uint8 { None, NoAnchor, WindowClosed, TooFar, NoLineOfSight, PathBlocked, Incapacitated };

namespace SFRiftRecallRules
{
    /** Release-bound security rule (not tuning): the return path may be at most this much longer than the straight line. */
    constexpr double MaxPathRatio = 1.5;
    /** PathRatio <= 0 means no path could be established. */
    SHATTEREDFRONTIER_API ESFRiftRecallReject Validate(bool bHasAnchor, double Now, double WindowEnd, double DistanceCm,
        double AnchorMaxCm, bool bLineOfSight, double PathRatio, bool bIncapacitated);
}

/** Ability phase; replicated so every client plays the matching clip and effects (OnRep_Phase). */
UENUM()
enum class ESFVoidreignPhase : uint8 { None, Attack, Windup, Charge, Impact, Hold, RecallEnter, RecallExit };

/**
 * Voidreign mount: rider attached at the RiderSeat bone with a speed-blended seat pose, basic attack, and the
 * Rift Recall legendary signature. Abilities run on the server: Rift Charge places the anchor at its origin, and an
 * optional re-press within the return window recalls the Voidreign to it after server validation (no damage, no CC).
 * The phase and the anchor location replicate; each machine plays mount clips as montages in ABP_Voidreign's
 * DefaultSlot and spawns the effects from the same phase presentation.
 */
UCLASS()
class SHATTEREDFRONTIER_API ASFVoidreignCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASFVoidreignCharacter();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category = "Voidreign") void PlayBasicAttack();
    UFUNCTION(BlueprintCallable, Category = "Voidreign") void PlayRiftRecall();
    /** The signature re-press: asks the server to recall to the anchor; the server's verdict is LastRecallReject. */
    UFUNCTION(BlueprintCallable, Category = "Voidreign") void RequestRiftRecall();
    UFUNCTION(BlueprintPure, Category = "Voidreign") bool IsPerformingAction() const { return Phase != ESFVoidreignPhase::None; }
    UFUNCTION(BlueprintPure, Category = "Voidreign") bool IsRecallWindowOpen() const { return Phase == ESFVoidreignPhase::Hold; }
    ESFVoidreignPhase GetPhase() const { return Phase; }
    /** Rider abilities are refused from recall start until arrival plus the rider lockout. */
    UFUNCTION(BlueprintPure, Category = "Voidreign") bool IsRiderLockedOut() const;
    ESFRiftRecallReject LastRecallReject() const { return LastReject; }

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
    /** Demo only: when the demo presses recall after impact (players re-press within the return window). */
    UPROPERTY(EditAnywhere, Category = "Voidreign|Demo", meta = (ClampMin = "0.2", ClampMax = "3", Units = "s"))
    float RecallDelay = 0.8f;

    /** Play mode review: alternate basic attack and Rift Recall every N seconds (0 = off). */
    UPROPERTY(EditAnywhere, Category = "Voidreign|Demo", meta = (ClampMin = "0", ClampMax = "60", Units = "s"))
    float DemoAbilityInterval = 0.f;

private:
    friend class FSFVoidreignReplicationTest;

    UFUNCTION(Server, Reliable) void ServerRiftRecall();
    /** Client: the server's phase arrived; play its clips and effects. */
    UFUNCTION() void OnRep_Phase();
    void TryRiftRecall();
    void EnterPhase(ESFVoidreignPhase NewPhase);
    /** Clips, rider pose and one-shot effects for entering a phase; runs on the server and on every client. */
    void BeginPhasePresentation();
    void ReleaseAnchorVfx();
    void EndAction();
    void PlayMount(UAnimSequence* Clip, bool bLoop);
    void PlayRider(UAnimSequence* Clip);
    UStaticMeshComponent* SpawnVfx(UMaterialInterface* Material, const FVector& Location, float SizeMetres, float Duration, bool bPersistent, bool bUpright = false);
    void TickVfx(float DeltaSeconds);
    float ClipLength(const UAnimSequence* Clip) const;

    UPROPERTY(ReplicatedUsing = OnRep_Phase) ESFVoidreignPhase Phase = ESFVoidreignPhase::None;
    /** Rift anchor origin (capsule centre), set with the Windup phase; clients draw the anchor effect from it. */
    UPROPERTY(Replicated) FVector_NetQuantize AnchorLocation = FVector::ZeroVector;
    float PhaseTime = 0.f;
    float ChargeTravelled = 0.f;
    float TrailTimer = 0.f;
    float DemoTimer = 0.f;
    bool bDemoNextIsRift = false;
    bool bHitSpawned = false;
    FTransform Anchor;
    // Captured at Rift Charge start (window, distance) and recall start (lockout); never changed mid-effect.
    float WindowSeconds = 3.f;
    float AnchorMaxCm = 1500.f;
    float RiderLockoutSeconds = 0.5f;
    double LocalTime = 0.0;  // actor clock (advances with Tick, so tests can step it)
    double RiderLockedUntil = -1.0;
    ESFRiftRecallReject LastReject = ESFRiftRecallReject::None;

    UPROPERTY() TArray<FSFVoidreignVfx> Vfx;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> AnchorVfx;
};
