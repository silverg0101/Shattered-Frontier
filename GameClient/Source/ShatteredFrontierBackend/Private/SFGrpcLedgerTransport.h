#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Inventory/SFInventoryLedgerClient.h"

class FRunnableThread;
struct FSFGrpcLedgerChannel;

// One insecure loopback channel plus one completion-queue thread. Every call carries a deadline;
// completions are converted on the queue thread and delivered on the game thread.
class FSFGrpcLedgerTransport : public FRunnable
{
public:
    explicit FSFGrpcLedgerTransport(const FString& Endpoint);
    virtual ~FSFGrpcLedgerTransport() override;
    void Send(const FSFLedgerRequest& Request, float DeadlineSeconds, TFunction<void(const FSFLedgerResponse&)> Done);
    virtual uint32 Run() override;
private:
    TUniquePtr<FSFGrpcLedgerChannel> Channel;
    FRunnableThread* Thread = nullptr;
};
