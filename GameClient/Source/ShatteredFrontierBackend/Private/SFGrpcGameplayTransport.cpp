#include "SFGrpcGameplayTransport.h"
#include "SFGrpcGameplayIncludes.h"
#include "Async/Async.h"
#include "HAL/PlatformMisc.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"

namespace
{
    std::string Utf8(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }
    FString FromUtf8(const std::string& Value) { return FString(UTF8_TO_TCHAR(Value.c_str())); }

    // One in-flight call. The reply message type differs per RPC; Fill copies it into the flat response.
    struct FCall
    {
        virtual ~FCall() = default;
        grpc::ClientContext Context;
        grpc::Status Status;
        TFunction<void(const FSFGameplayResponse&)> Done;
        virtual void Fill(FSFGameplayResponse& Out) const = 0;
    };

    template<typename TReply>
    struct TCall : FCall
    {
        TReply Reply;
        std::unique_ptr<grpc::ClientAsyncResponseReader<TReply>> Reader;
        TFunction<void(const TReply&, FSFGameplayResponse&)> Copy;
        virtual void Fill(FSFGameplayResponse& Out) const override { Copy(Reply, Out); }
    };

    FSFGameplayResponse Convert(const FCall& Call)
    {
        FSFGameplayResponse Out;
        Out.GrpcCode = int32(Call.Status.error_code());
        Out.Message = FromUtf8(Call.Status.error_message());
        switch (Call.Status.error_code())
        {
        case grpc::StatusCode::OK: Out.Status = ESFGameplayStatus::Ok; break;
        case grpc::StatusCode::UNAVAILABLE:
        case grpc::StatusCode::DEADLINE_EXCEEDED:
        case grpc::StatusCode::CANCELLED: Out.Status = ESFGameplayStatus::Unavailable; break;
        default: Out.Status = ESFGameplayStatus::Rejected; break;
        }
        if (Out.Status == ESFGameplayStatus::Ok) { Call.Fill(Out); }
        return Out;
    }

    void CopyClaim(const sf::housing::v1::ClaimReply& R, FSFGameplayResponse& Out)
    { Out.State = FromUtf8(R.state()); Out.Revision = R.revision(); Out.PermissionRevision = R.permission_revision(); }

    void CopyPlacement(const sf::housing::v1::PlacementReply& R, FSFGameplayResponse& Out)
    { Out.State = FromUtf8(R.state()); Out.StructureId = FromUtf8(R.structure_id()); Out.StructureRevision = R.structure_revision(); }
}

struct FSFGrpcGameplayChannel
{
    std::unique_ptr<sf::rewards::v1::Rewards::Stub> Rewards;
    std::unique_ptr<sf::progression::v1::Progression::Stub> Progression;
    std::unique_ptr<sf::housing::v1::Housing::Stub> Housing;
    std::unique_ptr<sf::crafting::v1::Crafting::Stub> Crafting;
    grpc::CompletionQueue Queue;
    std::string Token;
    FCriticalSection PendingLock;
    TSet<FCall*> Pending;
};

FSFGrpcGameplayTransport::FSFGrpcGameplayTransport(const FString& Endpoint) : Channel(MakeUnique<FSFGrpcGameplayChannel>())
{
    // Development fixture: loopback-only insecure channel; settings validation rejects other hosts.
    const std::shared_ptr<grpc::Channel> Wire = grpc::CreateChannel(Utf8(Endpoint), grpc::InsecureChannelCredentials());
    Channel->Rewards = sf::rewards::v1::Rewards::NewStub(Wire);
    Channel->Progression = sf::progression::v1::Progression::NewStub(Wire);
    Channel->Housing = sf::housing::v1::Housing::NewStub(Wire);
    Channel->Crafting = sf::crafting::v1::Crafting::NewStub(Wire);
    Channel->Token = Utf8(FPlatformMisc::GetEnvironmentVariable(TEXT("SF_GAMEPLAY_TOKEN")));
    if (Channel->Token.empty()) { UE_LOG(LogTemp, Warning, TEXT("GAMEPLAY_SERVICES_TOKEN_MISSING SF_GAMEPLAY_TOKEN is empty; the host will reject calls.")); }
    Thread = FRunnableThread::Create(this, TEXT("SFGameplayServicesGrpc"));
}

FSFGrpcGameplayTransport::~FSFGrpcGameplayTransport()
{
    {
        FScopeLock Lock(&Channel->PendingLock);
        for (FCall* Call : Channel->Pending) { Call->Context.TryCancel(); }
    }
    Channel->Queue.Shutdown();
    if (Thread) { Thread->WaitForCompletion(); delete Thread; }
}

void FSFGrpcGameplayTransport::Send(const FSFGameplayRequest& Q, float DeadlineSeconds, TFunction<void(const FSFGameplayResponse&)> Done)
{
    // Builds the typed call, starts it on the shared queue and tracks it for cancellation at shutdown.
    auto Start = [this, &Q, DeadlineSeconds, &Done](auto* Call, auto&& Begin)
    {
        Call->Done = MoveTemp(Done);
        Call->Context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(int64(FMath::Max(0.001f, DeadlineSeconds) * 1000.0f)));
        Call->Context.AddMetadata("authorization", "Bearer " + Channel->Token);
        {
            FScopeLock Lock(&Channel->PendingLock);
            Channel->Pending.Add(Call);
        }
        Call->Reader = Begin(&Call->Context);
        Call->Reader->Finish(&Call->Reply, &Call->Status, static_cast<FCall*>(Call));
    };
    switch (Q.Rpc)
    {
    case ESFGameplayRpc::GrantDefeatReward:
    {
        sf::rewards::v1::GrantDefeatRewardRequest M;
        M.set_defeat_event_id(Utf8(Q.OperationId)); M.set_killer_character_id(Utf8(Q.CharacterId)); M.set_recipient_character_id(Utf8(Q.CharacterId));
        M.set_container_id(Utf8(Q.ContainerId)); M.set_expected_container_revision(Q.ExpectedContainerRevision);
        M.set_killer_in_protected_town(Q.bKillerProtected); M.set_boar_in_protected_town(Q.bBoarProtected); M.set_table_id(Utf8(Q.TableId));
        auto* Call = new TCall<sf::rewards::v1::RewardReceipt>();
        Call->Copy = [](const sf::rewards::v1::RewardReceipt& R, FSFGameplayResponse& Out)
        {
            Out.bGranted = R.granted(); Out.SourceCode = FromUtf8(R.source_code()); Out.LedgerOperationId = FromUtf8(R.ledger_operation_id());
            Out.DefinitionId = FromUtf8(R.definition_id()); Out.Quantity = R.quantity(); Out.ContainerRevision = R.container_revision();
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncGrantDefeatReward(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ApplyReceipt:
    {
        sf::progression::v1::ApplyReceiptRequest M;
        M.set_source_receipt_id(Utf8(Q.OperationId)); M.set_kind(Utf8(Q.Kind)); M.set_character_id(Utf8(Q.CharacterId));
        M.set_expected_progression_revision(Q.ExpectedProgressionRevision); M.set_expected_quest_revision(Q.ExpectedQuestRevision);
        if (Q.Kind != TEXT("quest"))
        {
            auto* P = M.mutable_payload();
            P->set_skill_id(Utf8(Q.SkillId)); P->set_difficulty(Q.Difficulty); P->set_action_seconds(Q.ActionSeconds); P->set_context_hash(Q.ContextHash);
            P->set_success(Q.bSuccess); P->set_enemy_level(Q.EnemyLevel); P->set_enemy_tier_multiplier(Q.EnemyTierMultiplier); P->set_eligible_party_size(Q.PartySize);
        }
        if (Q.bHasQuest)
        {
            auto* E = M.mutable_quest();
            E->set_quest_id(Utf8(Q.QuestId)); E->set_step_id(Utf8(Q.StepId)); E->set_kind(Utf8(Q.QuestKind)); E->set_target_id(Utf8(Q.TargetId));
            E->set_node_id(Utf8(Q.NodeId)); E->set_quantity(Q.Quantity); E->set_occurred_at_unix_ms(Q.OccurredAtUnixMs);
        }
        auto* Call = new TCall<sf::progression::v1::ApplyReceiptReply>();
        Call->Copy = [](const sf::progression::v1::ApplyReceiptReply& R, FSFGameplayResponse& Out)
        {
            Out.bProgressionApplied = R.progression_applied(); Out.SkillGain = R.skill_gain(); Out.XpGain = R.xp_gain(); Out.Level = R.level();
            Out.ProgressionRevision = R.progression_revision(); Out.bQuestAccepted = R.quest_accepted(); Out.ActiveStepId = FromUtf8(R.active_step_id());
            Out.bQuestCompleted = R.quest_completed(); Out.QuestRevision = R.quest_revision();
            for (const auto& Bundle : R.reward_bundle_ids()) { Out.RewardBundleIds.Add(FromUtf8(Bundle)); }
            for (const auto& Item : R.granted_items()) { Out.GrantedItems.FindOrAdd(FromUtf8(Item.definition_id())) += Item.quantity(); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncApplyReceipt(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::GetCharacterState:
    {
        sf::progression::v1::GetCharacterStateRequest M;
        M.set_character_id(Utf8(Q.CharacterId));
        auto* Call = new TCall<sf::progression::v1::CharacterStateReply>();
        Call->Copy = [](const sf::progression::v1::CharacterStateReply& R, FSFGameplayResponse& Out)
        {
            Out.ProgressionRevision = R.progression_revision(); Out.Level = R.level(); Out.QuestRevision = R.quest_revision();
            Out.ActiveStepId = FromUtf8(R.active_step_id()); Out.bQuestCompleted = R.quest_completed();
            for (const auto& Skill : R.skills()) { Out.Skills.Add(FromUtf8(Skill.first), Skill.second); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncGetCharacterState(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ClaimPlot:
    {
        sf::housing::v1::ClaimPlotRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_claim_id(Utf8(Q.ClaimId)); M.set_owner_id(Utf8(Q.CharacterId));
        M.set_center_x(Q.CenterX); M.set_center_y(Q.CenterY); M.set_side_metres(Q.SideMetres); M.set_prepaid_days(Q.PrepaidDays);
        auto* Call = new TCall<sf::housing::v1::ClaimReply>(); Call->Copy = &CopyClaim;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Housing->AsyncClaimPlot(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::GetClaim:
    {
        sf::housing::v1::GetClaimRequest M;
        M.set_claim_id(Utf8(Q.ClaimId));
        auto* Call = new TCall<sf::housing::v1::ClaimReply>(); Call->Copy = &CopyClaim;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Housing->AsyncGetClaim(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::Place:
    {
        sf::housing::v1::PlaceRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_claim_id(Utf8(Q.ClaimId)); M.set_actor_id(Utf8(Q.CharacterId));
        M.set_expected_permission_revision(Q.ExpectedPermissionRevision); M.set_expected_structure_revision(Q.ExpectedStructureRevision);
        M.set_structure_id(Utf8(Q.StructureId)); M.set_cell_x(Q.Cell.X); M.set_cell_y(Q.Cell.Y); M.set_cell_z(Q.Cell.Z);
        M.set_content_id(Utf8(Q.ContentId)); M.set_material_id(Utf8(Q.MaterialId)); M.set_material_quantity(Q.MaterialQuantity);
        M.set_container_id(Utf8(Q.ContainerId)); M.set_expected_container_revision(Q.ExpectedContainerRevision);
        auto* Call = new TCall<sf::housing::v1::PlacementReply>(); Call->Copy = &CopyPlacement;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Housing->AsyncPlace(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::Remove:
    {
        sf::housing::v1::RemoveRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_claim_id(Utf8(Q.ClaimId)); M.set_actor_id(Utf8(Q.CharacterId)); M.set_structure_id(Utf8(Q.StructureId));
        M.set_expected_permission_revision(Q.ExpectedPermissionRevision); M.set_expected_structure_revision(Q.ExpectedStructureRevision);
        M.set_expected_container_revision(Q.ExpectedContainerRevision); M.set_removal_kind(Utf8(Q.RemovalKind));
        auto* Call = new TCall<sf::housing::v1::PlacementReply>(); Call->Copy = &CopyPlacement;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Housing->AsyncRemove(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::Craft:
    {
        sf::crafting::v1::CraftRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId)); M.set_container_id(Utf8(Q.ContainerId));
        M.set_recipe_id(Utf8(Q.RecipeId)); M.set_expected_container_revision(Q.ExpectedContainerRevision); M.set_count(Q.Count);
        auto* Call = new TCall<sf::crafting::v1::CraftReply>();
        Call->Copy = [](const sf::crafting::v1::CraftReply& R, FSFGameplayResponse& Out)
        {
            Out.ContainerRevision = R.container_revision();
            for (const auto& Change : R.changes()) { Out.Changes.FindOrAdd(FromUtf8(Change.definition_id())) += Change.delta(); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Crafting->AsyncCraft(C, M, &Channel->Queue); });
        break;
    }
    }
}

uint32 FSFGrpcGameplayTransport::Run()
{
    void* Tag = nullptr; bool bOk = false;
    while (Channel->Queue.Next(&Tag, &bOk))
    {
        FCall* Call = static_cast<FCall*>(Tag);
        {
            FScopeLock Lock(&Channel->PendingLock);
            Channel->Pending.Remove(Call);
        }
        FSFGameplayResponse Response = Convert(*Call);
        TFunction<void(const FSFGameplayResponse&)> Done = MoveTemp(Call->Done);
        delete Call;
        AsyncTask(ENamedThreads::GameThread, [Done = MoveTemp(Done), Response = MoveTemp(Response)]() { Done(Response); });
    }
    return 0;
}
