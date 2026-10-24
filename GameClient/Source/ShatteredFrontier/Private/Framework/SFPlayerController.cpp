#include "Framework/SFPlayerController.h"
#include "Quest/SFQuestBoard.h"
#include "Construction/SFConstructionSandboxPalette.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Framework/SFE1Settings.h"
#include "Gathering/SFGatheringPilot.h"
#include "Framework/SFHudLayout.h"
#include "UI/SFHudWidgets.h"
#include "Gathering/SFGatheringHud.h"
#include "UI/SFInventoryWidget.h"
#include "UI/SFEquipmentWidget.h"
#include "UI/SFWindowStack.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Exchange/SFExchangeComponent.h"
#include "Exchange/SFMarketBoardActor.h"
#include "Services/SFGameplayServicesClient.h"
#include "Exchange/SFExchangeWidgets.h"
#include "Exchange/SFFakeExchange.h"
#include "Combat/SFCombatPilot.h"
#include "Combat/SFTabTargetSelection.h"
#include "Townsfolk/SFTownsfolk.h"
#include "Characters/SFE1Character.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
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
    TickDialogue();
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (IsLocalController() && FParse::Param(FCommandLine::Get(), TEXT("ExchangeLiveRefreshProbe")))
    {
        static bool bOpened = false;
        if (!bOpened && GetPawn() && FindComponentByClass<USFExchangeComponent>())
        {
            for (TActorIterator<ASFMarketBoardActor> It(GetWorld()); It; ++It)
            {
                if (It->MarketId == TEXT("market/silverbrook") &&
                    FVector::DistSquared(GetPawn()->GetActorLocation(), It->GetActorLocation()) <=
                        FMath::Square(GetDefault<USFGameplayServicesSettings>()->MarketReachM * 100.0f))
                {
                    ToggleMarketDemo();
                    bOpened = MarketBoard && GetWindowStack()->IsOpen(MarketBoard);
                    if (bOpened) { UE_LOG(LogTemp, Display, TEXT("EXCHANGE_LIVE_BOARD_OPEN")); }
                    break;
                }
            }
        }
        if (bOpened && MarketBoard) { MarketBoard->ProbeLiveRefresh(); }
    }
#endif
    if (IsLocalController() && WasInputKeyJustPressed(EKeys::Tab)) { SelectNextCombatTarget(); }
    const bool bHasPlayer = GetLocalPlayer() != nullptr;  // CreateWidget needs an attached player (a controller made local without one, as the cursor tests do, has no HUD)
    if (bHasPlayer && IsLocalController() && !GatheringHud)
    {
        // The pilot component arrives after BeginPlay, so install the gathering HUD on first sight of an enabled pilot.
        USFGatheringPilotComponent* Pilot = FindComponentByClass<USFGatheringPilotComponent>();
        if (Pilot && Pilot->IsEnabled() && (GatheringHud = CreateWidget<USFGatheringHud>(this, USFGatheringHud::StaticClass())))
        {
            GatheringHud->Configure(Pilot);
            GatheringHud->AddToViewport(10);
            GatheringHud->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
    // The target frame is also needed in combat-only sessions without a gathering pilot.
    if (bHasPlayer && IsLocalController() && !HudLayout)
    {
        USFGatheringPilotComponent* Pilot = FindComponentByClass<USFGatheringPilotComponent>();
        const ASFE1Character* E1Character = Cast<ASFE1Character>(GetPawn());
        if ((Pilot && Pilot->IsEnabled()) || (E1Character && E1Character->CombatHealth() > 0))
        {
            HudLayout = CreateWidget<USFHudLayout>(this, USFHudLayout::StaticClass());
            if (HudLayout)
            {
                HudLayout->Configure(Pilot && Pilot->IsEnabled() ? Pilot : nullptr);
                HudLayout->AddToViewport(9);
                HudLayout->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
        }
    }
    // The persistent HUD is a Widget Blueprint (layout and look in the Designer); C++ feeds it.
    if (bHasPlayer && IsLocalController() && !HudWidget && (Cast<ASFE1Character>(GetPawn()) || FindComponentByClass<USFGatheringPilotComponent>()))
    {
        const TSubclassOf<USFHudWidget> HudClass = LoadClass<USFHudWidget>(nullptr, USFHudWidget::AssetPath);
        static bool bMissingReported = false;
        if (!HudClass) { if (!bMissingReported) { bMissingReported = true; UE_LOG(LogTemp, Error, TEXT("HUD_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFHudWidget::AssetPath); } }
        else if ((HudWidget = CreateWidget<USFHudWidget>(this, HudClass)))
        {
            HudWidget->AddToViewport(8);
            HudWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
    if (IsLocalController() && (HudLayout || HudWidget))
    {
        USFGatheringPilotComponent* Pilot = FindComponentByClass<USFGatheringPilotComponent>();
        if (Pilot && Pilot->IsEnabled())
        {
            if (HudLayout) { HudLayout->Configure(Pilot); }
        }
        // Progression state is restored on this component even in combat-only sessions.
        if (HudWidget && Pilot) { HudWidget->Configure(Pilot); }
    }
    if (IsLocalController() && GetWorld()->GetTimeSeconds() >= NextLocalRefreshSeconds)
    {
        NextLocalRefreshSeconds = GetWorld()->GetTimeSeconds() + 0.1;
        RefreshLocalFeedback();
        RefreshPartyHud();
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        TickPartyScenario();
#endif
    }
}

namespace
{
bool IsLiveCombatTarget(const AActor* Actor)
{
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) { return false; }
    if (const ASFPilotBoar* Boar = Cast<ASFPilotBoar>(Actor)) { return Boar->Health() > 0; }
    if (const ASFTownsfolkNpc* Npc = Cast<ASFTownsfolkNpc>(Actor)) { return Npc->Health() > 0; }
    return false;
}
}

AActor* ASFPlayerController::GetSelectedCombatTarget() const
{
    const ASFE1Character* E1Character = Cast<ASFE1Character>(GetPawn());
    return E1Character ? E1Character->GetCombatTarget() : nullptr;
}

FString ASFPlayerController::GetPilotMessage() const
{ return GetWorld() && GetWorld()->GetTimeSeconds() < PilotMessageUntil ? PilotMessage : FString(); }

void ASFPlayerController::ClientPilotHit_Implementation(const FString& Message)
{ PilotMessage = Message; PilotMessageUntil = GetWorld()->GetTimeSeconds() + 1.0; }

void ASFPlayerController::SelectNextCombatTarget()
{
    if (!IsLocalController() || !GetPawn() || !GetWorld()) { return; }
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    const double Now = GetWorld()->GetTimeSeconds();
    const bool bRefresh = FSFTabTargetSelection::ShouldRefresh(Now, LastTabAt,
        Settings->TabTargetCycleResetSeconds, NextTabTargetIndex, TabTargetSnapshot.Num());
    TArray<FSFTabTargetCandidate> Candidates;
    TMap<int32, AActor*> Actors;
    const FVector From = GetPawn()->GetActorLocation();
    FVector CameraLocation; FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);
    const FVector CameraYaw = FRotator(0, CameraRotation.Yaw, 0).Vector();
    auto AddCandidate = [&](AActor* Actor)
    {
        if (!IsLiveCombatTarget(Actor)) { return; }
        const FVector Delta = Actor->GetActorLocation() - From;
        const double Distance = Delta.Size2D();
        if (Distance > Settings->TabTargetRangeMetres * 100.0) { return; }
        bool bVisible = true;
        if (Settings->bTabTargetRequireLineOfSight)
        {
            FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(TabTargetVisibility), false, GetPawn());
            const FVector Centre = Actor->GetActorLocation();
            bVisible = !GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, Centre, ECC_Visibility, Query) || Hit.GetActor() == Actor;
        }
        Candidates.Add({static_cast<int32>(Actor->GetUniqueID()), Distance, FVector::DotProduct(CameraYaw, Delta.GetSafeNormal2D()), bVisible});
        Actors.Add(static_cast<int32>(Actor->GetUniqueID()), Actor);
    };
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    { AddCandidate(*It); }
    for (TActorIterator<ASFTownsfolkNpc> It(GetWorld()); It; ++It)
    { AddCandidate(*It); }
    if (bRefresh)
    {
        TabTargetSnapshot.Reset(); NextTabTargetIndex = 0;
        for (int32 Id : FSFTabTargetSelection::Snapshot(Candidates, Settings->TabTargetRangeMetres * 100.0,
            Settings->TabTargetConeDegrees, Settings->bTabTargetRequireLineOfSight))
        { TabTargetSnapshot.Add(Actors.FindRef(Id)); }
    }
    AActor* Next = nullptr;
    while (NextTabTargetIndex < TabTargetSnapshot.Num() && !Next)
    {
        AActor* Candidate = TabTargetSnapshot[NextTabTargetIndex++].Get();
        if (IsLiveCombatTarget(Candidate) && FVector::Dist2D(From, Candidate->GetActorLocation()) <= Settings->TabTargetRangeMetres * 100.0)
        { Next = Candidate; }
    }
    LastTabAt = Now;
    if (Next && Next != GetSelectedCombatTarget()) { ServerSelectCombatTarget(Next); }
}

void ASFPlayerController::ServerSelectCombatTarget_Implementation(AActor* Target)
{
    ASFE1Character* E1Character = Cast<ASFE1Character>(GetPawn());
    if (!E1Character || E1Character->CombatHealth() <= 0 || !IsLiveCombatTarget(Target) || Target->GetWorld() != GetWorld()) { return; }
    if (FVector::Dist2D(E1Character->GetActorLocation(), Target->GetActorLocation()) >
        GetDefault<USFCombatPilotSettings>()->TabTargetRangeMetres * 100.0) { return; }
    E1Character->SetCombatTarget(Target);
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
    if (!InventoryWidget)
    {
        const TSubclassOf<USFInventoryWidget> InventoryClass = LoadClass<USFInventoryWidget>(nullptr, USFInventoryWidget::AssetPath);
        if (!InventoryClass) { UE_LOG(LogTemp, Error, TEXT("INVENTORY_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFInventoryWidget::AssetPath); return; }
        InventoryWidget = CreateWidget<USFInventoryWidget>(this, InventoryClass);
        if (!InventoryWidget) { return; }
        InventoryWidget->Configure(Pilot);
        InventoryWidget->AddToViewport(20);
        InventoryWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    // A window: sized by its content and placed by the window stack (Visible so tabs, slots and tooltips take the mouse).
    const bool bOpen = !GetWindowStack()->IsOpen(InventoryWidget);
    if (bOpen) { GetWindowStack()->Open(InventoryWidget); } else { GetWindowStack()->Close(InventoryWidget); }
}

void ASFPlayerController::ToggleEquipmentPanel()
{
    USFGatheringPilotComponent* Pilot = FindComponentByClass<USFGatheringPilotComponent>();
    if (!IsLocalController() || !Pilot || !Pilot->IsEnabled()) { return; }
    if (!EquipmentWidget)
    {
        const TSubclassOf<USFEquipmentWidget> EquipmentClass = LoadClass<USFEquipmentWidget>(nullptr, USFEquipmentWidget::AssetPath);
        if (!EquipmentClass) { UE_LOG(LogTemp, Error, TEXT("EQUIPMENT_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFEquipmentWidget::AssetPath); return; }
        EquipmentWidget = CreateWidget<USFEquipmentWidget>(this, EquipmentClass);
        if (!EquipmentWidget) { return; }
        EquipmentWidget->Configure(Pilot);
        EquipmentWidget->AddToViewport(20);
        EquipmentWidget->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (GetWindowStack()->IsOpen(EquipmentWidget)) { GetWindowStack()->Close(EquipmentWidget); } else { GetWindowStack()->Open(EquipmentWidget); }
}

void ASFPlayerController::InteractWithSelectedTarget()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (InteractionInputAttempts != MAX_uint64) { ++InteractionInputAttempts; }
#endif
    if (TryOpenDialogue() || USFQuestBoardPanel::TryOpenBoard(this)) { return; }
    if (IsLocalController() && GetPawn() && GetWorld() && GetDefault<USFGameplayServicesSettings>()->bExchangeEnabled)
    {
        const float ReachCm = GetDefault<USFGameplayServicesSettings>()->MarketReachM * 100.0f;
        for (TActorIterator<ASFMarketBoardActor> It(GetWorld()); It; ++It)
        {
            if (FVector::DistSquared(GetPawn()->GetActorLocation(), It->GetActorLocation()) <= FMath::Square(ReachCm))
            { ToggleMarketDemo(); return; }
        }
    }
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


// The live client service when the server gave this controller an exchange component; the in-memory fake only on
// request (-ExchangeFake, development builds), never silently; otherwise none and the screen stays closed.
TSharedPtr<ISFExchangeService> ExchangeServiceFor(APlayerController* Controller)
{
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("ExchangeFake"))) { return MakeShared<FSFFakeExchange>(); }  // the explicit dev flag wins over a live component
#endif
    if (USFExchangeComponent* Live = Controller->FindComponentByClass<USFExchangeComponent>()) { return MakeShared<FSFExchangeClientService>(Live); }
    UE_LOG(LogTemp, Display, TEXT("EXCHANGE_UNAVAILABLE No exchange service on this controller"));
    return nullptr;
}
}

// A modal (trade, market) opens over the dimmed HUD only: the inventory and the build catalogue close first.
USFWindowStack* ASFPlayerController::GetWindowStack()
{
    if (!WindowStack) { WindowStack = NewObject<USFWindowStack>(this); }
    return WindowStack;
}

void ASFPlayerController::CloseSidePanelsForModal()
{
    GetWindowStack()->CloseAll();
    if (Palette) { Palette->SetBuildModeActive(false); }
}

// Trade and market are windows of the stack like the inventory: they line up with the other open windows and Esc closes the newest.
void ASFPlayerController::ToggleTradeDemo()
{
    if (!IsLocalController()) { return; }
    if (!TradeWindow)
    {
        DemoExchange = DemoExchange ? DemoExchange : ExchangeServiceFor(this);
        if (!DemoExchange) { return; }
        const TSubclassOf<USFTradeWindow> TradeClass = LoadClass<USFTradeWindow>(nullptr, USFTradeWindow::AssetPath);
        if (!TradeClass) { UE_LOG(LogTemp, Error, TEXT("TRADE_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFTradeWindow::AssetPath); return; }
        const double Now = GetWorld()->GetTimeSeconds();
        DemoTrade = MakeShared<FSFTradeViewModel>(*DemoExchange, FSFFakeExchange::DemoTrade(Now + 108.0), true);
        DemoTrade->AddMyItem({TEXT("item/oak_log"), TEXT("Oak Log"), 24});
        DemoTrade->AddMyItem({TEXT("item/iron_ore"), TEXT("Iron Ore"), 12});
        TradeWindow = CreateWidget<USFTradeWindow>(this, TradeClass);
        if (!TradeWindow) { return; }
        TradeWindow->Bind(DemoTrade);
        TradeWindow->AddToViewport(30);
        TradeWindow->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (GetWindowStack()->IsOpen(TradeWindow)) { GetWindowStack()->Close(TradeWindow); } else { GetWindowStack()->Open(TradeWindow); }
    if (GatheringHud) { GatheringHud->SetStationModal(TradeWindow); }
}

void ASFPlayerController::ToggleMarketDemo()
{
    if (!IsLocalController()) { return; }
    if (!MarketBoard)
    {
        DemoExchange = DemoExchange ? DemoExchange : ExchangeServiceFor(this);
        if (!DemoExchange) { return; }
        const TSubclassOf<USFMarketBoard> MarketClass = LoadClass<USFMarketBoard>(nullptr, USFMarketBoard::AssetPath);
        if (!MarketClass) { UE_LOG(LogTemp, Error, TEXT("MARKET_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFMarketBoard::AssetPath); return; }
        DemoMarket = MakeShared<FSFMarketViewModel>(*DemoExchange);
        if (FParse::Param(FCommandLine::Get(), TEXT("ExchangeFake"))) { DemoMarket->SetListings(FSFFakeExchange::DemoListings()); }
        MarketBoard = CreateWidget<USFMarketBoard>(this, MarketClass);
        if (!MarketBoard) { return; }
        MarketBoard->Bind(DemoMarket);
        if (FParse::Param(FCommandLine::Get(), TEXT("ExchangeFake"))) { MarketBoard->SetSellItem(TEXT("item/oak_log"), TEXT("Oak Log"), 24); }
        MarketBoard->AddToViewport(30);
        MarketBoard->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (GetWindowStack()->IsOpen(MarketBoard))
    {
        MarketBoard->StopLiveRefresh();
        GetWindowStack()->Close(MarketBoard);
    }
    else
    {
        if (!FParse::Param(FCommandLine::Get(), TEXT("ExchangeFake")))
        {
            ASFMarketBoardActor* Nearby = nullptr;
            const APawn* LocalPawn = GetPawn();
            const float ReachCm = GetDefault<USFGameplayServicesSettings>()->MarketReachM * 100.0f;
            if (LocalPawn && GetWorld())
            {
                for (TActorIterator<ASFMarketBoardActor> It(GetWorld()); It; ++It)
                {
                    if (FVector::DistSquared(LocalPawn->GetActorLocation(), It->GetActorLocation()) <= FMath::Square(ReachCm))
                    { Nearby = *It; break; }
                }
            }
            USFExchangeComponent* Exchange = FindComponentByClass<USFExchangeComponent>();
            if (!Nearby || !Exchange) { return; }
            MarketBoard->StartLiveRefresh(Exchange, Nearby);
        }
        GetWindowStack()->Open(MarketBoard);
    }
    if (GatheringHud) { GatheringHud->SetStationModal(MarketBoard); }
}

bool ASFPlayerController::IsStationPanelOpen() const
{
    return GatheringHud && GatheringHud->IsStationShown();
}
