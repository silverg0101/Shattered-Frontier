#include "SFGrantFollowUp.h"
#include "SFGrpcGameplayIncludes.h"
#include "sf/progression/v1/progression.pb.h"
#include <openssl/sha.h>

namespace
{
    std::string Utf8(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }
}

bool SFSerializeGatherFollowUp(const FSFGameplayRequest& Q, TArray<uint8>& Payload, FString& Digest)
{
    Payload.Reset(); Digest.Reset();
    if (Q.Rpc != ESFGameplayRpc::ApplyReceipt || Q.Kind != TEXT("gather") || Q.OperationId.IsEmpty()
        || Q.CharacterId.IsEmpty() || !Q.bHasQuest || Q.QuestId.IsEmpty() || Q.StepId.IsEmpty()
        || Q.NodeId.IsEmpty() || Q.OccurredAtUnixMs <= 0) { return false; }
    sf::progression::v1::ApplyReceiptRequest Message;
    Message.set_source_receipt_id(Utf8(Q.OperationId)); Message.set_kind("gather"); Message.set_character_id(Utf8(Q.CharacterId));
    Message.set_expected_progression_revision(Q.ExpectedProgressionRevision);
    Message.set_expected_quest_revision(Q.ExpectedQuestRevision);
    auto* Action = Message.mutable_payload();
    Action->set_skill_id(Utf8(Q.SkillId)); Action->set_difficulty(Q.Difficulty);
    Action->set_action_seconds(Q.ActionSeconds); Action->set_context_hash(Q.ContextHash);
    Action->set_success(Q.bSuccess); Action->set_enemy_level(Q.EnemyLevel);
    Action->set_enemy_tier_multiplier(Q.EnemyTierMultiplier); Action->set_eligible_party_size(Q.PartySize);
    auto* Quest = Message.mutable_quest();
    Quest->set_quest_id(Utf8(Q.QuestId)); Quest->set_step_id(Utf8(Q.StepId)); Quest->set_kind(Utf8(Q.QuestKind));
    Quest->set_target_id(Utf8(Q.TargetId)); Quest->set_node_id(Utf8(Q.NodeId));
    Quest->set_quantity(Q.Quantity); Quest->set_occurred_at_unix_ms(Q.OccurredAtUnixMs);
    std::string Bytes;
    if (!Message.SerializeToString(&Bytes) || Bytes.empty() || Bytes.size() > 65536) { return false; }
    Payload.Append(reinterpret_cast<const uint8*>(Bytes.data()), int32(Bytes.size()));
    unsigned char Hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(Bytes.data()), Bytes.size(), Hash);
    static constexpr TCHAR Hex[] = TEXT("0123456789abcdef");
    for (unsigned char Byte : Hash) { Digest.AppendChar(Hex[Byte >> 4]); Digest.AppendChar(Hex[Byte & 15]); }
    return true;
}
