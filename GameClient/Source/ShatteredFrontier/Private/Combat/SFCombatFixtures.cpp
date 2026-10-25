#include "Combat/SFCombatFixtures.h"
#include "Combat/SFCombatPilot.h"
#include "Combat/SFCommonRespawn.h"
#include "Framework/SFGameMode.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

const SFCombatFixtures::FSpec* SFCombatFixtures::Find(const FString& MapName)
{
    using F = ESFNpcFixtureFlags;
    using K = EKind;
    static const FSpec Specs[] = {
        {TEXT("SB_Companions"), TEXT("SF_CompanionSandboxWildlife"), TEXT("companions_sandbox"), 45, false, false, F::None, K::Standard},
        {TEXT("SB_Evade"), TEXT("SF_EvadeSandboxWildlife"), TEXT("evade_sandbox"), 85, false, false, F::None, K::Standard},
        {TEXT("SB_Threat"), TEXT("SF_ThreatSandboxWildlife"), TEXT("threat_sandbox"), 85, false, false, F::None, K::Standard},
        {TEXT("SB_Taunt"), TEXT("SF_TauntSandboxWildlife"), TEXT("taunt_sandbox"), 85, false, false, F::None, K::Standard},
        {TEXT("SB_Aggro"), TEXT("SF_AggroSandboxWildlife"), TEXT("aggro_sandbox"), 85, false, false, F::HoldIdle | F::TraceAggro, K::Standard},
        {TEXT("SB_AttackerMemory"), TEXT("SF_AttackerMemorySandboxWildlife"), TEXT("memory_sandbox"), 85, true, false, F::HoldIdle, K::Standard},
        {TEXT("SB_Regeneration"), TEXT("SF_RegenerationSandboxWildlife"), TEXT("regeneration_sandbox"), 85, false, false, F::None, K::Standard},
        {TEXT("SB_RestRegen"), TEXT("SF_RestRegenSandboxWildlife"), TEXT("rest_regen_sandbox"), 85, false, false, F::None, K::Standard},
        {TEXT("SB_FallDamage"), TEXT("SF_CompanionSandboxWildlife"), TEXT("companions_sandbox"), 45, false, false, F::None, K::EmptyAllowed},
        {TEXT("SB_Assist"), TEXT("SF_AssistSandboxWildlife"), TEXT("assist_sandbox"), 85, false, true, F::HoldIdle, K::Standard},
        {TEXT("SB_Respawn"), TEXT("SF_RespawnSandboxWildlife"), TEXT("respawn_sandbox"), 85, true, false, F::HoldIdle, K::Respawn},
        {TEXT("SB_AutoAttack"), TEXT("SF_AutoAttackSandboxWildlife"), TEXT("auto_attack_sandbox"), 85, true, false, F::None, K::Standard},
        {TEXT("SB_Dodge"), TEXT("SF_DodgeSandboxWildlife"), TEXT("dodge_sandbox"), 85, true, false, F::HoldIdle, K::Standard},
        {TEXT("SB_Unstuck"), TEXT("SF_CompanionSandboxWildlife"), TEXT("companions_sandbox"), 45, false, false, F::None, K::EmptyAllowed},
        {TEXT("SB_SpiritRelease"), TEXT("SF_SpiritReleaseSandboxWildlife"), TEXT("spirit_release_sandbox"), 85, true, false, F::None, K::SpiritRelease},
        {TEXT("SB_ResurrectionAccept"), TEXT("SF_ResurrectionAcceptSandboxWildlife"), TEXT("resurrection_accept_sandbox"), 85, true, false, F::None, K::Standard},
        {TEXT("SB_PersonalLoot"), TEXT("SF_PersonalLootSandboxWildlife"), TEXT("personal_loot_sandbox"), 45, true, false, F::None, K::PersonalLoot},
        {TEXT("SB_OutOfPlaceNpc"), TEXT("SF_OutOfPlaceNpcSandboxWildlife"), TEXT("out_of_place_sandbox"), 45, false, false, F::None, K::Standard}
    };
    const FString Prefix = TEXT("/Game/Frontier/Maps/Sandbox/");
    if (!MapName.StartsWith(Prefix, ESearchCase::CaseSensitive)) return nullptr;
    const FString Suffix = MapName.RightChop(Prefix.Len());
    for (const FSpec& Spec : Specs) if (Suffix == Spec.MapSuffix) return &Spec;
    return nullptr;
}

namespace
{
    TArray<FVector2D> Area(const SFCombatFixtures::FSpec& Spec)
    {
        const double Extent = Spec.ExtentMetres;
        return {FVector2D(-Extent,-Extent), FVector2D(Extent,-Extent), FVector2D(Extent,Extent), FVector2D(-Extent,Extent)};
    }
}

bool ASFGameMode::StartCombatFixture()
{
    const FString MapName = GetWorld()->GetOutermost()->GetName();
    const auto* Spec = SFCombatFixtures::Find(MapName);
    bRespawnSandbox = Spec && Spec->Kind == SFCombatFixtures::EKind::Respawn;
    bCompanionSandbox = Spec && GetNetMode() != NM_Client;
    ActiveCombatFixture = bCompanionSandbox ? Spec : nullptr;
    if (!bCompanionSandbox) return false;
    CapturedSettings = GetDefault<USFCombatPilotSettings>();
    FString Error;
    if (!CapturedSettings->Validate(Error))
    { UE_LOG(LogTemp, Error, TEXT("COMPANION_SANDBOX_DISABLED Invalid combat settings: %s"), *Error); return true; }
    const auto SandboxArea = Area(*Spec);
    int32 Slot = 0;
    for (TActorIterator<ASFCombatNpc> It(GetWorld()); It; ++It)
    {
        if (!It->ActorHasTag(Spec->WildlifeTag)) continue;
        if (bRespawnSandbox) RespawnSandboxPoints.Add(It->GetActorTransform());
        It->Initialize(this, Spec->GroupId, Slot++, SandboxArea, CapturedSettings->BoarHealth, ++NextBoarSerial,
            Spec->bDefensive || (Spec->bAssistDefensiveTag && It->ActorHasTag(TEXT("SF_AssistDefensive"))), Spec->NpcFlags);
    }
    if (bRespawnSandbox)
    {
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
            if (It->ActorHasTag(TEXT("SF_RespawnPoint"))) RespawnSandboxPoints.Add(It->GetActorTransform());
        if (Slot != 1 || RespawnSandboxPoints.Num() != 2)
        { UE_LOG(LogTemp, Error, TEXT("BOAR_RESPAWN_INVALID_POINTS Boars=%d Points=%d"), Slot, RespawnSandboxPoints.Num()); return true; }
    }
    bPilotActive = Spec->Kind == SFCombatFixtures::EKind::EmptyAllowed || Slot > 0;
    RespawnPoints.Add(FVector(0, 0, 110));
    if (Spec->Kind == SFCombatFixtures::EKind::SpiritRelease)
    {
        RespawnPoints.Reset();
        for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
            if (It->ActorHasTag(TEXT("SF_RespawnPoint"))) RespawnPoints.Add(It->GetActorLocation());
        if (Slot != 1 || RespawnPoints.Num() != 1)
        { UE_LOG(LogTemp, Error, TEXT("SPIRIT_RELEASE_INVALID_FIXTURE Boars=%d Points=%d"), Slot, RespawnPoints.Num()); bPilotActive = false; return true; }
    }
    UE_LOG(LogTemp, Display, TEXT("COMBAT_SANDBOX_READY Map=%s Boars=%d"), *MapName, Slot);
    return true;
}

void ASFGameMode::TickCombatFixture()
{
    if (!bPilotActive || !bRespawnSandbox || !ActiveCombatFixture ||
        !FSFCommonRespawn::IsDue(GetWorld()->GetTimeSeconds(), RespawnSandboxDueAt)) return;
    const int32 Preferred = RespawnSandboxCycle % RespawnSandboxPoints.Num();
    int32 NextPoint = INDEX_NONE;
    for (int32 Offset = 0; Offset < RespawnSandboxPoints.Num(); ++Offset)
    {
        const int32 Candidate = (Preferred + Offset) % RespawnSandboxPoints.Num();
        if (!GetWorld()->OverlapAnyTestByChannel(RespawnSandboxPoints[Candidate].GetLocation(), FQuat::Identity,
            ECC_Pawn, FCollisionShape::MakeSphere(110.0))) { NextPoint = Candidate; break; }
    }
    if (NextPoint == INDEX_NONE) return;
    const FTransform& Point = RespawnSandboxPoints[NextPoint];
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Npc = GetWorld()->SpawnActor<ASFCombatNpc>(ASFCombatNpc::StaticClass(), Point, Params);
    if (!Npc) return;
    Npc->Initialize(this, ActiveCombatFixture->GroupId, 0, Area(*ActiveCombatFixture),
        CapturedSettings->BoarHealth, ++NextBoarSerial, ActiveCombatFixture->bDefensive, ActiveCombatFixture->NpcFlags);
    RespawnSandboxDueAt = -1.0;
    UE_LOG(LogTemp, Display, TEXT("BOAR_RESPAWN_ACTIVE Cycle=%d Point=%d T=%.3f HP=%d X=%.0f Y=%.0f"),
        RespawnSandboxCycle, NextPoint, GetWorld()->GetTimeSeconds(), Npc->Health(), Point.GetLocation().X, Point.GetLocation().Y);
}

bool ASFGameMode::HandleCombatFixtureDefeat(ASFCombatNpc* Npc, ASFE1Character* Killer)
{
    if (!bCompanionSandbox || !ActiveCombatFixture) return false;
    if (ActiveCombatFixture->Kind == SFCombatFixtures::EKind::PersonalLoot) RewardKill(Npc, Killer);
    if (bRespawnSandbox)
    {
        int32 PlayersInArea = 0;
        const auto SandboxArea = Area(*ActiveCombatFixture);
        for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (const APawn* Pawn = It->Get()->GetPawn())
                if (IsInsideTerritory(Pawn->GetActorLocation(), SandboxArea)) ++PlayersInArea;
        const double Now = GetWorld()->GetTimeSeconds();
        const double Delay = FSFCommonRespawn::DelaySeconds(CapturedSettings->CommonRespawnSeconds,
            PlayersInArea, CapturedSettings->RespawnPlayerSlope, CapturedSettings->RespawnPlayerFloor,
            CapturedSettings->RespawnJitter, FMath::FRand());
        ++RespawnSandboxCycle; RespawnSandboxDueAt = Now + Delay;
        UE_LOG(LogTemp, Display, TEXT("BOAR_RESPAWN_DEAD Cycle=%d Players=%d Delay=%.3f Due=%.3f"),
            RespawnSandboxCycle, PlayersInArea, Delay, RespawnSandboxDueAt);
    }
    UE_LOG(LogTemp, Display, TEXT("COMPANION_SANDBOX_BOAR_DEAD Slot=%d"), Npc->SlotIndex());
    Npc->Destroy();
    return true;
}
