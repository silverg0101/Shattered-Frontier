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

    void CopyCombat(const sf::progression::v1::CombatAttributes& R, FSFGameplayResponse& Out)
    {
        auto& A = Out.CombatAttributes;
        A.Archetype = FromUtf8(R.archetype()); A.Level = R.level();
        A.Strength = R.strength(); A.Dexterity = R.dexterity(); A.Constitution = R.constitution();
        A.Intelligence = R.intelligence(); A.Wisdom = R.wisdom(); A.Willpower = R.willpower();
    }

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

    FSFLootSourceSnapshot CopyLoot(const sf::rewards::v1::LootSourceView& R)
    {
        FSFLootSourceSnapshot Out;
        const auto& S = R.source();
        Out.Id = FromUtf8(S.id()); Out.ZoneId = FromUtf8(S.zone_id()); Out.CreatureId = FromUtf8(S.creature_id());
        Out.MeshPath = FromUtf8(S.mesh_path()); Out.Position = FVector(S.x_cm(), S.y_cm(), S.z_cm());
        Out.BodyId = FromUtf8(S.body_id());
        Out.bGround = S.ground(); Out.bShared = S.shared(); Out.Revision = R.revision();
        Out.MeshScale = FVector(S.scale_x(), S.scale_y(), S.scale_z()); Out.MeshRotation = FRotator(S.pitch(), S.yaw(), S.roll());
        Out.bHasMeshBounds = S.has_mesh_bounds();
        if (Out.bHasMeshBounds)
        {
            Out.MeshBoundsOriginCm = FVector(S.mesh_bounds().origin_x_cm(), S.mesh_bounds().origin_y_cm(), S.mesh_bounds().origin_z_cm());
            Out.MeshBoundsExtentCm = FVector(S.mesh_bounds().extent_x_cm(), S.mesh_bounds().extent_y_cm(), S.mesh_bounds().extent_z_cm());
        }
        for (const auto& Id : S.eligible_character_ids()) { Out.EligibleCharacters.Add(FromUtf8(Id)); }
        TMap<FString, int32> Copies;
        for (const auto& I : R.instances())
        {
            FSFLootEntry E; E.DefinitionId = FromUtf8(I.definition_id()); E.InstanceId = FromUtf8(I.instance_id());
            E.Quantity = 1; E.DurabilityPercent = I.durability_percent(); Out.Items.Add(E); ++Copies.FindOrAdd(E.DefinitionId);
        }
        for (const auto& I : R.items())
        {
            FSFLootEntry E; E.DefinitionId = FromUtf8(I.definition_id());
            E.Quantity = static_cast<int32>(FMath::Clamp<int64>(I.quantity() - Copies.FindRef(E.DefinitionId), 0, 10000));
            if (E.Quantity > 0) { Out.Items.Add(E); }
        }
        return Out;
    }

    void CopyClaim(const sf::housing::v1::ClaimReply& R, FSFGameplayResponse& Out)
    { Out.State = FromUtf8(R.state()); Out.OwnerId = FromUtf8(R.owner_id()); Out.Revision = R.revision(); Out.PermissionRevision = R.permission_revision(); Out.bActorPermitted = R.actor_permitted(); }

    void CopyPlacement(const sf::housing::v1::PlacementReply& R, FSFGameplayResponse& Out)
    { Out.State = FromUtf8(R.state()); Out.HousingOperationId = FromUtf8(R.operation_id()); Out.StructureId = FromUtf8(R.structure_id()); Out.StructureRevision = R.structure_revision(); }

    void CopyDeath(const sf::progression::v1::DeathState& R, FSFGameplayResponse& Out)
    {
        Out.bHasDeath = true; Out.bIsDead = R.is_dead(); Out.DeathEventId = FromUtf8(R.death_event_id());
        Out.DeathAtUnixMs = R.occurred_at_unix_ms(); Out.RespawnAtUnixMs = R.respawn_at_unix_ms();
        if (R.has_death_position()) { Out.DeathZoneId = FromUtf8(R.death_position().zone_id());
            Out.DeathPosition = FVector(R.death_position().x(), R.death_position().y(), R.death_position().z()); }
        if (R.has_respawn_position()) { Out.RespawnPosition = FVector(R.respawn_position().x(), R.respawn_position().y(), R.respawn_position().z()); }
    }

    void CopyExchange(const sf::exchange::v1::ExchangeReply& R, FSFGameplayResponse& Out)
    {
        Out.Exchange.SubjectId = FromUtf8(R.subject_id()); Out.Exchange.State = FromUtf8(R.state()); Out.Exchange.bCommitted = R.committed();
        Out.Exchange.Amount = R.amount(); Out.Exchange.Fee = R.fee();
    }
    void CopyGroup(const sf::group::v1::Party& R, FSFGameplayResponse& Out)
    {
        Out.GroupPartyId = FromUtf8(R.party_id()); Out.GroupLeaderId = FromUtf8(R.leader_character_id());
        Out.GroupAssistantId = FromUtf8(R.assistant_character_id()); Out.GroupState = FromUtf8(R.state());
        Out.GroupVersion = R.version();
        for (const auto& Id : R.member_character_ids()) { Out.GroupMembers.Add(FromUtf8(Id)); }
        Out.RaidGroupId = FromUtf8(R.raid_group_id());
        for (const auto& Id : R.raid_member_character_ids()) { Out.RaidMembers.Add(FromUtf8(Id)); }
    }
    void CopyNode(const sf::gathering::v1::NodeReply& R, FSFGameplayResponse& Out)
    {
        Out.bHasNode = true; Out.NodeId = FromUtf8(R.node_id()); Out.NodeDefinitionId = FromUtf8(R.definition_id());
        Out.NodeGeneration = R.generation(); Out.NodeCharges = R.charges(); Out.NodeCapacity = R.max_charges();
        Out.NodeRegrowAtUnixMs = R.regrow_at_unix_ms(); Out.GrowthStage = FromUtf8(R.growth_stage());
        Out.NodeNextTransitionUnixMs = R.next_transition_unix_ms();
        Out.NodePoolId = FromUtf8(R.pool_id()); Out.NodePointId = FromUtf8(R.point_id());
        Out.NodePositionMetres = FVector(R.position_x_m(), R.position_y_m(), R.position_z_m());
        Out.NodeFacingDegrees = R.facing_degrees(); Out.bNodeActive = R.active();
    }
}

struct FSFGrpcGameplayChannel
{
    std::unique_ptr<sf::rewards::v1::Rewards::Stub> Rewards;
    std::unique_ptr<sf::progression::v1::Progression::Stub> Progression;
    std::unique_ptr<sf::housing::v1::Housing::Stub> Housing;
    std::unique_ptr<sf::crafting::v1::Crafting::Stub> Crafting;
    std::unique_ptr<sf::exchange::v1::Exchange::Stub> Exchange;
    std::unique_ptr<sf::gathering::v1::Gathering::Stub> Gathering;
    std::unique_ptr<sf::world::v1::WorldSlots::Stub> WorldSlots;
    std::unique_ptr<sf::group::v1::GroupService::Stub> Group;
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
    Channel->Gathering = sf::gathering::v1::Gathering::NewStub(Wire);
    Channel->WorldSlots = sf::world::v1::WorldSlots::NewStub(Wire);
    Channel->Group = sf::group::v1::GroupService::NewStub(Wire);
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
    case ESFGameplayRpc::GroupCreate:
    {
        sf::group::v1::CreatePartyRequest M;
        M.set_idempotency_key(Utf8(Q.OperationId)); M.set_leader_character_id(Utf8(Q.GroupActorId));
        M.set_account_id(Utf8(Q.GroupAccountId)); M.set_realm_id(Utf8(Q.GroupRealmId)); M.set_dc_region(Utf8(Q.GroupRegionId));
        auto* Call = new TCall<sf::group::v1::Party>(); Call->Copy = &CopyGroup;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Group->AsyncCreateParty(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::GroupJoin:
    {
        sf::group::v1::JoinPartyRequest M;
        M.set_idempotency_key(Utf8(Q.OperationId)); M.set_party_id(Utf8(Q.GroupPartyId)); M.set_actor_character_id(Utf8(Q.GroupActorId));
        M.set_character_id(Utf8(Q.GroupMemberId)); M.set_account_id(Utf8(Q.GroupAccountId)); M.set_realm_id(Utf8(Q.GroupRealmId));
        auto* Call = new TCall<sf::group::v1::Party>(); Call->Copy = &CopyGroup;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Group->AsyncJoinParty(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::GroupGet:
    {
        sf::group::v1::CharacterRef M; M.set_character_id(Utf8(Q.GroupActorId));
        auto* Call = new TCall<sf::group::v1::Party>(); Call->Copy = &CopyGroup;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Group->AsyncGetPartyForCharacter(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::GroupLeave:
    case ESFGameplayRpc::GroupKick:
    case ESFGameplayRpc::GroupPromote:
    case ESFGameplayRpc::GroupAssistant:
    {
        sf::group::v1::MemberActionRequest M;
        M.set_idempotency_key(Utf8(Q.OperationId)); M.set_party_id(Utf8(Q.GroupPartyId));
        M.set_actor_character_id(Utf8(Q.GroupActorId)); M.set_character_id(Utf8(Q.GroupMemberId));
        auto* Call = new TCall<sf::group::v1::Party>(); Call->Copy = &CopyGroup;
        switch (Q.Rpc)
        {
        case ESFGameplayRpc::GroupLeave: Start(Call, [&](grpc::ClientContext* C) { return Channel->Group->AsyncLeaveParty(C, M, &Channel->Queue); }); break;
        case ESFGameplayRpc::GroupKick: Start(Call, [&](grpc::ClientContext* C) { return Channel->Group->AsyncKickMember(C, M, &Channel->Queue); }); break;
        case ESFGameplayRpc::GroupPromote: Start(Call, [&](grpc::ClientContext* C) { return Channel->Group->AsyncPromoteLeader(C, M, &Channel->Queue); }); break;
        default: Start(Call, [&](grpc::ClientContext* C) { return Channel->Group->AsyncAssignAssistant(C, M, &Channel->Queue); }); break;
        }
        break;
    }
    case ESFGameplayRpc::AdvanceWorldSlots:
    {
        sf::world::v1::AdvanceSlotsRequest M;
        M.set_command(Utf8(Q.SlotCommand)); M.set_group_id(Utf8(Q.SlotGroupId)); M.set_slot_index(Q.SlotIndex);
        M.set_players_in_area(Q.PlayersInArea); M.set_hostile_spawning(Q.bHostileSpawning);
        auto* Call = new TCall<sf::world::v1::AdvanceSlotsReply>();
        Call->Copy = [](const sf::world::v1::AdvanceSlotsReply& R, FSFGameplayResponse& Out)
        {
            for (const auto& Slot : R.slots())
            {
                auto& Row = Out.BoarSlots.AddDefaulted_GetRef();
                Row.GroupId = FromUtf8(Slot.group_id()); Row.PointId = FromUtf8(Slot.point_id());
                Row.DefinitionId = FromUtf8(Slot.definition_id());
                Row.SlotIndex = Slot.slot_index(); Row.Cycle = Slot.cycle(); Row.bActive = Slot.active();
                Row.XMetres = Slot.x_metres(); Row.YMetres = Slot.y_metres(); Row.FacingDegrees = Slot.facing_degrees();
            }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->WorldSlots->AsyncAdvance(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::GetGatheringNode:
    {
        sf::gathering::v1::GetNodeRequest M; M.set_node_id(Utf8(Q.NodeId));
        auto* Call = new TCall<sf::gathering::v1::NodeReply>();
        Call->Copy = [](const sf::gathering::v1::NodeReply& R, FSFGameplayResponse& Out) { CopyNode(R, Out); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Gathering->AsyncGetNode(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ListGatheringZoneNodes:
    {
        sf::gathering::v1::ListZoneNodesRequest M; M.set_zone_id(Utf8(Q.ZoneId));
        auto* Call = new TCall<sf::gathering::v1::ListZoneNodesReply>();
        Call->Copy = [](const sf::gathering::v1::ListZoneNodesReply& R, FSFGameplayResponse& Out)
        {
            for (const auto& Node : R.nodes())
            {
                FSFGameplayResponse Single; CopyNode(Node, Single);
                auto& Row = Out.ZoneNodes.AddDefaulted_GetRef();
                Row.Id = Single.NodeId; Row.DefinitionId = Single.NodeDefinitionId; Row.GrowthStage = Single.GrowthStage;
                Row.PoolId = Single.NodePoolId; Row.PointId = Single.NodePointId;
                Row.Generation = Single.NodeGeneration; Row.Charges = Single.NodeCharges; Row.Capacity = Single.NodeCapacity;
                Row.RegrowAtUnixMs = Single.NodeRegrowAtUnixMs; Row.NextTransitionUnixMs = Single.NodeNextTransitionUnixMs;
                Row.PositionMetres = Single.NodePositionMetres; Row.FacingDegrees = Single.NodeFacingDegrees;
                Row.bActive = Single.bNodeActive;
            }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Gathering->AsyncListZoneNodes(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::HarvestGatheringNode:
    {
        sf::gathering::v1::HarvestRequest M; M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId));
        M.set_node_id(Utf8(Q.NodeId)); M.set_generation(Q.NodeGeneration);
        M.set_container_id(Utf8(Q.ContainerId)); M.set_follow_up_payload(Q.FollowUpPayload.GetData(), Q.FollowUpPayload.Num());
        M.set_follow_up_digest(Utf8(Q.FollowUpDigest)); M.set_follow_up_version(Q.FollowUpVersion);
        auto* Call = new TCall<sf::gathering::v1::HarvestReply>();
        Call->Copy = [](const sf::gathering::v1::HarvestReply& R, FSFGameplayResponse& Out)
        {
            Out.HarvestStatus = FromUtf8(R.status()); Out.ResourceId = FromUtf8(R.resource_id()); Out.HarvestQuantity = R.quantity();
            if (R.has_node()) { CopyNode(R.node(), Out); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Gathering->AsyncHarvestWithIntent(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::CreateCorpseLoot:
    {
        sf::rewards::v1::CreateCorpseLootRequest M;
        M.set_defeat_event_id(Utf8(Q.OperationId)); M.set_creature_id(Utf8(Q.CreatureId)); M.set_table_id(Utf8(Q.TableId));
        M.set_killer_character_id(Utf8(Q.KillerCharacterId)); M.set_protected_(Q.bKillerProtected || Q.bBoarProtected);
        M.set_zone_id(Utf8(Q.ZoneId)); M.set_x_cm(Q.Position.X); M.set_y_cm(Q.Position.Y); M.set_z_cm(Q.Position.Z);
        M.set_mesh_path(Utf8(Q.LootMeshPath)); M.set_shared(Q.bSharedLoot); M.set_raid_group_id(Utf8(Q.RaidGroupId));
        const FVector Scale = Q.LootMeshTransform.GetScale3D(); const FRotator Rotation = Q.LootMeshTransform.Rotator();
        if (Q.bHasLootMeshBounds)
        {
            auto* Bounds = M.mutable_mesh_bounds();
            Bounds->set_origin_x_cm(Q.LootMeshBoundsOriginCm.X); Bounds->set_origin_y_cm(Q.LootMeshBoundsOriginCm.Y); Bounds->set_origin_z_cm(Q.LootMeshBoundsOriginCm.Z);
            Bounds->set_extent_x_cm(Q.LootMeshBoundsExtentCm.X); Bounds->set_extent_y_cm(Q.LootMeshBoundsExtentCm.Y); Bounds->set_extent_z_cm(Q.LootMeshBoundsExtentCm.Z);
        }
        M.set_scale_x(Scale.X); M.set_scale_y(Scale.Y); M.set_scale_z(Scale.Z);
        M.set_pitch(Rotation.Pitch); M.set_yaw(Rotation.Yaw); M.set_roll(Rotation.Roll);
        for (const FString& Id : Q.EligibleCharacterIds) { M.add_eligible_character_ids(Utf8(Id)); }
        auto* Call = new TCall<sf::rewards::v1::LootSourcesReply>();
        Call->Copy = [](const sf::rewards::v1::LootSourcesReply& R, FSFGameplayResponse& Out)
        { for (const auto& S : R.sources()) { Out.LootSources.Add(CopyLoot(S)); } };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncCreateCorpseLoot(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::ListLootSources:
    {
        sf::rewards::v1::ListLootSourcesRequest M; M.set_zone_id(Utf8(Q.ZoneId));
        auto* Call = new TCall<sf::rewards::v1::LootSourcesReply>();
        Call->Copy = [](const sf::rewards::v1::LootSourcesReply& R, FSFGameplayResponse& Out)
        { for (const auto& S : R.sources()) { Out.LootSources.Add(CopyLoot(S)); } };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncListLootSources(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::OpenLootSource:
    {
        sf::rewards::v1::OpenLootSourceRequest M; M.set_source_id(Utf8(Q.LootSourceId)); M.set_character_id(Utf8(Q.CharacterId));
        auto* Call = new TCall<sf::rewards::v1::LootSourceView>();
        Call->Copy = [](const sf::rewards::v1::LootSourceView& R, FSFGameplayResponse& Out) { Out.LootSources.Add(CopyLoot(R)); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncOpenLootSource(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::TakeLoot:
    {
        sf::rewards::v1::TakeLootRequest M; M.set_operation_id(Utf8(Q.OperationId)); M.set_source_id(Utf8(Q.LootSourceId));
        M.set_character_id(Utf8(Q.CharacterId)); M.set_backpack_id(Utf8(Q.ContainerId));
        M.set_source_revision(Q.LootSourceRevision); M.set_backpack_revision(Q.ExpectedContainerRevision);
        M.set_definition_id(Utf8(Q.LootDefinitionId)); M.set_quantity(Q.LootQuantity); M.set_instance_id(Utf8(Q.LootInstanceId));
        auto* Call = new TCall<sf::rewards::v1::LootSourceView>();
        Call->Copy = [](const sf::rewards::v1::LootSourceView& R, FSFGameplayResponse& Out)
        { Out.LootSources.Add(CopyLoot(R)); Out.ContainerRevision = R.backpack_revision(); Out.LedgerOperationId = FromUtf8(R.ledger_operation_id()); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncTakeLoot(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::DropLoot:
    {
        sf::rewards::v1::DropLootRequest M; M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId));
        M.set_backpack_id(Utf8(Q.ContainerId)); M.set_backpack_revision(Q.ExpectedContainerRevision);
        M.set_definition_id(Utf8(Q.LootDefinitionId)); M.set_quantity(Q.LootQuantity); M.set_instance_id(Utf8(Q.LootInstanceId));
        M.set_zone_id(Utf8(Q.ZoneId)); M.set_x_cm(Q.Position.X); M.set_y_cm(Q.Position.Y); M.set_z_cm(Q.Position.Z); M.set_mesh_path(Utf8(Q.LootMeshPath));
        if (Q.bHasLootMeshBounds)
        {
            auto* Bounds = M.mutable_mesh_bounds();
            Bounds->set_origin_x_cm(Q.LootMeshBoundsOriginCm.X); Bounds->set_origin_y_cm(Q.LootMeshBoundsOriginCm.Y); Bounds->set_origin_z_cm(Q.LootMeshBoundsOriginCm.Z);
            Bounds->set_extent_x_cm(Q.LootMeshBoundsExtentCm.X); Bounds->set_extent_y_cm(Q.LootMeshBoundsExtentCm.Y); Bounds->set_extent_z_cm(Q.LootMeshBoundsExtentCm.Z);
            const FVector Scale = Q.LootMeshTransform.GetScale3D(); const FRotator Rotation = Q.LootMeshTransform.Rotator();
            M.set_scale_x(Scale.X); M.set_scale_y(Scale.Y); M.set_scale_z(Scale.Z);
            M.set_pitch(Rotation.Pitch); M.set_yaw(Rotation.Yaw); M.set_roll(Rotation.Roll);
        }
        auto* Call = new TCall<sf::rewards::v1::LootSourceView>();
        Call->Copy = [](const sf::rewards::v1::LootSourceView& R, FSFGameplayResponse& Out)
        { Out.LootSources.Add(CopyLoot(R)); Out.ContainerRevision = R.backpack_revision(); Out.LedgerOperationId = FromUtf8(R.ledger_operation_id()); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncDropLoot(C, M, &Channel->Queue); }); break;
    }
    case ESFGameplayRpc::GrantDefeatReward:
    {
        sf::rewards::v1::GrantDefeatRewardRequest M;
        M.set_defeat_event_id(Utf8(Q.OperationId)); M.set_killer_character_id(Utf8(Q.KillerCharacterId)); M.set_recipient_character_id(Utf8(Q.CharacterId));
        M.set_container_id(Utf8(Q.ContainerId)); M.set_expected_container_revision(Q.ExpectedContainerRevision);
        M.set_killer_in_protected_town(Q.bKillerProtected); M.set_boar_in_protected_town(Q.bBoarProtected); M.set_table_id(Utf8(Q.TableId));
        M.set_creature_id(Utf8(Q.CreatureId));
        auto* Call = new TCall<sf::rewards::v1::RewardReceipt>();
        Call->Copy = [](const sf::rewards::v1::RewardReceipt& R, FSFGameplayResponse& Out)
        {
            Out.bGranted = R.granted(); Out.SourceCode = FromUtf8(R.source_code()); Out.LedgerOperationId = FromUtf8(R.ledger_operation_id());
            Out.DefinitionId = FromUtf8(R.definition_id()); Out.Quantity = R.quantity(); Out.ContainerRevision = R.container_revision();
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncGrantDefeatReward(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::CreateSharedDefeatDrop:
    {
        sf::rewards::v1::CreateSharedDefeatDropRequest M;
        M.set_defeat_event_id(Utf8(Q.OperationId)); M.set_creature_id(Utf8(Q.CreatureId));
        M.set_table_id(Utf8(Q.TableId)); M.set_killer_character_id(Utf8(Q.KillerCharacterId));
        for (const FString& Id : Q.EligibleCharacterIds) { M.add_eligible_character_ids(Utf8(Id)); }
        M.set_raid_group_id(Utf8(Q.RaidGroupId));
        M.set_killer_in_protected_town(Q.bKillerProtected); M.set_creature_in_protected_town(Q.bBoarProtected);
        auto* Call = new TCall<sf::rewards::v1::SharedDefeatDropReceipt>();
        Call->Copy = [](const sf::rewards::v1::SharedDefeatDropReceipt& R, FSFGameplayResponse& Out)
        {
            Out.DeathEventId = FromUtf8(R.defeat_event_id()); Out.ClaimantCharacterId = FromUtf8(R.claimant_character_id());
            Out.LedgerOperationId = FromUtf8(R.ledger_operation_id()); Out.ContainerRevision = R.container_revision();
            Out.Quantity = R.items_size();
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncCreateSharedDefeatDrop(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ClaimSharedDefeatDrop:
    {
        sf::rewards::v1::ClaimSharedDefeatDropRequest M;
        M.set_defeat_event_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId)); M.set_container_id(Utf8(Q.ContainerId));
        auto* Call = new TCall<sf::rewards::v1::SharedDefeatDropReceipt>();
        Call->Copy = [](const sf::rewards::v1::SharedDefeatDropReceipt& R, FSFGameplayResponse& Out)
        {
            Out.DeathEventId = FromUtf8(R.defeat_event_id()); Out.ClaimantCharacterId = FromUtf8(R.claimant_character_id());
            Out.LedgerOperationId = FromUtf8(R.ledger_operation_id()); Out.ContainerRevision = R.container_revision();
            Out.Quantity = R.items_size();
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Rewards->AsyncClaimSharedDefeatDrop(C, M, &Channel->Queue); });
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
            P->set_reference_party_level(Q.ReferencePartyLevel);
            P->set_xp_eligible(Q.bXpEligible);
            P->set_session_end(Q.bSessionEnd); P->set_in_rest_area(Q.bInRestArea);
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
            if (R.has_combat_attributes()) { CopyCombat(R.combat_attributes(), Out); }
            Out.bProgressionApplied = R.progression_applied(); Out.SkillGain = R.skill_gain(); Out.XpGain = R.xp_gain(); Out.Level = R.level();
            Out.ProgressionRevision = R.progression_revision(); Out.bQuestAccepted = R.quest_accepted(); Out.ActiveStepId = FromUtf8(R.active_step_id());
            Out.RestedMinutes = R.rested_minutes();
            for (const auto& Skill : R.skills()) { Out.Skills.Add(FromUtf8(Skill.first), Skill.second); }
            Out.bGatheringSkillLocksAuthoritative = R.gathering_skill_locks_authoritative();
            for (const auto& Lock : R.gathering_skill_locks()) { Out.GatheringSkillLocks.Add(FromUtf8(Lock.first), FromUtf8(Lock.second)); }
            for (const auto& Ready : R.gathering_skill_lock_ready_at_unix_ms()) { Out.GatheringSkillLockReadyAtUnixMs.Add(FromUtf8(Ready.first), Ready.second); }
            Out.bQuestCompleted = R.quest_completed(); Out.QuestRevision = R.quest_revision(); Out.QuestReason = FromUtf8(R.quest_reason());
            for (const auto& Bundle : R.reward_bundle_ids()) { Out.RewardBundleIds.Add(FromUtf8(Bundle)); }
            for (const auto& Item : R.granted_items()) { Out.GrantedItems.FindOrAdd(FromUtf8(Item.definition_id())) += Item.quantity(); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncApplyReceipt(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::EndSession:
    {
        sf::progression::v1::EndSessionRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId));
        M.set_expected_progression_revision(Q.ExpectedProgressionRevision); M.set_in_rest_area(Q.bInRestArea);
        auto* Call = new TCall<sf::progression::v1::EndSessionReply>();
        Call->Copy = [](const sf::progression::v1::EndSessionReply& R, FSFGameplayResponse& Out)
        { Out.ProgressionRevision = R.progression_revision(); Out.RestedMinutes = R.rested_minutes(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncEndSession(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::CreateCharacter:
    {
        sf::progression::v1::CreateCharacterRequest M;
        M.set_character_id(Utf8(Q.CharacterId));
        M.set_appearance_packet(std::string(reinterpret_cast<const char*>(Q.AppearancePacket.GetData()), Q.AppearancePacket.Num()));
        M.set_archetype_id(Utf8(Q.ArchetypeId));
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
            if (R.has_combat_attributes()) { CopyCombat(R.combat_attributes(), Out); }
            Out.ProgressionRevision = R.progression_revision(); Out.Level = R.level(); Out.QuestRevision = R.quest_revision();
            Out.RestedMinutes = R.rested_minutes();
            Out.ActiveStepId = FromUtf8(R.active_step_id()); Out.bQuestCompleted = R.quest_completed();
            for (const auto& Skill : R.skills()) { Out.Skills.Add(FromUtf8(Skill.first), Skill.second); }
            for (const auto& Skill : R.gathering_skills()) { Out.Skills.Add(FromUtf8(Skill.first), Skill.second); }
            Out.bGatheringSkillLocksAuthoritative = R.gathering_skill_locks_authoritative();
            for (const auto& Lock : R.gathering_skill_locks()) { Out.GatheringSkillLocks.Add(FromUtf8(Lock.first), FromUtf8(Lock.second)); }
            for (const auto& Ready : R.gathering_skill_lock_ready_at_unix_ms()) { Out.GatheringSkillLockReadyAtUnixMs.Add(FromUtf8(Ready.first), Ready.second); }
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
            if (R.has_death()) { CopyDeath(R.death(), Out); }
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncGetCharacterState(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ChangeSkillLock:
    {
        sf::progression::v1::ChangeSkillLockRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId));
        M.set_expected_progression_revision(Q.ExpectedProgressionRevision);
        M.set_skill_id(Utf8(Q.SkillId)); M.set_lock(Utf8(Q.SkillLock));
        auto* Call = new TCall<sf::progression::v1::ChangeSkillLockReply>();
        Call->Copy = [](const sf::progression::v1::ChangeSkillLockReply& R, FSFGameplayResponse& Out)
        {
            Out.ProgressionRevision = R.progression_revision();
            Out.SkillId = FromUtf8(R.skill_id()); Out.SkillLock = FromUtf8(R.lock());
            Out.SkillLockReadyAtUnixMs = R.ready_at_unix_ms();
            Out.bGatheringSkillLocksAuthoritative = true;
        };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncChangeSkillLock(C, M, &Channel->Queue); });
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
    case ESFGameplayRpc::CommitUnstuck:
    {
        sf::progression::v1::CommitUnstuckRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_character_id(Utf8(Q.CharacterId));
        M.set_cooldown_seconds(Q.UnstuckCooldownSeconds);
        auto* P = M.mutable_position();
        P->set_zone_id(Utf8(Q.ZoneId)); P->set_x(Q.Position.X); P->set_y(Q.Position.Y); P->set_z(Q.Position.Z); P->set_yaw(Q.Yaw);
        auto* Call = new TCall<sf::progression::v1::SavePositionReply>();
        Call->Copy = [](const sf::progression::v1::SavePositionReply& R, FSFGameplayResponse& Out) { Out.PositionRevision = R.position_revision(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncCommitUnstuck(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ReportDeath:
    {
        sf::progression::v1::ReportDeathRequest M;
        M.set_death_event_id(Utf8(Q.DeathEventId)); M.set_character_id(Utf8(Q.CharacterId));
        M.set_occurred_at_unix_ms(Q.DeathAtUnixMs); M.set_respawn_delay_ms(Q.RespawnDelayMs);
        auto* D = M.mutable_death_position(); D->set_zone_id(Utf8(Q.ZoneId));
        D->set_x(Q.Position.X); D->set_y(Q.Position.Y); D->set_z(Q.Position.Z);
        auto* P = M.mutable_respawn_position(); P->set_zone_id(Utf8(Q.ZoneId));
        P->set_x(Q.RespawnPosition.X); P->set_y(Q.RespawnPosition.Y); P->set_z(Q.RespawnPosition.Z);
        auto* Call = new TCall<sf::progression::v1::DeathState>(); Call->Copy = CopyDeath;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncReportDeath(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::ClearDeath:
    {
        sf::progression::v1::ClearDeathRequest M;
        M.set_character_id(Utf8(Q.CharacterId)); M.set_death_event_id(Utf8(Q.DeathEventId));
        M.set_resurrect_at_death_position(Q.bResurrectAtDeathPosition);
        auto* Call = new TCall<sf::progression::v1::DeathState>(); Call->Copy = CopyDeath;
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Progression->AsyncClearDeath(C, M, &Channel->Queue); });
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
        M.set_claim_id(Utf8(Q.ClaimId)); M.set_actor_id(Utf8(Q.CharacterId));
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
    case ESFGameplayRpc::QuoteRepair:
    {
        sf::exchange::v1::RepairQuoteRequest M;
        M.set_owner_id(Utf8(Q.CharacterId)); M.set_instance_id(Utf8(Q.Exchange.RepairInstanceId));
        M.set_all_equipped(Q.Exchange.bRepairAllEquipped);
        auto* Call = new TCall<sf::exchange::v1::RepairQuoteReply>();
        Call->Copy = [](const sf::exchange::v1::RepairQuoteReply& R, FSFGameplayResponse& Out)
        { Out.Exchange.RepairQuoteId = FromUtf8(R.quote_id()); Out.Exchange.Amount = R.cost_copper();
          Out.Exchange.RepairItemCount = R.item_count(); Out.Exchange.RepairQuoteExpiresAtUnixMs = R.expires_at_unix_ms(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncQuoteRepair(C, M, &Channel->Queue); });
        break;
    }
    case ESFGameplayRpc::RepairGear:
    {
        sf::exchange::v1::RepairGearRequest M;
        M.set_operation_id(Utf8(Q.OperationId)); M.set_quote_id(Utf8(Q.Exchange.RepairQuoteId)); M.set_owner_id(Utf8(Q.CharacterId));
        auto* Call = new TCall<sf::exchange::v1::RepairGearReply>();
        Call->Copy = [](const sf::exchange::v1::RepairGearReply& R, FSFGameplayResponse& Out)
        { Out.Exchange.Amount = R.cost_copper(); Out.Exchange.RepairItemCount = R.item_count(); Out.Exchange.bCommitted = R.committed(); };
        Start(Call, [&](grpc::ClientContext* C) { return Channel->Exchange->AsyncRepairGear(C, M, &Channel->Queue); });
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
