#include "Modules/ModuleManager.h"
#include "SFGrpcLedgerTransport.h"

// Registers the gRPC transport with FSFInventoryLedgerClient. One transport (channel + completion-queue
// thread) per endpoint lives until module shutdown; senders hold weak references.
class FShatteredFrontierBackendModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
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
    }
    virtual void ShutdownModule() override
    {
        FSFInventoryLedgerClient::SetTransportFactory(nullptr);
        Transports.Empty();
    }
private:
    TMap<FString, TSharedPtr<FSFGrpcLedgerTransport>> Transports;
};

IMPLEMENT_MODULE(FShatteredFrontierBackendModule, ShatteredFrontierBackend)
