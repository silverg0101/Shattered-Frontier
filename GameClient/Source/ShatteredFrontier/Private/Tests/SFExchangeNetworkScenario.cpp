#include "Exchange/SFExchangeComponent.h"
#include "Exchange/SFMarketBoardActor.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
#include "Services/SFBackpackComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"

// Server-side driver for tools/Test-ExchangeTrade.ps1 (-ExchangeScenario): with two players connected it plays both sides
// through the same ExecuteIntent path real clients use, then replays the accept and the buy with the same derived operation
// ids. EXCHANGE_SCENARIO_* lines are what the runner checks; the replay must commit again without moving anything twice.
namespace
{
constexpr int32 ExchangeScenarioDone = 100;
// Exchange ids carry their kind prefix (trd_ / lst_); the live store (PostgresExchangeStore) rejects any other id.
const TCHAR* ExchangeScenarioTrade = TEXT("trd_scenario");
const TCHAR* ExchangeScenarioListing = TEXT("lst_scenario");
}

void USFExchangeComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    TickScenario();
}

void USFExchangeComponent::TickScenario()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ScenarioStep >= ExchangeScenarioDone || !FParse::Param(FCommandLine::Get(), TEXT("ExchangeScenario"))) { return; }
    UWorld* World = GetWorld();
    TArray<APlayerController*> Players;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It) { if (It->Get()) { Players.Add(It->Get()); } }
    if (Players.Num() < 2 || Players[0] != GetOwner()) { return; }  // the first player's component drives both
    USFExchangeComponent* A = this;
    USFExchangeComponent* B = FindOrCreate(Players[1]);
    USFBackpackComponent* PackA = USFBackpackComponent::FindOrCreate(Players[0]);
    USFBackpackComponent* PackB = USFBackpackComponent::FindOrCreate(Players[1]);
    if (!B || !PackA || !PackB || !USFBackpackComponent::Services() || PackA->GetCharacterId().IsEmpty() || PackB->GetCharacterId().IsEmpty() || !PackA->IsIdle() || !PackB->IsIdle()) { return; }

    auto Fail = [this](const TCHAR* Why) { UE_LOG(LogTemp, Error, TEXT("EXCHANGE_SCENARIO_FAILED Step=%d %s"), ScenarioStep, Why); ScenarioStep = ExchangeScenarioDone; };
    // Each step issues one intent and waits for its server-side outcome before the next.
    auto Run = [this](USFExchangeComponent* Who, FSFExchangeIntent Intent, uint32 Nonce)
    {
        Intent.Nonce = Nonce; ScenarioWaitNonce = Nonce;
        Who->ServerRun(Intent);
    };
    if (ScenarioWaitNonce != 0) { return; }  // an intent is in flight; OnServerResult below clears the wait

    if (ScenarioStep == 0)
    {
        // The runner seeds the wallets for the two logged character ids, then creates the gate file.
        FString Gate; FParse::Value(FCommandLine::Get(), TEXT("ExchangeScenarioGate="), Gate);
        static bool bAnnounced = false;
        if (!bAnnounced) { bAnnounced = true; UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_START A=%s ABag=%s B=%s BBag=%s"), *PackA->GetCharacterId(), *PackA->GetContainerId(), *PackB->GetCharacterId(), *PackB->GetContainerId()); }
        static bool bPrepared = false;
        if (bPrepared)
        {
            if (!Gate.IsEmpty() && !IFileManager::Get().FileExists(*Gate)) { return; }
            ScenarioStep = 1;
            return;
        }
        APawn* PawnA = Players[0]->GetPawn();
        APawn* PawnB = Players[1]->GetPawn();
        if (!PawnA || !PawnB) { return; }
        PawnB->SetActorLocation(PawnA->GetActorLocation() + FVector(100, 0, 0));
        World->SpawnActor<ASFMarketBoardActor>(PawnA->GetActorLocation(), FRotator::ZeroRotator);
        auto RefusedBeforeRpc = [A](const FString& MarketId, uint32 Nonce)
        {
            bool bRefused = false;
            const FDelegateHandle Handle = A->OnServerResult.AddLambda([&](uint32 ResultNonce, bool bCommitted)
            { if (ResultNonce == Nonce) { bRefused = !bCommitted; } });
            FSFExchangeIntent Probe; Probe.Kind = ESFExchangeIntentKind::ListItem; Probe.Nonce = Nonce;
            Probe.MarketId = MarketId; Probe.ListingId = TEXT("lst_probe"); Probe.ItemId = TEXT("item/iron_ore"); Probe.Quantity = 1; Probe.UnitPrice = 5;
            A->ServerRun(Probe);
            A->OnServerResult.Remove(Handle);
            return bRefused;
        };
        if (!RefusedBeforeRpc(TEXT("market/eastford"), 90)) { Fail(TEXT("ForgedMarketAccepted")); return; }
        const FVector Original = PawnA->GetActorLocation();
        PawnA->SetActorLocation(Original + FVector(600, 0, 0));
        const bool bReachRefused = RefusedBeforeRpc(TEXT("market/silverbrook"), 91);
        PawnA->SetActorLocation(Original);
        if (!bReachRefused) { Fail(TEXT("DistantMarketAccepted")); return; }
        UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_DENIAL Forged=1 Reach=1"));
        A->OnServerResult.AddWeakLambda(this, [this](uint32 Nonce, bool bCommitted)
        {
            if (Nonce != ScenarioWaitNonce) { return; }
            UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_RESULT Step=%d Committed=%d"), ScenarioStep, bCommitted ? 1 : 0);
            ScenarioWaitNonce = 0;
            if (!bCommitted) { UE_LOG(LogTemp, Error, TEXT("EXCHANGE_SCENARIO_FAILED Step=%d Rejected"), ScenarioStep); ScenarioStep = ExchangeScenarioDone; return; }
            ++ScenarioStep;
        });
        B->OnServerResult.AddWeakLambda(this, [this](uint32 Nonce, bool bCommitted)
        {
            if (Nonce != ScenarioWaitNonce) { return; }
            UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_RESULT Step=%d Committed=%d"), ScenarioStep, bCommitted ? 1 : 0);
            ScenarioWaitNonce = 0;
            if (!bCommitted) { UE_LOG(LogTemp, Error, TEXT("EXCHANGE_SCENARIO_FAILED Step=%d Rejected"), ScenarioStep); ScenarioStep = ExchangeScenarioDone; return; }
            ++ScenarioStep;
        });
        bPrepared = true;
        UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_BOARD_READY"));
        return;
    }
    FSFExchangeIntent Intent;
    switch (ScenarioStep)
    {
    case 1:  // A offers two oak logs for ten gold from B
        Intent.Kind = ESFExchangeIntentKind::OfferTrade; Intent.TradeId = ExchangeScenarioTrade; Intent.PartnerId = PackB->GetCharacterId();
        Intent.TheirGold = 10; { FSFExchangeStackWire Stack; Stack.ItemId = TEXT("item/oak_log"); Stack.Count = 2; Intent.MyItems.Add(Stack); }
        Run(A, Intent, 101); break;
    case 2:  // B accepts
        Intent.Kind = ESFExchangeIntentKind::AcceptTrade; Intent.TradeId = ExchangeScenarioTrade;
        Run(B, Intent, 102); break;
    case 3:  // B lists one iron ore while A's market board is open
        Intent.Kind = ESFExchangeIntentKind::ListItem; Intent.MarketId = TEXT("market/silverbrook"); Intent.ListingId = ExchangeScenarioListing; Intent.ItemId = TEXT("item/iron_ore"); Intent.Quantity = 1; Intent.UnitPrice = 5;
        Run(B, Intent, 103); break;
    case 4:  // A buys it (a new listing is revision 0: exchange.listings.revision defaults to 0)
    {
        FString Gate; FParse::Value(FCommandLine::Get(), TEXT("ExchangeScenarioGate="), Gate);
        if (FParse::Param(FCommandLine::Get(), TEXT("ExchangeLiveRefreshScenario")) && !Gate.IsEmpty() &&
            !IFileManager::Get().FileExists(*(Gate + TEXT(".listing")))) { return; }
        Intent.Kind = ESFExchangeIntentKind::BuyListing; Intent.MarketId = TEXT("market/silverbrook"); Intent.ListingId = ExchangeScenarioListing; Intent.Quantity = 1; Intent.UnitPrice = 5; Intent.ExpectedRevision = 0;
        Run(A, Intent, 104); break;
    }
    case 5:  // read B's wallet, then replay accept and buy under the same derived operation ids
        Intent.Kind = ESFExchangeIntentKind::Wallet; Run(B, Intent, 105); break;
    case 6:
        WalletBeforeReplay = B->GetWallet();
        UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_WALLET Phase=Before B=%lld Reserved=%lld"), WalletBeforeReplay.Balance, WalletBeforeReplay.Reserved);
        Intent.Kind = ESFExchangeIntentKind::AcceptTrade; Intent.TradeId = ExchangeScenarioTrade; Run(B, Intent, 106); break;
    case 7:
        Intent.Kind = ESFExchangeIntentKind::BuyListing; Intent.MarketId = TEXT("market/silverbrook"); Intent.ListingId = ExchangeScenarioListing; Intent.Quantity = 1; Intent.UnitPrice = 5; Intent.ExpectedRevision = 0; Run(A, Intent, 107); break;
    case 8:
        Intent.Kind = ESFExchangeIntentKind::Wallet; Run(B, Intent, 108); break;
    case 9:
    {
        const FSFWalletView After = B->GetWallet();
        UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_WALLET Phase=After B=%lld Reserved=%lld"), After.Balance, After.Reserved);
        if (After.Balance != WalletBeforeReplay.Balance || After.Reserved != WalletBeforeReplay.Reserved) { Fail(TEXT("ReplayMovedGold")); return; }
        UE_LOG(LogTemp, Display, TEXT("EXCHANGE_SCENARIO_DONE Trade=1 Listing=1 ReplayUnchanged=1"));
        ScenarioStep = ExchangeScenarioDone;
        break;
    }
    default: break;
    }
}
#endif
