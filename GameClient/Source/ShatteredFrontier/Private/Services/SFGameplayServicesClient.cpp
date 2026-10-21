#include "Services/SFGameplayServicesClient.h"
#include "Containers/Ticker.h"

namespace
{
    TFunction<FSFGameplaySend(const FString&)>& TransportFactory() { static TFunction<FSFGameplaySend(const FString&)> Factory; return Factory; }
    FSFGameplaySend& SendOverride() { static FSFGameplaySend Send; return Send; }
}

bool USFGameplayServicesSettings::Validate(FString& Error) const
{
    FString Host, Port;
    if (!Endpoint.Split(TEXT(":"), &Host, &Port) || Host != TEXT("127.0.0.1") || !Port.IsNumeric() || FCString::Atoi(*Port) < 1 || FCString::Atoi(*Port) > 65535)
    { Error = TEXT("Endpoint must be 127.0.0.1:<port> in the development topology."); return false; }
    if (!FMath::IsFinite(TimeoutSeconds) || TimeoutSeconds < 1 || TimeoutSeconds > 30) { Error = TEXT("TimeoutSeconds must be 1-30."); return false; }
    if (MaxAttempts < 1 || MaxAttempts > 5) { Error = TEXT("MaxAttempts must be 1-5."); return false; }
    return true;
}

void FSFGameplayServicesClient::SetTransportFactory(TFunction<FSFGameplaySend(const FString&)> Factory) { TransportFactory() = MoveTemp(Factory); }
void FSFGameplayServicesClient::SetSendOverrideForTests(FSFGameplaySend Send) { SendOverride() = MoveTemp(Send); }

TSharedPtr<FSFGameplayServicesClient> FSFGameplayServicesClient::Create(const USFGameplayServicesSettings& Settings)
{
    if (!Settings.bEnabled) { return nullptr; }
    FString Error;
    if (!Settings.Validate(Error)) { UE_LOG(LogTemp, Error, TEXT("GAMEPLAY_SERVICES_CONFIGURATION_FAILED %s"), *Error); return nullptr; }
    FSFGameplaySend Send = SendOverride() ? SendOverride() : TransportFactory() ? TransportFactory()(Settings.Endpoint) : FSFGameplaySend();
    if (!Send) { UE_LOG(LogTemp, Error, TEXT("GAMEPLAY_SERVICES_TRANSPORT_MISSING The server-only backend module is not loaded.")); return nullptr; }
    return MakeShareable(new FSFGameplayServicesClient(MoveTemp(Send), Settings.TimeoutSeconds, Settings.MaxAttempts));
}

void FSFGameplayServicesClient::Call(const FSFGameplayRequest& Request, TFunction<void(const FSFGameplayResponse&)> Done)
{
    Attempt(Request, 1, MoveTemp(Done));
}

void FSFGameplayServicesClient::Attempt(const FSFGameplayRequest& Request, int32 Number, TFunction<void(const FSFGameplayResponse&)> Done)
{
    Send(Request, TimeoutSeconds, [Self = AsShared(), Request, Number, Done = MoveTemp(Done)](const FSFGameplayResponse& Response) mutable
    {
        if (Response.Status != ESFGameplayStatus::Unavailable || Number >= Self->MaxAttempts) { Done(Response); return; }
        UE_LOG(LogTemp, Display, TEXT("GAMEPLAY_SERVICES_RETRY Rpc=%d Op=%s Attempt=%d Code=%d"), int32(Request.Rpc), *Request.OperationId, Number + 1, Response.GrpcCode);
        const float Delay = 0.25f * float(1 << (Number - 1));
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Self, Request, Number, Done = MoveTemp(Done)](float) mutable
        {
            Self->Attempt(Request, Number + 1, MoveTemp(Done));
            return false;
        }), Delay);
    });
}
