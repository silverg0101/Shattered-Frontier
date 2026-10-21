#include "Framework/SFPlayerController.h"
#include "Construction/SFConstructionSandboxPalette.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Framework/SFE1Settings.h"
#include "Gathering/SFGatheringPilot.h"
#include "Framework/SFHudLayout.h"
#include "Gathering/SFGatheringHud.h"
#include "Inventory/SFInventoryPanel.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Exchange/SFExchangeComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Exchange/SFExchangeWidgets.h"
#include "Exchange/SFFakeExchange.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Tests/SFE1NetworkScenario.h"

namespace
{
bool IsFinitePosition(const FVector& Position)
{
    return FMath::IsFinite(Position.X) && FMath::IsFinite(Position.Y) && FMath::IsFinite(Position.Z);
}

const TCHAR* InteractionResultText(ESFE1InteractionResult Result)
{
    switch (Result)
    {
    case ESFE1InteractionResult::Accepted: return TEXT("Accepted (non-economic probe)");
    case ESFE1InteractionResult::InvalidController: return TEXT("Unavailable: no current owned pawn");
    case ESFE1InteractionResult::StaleTarget: return TEXT("Unavailable: target changed");
    case ESFE1InteractionResult::Disabled: return TEXT("Unavailable: target disabled");
    case ESFE1InteractionResult::OutOfRange: return TEXT("Unavailable: out of range");
    case ESFE1InteractionResult::Occluded: return TEXT("Unavailable: line of sight blocked");
    case ESFE1InteractionResult::Cooldown: return TEXT("Unavailable: accepted interaction cooldown");
    default: return TEXT("Unavailable: no live target");
    }
}
}

void ASFPlayerController::BeginPlayInteraction()
{
    struct FStartupRules { float RangeCm; float CooldownSeconds; };
    static const FStartupRules Rules = []()
    {
        const USFE1Settings* Settings = GetDefault<USFE1Settings>();
        FString Error;
        if (!Settings->Validate(Error))
        {
            UE_LOG(LogTemp, Fatal, TEXT("Invalid E1 release configuration: %s"), *Error);
        }
        return FStartupRules{Settings->InteractionRangeMetres * 100.0f, Settings->InteractionAcceptedCooldownSeconds};
    }();
    // One process snapshot, shared by later joins too. No active config reload path.
    InteractionRangeCm = Rules.RangeCm;
    AcceptedCooldownSeconds = Rules.CooldownSeconds;
    bSettingsCaptured = true;
    // The trading UI reaches Gameplay.Host Exchange through this server-side component, created only when the typed setting is on.
    if (HasAuthority() && GetDefault<USFGameplayServicesSettings>()->bExchangeEnabled) { USFExchangeComponent::FindOrCreate(this); }
#if WITH_DEV_AUTOMATION_TESTS
    StartE1NetworkScenario(GetWorld());
#endif
}

ESFE1InteractionResult ASFPlayerController::ValidateTarget(AActor* ProposedTarget, uint32 ExpectedRevision) const
{
    const APawn* ControlledPawn = GetPawn();
    if (!bSettingsCaptured || !IsValid(ControlledPawn) || ControlledPawn->IsActorBeingDestroyed()
        || ControlledPawn->GetController() != this || ControlledPawn->GetOwner() != this || ControlledPawn->GetWorld() != GetWorld())
    {
        return ESFE1InteractionResult::InvalidController;
    }
    const ASFE1InteractionTarget* Target = Cast<ASFE1InteractionTarget>(ProposedTarget);
    if (!IsValid(Target) || Target->IsActorBeingDestroyed() || Target->GetWorld() != GetWorld())
    {
        return ESFE1InteractionResult::InvalidTarget;
    }
    if (Target->GetInteractionRevision() != ExpectedRevision) { return ESFE1InteractionResult::StaleTarget; }
    if (!Target->IsInteractionEnabled()) { return ESFE1InteractionResult::Disabled; }
    const FVector Origin = ControlledPawn->GetActorLocation();
    const FVector Anchor = Target->GetInteractionAnchor();
    FVector Eyes;
    FRotator EyesRotation;
    ControlledPawn->GetActorEyesViewPoint(Eyes, EyesRotation);
    if (!IsFinitePosition(Origin) || !IsFinitePosition(Anchor) || !IsFinitePosition(Eyes))
    {
        return ESFE1InteractionResult::InvalidTarget;
    }
    if (FVector::DistSquared(Origin, Anchor) > FMath::Square(static_cast<double>(InteractionRangeCm)))
    {
        return ESFE1InteractionResult::OutOfRange;
    }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(E1InteractionVisibility), false, ControlledPawn);
    FHitResult Hit;
    // Mandatory trace from authoritative pawn eyes. Hitting the target's own box is permitted.
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Anchor, ECC_Visibility, Query);
    if (bBlocked && Hit.GetActor() != Target) { return ESFE1InteractionResult::Occluded; }
    return ESFE1InteractionResult::Accepted;
}

ESFE1InteractionResult ASFPlayerController::ProcessInteraction(AActor* Target, uint32 ExpectedRevision)
{
    if (!HasAuthority() || IsActorBeingDestroyed()) { return ESFE1InteractionResult::InvalidController; }
    const ESFE1InteractionResult Validation = ValidateTarget(Target, ExpectedRevision);
    if (Validation != ESFE1InteractionResult::Accepted) { return Validation; }
    if (!GetPawn()->HasAuthority() || !Target->HasAuthority()) { return ESFE1InteractionResult::InvalidController; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!FMath::IsFinite(Now) || Now < LastAcceptedServerSeconds
        || Now - LastAcceptedServerSeconds < AcceptedCooldownSeconds)
    {
        return ESFE1InteractionResult::Cooldown;
    }
    LastAcceptedServerSeconds = Now;
    // The only state written is this controller's transient rate limiter. No target/economy mutation.
    return ESFE1InteractionResult::Accepted;
}

void ASFPlayerController::ServerRequestInteraction_Implementation(AActor* Target, uint32 ExpectedRevision)
{
    const ESFE1InteractionResult Result = ProcessInteraction(Target, ExpectedRevision);
    const APlayerState* CurrentPlayer = GetPawn() ? GetPawn()->GetPlayerState() : nullptr;
    UE_LOG(LogTemp, Display, TEXT("E1_INTERACTION_SERVER_RESULT Pawn=%s PlayerId=%d Result=%d Authority=%d"),
        *GetNameSafe(GetPawn()), CurrentPlayer ? CurrentPlayer->GetPlayerId() : INDEX_NONE,
        static_cast<int32>(Result), HasAuthority());
    const double Now = GetWorld()->GetTimeSeconds();
    const float Remaining = FMath::IsFinite(Now)
        ? static_cast<float>(FMath::Clamp(LastAcceptedServerSeconds + AcceptedCooldownSeconds - Now,
            0.0, static_cast<double>(AcceptedCooldownSeconds))) : AcceptedCooldownSeconds;
    ClientInteractionResult(Result, Remaining);
}

void ASFPlayerController::ClientInteractionResult_Implementation(ESFE1InteractionResult Result, float ServerCooldownRemainingSeconds)
{
    LastResult = Result;
    LocalFeedback.ReceiveServerResult(GetWorld()->GetTimeSeconds(), ServerCooldownRemainingSeconds);
    ResultVisibleUntilSeconds = GetWorld()->GetTimeSeconds() + 2.0;
    // One bounded line per explicit input, useful in separate client logs; no tick logging.
    UE_LOG(LogTemp, Display, TEXT("E1_INTERACTION_RESULT Result=%d %s"), static_cast<int32>(Result), InteractionResultText(Result));
}

void ASFPlayerController::TickInteraction()
{
    if (IsLocalController() && !GatheringHud)
    {
        // The pilot component arrives after BeginPlay, so install the gathering HUD on first sight of an enabled pilot.
        USFGatheringPilotComponent* Pilot = FindComponentByClass<USFGatheringPilotComponent>();
        if (Pilot && Pilot->IsEnabled() && (GatheringHud = CreateWidget<USFGatheringHud>(this, USFGatheringHud::StaticClass())))
        {
            GatheringHud->Configure(Pilot);
            GatheringHud->AddToViewport(10);
            GatheringHud->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        // Hotbar, prompt bar, target frame and minimap frame (hud-layout-v4.md), installed with the gathering HUD.
        if (Pilot && Pilot->IsEnabled() && !HudLayout && (HudLayout = CreateWidget<USFHudLayout>(this, USFHudLayout::StaticClass())))
        {
            HudLayout->Configure(Pilot);
            HudLayout->AddToViewport(9);
            HudLayout->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
    if (IsLocalController() && GetWorld()->GetTimeSeconds() >= NextLocalRefreshSeconds)
    {
        NextLocalRefreshSeconds = GetWorld()->GetTimeSeconds() + 0.1;
        RefreshLocalFeedback();
    }
}

void ASFPlayerController::RefreshLocalFeedback()
{
    SelectedTarget.Reset();
    double NearestDistanceSquared = TNumericLimits<double>::Max();
    if (const APawn* ControlledPawn = GetPawn())
    {
        for (TActorIterator<ASFE1InteractionTarget> It(GetWorld()); It; ++It)
        {
            if (!IsValid(*It) || It->IsActorBeingDestroyed()) { continue; }
            const double DistanceSquared = FVector::DistSquared(ControlledPawn->GetActorLocation(), It->GetInteractionAnchor());
            if (DistanceSquared < NearestDistanceSquared)
            {
                NearestDistanceSquared = DistanceSquared;
                SelectedTarget = *It;
            }
        }
    }
    SelectedRevision = SelectedTarget.IsValid() ? SelectedTarget->GetInteractionRevision() : 0;
    const ESFE1InteractionResult Advisory = ValidateTarget(SelectedTarget.Get(), SelectedRevision);
    const bool bShowResult = GetWorld()->GetTimeSeconds() < ResultVisibleUntilSeconds;
    const ESFE1LocalInteractionState LocalState = LocalFeedback.GetState(GetWorld()->GetTimeSeconds());
    FString Message;
    switch (LocalState)
    {
    case ESFE1LocalInteractionState::Pending: Message = TEXT("Unavailable: awaiting server result"); break;
    case ESFE1LocalInteractionState::Cooldown:
        Message = bShowResult && LastResult == ESFE1InteractionResult::Accepted
            ? TEXT("Accepted (non-economic probe); Unavailable: cooldown") : TEXT("Unavailable: accepted interaction cooldown");
        break;
    case ESFE1LocalInteractionState::Coalescing: Message = TEXT("Unavailable: input settling"); break;
    default:
        Message = bShowResult ? InteractionResultText(LastResult)
            : Advisory == ESFE1InteractionResult::Accepted ? TEXT("E: Available (server revalidates)") : InteractionResultText(Advisory);
        break;
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.25f,
            LocalState == ESFE1LocalInteractionState::Ready && Advisory == ESFE1InteractionResult::Accepted
                && (!bShowResult || LastResult == ESFE1InteractionResult::Accepted)
                ? FColor::Green : FColor::Yellow, Message);
    }
}

void ASFPlayerController::ToggleInventoryPanel()
{
    USFGatheringPilotComponent* Pilot = FindComponentByClass<USFGatheringPilotComponent>();
    if (!IsLocalController() || !Pilot || !Pilot->IsEnabled()) { return; }
    if (!InventoryPanel)
    {
        InventoryPanel = CreateWidget<USFInventoryPanel>(this, USFInventoryPanel::StaticClass());
        if (!InventoryPanel) { return; }
        InventoryPanel->Configure(Pilot);
        InventoryPanel->AddToViewport(20);
        InventoryPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    // Visible (hit-testable) so the category tabs and item tooltips work; it only covers its own slot.
    const bool bOpen = InventoryPanel->GetVisibility() == ESlateVisibility::Collapsed;
    if (bOpen)
    {
        // UI v5: fill the viewport; the canvas root places the 576 x 886 panel at 4% x 3% itself.
        int32 Width = 0, Height = 0; GetViewportSize(Width, Height);
        const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
        InventoryPanel->SetDesiredSizeInViewport(FVector2D(Width / Scale, Height / Scale));
        InventoryPanel->SetPositionInViewport(FVector2D::ZeroVector, false);
    }
    InventoryPanel->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    // The build catalogue shares the left column: hidden while the inventory is open, back with build mode on close.
    if (Palette) { Palette->SetBuildModeActive(!bOpen && bBuildMode); }
}

void ASFPlayerController::InteractWithSelectedTarget()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (InteractionInputAttempts != MAX_uint64) { ++InteractionInputAttempts; }
#endif
    if (!IsLocalController() || !bSettingsCaptured) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!LocalFeedback.TryBeginRequest(Now)) { return; }
    ServerRequestInteraction(SelectedTarget.Get(), SelectedRevision);
}

namespace
{
// hud-layout-v4 placement by viewport fractions; SetAnchorsInViewport did not take effect for these widgets.
void PlaceCentered(APlayerController* Controller, UUserWidget* Widget)
{
    int32 Width = 0, Height = 0; Controller->GetViewportSize(Width, Height);
    const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(Controller));
    Widget->ForceLayoutPrepass();
    const FVector2D Desired = Widget->GetDesiredSize();
    Widget->SetPositionInViewport(FVector2D((Width - Desired.X * Scale) * 0.5f, (Height - Desired.Y * Scale) * 0.5f), true);
}

void FillViewport(APlayerController* Controller, UUserWidget* Widget)
{
    int32 Width = 0, Height = 0; Controller->GetViewportSize(Width, Height);
    const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(Controller));
    Widget->SetDesiredSizeInViewport(FVector2D(Width / Scale, Height / Scale));
    Widget->SetPositionInViewport(FVector2D::ZeroVector, false);
}

// The live client service when the server gave this controller an exchange component, else the in-memory fake.
TSharedPtr<ISFExchangeService> ExchangeServiceFor(APlayerController* Controller)
{
    if (USFExchangeComponent* Live = Controller->FindComponentByClass<USFExchangeComponent>()) { return MakeShared<FSFExchangeClientService>(Live); }
    return MakeShared<FSFFakeExchange>();
}
}

void ASFPlayerController::ToggleTradeDemo()
{
    if (!IsLocalController()) { return; }
    if (!TradeWindow)
    {
        DemoExchange = DemoExchange ? DemoExchange : ExchangeServiceFor(this);
        const double Now = GetWorld()->GetTimeSeconds();
        DemoTrade = MakeShared<FSFTradeViewModel>(*DemoExchange, FSFFakeExchange::DemoTrade(Now + 108.0), true);
        DemoTrade->AddMyItem({TEXT("item/oak_log"), TEXT("Oak Log"), 24});
        DemoTrade->AddMyItem({TEXT("item/iron_ore"), TEXT("Iron Ore"), 12});
        DemoTrade->OnPartnerLocked(true);  // the partner has locked in; adding items above cleared it
        TradeWindow = CreateWidget<USFTradeWindow>(this, USFTradeWindow::StaticClass());
        if (!TradeWindow) { return; }
        TradeWindow->Bind(DemoTrade);
        TradeWindow->AddToViewport(30);
        TradeWindow->SetVisibility(ESlateVisibility::Collapsed);
    }
    TradeWindow->SetVisibility(TradeWindow->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    FillViewport(this, TradeWindow);  // modal root: dims the scene and places the window at 26-74% x 14-78% itself
}

void ASFPlayerController::ToggleMarketDemo()
{
    if (!IsLocalController()) { return; }
    if (!MarketBoard)
    {
        DemoExchange = DemoExchange ? DemoExchange : ExchangeServiceFor(this);
        DemoMarket = MakeShared<FSFMarketViewModel>(*DemoExchange);
        DemoMarket->SetListings(FSFFakeExchange::DemoListings());
        MarketBoard = CreateWidget<USFMarketBoard>(this, USFMarketBoard::StaticClass());
        if (!MarketBoard) { return; }
        MarketBoard->Bind(DemoMarket);
        MarketBoard->SetSellItem(TEXT("item/oak_log"), TEXT("Oak Log"), 24);
        MarketBoard->AddToViewport(30);
        MarketBoard->SetVisibility(ESlateVisibility::Collapsed);
    }
    MarketBoard->SetVisibility(MarketBoard->GetVisibility() == ESlateVisibility::Collapsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    FillViewport(this, MarketBoard);  // modal: the board dims the whole scene and centres itself
}
