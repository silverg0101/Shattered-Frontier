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
    { Out.State = FromUtf8(R.state()); Out.OwnerId = FromUtf8(R.owner_id()); Out.Revision = R.revision(); Out.PermissionRevision = R.permission_revision(); }

    void CopyPlacement(const sf::housing::v1::PlacementReply& R, FSFGameplayResponse& Out)
    { Out.State = FromUtf8(R.state()); Out.StructureId = FromUtf8(R.structure_id()); Out.StructureRevision = R.structure_revision(); }

    void CopyExchange(const sf::exchange::v1::ExchangeReply& R, FSFGameplayResponse& Out)
    {
        Out.Exchange.SubjectId = FromUtf8(R.subject_id()); Out.Exchange.State = FromUtf8(R.state()); Out.Exchange.bCommitted = R.committed();
        Out.Exchange.Amount = R.amount(); Out.Exchange.Fee = R.fee();
    }
}

struct FSFGrpcGameplayChannel
{
    std::unique_ptr<sf::rewards::v1::Rewards::Stub> Rewards;
    std::unique_ptr<sf::progression::v1::Progression::Stub> Progression;
    std::unique_ptr<sf::housing::v1::Housing::Stub> Housing;
    std::unique_ptr<sf::crafting::v1::Crafting::Stub> Crafting;
    std::unique_ptr<sf::exchange::v1::Exchange::Stub> Exchange;
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
    Channel->Exchange = sf::exchange::v1::Exchange::NewStub(Wire);
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
            Out.bQuestCompleted = R.quest_completed(); Out.QuestRevision = R.quest_revision(); Out.QuestReason = FromUtf8(R.quest_reason());
            for (const auto& Bundle : R.reward_bundle_ids()) { Out.RewardBundleIds.Add(FromUtf8(Bundle)); }
            for (const auto& Item : R.granted_items()) { Out.GrantedItems.FindOrAdd(FromUtf8(Item.definition_id())) += Item.quantity(); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncApplyReceipt(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::CreateCharacter:
    {
        sf::progression::v1::CreateCharacterRequest M;
        M.set_character_id(Utf8(Q.CharacterId));
        M.set_appearance_packet(std::string(reinterpret_cast<const char*>(Q.AppearancePacket.GetData()), Q.AppearancePacket.Num()));
        auto* Call = new TCall<sf::progression::v1::CreateCharacterReply>();
        Call->Copy = [](const sf::progression::v1::CreateCharacterReply& R, FSFGameplayResponse& Out)
        { Out.AppearanceRevision = R.appearance_revision(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncCreateCharacter(C, M, &Channel->Queue); });
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
            for (const auto& Skill : R.gathering_skills()) { Out.Skills.Add(FromUtf8(Skill.first), Skill.second); }
            for (const auto& Known : R.known_recipes()) { Out.KnownRecipes.Add(FromUtf8(Known.recipe_id())); }
            for (const auto& Count : R.objective_counts()) { Out.ObjectiveCounts.Add(FromUtf8(Count.first), Count.second); }
            for (const auto& Quest : R.quests())
            {
                FSFGameplayResponse::FQuestState& Row = Out.Quests.AddDefaulted_GetRef();
                Row.QuestId = FromUtf8(Quest.quest_id()); Row.State = FromUtf8(Quest.state()); Row.ActiveStepId = FromUtf8(Quest.active_step_id()); Row.Revision = Quest.revision(); Row.bRewardPending = Quest.reward_pending();
                for (const auto& Count : Quest.counts()) { Row.Counts.Add(FromUtf8(Count.first), Count.second); }
            }
            if (R.has_face_id()) { Out.FaceId = FromUtf8(R.face_id()); }
            Out.PositionRevision = R.position_revision(); Out.bHasPosition = R.has_position();
            if (Out.bHasPosition)
            { Out.ZoneId = FromUtf8(R.position().zone_id()); Out.Position = FVector(R.position().x(), R.position().y(), R.position().z()); Out.Yaw = R.position().yaw(); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncGetCharacterState(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::LearnRecipe:
    {
        sf::progression::v1::LearnRecipeRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId)); M.set_recipe_id(Utf8(Q.RecipeId));
        M.set_source(Utf8(Q.LearnSource)); M.set_trainer_npc(Utf8(Q.TrainerNpc));
        M.set_scroll_container(Utf8(Q.ScrollContainer)); M.set_expected_container_revision(Q.ExpectedContainerRevision);
        auto* Call = new TCall<sf::progression::v1::LearnRecipeReply>();
        Call->Copy = [](const sf::progression::v1::LearnRecipeReply& R, FSFGameplayResponse& Out)
        { Out.ProgressionRevision = R.progression_revision(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncLearnRecipe(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::SavePosition:
    {
        sf::progression::v1::SavePositionRequest M;
        M.set_character_id(Utf8(Q.CharacterId)); M.set_expected_position_revision(Q.ExpectedPositionRevision);
        auto* P = M.mutable_position();
        P->set_zone_id(Utf8(Q.ZoneId)); P->set_x(Q.Position.X); P->set_y(Q.Position.Y); P->set_z(Q.Position.Z); P->set_yaw(Q.Yaw);
        auto* Call = new TCall<sf::progression::v1::SavePositionReply>();
        Call->Copy = [](const sf::progression::v1::SavePositionReply& R, FSFGameplayResponse& Out) { Out.PositionRevision = R.position_revision(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncSavePosition(C, M, &Channel->Queue); });
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
        if (!Q.ShapeKey.IsEmpty()) { M.set_shape_key(Utf8(Q.ShapeKey)); }
        M.set_orientation_index(Q.OrientationIndex);
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
    case ESFGameplayRpc::ListStructures:
    {
        sf::housing::v1::ListStructuresRequest M;
        M.set_claim_id(Utf8(Q.ClaimId));
        auto* Call = new TCall<sf::housing::v1::ListStructuresReply>();
        Call->Copy = [](const sf::housing::v1::ListStructuresReply& R, FSFGameplayResponse& Out)
        {
            Out.StructureRevision = R.structure_revision();
            Out.MaskOriginX=R.mask_origin_x(); Out.MaskOriginY=R.mask_origin_y();
            Out.MaskWidth=R.mask_width(); Out.MaskHeight=R.mask_height();
            Out.ClaimMask.Append(reinterpret_cast<const uint8*>(R.claim_mask().data()),R.claim_mask().size());
            for (const auto& S : R.structures())
            { Out.Structures.Add({FromUtf8(S.structure_id()), FromUtf8(S.content_id()), FIntVector(S.cell().x(), S.cell().y(), S.cell().z()), S.revision(), FromUtf8(S.shape_key()), S.orientation_index()}); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Housing->AsyncListStructures(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::Grant:
    {
        sf::housing::v1::GrantRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_claim_id(Utf8(Q.ClaimId)); M.set_actor_id(Utf8(Q.CharacterId));
        M.set_target_id(Utf8(Q.GranteeId)); M.set_rights(Q.Rights); M.set_expected_permission_revision(Q.ExpectedPermissionRevision);
        auto* Call = new TCall<sf::housing::v1::ClaimReply>(); Call->Copy = &CopyClaim;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Housing->AsyncGrant(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::Craft:
    {
        sf::crafting::v1::CraftRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId)); M.set_container_id(Utf8(Q.ContainerId));
        M.set_recipe_id(Utf8(Q.RecipeId)); M.set_expected_container_revision(Q.ExpectedContainerRevision); M.set_count(Q.Count);
        for (const auto& Grade : Q.InputGrades) { auto* I = M.add_input_grades(); I->set_definition_id(Utf8(Grade.Key)); I->set_selected_definition_id(Utf8(Grade.Value)); }
        auto* Call = new TCall<sf::crafting::v1::CraftReply>();
        Call->Copy = [](const sf::crafting::v1::CraftReply& R, FSFGameplayResponse& Out)
        {
            Out.ContainerRevision = R.container_revision();
            Out.QualityScore = R.quality_score(); Out.Grade = FromUtf8(R.grade()); Out.DiscoveredRecipeId = FromUtf8(R.discovered_recipe_id());
            for (const auto& Change : R.changes()) { Out.Changes.FindOrAdd(FromUtf8(Change.definition_id())) += Change.delta(); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Crafting->AsyncCraft(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ExchangeWallet:
    {
        sf::exchange::v1::WalletRequest M; M.set_owner_id(Utf8(Q.CharacterId));
        auto* Call = new TCall<sf::exchange::v1::WalletReply>();
        Call->Copy = [](const sf::exchange::v1::WalletReply& R, FSFGameplayResponse& Out)
        { Out.Exchange.Balance = R.balance(); Out.Exchange.Reserved = R.reserved(); Out.Exchange.Available = R.available(); Out.Exchange.WalletRevision = R.revision(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncWallet(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ListListings:
    {
        sf::exchange::v1::ListListingsRequest M;
        M.set_market_id(Utf8(Q.Exchange.MarketId)); M.set_page_size(Q.Count);
        auto* Call = new TCall<sf::exchange::v1::ListListingsReply>();
        Call->Copy = [](const sf::exchange::v1::ListListingsReply& R, FSFGameplayResponse& Out)
        {
            for (const auto& Listing : R.listings())
            {
                FSFMarketListingResponse& Row = Out.MarketListings.AddDefaulted_GetRef();
                Row.Id = FromUtf8(Listing.id()); Row.SellerId = FromUtf8(Listing.seller_id()); Row.ItemId = FromUtf8(Listing.item_id());
                Row.Quantity = Listing.quantity(); Row.UnitPrice = Listing.unit_price();
            }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncListListings(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::OfferTrade:
    {
        const FSFExchangeRequest& E = Q.Exchange;
        sf::exchange::v1::TradeOfferRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_trade_id(Utf8(E.TradeId)); M.set_party_a(Utf8(E.PartyA)); M.set_party_b(Utf8(E.PartyB));
        M.set_gold_a(E.GoldA); M.set_gold_b(E.GoldB);
        for (const FSFExchangeDelivery& D : E.Deliveries)
        {
            auto* Line = M.add_deliveries();
            Line->set_escrow_id(Utf8(D.EscrowId)); Line->set_destination_id(Utf8(D.DestinationId)); Line->set_item_id(Utf8(D.ItemId)); Line->set_quantity(D.Quantity);
        }
        auto* Call = new TCall<sf::exchange::v1::ExchangeReply>(); Call->Copy = &CopyExchange;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncOfferTrade(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::AcceptTrade:
    case ESFGameplayRpc::CancelTrade:
    {
        sf::exchange::v1::TradeActionRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_trade_id(Utf8(Q.Exchange.TradeId)); M.set_actor_id(Utf8(Q.CharacterId));
        M.set_offer_operation_id(Utf8(Q.Exchange.OfferOperationId));
        auto* Call = new TCall<sf::exchange::v1::ExchangeReply>(); Call->Copy = &CopyExchange;
        if (Q.Rpc == ESFGameplayRpc::AcceptTrade) { Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncAcceptTrade(C, M, &Channel->Queue); }); }
        else { Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncCancelTrade(C, M, &Channel->Queue); }); }
        break;
    }
    case ESFGameplayRpc::ListItem:
    {
        const FSFExchangeRequest& E = Q.Exchange;
        sf::exchange::v1::ListItemRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_listing_id(Utf8(E.ListingId)); M.set_seller_id(Utf8(E.SellerId));
        M.set_source_container_id(Utf8(E.SourceContainerId)); M.set_escrow_container_id(Utf8(E.EscrowContainerId));
        M.set_item_id(Utf8(E.ItemId)); M.set_quantity(E.Quantity); M.set_unit_price(E.UnitPrice); M.set_listing_fee(E.Fee); M.set_created_at_unix_ms(E.CreatedAtUnixMs); M.set_market_id(Utf8(E.MarketId));
        auto* Call = new TCall<sf::exchange::v1::ExchangeReply>(); Call->Copy = &CopyExchange;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncListItem(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::BuyListing:
    {
        const FSFExchangeRequest& E = Q.Exchange;
        sf::exchange::v1::BuyListingRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_listing_id(Utf8(E.ListingId)); M.set_buyer_id(Utf8(Q.CharacterId));
        M.set_destination_container_id(Utf8(E.DestinationContainerId)); M.set_expected_revision(E.ExpectedRevision);
        M.set_market_id(Utf8(E.MarketId));
        M.set_expected_unit_price(E.ExpectedUnitPrice); M.set_transaction_fee(E.Fee);
        auto* Call = new TCall<sf::exchange::v1::ExchangeReply>(); Call->Copy = &CopyExchange;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncBuyListing(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::CancelListing:
    {
        sf::exchange::v1::ListingActionRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_listing_id(Utf8(Q.Exchange.ListingId)); M.set_seller_id(Utf8(Q.Exchange.SellerId));
        M.set_market_id(Utf8(Q.Exchange.MarketId));
        auto* Call = new TCall<sf::exchange::v1::ExchangeReply>(); Call->Copy = &CopyExchange;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncCancelListing(C, M, &Channel->Queue); });
        break;
    }
    default:
    {
        // An RPC this transport does not carry yet answers Unimplemented at once instead of never.
        FSFGameplayResponse Missing; Missing.Status = ESFGameplayStatus::Rejected; Missing.GrpcCode = 12; Missing.Message = TEXT("RPC not carried by this transport.");
        AsyncTask(ENamedThreads::GameThread, [Done = MoveTemp(Done), Missing]() { Done(Missing); });
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
