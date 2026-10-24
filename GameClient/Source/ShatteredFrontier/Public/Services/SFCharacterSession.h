#pragma once

#include "Components/ActorComponent.h"
#include "SFCharacterSession.generated.h"

class APlayerController;

// INT-05: a dev character's server session on its controller. It restores the character from the services on connect
// (ledger backpack, Progression skills and quest step, saved position), refuses actions until that restore succeeds, saves
// the position on an interval and at logout, and keeps a disconnected controller alive until its in-flight commits resolve.
// Exists with Gameplay services, a bound dev identity and either restore-on-connect or an active combat pilot.
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API USFCharacterSessionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USFCharacterSessionComponent();
    // Server: attaches a session when restore is enabled and the controller has a dev identity.
    static void AttachIfEnabled(APlayerController* Controller);
    // Server: true while this controller's character is still being restored (actions are refused).
    static bool IsRestoring(const AActor* Owner);
    // Server: true while this controller is disconnected and draining (its pawn is not a combat target).
    static bool IsLinkdead(const AActor* Owner);
    // Connection closed: true keeps the controller alive (detached) until its pending work resolves.
    bool BeginDrain();
    bool IsDraining() const { return DrainStartedAt >= 0; }
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool HasPendingWork() const;
    void BeginRestore();
    void SavePosition(bool bFinal);
    FString CharacterId;
    bool bRestored = false, bRestoreInFlight = false, bSaveInFlight = false;
    int32 RestoreParts = 0;  // bit 0 ledger, bit 1 progression; both set completes the restore
    double NextRestoreAt = 0, NextSaveAt = 0, DrainStartedAt = -1;
    int64 PositionRevision = 0;
    FVector LastSavedLocation = FVector(TNumericLimits<double>::Max());
    FVector LastKnownLocation = FVector::ZeroVector; float LastKnownYaw = 0; bool bHasKnownPose = false, bHasRestoredLocation = false;
    FName PendingFaceId;  // the stored face, applied to the pawn once it exists
};
