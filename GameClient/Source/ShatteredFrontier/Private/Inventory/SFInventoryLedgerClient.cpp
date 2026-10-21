#include "Inventory/SFInventoryLedgerClient.h"
#include "Containers/Ticker.h"

namespace
{
    TFunction<FSFLedgerSend(const FString&)>& TransportFactory() { static TFunction<FSFLedgerSend(const FString&)> Factory; return Factory; }
    FSFLedgerSend& SendOverride() { static FSFLedgerSend Send; return Send; }
    FSFInventoryLedgerClient::FFollowUpSerializer& FollowUpSerializer() { static FSFInventoryLedgerClient::FFollowUpSerializer Serializer; return Serializer; }
}

void FSFInventoryLedgerClient::SetFollowUpSerializer(FFollowUpSerializer Serializer) { FollowUpSerializer() = MoveTemp(Serializer); }
bool FSFInventoryLedgerClient::SerializeFollowUp(const FSFGameplayRequest& Receipt, TArray<uint8>& Payload, FString& Digest)
{
    return FollowUpSerializer() && FollowUpSerializer()(Receipt, Payload, Digest) && !Payload.IsEmpty() && !Digest.IsEmpty();
}

bool USFInventoryLedgerSettings::Validate(FString& Error) const
{
    FString Host, Port;
    if (!Endpoint.Split(TEXT(":"), &Host, &Port) || Host != TEXT("127.0.0.1") || !Port.IsNumeric() || FCString::Atoi(*Port) < 1 || FCString::Atoi(*Port) > 65535)
    { Error = TEXT("Endpoint must be 127.0.0.1:<port> in the development topology."); return false; }
    if (!FMath::IsFinite(TimeoutSeconds) || TimeoutSeconds < 1 || TimeoutSeconds > 30) { Error = TEXT("TimeoutSeconds must be 1-30."); return false; }
    if (MaxAttempts < 1 || MaxAttempts > 5) { Error = TEXT("MaxAttempts must be 1-5."); return false; }
    return true;
}

void FSFInventoryLedgerClient::SetTransportFactory(TFunction<FSFLedgerSend(const FString&)> Factory) { TransportFactory() = MoveTemp(Factory); }
void FSFInventoryLedgerClient::SetSendOverrideForTests(FSFLedgerSend Send) { SendOverride() = MoveTemp(Send); }

TSharedPtr<FSFInventoryLedgerClient> FSFInventoryLedgerClient::Create(const USFInventoryLedgerSettings& Settings)
{
    FString Error;
    if (!Settings.Validate(Error)) { UE_LOG(LogTemp, Error, TEXT("GATHERING_LEDGER_CONFIGURATION_FAILED %s"), *Error); return nullptr; }
    FSFLedgerSend Send = SendOverride() ? SendOverride() : TransportFactory() ? TransportFactory()(Settings.Endpoint) : FSFLedgerSend();
    if (!Send) { UE_LOG(LogTemp, Error, TEXT("GATHERING_LEDGER_TRANSPORT_MISSING The server-only backend module is not loaded.")); return nullptr; }
    return MakeShareable(new FSFInventoryLedgerClient(MoveTemp(Send), Settings.TimeoutSeconds, Settings.MaxAttempts));
}

void FSFInventoryLedgerClient::Call(const FSFLedgerRequest& Request, TFunction<void(const FSFLedgerResponse&)> Done)
{
    Attempt(Request, 1, MoveTemp(Done));
}

void FSFInventoryLedgerClient::Attempt(const FSFLedgerRequest& Request, int32 Number, TFunction<void(const FSFLedgerResponse&)> Done)
{
    Send(Request, TimeoutSeconds, [Self = AsShared(), Request, Number, Done = MoveTemp(Done)](const FSFLedgerResponse& Response) mutable
    {
        if (Response.Status != ESFLedgerStatus::Unavailable || Number >= Self->MaxAttempts) { Done(Response); return; }
        UE_LOG(LogTemp, Display, TEXT("GATHERING_LEDGER_RETRY Rpc=%d Container=%s Op=%s Attempt=%d Code=%d"), int32(Request.Rpc), *Request.ContainerId, *Request.OperationId, Number + 1, Response.GrpcCode);
        // Exponential backoff on the core ticker: the game thread never waits.
        const float Delay = 0.25f * float(1 << (Number - 1));
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self, Request, Number, Done = MoveTemp(Done)](float) mutable
        {
            Self->Attempt(Request, Number + 1, MoveTemp(Done));
            return false;
        }), Delay);
    });
}
