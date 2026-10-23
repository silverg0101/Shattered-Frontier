#include "SFGrpcLedgerTransport.h"
#include "SFGrpcIncludes.h"
#include "Async/Async.h"
#include "HAL/PlatformMisc.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"

namespace v1 = sf::inventory::v1;

namespace
{
    std::string Utf8(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }
    FString FromUtf8(const std::string& Value) { return FString(UTF8_TO_TCHAR(Value.c_str())); }

    struct FCall
    {
        grpc::ClientContext Context;
        grpc::Status Status;
        ESFLedgerRpc Rpc = ESFLedgerRpc::GetContainer;
        FString OperationId;
        FString ContainerId;
        FString ToContainerId;  // Transfer
        v1::ContainerView View;
        v1::LedgerReceipt Receipt;
        std::unique_ptr<grpc::ClientAsyncResponseReader<v1::ContainerView>> ViewReader;
        std::unique_ptr<grpc::ClientAsyncResponseReader<v1::LedgerReceipt>> ReceiptReader;
        TFunction<void(const FSFLedgerResponse&)> Done;
    };

    FSFLedgerResponse Convert(const FCall& Call)
    {
        FSFLedgerResponse Out;
        Out.GrpcCode = int32(Call.Status.error_code());
        Out.Message = FromUtf8(Call.Status.error_message());
        switch (Call.Status.error_code())
        {
        case grpc::StatusCode::OK: Out.Status = ESFLedgerStatus::Ok; break;
        case grpc::StatusCode::ABORTED: Out.Status = ESFLedgerStatus::Stale; break;
        case grpc::StatusCode::UNAVAILABLE:
        case grpc::StatusCode::DEADLINE_EXCEEDED:
        case grpc::StatusCode::CANCELLED: Out.Status = ESFLedgerStatus::Unavailable; break;
        default: Out.Status = ESFLedgerStatus::Rejected; break;
        }
        if (Out.Status != ESFLedgerStatus::Ok) { return Out; }
        if (Call.Rpc == ESFLedgerRpc::Grant || Call.Rpc == ESFLedgerRpc::Transfer)
        {
            Out.OperationId = FromUtf8(Call.Receipt.operation_id());
            for (const auto& Revision : Call.Receipt.revisions())
            {
                const FString Id = FromUtf8(Revision.container_id());
                if (Id == Call.ContainerId) { Out.Revision = Revision.after(); }
                else if (Id == Call.ToContainerId) { Out.ToRevision = Revision.after(); }
            }
            for (const auto& Change : Call.Receipt.changes())
            {
                if (FromUtf8(Change.container_id()) != Call.ContainerId) { continue; }
                Out.GrantedDelta += Change.delta();
                Out.Quantities.Add(FromUtf8(Change.definition_id()), Change.delta());
            }
            return Out;
        }
        Out.Revision = Call.View.revision();
        for (const auto& Entry : Call.View.quantities()) { Out.Quantities.Add(FromUtf8(Entry.first), Entry.second); }
        return Out;
    }
}

struct FSFGrpcLedgerChannel
{
    std::unique_ptr<v1::InventoryLedger::Stub> Stub;
    grpc::CompletionQueue Queue;
    std::string Token;
    FCriticalSection PendingLock;
    TSet<FCall*> Pending;
};

FSFGrpcLedgerTransport::FSFGrpcLedgerTransport(const FString& Endpoint) : Channel(MakeUnique<FSFGrpcLedgerChannel>())
{
    // Development fixture: loopback-only insecure channel; settings validation rejects other hosts.
    Channel->Stub = v1::InventoryLedger::NewStub(grpc::CreateChannel(Utf8(Endpoint), grpc::InsecureChannelCredentials()));
    Channel->Token = Utf8(FPlatformMisc::GetEnvironmentVariable(TEXT("SF_INVENTORY_LEDGER_TOKEN")));
    if (Channel->Token.empty()) { UE_LOG(LogTemp, Warning, TEXT("GATHERING_LEDGER_TOKEN_MISSING SF_INVENTORY_LEDGER_TOKEN is empty; the host will reject calls.")); }
    Thread = FRunnableThread::Create(this, TEXT("SFInventoryLedgerGrpc"));
}

FSFGrpcLedgerTransport::~FSFGrpcLedgerTransport()
{
    {
        FScopeLock Lock(&Channel->PendingLock);
        for (FCall* Call : Channel->Pending) { Call->Context.TryCancel(); }
    }
    Channel->Queue.Shutdown();
    if (Thread) { Thread->WaitForCompletion(); delete Thread; }
}

void FSFGrpcLedgerTransport::Send(const FSFLedgerRequest& Request, float DeadlineSeconds, TFunction<void(const FSFLedgerResponse&)> Done)
{
    FCall* Call = new FCall();
    Call->Rpc = Request.Rpc; Call->OperationId = Request.OperationId; Call->ContainerId = Request.ContainerId; Call->ToContainerId = Request.ToContainerId;
    Call->Done = MoveTemp(Done);
    Call->Context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(int64(FMath::Max(0.001f, DeadlineSeconds) * 1000.0f)));
    Call->Context.AddMetadata("authorization", "Bearer " + Channel->Token);
    {
        FScopeLock Lock(&Channel->PendingLock);
        Channel->Pending.Add(Call);
    }
    switch (Request.Rpc)
    {
    case ESFLedgerRpc::EnsureContainer:
    {
        v1::EnsureContainerRequest Message;
        auto* Spec = Message.mutable_spec();
        Spec->set_id(Utf8(Request.ContainerId)); Spec->set_owner_kind(v1::OWNER_KIND_CHARACTER); Spec->set_owner_id(Utf8(Request.OwnerId));
        Spec->set_kind(Request.bEquipmentContainer ? v1::CONTAINER_KIND_EQUIPMENT : v1::CONTAINER_KIND_BACKPACK); Spec->set_capacity(Request.Capacity);
        Call->ViewReader = Channel->Stub->AsyncEnsureContainer(&Call->Context, Message, &Channel->Queue);
        Call->ViewReader->Finish(&Call->View, &Call->Status, Call);
        break;
    }
    case ESFLedgerRpc::GetContainer:
    {
        v1::GetContainerRequest Message;
        Message.set_container_id(Utf8(Request.ContainerId));
        Call->ViewReader = Channel->Stub->AsyncGetContainer(&Call->Context, Message, &Channel->Queue);
        Call->ViewReader->Finish(&Call->View, &Call->Status, Call);
        break;
    }
    case ESFLedgerRpc::Grant:
    {
        v1::GrantRequest Message;
        Message.set_operation_id(Utf8(Request.OperationId)); Message.set_container_id(Utf8(Request.ContainerId));
        Message.set_expected_revision(Request.ExpectedRevision); Message.set_definition_id(Utf8(Request.DefinitionId));
        Message.set_quantity(Request.Quantity); Message.set_source(Utf8(Request.Source)); Message.set_causation_id(Utf8(Request.CausationId));
        if (!Request.FollowUpPayload.IsEmpty())
        {
            auto* Intent = Message.mutable_follow_up_intent();
            Intent->set_payload_version(1);
            Intent->set_payload(Request.FollowUpPayload.GetData(), Request.FollowUpPayload.Num());
            Intent->set_payload_digest(Utf8(Request.FollowUpDigest));
        }
        Call->ReceiptReader = Request.FollowUpPayload.IsEmpty()
            ? Channel->Stub->AsyncGrant(&Call->Context, Message, &Channel->Queue)
            : Channel->Stub->AsyncGrantWithIntent(&Call->Context, Message, &Channel->Queue);
        Call->ReceiptReader->Finish(&Call->Receipt, &Call->Status, Call);
        break;
    }
    case ESFLedgerRpc::Transfer:
    {
        v1::TransferRequest Message;
        Message.set_operation_id(Utf8(Request.OperationId));
        Message.set_from_container_id(Utf8(Request.ContainerId)); Message.set_from_expected_revision(Request.ExpectedRevision);
        Message.set_to_container_id(Utf8(Request.ToContainerId)); Message.set_to_expected_revision(Request.ToExpectedRevision);
        Message.set_definition_id(Utf8(Request.DefinitionId)); Message.set_quantity(Request.Quantity);
        Call->ReceiptReader = Channel->Stub->AsyncTransfer(&Call->Context, Message, &Channel->Queue);
        Call->ReceiptReader->Finish(&Call->Receipt, &Call->Status, Call);
        break;
    }
    }
}

uint32 FSFGrpcLedgerTransport::Run()
{
    void* Tag = nullptr; bool bOk = false;
    while (Channel->Queue.Next(&Tag, &bOk))
    {
        FCall* Call = static_cast<FCall*>(Tag);
        {
            FScopeLock Lock(&Channel->PendingLock);
            Channel->Pending.Remove(Call);
        }
        FSFLedgerResponse Response = Convert(*Call);
        TFunction<void(const FSFLedgerResponse&)> Done = MoveTemp(Call->Done);
        delete Call;
        AsyncTask(ENamedThreads::GameThread, [Done = MoveTemp(Done), Response = MoveTemp(Response)]() { Done(Response); });
    }
    return 0;
}
