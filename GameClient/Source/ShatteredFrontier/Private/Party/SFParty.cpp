#include "Framework/SFGameMode.h"
#include "Framework/SFPlayerController.h"
#include "Party/SFPartySettings.h"
#include "UI/SFHudWidgets.h"
#include "Characters/SFE1Character.h"
#include "Gathering/SFGatheringPilot.h"
#include "Combat/SFCombatPilot.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/IConsoleManager.h"

namespace
{
enum : uint8 { Invite = 1, Accept, Decline, Leave, Kick, Promote, Assistant };

FString PartyName(const ASFPlayerController* Player)
{
    const APlayerState* State = Player ? Player->GetPlayerState<APlayerState>() : nullptr;
    return State ? State->GetPlayerName() : FString();
}
}

void ASFGameMode::RestorePartyForPlayer(ASFPlayerController* Player)
{
    const auto Service = USFBackpackComponent::Services();
    auto* Pack = USFBackpackComponent::FindOrCreate(Player);
    if (!Service || !Pack || Pack->GetCharacterId().IsEmpty()) { return; }
    const FString CharacterId = Pack->GetCharacterId();
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::GroupGet; Request.GroupActorId = CharacterId;
    Service->Call(Request, [WeakMode = TWeakObjectPtr<ASFGameMode>(this), WeakPlayer = TWeakObjectPtr<ASFPlayerController>(Player), CharacterId](const FSFGameplayResponse& Reply)
    {
        ASFGameMode* Mode = WeakMode.Get();
        ASFPlayerController* Connected = WeakPlayer.Get();
        if (!Mode || !Connected || Reply.Status != ESFGameplayStatus::Ok || Reply.GroupPartyId.IsEmpty() ||
            !Reply.GroupMembers.Contains(CharacterId)) { return; }
        int32 SessionId = Mode->SessionPartyByDurableId.FindRef(Reply.GroupPartyId);
        TMap<FString, int32> ConnectedIds;
        for (auto It = Mode->GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            auto* Candidate = Cast<ASFPlayerController>(It->Get());
            auto* Bag = Candidate ? Candidate->FindComponentByClass<USFBackpackComponent>() : nullptr;
            if (Candidate && Bag && Reply.GroupMembers.Contains(Bag->GetCharacterId()))
            { ConnectedIds.Add(Bag->GetCharacterId(), Candidate->GetUniqueID()); }
        }
        for (const FString& Member : Reply.GroupMembers)
        {
            const int32 ControllerId = ConnectedIds.FindRef(Member);
            if (!ControllerId) { continue; }
            if (const auto* Existing = Mode->PartyModel.FindParty(ControllerId))
            {
                if (SessionId == 0 && Mode->DurablePartyBySessionId.FindRef(Existing->Id) == Reply.GroupPartyId)
                { SessionId = Existing->Id; }
                continue;
            }
            const int32 Restored = Mode->PartyModel.RestoreMember(ControllerId, SessionId);
            if (Restored) { SessionId = Restored; }
        }
        if (SessionId)
        {
            Mode->SessionPartyByDurableId.Add(Reply.GroupPartyId, SessionId);
            Mode->DurablePartyBySessionId.Add(SessionId, Reply.GroupPartyId);
            Mode->PartyModel.RestoreRoles(SessionId, ConnectedIds.FindRef(Reply.GroupLeaderId), ConnectedIds.FindRef(Reply.GroupAssistantId));
            Mode->RefreshPartyViews();
            UE_LOG(LogTemp, Display, TEXT("PARTY_RESTORED Character=%s Party=%s Members=%d"), *CharacterId, *Reply.GroupPartyId, Reply.GroupMembers.Num());
        }
    });
}

ASFPlayerController* ASFGameMode::FindPartyPlayer(const FString& Name) const
{
    if (Name.IsEmpty() || Name.Len() > 64 || !GetWorld()) { return nullptr; }
    ASFPlayerController* Found = nullptr;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ASFPlayerController* Candidate = Cast<ASFPlayerController>(It->Get());
        if (!IsValid(Candidate) || Candidate->IsActorBeingDestroyed()
            || !PartyName(Candidate).Equals(Name, ESearchCase::IgnoreCase)) { continue; }
        if (Found) { return nullptr; } // Ambiguous names cannot select an arbitrary character.
        Found = Candidate;
    }
    return Found;
}

void ASFGameMode::PartyAction(ASFPlayerController* Actor, uint8 Action, const FString& TargetName)
{
    if (!HasAuthority() || !IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != GetWorld()
        || PartyName(Actor).IsEmpty()) { return; }
    const int32 Self = Actor->GetUniqueID();
    ASFPlayerController* Target = Action == Leave ? nullptr : FindPartyPlayer(TargetName);
    const int32 Other = Target ? Target->GetUniqueID() : 0;
    const double Now = GetWorld()->GetTimeSeconds();
    PartyModel.Expire(Now);
    const auto Service = USFBackpackComponent::Services();
    auto* ActorBag = Actor->FindComponentByClass<USFBackpackComponent>();
    auto* TargetBag = Target ? Target->FindComponentByClass<USFBackpackComponent>() : nullptr;
    const FSFPartyModel::FParty* Current = PartyModel.FindParty(Self);
    if (Service && !ActorBag && (Action == Accept || Action == Leave || Action == Kick || Action == Promote || Action == Assistant)) { return; }
    if (Service && ActorBag && (Action == Accept || Action == Leave || Action == Kick || Action == Promote || Action == Assistant))
    {
        if (PendingPartyActions.Contains(Self) || (Action != Leave && !TargetBag)) { return; }
        FSFGameplayRequest Request;
        Request.OperationId = FString::Printf(TEXT("group:%s:%s"), *ActorBag->GetCharacterId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
        Request.GroupActorId = ActorBag->GetCharacterId();
        Request.GroupMemberId = TargetBag ? TargetBag->GetCharacterId() : ActorBag->GetCharacterId();
        Request.GroupAccountId = TargetBag ? TargetBag->GetAccountId() : ActorBag->GetAccountId();
        Request.GroupRealmId = TargetBag ? TargetBag->GetRealmId() : ActorBag->GetRealmId();
        Request.GroupRegionId = GetDefault<USFGameplayServicesSettings>()->GroupRegionId;
        if (Action == Accept)
        {
            const bool bInvited = PartyModel.GetInvites().ContainsByPredicate([=](const FSFPartyModel::FInvite& Pending)
            { return Pending.From == Other && Pending.To == Self && Now < Pending.ExpiresAt; });
            if (!bInvited || !Other || !TargetBag) { return; }
            const auto* Source = PartyModel.FindParty(Other);
            Request.GroupPartyId = Source ? DurablePartyBySessionId.FindRef(Source->Id) : FString();
            if (Source && Request.GroupPartyId.IsEmpty()) { return; }
            Request.GroupActorId = TargetBag->GetCharacterId();
            Request.GroupMemberId = ActorBag->GetCharacterId();
            Request.GroupAccountId = ActorBag->GetAccountId();
            Request.GroupRealmId = ActorBag->GetRealmId();
            Request.Rpc = Request.GroupPartyId.IsEmpty() ? ESFGameplayRpc::GroupCreate : ESFGameplayRpc::GroupJoin;
        }
        else
        {
            if (!Current) { return; }
            Request.GroupPartyId = DurablePartyBySessionId.FindRef(Current->Id);
            if (Request.GroupPartyId.IsEmpty()) { return; }
            Request.Rpc = Action == Leave ? ESFGameplayRpc::GroupLeave : Action == Kick ? ESFGameplayRpc::GroupKick :
                Action == Promote ? ESFGameplayRpc::GroupPromote : ESFGameplayRpc::GroupAssistant;
        }
        PendingPartyActions.Add(Self);
        auto Finish = [WeakMode = TWeakObjectPtr<ASFGameMode>(this), WeakActor = TWeakObjectPtr<ASFPlayerController>(Actor),
            WeakTarget = TWeakObjectPtr<ASFPlayerController>(Target), Action, Self, Other, TargetName](const FSFGameplayResponse& Reply) mutable
        {
            ASFGameMode* Mode = WeakMode.Get();
            if (!Mode) { return; }
            Mode->PendingPartyActions.Remove(Self);
            bool bDone = false;
            if (Reply.Status == ESFGameplayStatus::Ok && WeakActor.IsValid() && (Action == Leave || WeakTarget.IsValid()))
            {
                switch (Action)
                {
                case Accept: bDone = Mode->PartyModel.Accept(Other, Self, Mode->GetWorld()->GetTimeSeconds()); break;
                case Leave: bDone = Mode->PartyModel.Leave(Self); break;
                case Kick: bDone = Mode->PartyModel.Kick(Self, Other); break;
                case Promote: bDone = Mode->PartyModel.Promote(Self, Other); break;
                case Assistant: bDone = Mode->PartyModel.AssignAssistant(Self, Other); break;
                default: break;
                }
                if (bDone && Action == Accept)
                {
                    if (const auto* Joined = Mode->PartyModel.FindParty(Self))
                    {
                        Mode->DurablePartyBySessionId.Add(Joined->Id, Reply.GroupPartyId);
                        Mode->SessionPartyByDurableId.Add(Reply.GroupPartyId, Joined->Id);
                    }
                }
            }
            UE_LOG(LogTemp, Display, TEXT("PARTY_ACTION Actor=%s Action=%d Target=%s Accepted=%d Service=%d"),
                *PartyName(WeakActor.Get()), int32(Action), *TargetName.Left(64), bDone, int32(Reply.Status));
            Mode->RefreshPartyViews();
        };
        if (Action == Accept && Request.Rpc == ESFGameplayRpc::GroupCreate)
        {
            const FString MemberId = Request.GroupMemberId, AccountId = Request.GroupAccountId, RealmId = Request.GroupRealmId;
            FSFGameplayRequest Create = Request;
            Create.GroupAccountId = TargetBag->GetAccountId(); Create.GroupRealmId = TargetBag->GetRealmId();
            FSFGameplayRequest Lookup; Lookup.Rpc = ESFGameplayRpc::GroupGet; Lookup.GroupActorId = Create.GroupActorId;
            Service->Call(Lookup, [Service, Create, MemberId, AccountId, RealmId, Finish = MoveTemp(Finish)](const FSFGameplayResponse& Found) mutable
            {
                if (Found.Status != ESFGameplayStatus::Ok) { Finish(Found); return; }
                auto JoinExisting = [Service, Create, MemberId, AccountId, RealmId, Finish = MoveTemp(Finish)](const FSFGameplayResponse& Party) mutable
                {
                    if (Party.Status != ESFGameplayStatus::Ok) { Finish(Party); return; }
                    FSFGameplayRequest Join; Join.Rpc = ESFGameplayRpc::GroupJoin; Join.OperationId = Create.OperationId + TEXT(":join");
                    Join.GroupPartyId = Party.GroupPartyId; Join.GroupActorId = Create.GroupActorId;
                    Join.GroupMemberId = MemberId; Join.GroupAccountId = AccountId; Join.GroupRealmId = RealmId;
                    Service->Call(Join, MoveTemp(Finish));
                };
                if (Found.GroupPartyId.IsEmpty()) { Service->Call(Create, MoveTemp(JoinExisting)); }
                else { JoinExisting(Found); }
            });
        }
        else { Service->Call(Request, MoveTemp(Finish)); }
        return;
    }
    bool bAccepted = false;
    switch (Action)
    {
    case Invite:
        bAccepted = Other && PartyModel.Invite(Self, Other, Now,
            FMath::Clamp(GetDefault<USFPartySettings>()->InviteTtlSeconds, 1.0f, 300.0f)); break;
    case Accept: bAccepted = Other && PartyModel.Accept(Other, Self, Now); break;
    case Decline: bAccepted = Other && PartyModel.Decline(Other, Self); break;
    case Leave: bAccepted = PartyModel.Leave(Self); break;
    case Kick: bAccepted = Other && PartyModel.Kick(Self, Other); break;
    case Promote: bAccepted = Other && PartyModel.Promote(Self, Other); break;
    case Assistant: bAccepted = Other && PartyModel.AssignAssistant(Self, Other); break;
    default: break;
    }
    UE_LOG(LogTemp, Display, TEXT("PARTY_ACTION Actor=%s Action=%d Target=%s Accepted=%d"),
        *PartyName(Actor), int32(Action), *TargetName.Left(64), bAccepted);
    RefreshPartyViews();
}

void ASFGameMode::RefreshPartyViews()
{
    if (!HasAuthority() || !GetWorld()) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    PartyModel.Expire(Now);
    TMap<int32, ASFPlayerController*> Players;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ASFPlayerController* Player = Cast<ASFPlayerController>(It->Get());
        if (IsValid(Player) && !Player->IsActorBeingDestroyed() && !PartyName(Player).IsEmpty())
        { Players.Add(Player->GetUniqueID(), Player); }
    }
    for (const auto& Pair : Players)
    {
        ASFPlayerController* Player = Pair.Value;
        TArray<FSFPartyMemberView> NewView;
        FString NewLeaderName;
        if (const FSFPartyModel::FParty* Party = PartyModel.FindParty(Pair.Key))
        {
            NewLeaderName = PartyName(Players.FindRef(Party->Leader));
            for (int32 MemberId : Party->Members)
            {
                ASFPlayerController* Member = Players.FindRef(MemberId);
                if (!Member) { continue; }
                FSFPartyMemberView Row;
                Row.Name = PartyName(Member);
                if (const USFGatheringPilotComponent* Pilot = Member->FindComponentByClass<USFGatheringPilotComponent>())
                { Row.Level = Pilot->GetCharacterLevel(); }
                if (const ASFE1Character* Character = Cast<ASFE1Character>(Member->GetPawn()))
                {
                    Row.Health = Character->CombatHealth();
                    Row.MaxHealth = GetDefault<USFCombatPilotSettings>()->PlayerHealth;
                }
                NewView.Add(MoveTemp(Row));
            }
        }
        TArray<FString> NewInvites;
        for (const FSFPartyModel::FInvite& Pending : PartyModel.GetInvites())
        {
            if (Pending.To == Pair.Key)
            { if (ASFPlayerController* From = Players.FindRef(Pending.From)) { NewInvites.Add(PartyName(From)); } }
        }
        NewInvites.Sort();
        if (Player->PartyView != NewView || Player->PartyInvites != NewInvites || Player->PartyLeaderName != NewLeaderName)
        {
            Player->PartyView = MoveTemp(NewView);
            Player->PartyInvites = MoveTemp(NewInvites);
            Player->PartyLeaderName = MoveTemp(NewLeaderName);
            Player->bPartyHudDirty = true;
            Player->ForceNetUpdate();
        }
    }
}

void ASFPlayerController::ServerPartyAction_Implementation(uint8 Action, const FString& TargetName)
{
    if (ASFGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ASFGameMode>() : nullptr)
    { Mode->PartyAction(this, Action, TargetName); }
}

void ASFPlayerController::OnRep_PartyView()
{
    bPartyHudDirty = true;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bPartyScenarioJoined |= PartyView.Num() == 3;
#endif
}

void ASFPlayerController::OnRep_PartyInvites()
{
    UE_LOG(LogTemp, Display, TEXT("PARTY_INVITES Count=%d From=%s"), PartyInvites.Num(),
        *FString::Join(PartyInvites, TEXT(",")));
}

void ASFPlayerController::RefreshPartyHud()
{
    if (!IsLocalController() || !HudWidget || (!bPartyHudDirty && PartyPresentedHud.Get() == HudWidget)) { return; }
    PartyPresentedHud = HudWidget;
    bPartyHudDirty = false;
    HudWidget->ClearParty();
    for (const FSFPartyMemberView& Row : PartyView)
    { HudWidget->AddPartyMember(Row.Name, Row.Level, Row.Health, Row.MaxHealth, 0, 0); }
    UE_LOG(LogTemp, Display, TEXT("PARTY_HUD Members=%d Visible=%d"), PartyView.Num(), HudWidget->PartyCount());
}

#if !UE_BUILD_SHIPPING
void ASFPlayerController::SFPartyInvite(const FString& Name) { ServerPartyAction(Invite, Name); }
void ASFPlayerController::SFPartyAccept(const FString& Name) { ServerPartyAction(Accept, Name); }
void ASFPlayerController::SFPartyDecline(const FString& Name) { ServerPartyAction(Decline, Name); }
void ASFPlayerController::SFPartyLeave() { ServerPartyAction(Leave, FString()); }
void ASFPlayerController::SFPartyKick(const FString& Name) { ServerPartyAction(Kick, Name); }
void ASFPlayerController::SFPartyLeader(const FString& Name) { ServerPartyAction(Promote, Name); }
void ASFPlayerController::SFPartyAssistant(const FString& Name) { ServerPartyAction(Assistant, Name); }

namespace
{
void PartyConsole(UWorld* World, uint8 Action, const TArray<FString>& Args)
{
    ASFPlayerController* Player = World ? Cast<ASFPlayerController>(World->GetFirstPlayerController()) : nullptr;
    if (Player && Player->IsLocalController() && (Action == Leave || Args.Num() == 1))
    { Player->ServerPartyAction(Action, Args.IsEmpty() ? FString() : Args[0]); }
}
#define SF_PARTY_CONSOLE(Name, Action, CommandText) \
    static FAutoConsoleCommandWithWorldAndArgs Name##Command(CommandText, TEXT("Development party command."), \
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { PartyConsole(World, Action, Args); }));
SF_PARTY_CONSOLE(Invite, Invite, TEXT("SFPartyInvite"))
SF_PARTY_CONSOLE(Accept, Accept, TEXT("SFPartyAccept"))
SF_PARTY_CONSOLE(Decline, Decline, TEXT("SFPartyDecline"))
SF_PARTY_CONSOLE(Leave, Leave, TEXT("SFPartyLeave"))
SF_PARTY_CONSOLE(Kick, Kick, TEXT("SFPartyKick"))
SF_PARTY_CONSOLE(Leader, Promote, TEXT("SFPartyLeader"))
SF_PARTY_CONSOLE(Assistant, Assistant, TEXT("SFPartyAssistant"))
#undef SF_PARTY_CONSOLE
}
#endif

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFPlayerController::TickPartyScenario()
{
    if (!IsLocalController() || GetNetMode() != NM_Client || !GetWorld()) { return; }
    if (FParse::Param(FCommandLine::Get(), TEXT("PartyXpScenario")))
    {
        int32 ScenarioRole = 0;
        if (!FParse::Value(FCommandLine::Get(), TEXT("PartyXpScenarioRole="), ScenarioRole) || ScenarioRole < 1 || ScenarioRole > 3) { return; }
        const double Time = GetWorld()->GetTimeSeconds();
        if (Time < PartyScenarioNextAt || PartyScenarioStep == 3) { return; }
        if (PartyView.Num() == 3)
        {
            PartyScenarioStep = 3;
            UE_LOG(LogTemp, Display, TEXT("PARTY_XP_READY Role=%d Members=%d"), ScenarioRole, PartyView.Num());
            return;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("PartyXpRestoreOnly"))) { return; }
        if (ScenarioRole == 1)
        {
            if (PartyView.IsEmpty()) { SFPartyInvite(TEXT("beta")); PartyScenarioNextAt = Time + 2; }
            else if (PartyView.Num() == 2) { SFPartyInvite(TEXT("gamma")); PartyScenarioNextAt = Time + 2; }
        }
        else if (PartyInvites.Contains(TEXT("alpha")))
        { SFPartyAccept(TEXT("alpha")); PartyScenarioNextAt = Time + 2; }
        return;
    }
    int32 ScenarioRole = 0;
    if (!FParse::Value(FCommandLine::Get(), TEXT("PartyScenarioRole="), ScenarioRole) || ScenarioRole < 1 || ScenarioRole > 3) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < PartyScenarioNextAt) { return; }
    if (ScenarioRole == 1)
    {
        if (PartyScenarioStep == 0 && PartyView.Num() == 0)
        { SFPartyInvite(TEXT("beta")); PartyScenarioNextAt = Now + 2; return; }
        if (PartyView.Num() == 2 && PartyLeaderName == TEXT("alpha") && PartyScenarioStep == 0)
        { PartyScenarioStep = 1; }
        if (PartyScenarioStep == 1 && PartyView.Num() == 2)
        { SFPartyInvite(TEXT("gamma")); PartyScenarioNextAt = Now + 2; return; }
        if (PartyScenarioStep == 1 && PartyView.Num() == 3)
        { PartyScenarioStep = 2; SFPartyLeader(TEXT("beta")); PartyScenarioNextAt = Now + 2; return; }
        if (PartyScenarioStep == 2 && PartyView.Num() == 1 && PartyLeaderName == TEXT("alpha"))
        { PartyScenarioStep = 3; UE_LOG(LogTemp, Display, TEXT("PARTY_SCENARIO_DONE Role=1 Members=1 Leader=alpha")); }
    }
    else if (ScenarioRole == 2)
    {
        if (PartyScenarioStep == 0 && PartyInvites.Contains(TEXT("alpha")))
        { PartyScenarioStep = 1; SFPartyAccept(TEXT("alpha")); PartyScenarioNextAt = Now + 2; return; }
        if (PartyScenarioStep == 1 && PartyView.Num() == 3 && PartyLeaderName == TEXT("beta"))
        { PartyScenarioStep = 2; SFPartyKick(TEXT("gamma")); PartyScenarioNextAt = Now + 2; return; }
        if (PartyScenarioStep == 2 && PartyView.Num() == 2)
        { PartyScenarioStep = 3; SFPartyLeave(); PartyScenarioNextAt = Now + 2; return; }
        if (PartyScenarioStep == 3 && PartyView.IsEmpty())
        { PartyScenarioStep = 4; UE_LOG(LogTemp, Display, TEXT("PARTY_SCENARIO_DONE Role=2 Left=1")); }
    }
    else
    {
        if (PartyScenarioStep == 0 && PartyInvites.Contains(TEXT("alpha")))
        { PartyScenarioStep = 1; SFPartyDecline(TEXT("alpha")); PartyScenarioNextAt = Now + 1; return; }
        if (PartyScenarioStep == 1 && PartyInvites.IsEmpty())
        { PartyScenarioStep = 2; }
        if (PartyScenarioStep == 2 && PartyInvites.Contains(TEXT("alpha")))
        { PartyScenarioStep = 3; SFPartyAccept(TEXT("alpha")); PartyScenarioNextAt = Now + 2; return; }
        if (PartyScenarioStep == 3 && bPartyScenarioJoined)
        { PartyScenarioStep = 4; }
        if (PartyScenarioStep == 4 && PartyView.IsEmpty())
        { PartyScenarioStep = 5; UE_LOG(LogTemp, Display, TEXT("PARTY_SCENARIO_DONE Role=3 Kicked=1")); }
    }
}
#endif
