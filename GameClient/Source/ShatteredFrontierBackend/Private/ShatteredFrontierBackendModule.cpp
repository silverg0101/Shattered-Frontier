#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "SFGrpcLedgerTransport.h"
#include "SFGrantFollowUp.h"
#include "SFGrpcGameplayTransport.h"

namespace
{
#if !UE_BUILD_SHIPPING
    // Packaged-server stand-in for ShatteredFrontier.Backend.Grpc (a packaged Server has no automation
    // controller): one EnsureContainer round trip to the host, then a deadline against a silent peer.
    // Logs GRPC_ROUNDTRIP_OK / GRPC_DEADLINE_EXCEEDED_OK and exits 0, or exits 1. Development fixture only.
    void RunProbe(const FString& Endpoint)
    {
        TSharedRef<FSFGrpcLedgerTransport> Host = MakeShared<FSFGrpcLedgerTransport>(Endpoint);
        FSFLedgerRequest Ensure; Ensure.Rpc = ESFLedgerRpc::EnsureContainer;
        Ensure.ContainerId = TEXT("ctn_dev_grpc_roundtrip"); Ensure.OwnerId = TEXT("chr_dev_grpc_roundtrip"); Ensure.Capacity = 10;
        const double Started = FPlatformTime::Seconds();
        Host->Send(Ensure, 5, [Host, Started](const FSFLedgerResponse& Response)
        {
            const bool bRoundTrip = Response.GrpcCode == 0 && IsInGameThread();
            UE_LOG(LogTemp, Display, TEXT("%s Rpc=EnsureContainer Code=%d Revision=%lld Ms=%.1f %s"), bRoundTrip ? TEXT("GRPC_ROUNDTRIP_OK") : TEXT("GRPC_ROUNDTRIP_FAILED"),
                Response.GrpcCode, Response.Revision, (FPlatformTime::Seconds() - Started) * 1000, *Response.Message);
            ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
            TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
            bool bValid = false; Address->SetIp(TEXT("127.0.0.1"), bValid); Address->SetPort(0);
            FSocket* Silent = Sockets->CreateSocket(NAME_Stream, TEXT("SFGrpcSilentPeer"), Address->GetProtocolType());
            if (!bValid || !Silent || !Silent->Bind(*Address) || !Silent->Listen(4))
            { UE_LOG(LogTemp, Error, TEXT("GRPC_DEADLINE_FAILED NoSilentPeer=1")); FPlatformMisc::RequestExitWithStatus(false, 1); return; }
            TSharedRef<FSFGrpcLedgerTransport> Peer = MakeShared<FSFGrpcLedgerTransport>(FString::Printf(TEXT("127.0.0.1:%d"), Silent->GetPortNo()));
            FSFLedgerRequest Get; Get.Rpc = ESFLedgerRpc::GetContainer; Get.ContainerId = TEXT("ctn_dev_deadline");
            const double DeadlineStarted = FPlatformTime::Seconds();
            Peer->Send(Get, 0.5f, [Peer, Silent, Sockets, DeadlineStarted, bRoundTrip](const FSFLedgerResponse& Late)
            {
                const double Ms = (FPlatformTime::Seconds() - DeadlineStarted) * 1000;
                const bool bDeadline = Late.GrpcCode == 4 && Ms < 3000;
                UE_LOG(LogTemp, Display, TEXT("%s Code=%d Ms=%.1f"), bDeadline ? TEXT("GRPC_DEADLINE_EXCEEDED_OK") : TEXT("GRPC_DEADLINE_FAILED"), Late.GrpcCode, Ms);
                Sockets->DestroySocket(Silent);
                FPlatformMisc::RequestExitWithStatus(false, bRoundTrip && bDeadline ? 0 : 1);
            });
        });
    }
#endif
}

// Registers the gRPC transport with FSFInventoryLedgerClient. One transport (channel + completion-queue
// thread) per endpoint lives until module shutdown; senders hold weak references.
class FShatteredFrontierBackendModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FSFInventoryLedgerClient::SetFollowUpSerializer(SFSerializeGatherFollowUp);
        FSFInventoryLedgerClient::SetTransportFactory([this](const FString& Endpoint) -> FSFLedgerSend
        {
            TSharedPtr<FSFGrpcLedgerTransport>& Transport = Transports.FindOrAdd(Endpoint);
            if (!Transport) { Transport = MakeShared<FSFGrpcLedgerTransport>(Endpoint); }
            return [Weak = TWeakPtr<FSFGrpcLedgerTransport>(Transport)](const FSFLedgerRequest& Request, float Deadline, TFunction<void(const FSFLedgerResponse&)> Done)
            {
                if (const TSharedPtr<FSFGrpcLedgerTransport> Pinned = Weak.Pin()) { Pinned->Send(Request, Deadline, MoveTemp(Done)); return; }
                FSFLedgerResponse Closed; Closed.Message = TEXT("Ledger transport shut down."); Done(Closed);
            };
        });
        // INT-04: the same pattern for Gameplay.Host (one channel + completion-queue thread per endpoint).
        FSFGameplayServicesClient::SetTransportFactory([this](const FString& Endpoint) -> FSFGameplaySend
        {
            TSharedPtr<FSFGrpcGameplayTransport>& Transport = GameplayTransports.FindOrAdd(Endpoint);
            if (!Transport) { Transport = MakeShared<FSFGrpcGameplayTransport>(Endpoint); }
            return [Weak = TWeakPtr<FSFGrpcGameplayTransport>(Transport)](const FSFGameplayRequest& Request, float Deadline, TFunction<void(const FSFGameplayResponse&)> Done)
            {
                if (const TSharedPtr<FSFGrpcGameplayTransport> Pinned = Weak.Pin()) { Pinned->Send(Request, Deadline, MoveTemp(Done)); return; }
                FSFGameplayResponse Closed; Closed.Message = TEXT("Gameplay transport shut down."); Done(Closed);
            };
        });
#if !UE_BUILD_SHIPPING
        FString ProbeEndpoint;
        if (FParse::Value(FCommandLine::Get(), TEXT("SFLedgerGrpcProbe="), ProbeEndpoint))
        {
            // Start once the engine loop is ticking, so game-thread completions are delivered.
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ProbeEndpoint](float) { RunProbe(ProbeEndpoint); return false; }), 1.0f);
        }
#endif
    }
    virtual void ShutdownModule() override
    {
        FSFInventoryLedgerClient::SetTransportFactory(nullptr);
        FSFInventoryLedgerClient::SetFollowUpSerializer(nullptr);
        FSFGameplayServicesClient::SetTransportFactory(nullptr);
        GameplayTransports.Empty();
        Transports.Empty();
    }
private:
    TMap<FString, TSharedPtr<FSFGrpcLedgerTransport>> Transports;
    TMap<FString, TSharedPtr<FSFGrpcGameplayTransport>> GameplayTransports;
};

IMPLEMENT_MODULE(FShatteredFrontierBackendModule, ShatteredFrontierBackend)
