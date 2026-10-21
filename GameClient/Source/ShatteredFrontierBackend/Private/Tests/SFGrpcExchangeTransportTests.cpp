#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "SFGrpcGameplayTransport.h"

#if WITH_DEV_AUTOMATION_TESTS

// Exchange plumbing against a real Gameplay.Host (tools/Test-ExchangeTransport.ps1 sets the endpoint and token): a fresh
// character's wallet reads back empty, and a trade with no items is a typed refusal (INVALID_ARGUMENT), not a retry.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGrpcExchangeRoundTripTest, "ShatteredFrontier.Backend.Grpc.Exchange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FSFGrpcExchangeRoundTripTest::RunTest(const FString&)
{
    const FString Endpoint = FPlatformMisc::GetEnvironmentVariable(TEXT("SF_GAMEPLAY_TEST_ENDPOINT"));
    if (Endpoint.IsEmpty()) { AddInfo(TEXT("GRPC_EXCHANGE_SKIPPED No SF_GAMEPLAY_TEST_ENDPOINT; run tools/Test-ExchangeTransport.ps1.")); return true; }
    struct FState { TSharedPtr<FSFGrpcGameplayTransport> Transport; TOptional<FSFGameplayResponse> Wallet, Refused; double Started = 0; };
    TSharedRef<FState> State = MakeShared<FState>();
    State->Transport = MakeShared<FSFGrpcGameplayTransport>(Endpoint);
    State->Started = FPlatformTime::Seconds();
    FSFGameplayRequest Wallet; Wallet.Rpc = ESFGameplayRpc::ExchangeWallet; Wallet.CharacterId = TEXT("chr_dev_exchange_roundtrip");
    State->Transport->Send(Wallet, 5, [State](const FSFGameplayResponse& R) { State->Wallet = R; });
    FSFGameplayRequest Offer; Offer.Rpc = ESFGameplayRpc::OfferTrade; Offer.OperationId = TEXT("trade/roundtrip/empty");
    Offer.Exchange.TradeId = TEXT("trd_roundtrip"); Offer.Exchange.PartyA = TEXT("chr_dev_a"); Offer.Exchange.PartyB = TEXT("chr_dev_b");
    State->Transport->Send(Offer, 5, [State](const FSFGameplayResponse& R) { State->Refused = R; });
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
    {
        if ((!State->Wallet || !State->Refused) && FPlatformTime::Seconds() - State->Started < 10) { return false; }
        if (!State->Wallet || !State->Refused) { AddError(TEXT("No completion within 10 s.")); State->Transport.Reset(); return true; }
        TestEqual(TEXT("Wallet status"), State->Wallet->GrpcCode, 0);
        TestEqual(TEXT("fresh wallet balance"), State->Wallet->Exchange.Balance, int64(0));
        TestEqual(TEXT("empty trade refused as INVALID_ARGUMENT"), State->Refused->GrpcCode, 3);
        TestTrue(TEXT("refusal is not retryable"), State->Refused->Status == ESFGameplayStatus::Rejected);
        if (State->Wallet->GrpcCode == 0 && State->Refused->GrpcCode == 3)
        { UE_LOG(LogTemp, Display, TEXT("GRPC_EXCHANGE_OK Balance=%lld Available=%lld Revision=%lld"), State->Wallet->Exchange.Balance, State->Wallet->Exchange.Available, State->Wallet->Exchange.WalletRevision); }
        else { UE_LOG(LogTemp, Error, TEXT("GRPC_EXCHANGE_FAILED Wallet=%d %s Offer=%d %s"), State->Wallet->GrpcCode, *State->Wallet->Message, State->Refused->GrpcCode, *State->Refused->Message); }
        State->Transport.Reset();
        return true;
    }));
    return true;
}

#endif
