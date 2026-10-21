#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Services/SFGameplayServicesClient.h"

class FRunnableThread;
struct FSFGrpcGameplayChannel;

// INT-04: one insecure loopback channel to Gameplay.Host plus one completion-queue thread, as FSFGrpcLedgerTransport.
// Every call carries a deadline and the per-run bearer token; completions are delivered on the game thread.
class FSFGrpcGameplayTransport : public FRunnable
{
public:
    explicit FSFGrpcGameplayTransport(const FString& Endpoint);
    virtual ~FSFGrpcGameplayTransport() override;
    void Send(const FSFGameplayRequest& Request, float DeadlineSeconds, TFunction<void(const FSFGameplayResponse&)> Done);
    virtual uint32 Run() override;
private:
    TUniquePtr<FSFGrpcGameplayChannel> Channel;
    FRunnableThread* Thread = nullptr;
};
