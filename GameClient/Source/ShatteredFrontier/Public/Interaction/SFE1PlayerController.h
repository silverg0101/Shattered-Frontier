#pragma once

#include "GameFramework/PlayerController.h"
#include "Interaction/SFE1InteractionFeedback.h"
#include "SFE1PlayerController.generated.h"

class ASFE1InteractionTarget;
class FSFFakeExchange;
class FSFMarketViewModel;
class FSFTradeViewModel;
class USFGatheringHud;
class USFMarketBoard;
class USFTradeWindow;
class USFInventoryPanel;

UENUM()
enum class ESFE1InteractionResult : uint8
{
    Accepted, InvalidController, InvalidTarget, StaleTarget, Disabled, OutOfRange, Occluded, Cooldown
};

UCLASS()
class SHATTEREDFRONTIER_API ASFE1PlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ASFE1PlayerController();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupInputComponent() override;
    void InteractWithSelectedTarget();
    void ToggleInventoryPanel();
    // Development screens for the trading UI (-ExchangeUiDemo): F6 trade window, F7 market board, both on a fake exchange.
    void ToggleTradeDemo();
    void ToggleMarketDemo();
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Observes entry before local request coalescing, so input-routing tests can
    // detect duplicate pawn dispatch even when cooldown hides another request.
    uint64 GetInteractionInputAttemptCount() const { return InteractionInputAttempts; }
#endif

private:
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    uint64 InteractionInputAttempts = 0;
#endif
    // The owning controller is the network intent endpoint; identity/pawn/position/time are never payloads.
    UFUNCTION(Server, Reliable) void ServerRequestInteraction(AActor* Target, uint32 ExpectedRevision);
    UFUNCTION(Client, Reliable) void ClientInteractionResult(ESFE1InteractionResult Result, float ServerCooldownRemainingSeconds);
    ESFE1InteractionResult ValidateTarget(AActor* Target, uint32 ExpectedRevision) const;
    ESFE1InteractionResult ProcessInteraction(AActor* Target, uint32 ExpectedRevision);
    void RefreshLocalFeedback();

    UPROPERTY(Transient) TObjectPtr<USFInventoryPanel> InventoryPanel;
    UPROPERTY(Transient) TObjectPtr<USFGatheringHud> GatheringHud;
    UPROPERTY(Transient) TObjectPtr<USFTradeWindow> TradeWindow;
    UPROPERTY(Transient) TObjectPtr<USFMarketBoard> MarketBoard;
    TSharedPtr<FSFFakeExchange> DemoExchange;
    TSharedPtr<FSFTradeViewModel> DemoTrade;
    TSharedPtr<FSFMarketViewModel> DemoMarket;
    TWeakObjectPtr<ASFE1InteractionTarget> SelectedTarget;
    uint32 SelectedRevision = 0;
    double LastAcceptedServerSeconds = -1.0e30;
    double NextLocalRefreshSeconds = 0.0;
    double ResultVisibleUntilSeconds = 0.0;
    FSFE1InteractionFeedback LocalFeedback;
    float InteractionRangeCm = 0.0f;
    float AcceptedCooldownSeconds = 0.0f;
    bool bSettingsCaptured = false;
    ESFE1InteractionResult LastResult = ESFE1InteractionResult::InvalidTarget;

    friend class FSFE1InteractionAuthorityTest;
};
