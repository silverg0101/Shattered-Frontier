#pragma once

#include "Components/ActorComponent.h"
#include "SFCombatPilotRewards.generated.h"

class APlayerController;
struct FSFGameplayRequest;

// Boar defeat rewards for the Silverbrook combat pilot (MVP journey step 6: eligible rewards, once).
// Without Gameplay Services, the local development path uses the project inventory (Inventory/SFInventory) as in the gathering pilot.
namespace SFCombatPilotRewards
{
    SHATTEREDFRONTIER_API const TCHAR* HidePath();
    /** Hides to grant for one boar death: HidesPerBoar the first time DeathId is seen outside a protected area, else 0.
      * The death is recorded either way, so a repeated or protected event never grants later. */
    SHATTEREDFRONTIER_API int32 HidesFor(TSet<FString>& Granted, const FString& DeathId, bool bProtected, int32 HidesPerBoar);
    /** Stable trusted facts for a defeat reward replay against the durable backpack. */
    SHATTEREDFRONTIER_API FSFGameplayRequest BuildDefeatRequest(const FString& EventId, const FString& CharacterId,
        const FString& ContainerId, int64 ExpectedRevision, bool bKillerProtected, bool bBoarProtected);
    /** Adds Count hides to the pilot bag on Owner in one transaction; false (nothing added) without a bag or room. */
    SHATTEREDFRONTIER_API bool GrantHides(AActor* Owner, int32 Count);
    SHATTEREDFRONTIER_API int32 CountHides(const AActor* Owner);
    /** Server: gives a controller the local development bag and owning-client hide log. */
    SHATTEREDFRONTIER_API void AddPilotBag(APlayerController* Controller);
}

/** On the owning client, logs COMBAT_PILOT_CLIENT_HIDES whenever the bag's hide count changes (network test evidence). */
UCLASS(NotBlueprintable)
class SHATTEREDFRONTIER_API USFCombatPilotHideLog : public UActorComponent
{
    GENERATED_BODY()
public:
    USFCombatPilotHideLog();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
    int32 Logged = INDEX_NONE;
};
