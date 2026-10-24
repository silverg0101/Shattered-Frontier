#include "Services/SFGameplayServicesClient.h"
#include "Containers/Ticker.h"

namespace
{
    TFunction<FSFGameplaySend(const FString&)>& ServicesTransportFactory() { static TFunction<FSFGameplaySend(const FString&)> Factory; return Factory; }
    FSFGameplaySend& ServicesSendOverride() { static FSFGameplaySend Send; return Send; }
}

bool USFGameplayServicesSettings::Validate(FString& Error) const
{
    FString Host, Port;
    if (!Endpoint.Split(TEXT(":"), &Host, &Port) || Host != TEXT("127.0.0.1") || !Port.IsNumeric() || FCString::Atoi(*Port) < 1 || FCString::Atoi(*Port) > 65535)
    { Error = TEXT("Endpoint must be 127.0.0.1:<port> in the development topology."); return false; }
    if (!FMath::IsFinite(TimeoutSeconds) || TimeoutSeconds < 1 || TimeoutSeconds > 30) { Error = TEXT("TimeoutSeconds must be 1-30."); return false; }
    if (MaxAttempts < 1 || MaxAttempts > 5) { Error = TEXT("MaxAttempts must be 1-5."); return false; }
    if (GroupRegionId.IsEmpty() || GroupRegionId.Len() > 32) { Error = TEXT("GroupRegionId must be 1-32 characters."); return false; }
    for (TCHAR C : GroupRegionId)
    { if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_')) { Error = TEXT("GroupRegionId must use lowercase letters, digits or underscores."); return false; } }
    if (!FMath::IsFinite(GatheringNodeRetrySeconds) || GatheringNodeRetrySeconds < 1 || GatheringNodeRetrySeconds > 30)
    { Error = TEXT("GatheringNodeRetrySeconds must be 1-30."); return false; }
    if (ExchangeFeeBasisPoints < 0 || ExchangeFeeBasisPoints > 2000 || ExchangeListingFeeBasisPoints < 0 || ExchangeListingFeeBasisPoints > 1000)
    { Error = TEXT("Exchange fee previews are outside their bounds."); return false; }
    if (!FMath::IsFinite(MarketReachM) || MarketReachM < 1 || MarketReachM > 10 || MarketPageSize < 10 || MarketPageSize > 200 ||
        !FMath::IsFinite(MarketRefreshSeconds) || MarketRefreshSeconds < 1 || MarketRefreshSeconds > 60)
    { Error = TEXT("Market controls are outside their bounds."); return false; }
    if (!FMath::IsFinite(PositionSaveIntervalSeconds) || PositionSaveIntervalSeconds < 5 || PositionSaveIntervalSeconds > 600) { Error = TEXT("PositionSaveIntervalSeconds must be 5-600."); return false; }
    if (!FMath::IsFinite(LinkdeadDrainSeconds) || LinkdeadDrainSeconds < 5 || LinkdeadDrainSeconds > 120) { Error = TEXT("LinkdeadDrainSeconds must be 5-120."); return false; }
    return true;
}

void FSFGameplayServicesClient::SetTransportFactory(TFunction<FSFGameplaySend(const FString&)> Factory) { ServicesTransportFactory() = MoveTemp(Factory); }
void FSFGameplayServicesClient::SetSendOverrideForTests(FSFGameplaySend Send) { ServicesSendOverride() = MoveTemp(Send); }

TSharedPtr<FSFGameplayServicesClient> FSFGameplayServicesClient::Create(const USFGameplayServicesSettings& Settings)
{
    if (!Settings.bEnabled) { return nullptr; }
    FString Error;
    if (!Settings.Validate(Error)) { UE_LOG(LogTemp, Error, TEXT("GAMEPLAY_SERVICES_CONFIGURATION_FAILED %s"), *Error); return nullptr; }
    FSFGameplaySend Send = ServicesSendOverride() ? ServicesSendOverride() : ServicesTransportFactory() ? ServicesTransportFactory()(Settings.Endpoint) : FSFGameplaySend();
    if (!Send) { UE_LOG(LogTemp, Error, TEXT("GAMEPLAY_SERVICES_TRANSPORT_MISSING The server-only backend module is not loaded.")); return nullptr; }
    return MakeShareable(new FSFGameplayServicesClient(MoveTemp(Send), Settings.TimeoutSeconds, Settings.MaxAttempts));
}

void FSFGameplayServicesClient::Call(const FSFGameplayRequest& Request, TFunction<void(const FSFGameplayResponse&)> Done)
{
    const double StartedAt = FPlatformTime::Seconds();
    const ESFGameplayRpc Rpc = Request.Rpc;
    Attempt(Request, 1, [StartedAt, Rpc, Done = MoveTemp(Done)](const FSFGameplayResponse& Response) mutable
    {
        if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")))
        {
            UE_LOG(LogTemp, Display, TEXT("M3_GAMEPLAY_SERVICE Rpc=%d StartedSec=%.6f LatencyMs=%.3f Status=%d"),
                int32(Rpc), StartedAt, (FPlatformTime::Seconds() - StartedAt) * 1000.0, int32(Response.Status));
        }
        Done(Response);
    });
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
