#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "SFGrpcLedgerTransport.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    struct FWait
    {
        TOptional<FSFLedgerResponse> Response;
        bool bOnGameThread = false;
        double Started = 0;
        TSharedPtr<FSFGrpcLedgerTransport> Transport;
        FSocket* Listener = nullptr;
    };

    void Expect(const TSharedRef<FWait>& Wait, FSFLedgerRequest Request, float Deadline)
    {
        Wait->Started = FPlatformTime::Seconds();
        Wait->Transport->Send(Request, Deadline, [Wait](const FSFLedgerResponse& Response) { Wait->bOnGameThread = IsInGameThread(); Wait->Response = Response; });
    }
}

// Step 0 acceptance: a dedicated-server process reaches InventoryLedger.Host over gRPC. The runner
// (tools/Test-GatheringPilot.ps1 -Ledger) sets the endpoint; without it there is no host to reach.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGrpcLedgerRoundTripTest, "ShatteredFrontier.Backend.Grpc.RoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FSFGrpcLedgerRoundTripTest::RunTest(const FString&)
{
    const FString Endpoint = FPlatformMisc::GetEnvironmentVariable(TEXT("SF_INVENTORY_LEDGER_TEST_ENDPOINT"));
    if (Endpoint.IsEmpty()) { AddInfo(TEXT("GRPC_ROUNDTRIP_SKIPPED No SF_INVENTORY_LEDGER_TEST_ENDPOINT; run tools/Test-GatheringPilot.ps1 -Ledger.")); return true; }
    TestTrue(TEXT("runs in a dedicated server process"), IsRunningDedicatedServer());
    TSharedRef<FWait> Wait = MakeShared<FWait>();
    Wait->Transport = MakeShared<FSFGrpcLedgerTransport>(Endpoint);
    FSFLedgerRequest Request; Request.Rpc = ESFLedgerRpc::EnsureContainer;
    Request.ContainerId = TEXT("ctn_dev_grpc_roundtrip"); Request.OwnerId = TEXT("chr_dev_grpc_roundtrip"); Request.Capacity = 10;
    Expect(Wait, Request, 5);
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Wait]()
    {
        if (!Wait->Response && FPlatformTime::Seconds() - Wait->Started < 10) { return false; }
        if (!Wait->Response) { AddError(TEXT("No completion within 10 s.")); return true; }
        TestEqual(TEXT("EnsureContainer status"), Wait->Response->GrpcCode, 0);
        TestTrue(TEXT("completion delivered on the game thread"), Wait->bOnGameThread);
        TestEqual(TEXT("new container revision"), Wait->Response->Revision, int64(0));
        if (Wait->Response->GrpcCode == 0 && Wait->bOnGameThread)
        { UE_LOG(LogTemp, Display, TEXT("GRPC_ROUNDTRIP_OK Rpc=EnsureContainer Revision=%lld Ms=%.1f"), Wait->Response->Revision, (FPlatformTime::Seconds() - Wait->Started) * 1000); }
        else { UE_LOG(LogTemp, Error, TEXT("GRPC_ROUNDTRIP_FAILED Code=%d %s"), Wait->Response->GrpcCode, *Wait->Response->Message); }
        Wait->Transport.Reset();
        return true;
    }));
    return true;
}

// A peer that accepts TCP but never speaks HTTP/2: the call must end by its deadline, not hang.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGrpcLedgerDeadlineTest, "ShatteredFrontier.Backend.Grpc.DeadlineExceeded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FSFGrpcLedgerDeadlineTest::RunTest(const FString&)
{
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr();
    bool bValid = false; Address->SetIp(TEXT("127.0.0.1"), bValid); Address->SetPort(0);
    TSharedRef<FWait> Wait = MakeShared<FWait>();
    Wait->Listener = Sockets->CreateSocket(NAME_Stream, TEXT("SFGrpcSilentPeer"), Address->GetProtocolType());
    if (!TestTrue(TEXT("silent peer listens"), bValid && Wait->Listener && Wait->Listener->Bind(*Address) && Wait->Listener->Listen(4))) { return false; }
    const int32 Port = Wait->Listener->GetPortNo();
    Wait->Transport = MakeShared<FSFGrpcLedgerTransport>(FString::Printf(TEXT("127.0.0.1:%d"), Port));
    FSFLedgerRequest Request; Request.Rpc = ESFLedgerRpc::GetContainer; Request.ContainerId = TEXT("ctn_dev_deadline");
    Expect(Wait, Request, 0.5f);
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Wait, Sockets]()
    {
        const double Elapsed = FPlatformTime::Seconds() - Wait->Started;
        if (!Wait->Response && Elapsed < 10) { return false; }
        if (!Wait->Response) { AddError(TEXT("Deadline did not fire within 10 s.")); }
        else
        {
            TestEqual(TEXT("DEADLINE_EXCEEDED"), Wait->Response->GrpcCode, 4);
            TestTrue(TEXT("retryable status"), Wait->Response->Status == ESFLedgerStatus::Unavailable);
            TestTrue(TEXT("completion delivered on the game thread"), Wait->bOnGameThread);
            TestTrue(TEXT("ended near its 0.5 s deadline"), Elapsed < 3);
            UE_LOG(LogTemp, Display, TEXT("GRPC_DEADLINE_EXCEEDED_OK Code=%d Ms=%.1f"), Wait->Response->GrpcCode, Elapsed * 1000);
        }
        Wait->Transport.Reset();
        Sockets->DestroySocket(Wait->Listener);
        return true;
    }));
    return true;
}

#endif
