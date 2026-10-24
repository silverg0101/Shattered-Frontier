#include "Framework/SFGameMode.h"
#include "FrontEnd/SFFrontEnd.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Framework/SFPlayerController.h"
#include "Construction/SFConstructionNetwork.h"
#include "Characters/SFE1Character.h"
#include "Interaction/SFE1InteractionTarget.h"
#include "Exchange/SFMarketBoardActor.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Tests/SFE1NetworkScenario.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"

FString ASFGameMode::TakeGatewayCharacter(const FString& Ticket, FString& Account, FString& Realm)
{
    FString Id;
    const FString Key = FMD5::HashAnsiString(*Ticket);
    PendingGatewayCharacters.RemoveAndCopyValue(Key, Id);
    PendingGatewayAccounts.RemoveAndCopyValue(Key, Account);
    PendingGatewayRealms.RemoveAndCopyValue(Key, Realm);
    return Id;
}

bool ASFGameMode::UsesGatewayAdmission() const
{
#if UE_BUILD_SHIPPING
    return true;
#else
    return GetDefault<USFFrontEndSettings>()->IdentitySource == ESFFrontEndIdentitySource::Gateway ||
        FParse::Param(FCommandLine::Get(), TEXT("SFIdentityGateway"));
#endif
}

void ASFGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty() || !UsesGatewayAdmission()) { return; }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const FString Dev = UGameplayStatics::ParseOption(Options, TEXT("SFDevCharacter"));
    if (IsAdmitted() && FParse::Param(FCommandLine::Get(), TEXT("SFDevIdentity")) && USFBackpackComponent::IsValidDevName(Dev)) { return; }
#endif
    const FString Ticket = UGameplayStatics::ParseOption(Options, TEXT("SFIdentityTicket"));
    if (Ticket.IsEmpty() || Ticket.Len() > 4096) { ErrorMessage = TEXT("A valid world ticket is required."); UE_LOG(LogTemp, Warning, TEXT("GATEWAY_TICKET_REJECTED Reason=missing")); return; }
    const FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("SF_IDENTITY_VERIFIER_KEY"));
    const FString Url = GetDefault<USFFrontEndSettings>()->SafeGatewayBaseUrl();
    if (Key.Len() < 32 || Url.IsEmpty()) { ErrorMessage = TEXT("World admission is unavailable."); UE_LOG(LogTemp, Warning, TEXT("GATEWAY_TICKET_REJECTED Reason=config")); return; }
    TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("ticket"), Ticket);
    Payload->SetStringField(TEXT("server_id"), GetDefault<USFFrontEndSettings>()->WorldServerId);
    FString Body;
    FJsonSerializer::Serialize(Payload, TJsonWriterFactory<>::Create(&Body));
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Url + TEXT("/v1/world/verify-ticket"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-World-Verifier-Key"), Key);
    Request->SetContentAsString(Body);
    Request->SetTimeout(4.f);
    Request->ProcessRequestUntilComplete();
    const FHttpResponsePtr Response = Request->GetResponse();
    TSharedPtr<FJsonObject> Result;
    if (!Response.IsValid() || Response->GetResponseCode() != 200 ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Result) || !Result.IsValid())
    { ErrorMessage = TEXT("World ticket was rejected."); UE_LOG(LogTemp, Warning, TEXT("GATEWAY_TICKET_REJECTED Reason=verify")); return; }
    FString CharacterId, AccountId, RealmId;
    if (!Result->TryGetStringField(TEXT("character_id"), CharacterId) || !CharacterId.StartsWith(TEXT("chr_")) || CharacterId.Len() > 80)
    { ErrorMessage = TEXT("World ticket was rejected."); UE_LOG(LogTemp, Warning, TEXT("GATEWAY_TICKET_REJECTED Reason=claims")); return; }
    if (!Result->TryGetStringField(TEXT("account_id"), AccountId) || !AccountId.StartsWith(TEXT("acct_")) || AccountId.Len() > 80 ||
        !Result->TryGetStringField(TEXT("realm_id"), RealmId) || RealmId.IsEmpty() || RealmId.Len() > 32)
    { ErrorMessage = TEXT("World ticket was rejected."); UE_LOG(LogTemp, Warning, TEXT("GATEWAY_TICKET_REJECTED Reason=group-claims")); return; }
    const FString TicketHash = FMD5::HashAnsiString(*Ticket);
    PendingGatewayCharacters.Add(TicketHash, CharacterId);
    PendingGatewayAccounts.Add(TicketHash, AccountId);
    PendingGatewayRealms.Add(TicketHash, RealmId);
    FTimerHandle Expiry;
    GetWorldTimerManager().SetTimer(Expiry, FTimerDelegate::CreateWeakLambda(this, [this, TicketHash]() { PendingGatewayCharacters.Remove(TicketHash); PendingGatewayAccounts.Remove(TicketHash); PendingGatewayRealms.Remove(TicketHash); }), 120.f, false);
}

ASFGameMode::ASFGameMode()
{
    DefaultPawnClass = ASFE1Character::StaticClass();
    PlayerControllerClass = ASFPlayerController::StaticClass();
    HUDClass = ASFConstructionNetworkHUD::StaticClass();
    // Combat slot refresh is the only per-tick work; 1 Hz is enough for it.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 1.0f;
}

void ASFGameMode::StartPlay()
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        // Cosmetic dressing (PCG ground cover and litter, tagged by pcg_dressing.py) is client-only: drop it before it begins play.
        TArray<AActor*> Cosmetic;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It) { if (It->ActorHasTag(TEXT("SF_ClientOnlyCosmetic"))) { Cosmetic.Add(*It); } }
        for (AActor* Actor : Cosmetic) { Actor->Destroy(); }
        UE_LOG(LogTemp, Display, TEXT("COSMETIC_STRIPPED Count=%d"), Cosmetic.Num());
    }
    Super::StartPlay();
    // Versioned E1-only placement beside one arrival spawn; no biome/resource claim.
    // Zone maps share this game mode, so the fixture stays in the E1 graybox.
    if (GetWorld()->GetMapName().EndsWith(TEXT("Greenlands_E1")))
    {
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASFE1InteractionTarget* Target = GetWorld()->SpawnActor<ASFE1InteractionTarget>(
            FVector(-300.0, -750.0, 60.0), FRotator::ZeroRotator, Spawn);
        if (!Target) { UE_LOG(LogTemp, Fatal, TEXT("E1 interaction fixture failed to spawn")); }
    }
    if (GetWorld()->GetMapName().EndsWith(TEXT("Greenlands_Silverbrook")))
    {
        bool bBoardExists = false;
        for (TActorIterator<ASFMarketBoardActor> It(GetWorld()); It; ++It) { bBoardExists = true; break; }
        if (!bBoardExists)
        {
            // Authored Market Square south edge [0,-8] metres; trace terrain so the marker follows the built map.
            FHitResult Ground;
            const FVector Above(0, -800, 50000), Below(0, -800, -50000);
            if (GetWorld()->LineTraceSingleByChannel(Ground, Above, Below, ECC_WorldStatic))
            {
                GetWorld()->SpawnActor<ASFMarketBoardActor>(Ground.Location + FVector(0, 0, 50), FRotator::ZeroRotator);
            }
            else { UE_LOG(LogTemp, Error, TEXT("MARKET_BOARD_GROUND_MISSING Silverbrook")); }
        }
    }
    UE_LOG(LogTemp, Display, TEXT("E1_WORLD_READY Map=%s NetMode=%d LocalDevelopmentOnly=1"), *GetWorld()->GetMapName(), static_cast<int32>(GetNetMode()));
#if WITH_DEV_AUTOMATION_TESTS
    StartE1NetworkScenario(GetWorld());
#endif
    StartConstruction();
    StartCombat();
}

void ASFGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")))
    { RecordM3Connection(NewPlayer ? NewPlayer->NetConnection : nullptr); }
    UE_LOG(LogTemp, Display, TEXT("E1_PLAYER_JOIN Controller=%s Players=%d Authority=%d"), *GetNameSafe(NewPlayer), GetNumPlayers(), HasAuthority());
    ScheduleLifecycleAudit(NewPlayer, false);
    PostLoginConstruction(NewPlayer);
    PostLoginCombat(NewPlayer);
    if (ASFPlayerController* PartyPlayer = Cast<ASFPlayerController>(NewPlayer)) { RestorePartyForPlayer(PartyPlayer); }
}

void ASFGameMode::Logout(AController* Exiting)
{
    if (ASFPlayerController* PartyPlayer = Cast<ASFPlayerController>(Exiting))
    {
        const int32 Leaving = PartyPlayer->GetUniqueID();
        if (const auto* Party = PartyModel.FindParty(Leaving); Party && Party->Leader == Leaving && Party->Members.Num() > 1)
        {
            const int32 Successor = Party->Assistant ? Party->Assistant : Party->Members[Party->Members[0] == Leaving ? 1 : 0];
            const FString PartyId = DurablePartyBySessionId.FindRef(Party->Id);
            auto* LeavingBag = PartyPlayer->FindComponentByClass<USFBackpackComponent>();
            ASFPlayerController* SuccessorPlayer = nullptr;
            for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            { if (auto* Candidate = Cast<ASFPlayerController>(It->Get()); Candidate && Candidate->GetUniqueID() == Successor) { SuccessorPlayer = Candidate; break; } }
            auto* SuccessorBag = SuccessorPlayer ? SuccessorPlayer->FindComponentByClass<USFBackpackComponent>() : nullptr;
            if (!PartyId.IsEmpty() && LeavingBag && SuccessorBag)
            {
                if (auto Service = USFBackpackComponent::Services())
                {
                    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::GroupPromote;
                    Request.OperationId = FString::Printf(TEXT("group:disconnect:%s:%s"), *LeavingBag->GetCharacterId(), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
                    Request.GroupPartyId = PartyId; Request.GroupActorId = LeavingBag->GetCharacterId(); Request.GroupMemberId = SuccessorBag->GetCharacterId();
                    Service->Call(Request, [PartyId](const FSFGameplayResponse& Reply)
                    { UE_LOG(LogTemp, Display, TEXT("PARTY_DISCONNECT_PROMOTE Party=%s Status=%d"), *PartyId, int32(Reply.Status)); });
                }
            }
        }
        PartyModel.RemoveConnection(PartyPlayer->GetUniqueID());
        RefreshPartyViews();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")))
    { FinishM3Connection(Cast<APlayerController>(Exiting) ? Cast<APlayerController>(Exiting)->NetConnection : nullptr); }
    UE_LOG(LogTemp, Display, TEXT("E1_PLAYER_LEAVE Controller=%s"), *GetNameSafe(Exiting));
    // Capture before Logout/Destroyed finishes; inspect the world on the next tick.
    ScheduleLifecycleAudit(Exiting, true);
    Super::Logout(Exiting);
}

void ASFGameMode::ScheduleLifecycleAudit(AController* Trigger, bool bLeaving)
{
    const TWeakObjectPtr<AController> TriggerController(Trigger);
    // PlayerController may already have destroyed/unpossessed its pawn at Logout.
    const TWeakObjectPtr<APawn> TriggerPawn = bLeaving
        ? LifecyclePawns.FindRef(TriggerController) : TWeakObjectPtr<APawn>(Trigger ? Trigger->GetPawn() : nullptr);
    const FString ControllerName = GetNameSafe(Trigger);
    const FString PawnName = bLeaving && LifecyclePawnNames.Contains(TriggerController)
        ? LifecyclePawnNames.FindRef(TriggerController) : GetNameSafe(TriggerPawn.Get());
    if (bLeaving)
    {
        LifecyclePawns.Remove(TriggerController);
        LifecyclePawnNames.Remove(TriggerController);
    }
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
        [this, TriggerController, TriggerPawn, ControllerName, PawnName, bLeaving]()
        {
            const uint64 Audit = ++LifecycleAuditSequence;
            int32 ActiveControllers = 0;
            int32 ActivePawns = 0;
            int32 PossessedPawns = 0;
            bool bDepartedControllerActive = false;
            bool bDepartedPawnActive = false;
            for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* PC = It->Get();
                if (!IsValid(PC) || PC->IsActorBeingDestroyed()) { continue; }
                ++ActiveControllers;
                bDepartedControllerActive |= bLeaving && PC == TriggerController.Get();
                APawn* Pawn = PC->GetPawn();
                if (IsValid(Pawn) && !Pawn->IsActorBeingDestroyed())
                {
                    ++PossessedPawns;
                    UE_LOG(LogTemp, Display, TEXT("E1_LIFECYCLE_PAIR Audit=%llu Controller=%s Pawn=%s Authority=%d"),
                        Audit, *PC->GetName(), *Pawn->GetName(), HasAuthority());
                }
            }
            for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
            {
                if (!IsValid(*It) || It->IsActorBeingDestroyed()) { continue; }
                ++ActivePawns;
                bDepartedPawnActive |= bLeaving && *It == TriggerPawn.Get();
            }
            const APawn* DepartedPawn = TriggerPawn.Get();
            const AController* DepartedPossessor = DepartedPawn ? DepartedPawn->GetController() : nullptr;
            const bool bDepartedPawnPossessed = bLeaving && IsValid(DepartedPossessor)
                && !DepartedPossessor->IsActorBeingDestroyed();
            UE_LOG(LogTemp, Display, TEXT("E1_LIFECYCLE_STATE Audit=%llu Event=%s Controller=%s Pawn=%s DepartedControllerActive=%d DepartedPawnActive=%d DepartedPawnPossessed=%d Players=%d ActiveControllers=%d ActivePawns=%d PossessedPawns=%d Authority=%d"),
                Audit, bLeaving ? TEXT("Leave") : TEXT("Join"), *ControllerName, *PawnName,
                bDepartedControllerActive, bDepartedPawnActive, bDepartedPawnPossessed,
                GetNumPlayers(), ActiveControllers, ActivePawns, PossessedPawns, HasAuthority());
        }));
}

void ASFGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    if (NewPlayer && NewPlayer->GetPawn())
    {
        LifecyclePawns.Add(NewPlayer, NewPlayer->GetPawn());
        LifecyclePawnNames.Add(NewPlayer, NewPlayer->GetPawn()->GetName());
    }
    UE_LOG(LogTemp, Display, TEXT("E1_POSSESSION Controller=%s Pawn=%s Authority=%d"),
        *GetNameSafe(NewPlayer), *GetNameSafe(NewPlayer ? NewPlayer->GetPawn() : nullptr), HasAuthority());
    RestartConstruction(NewPlayer);
    RestartCombat(NewPlayer);
}

void ASFGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    RefreshPartyViews();
    if (M3StartedAt > 0 && !bM3CaptureFinished)
    {
        for (auto& Pair : M3Connections)
        {
            if (UNetConnection* Connection = Pair.Key.Get())
            {
                Pair.Value.LastInBytes = Connection->InTotalBytes;
                Pair.Value.LastOutBytes = Connection->OutTotalBytes;
                Pair.Value.LastAt = FPlatformTime::Seconds();
            }
        }
    }
    TickCombat();
}

void ASFGameMode::RecordM3Connection(UNetConnection* Connection)
{
    if (!Connection || !GetWorld() || !GetWorld()->GetNetDriver()) { return; }
    if (M3StartedAt == 0)
    {
        M3StartedAt = FPlatformTime::Seconds();
        UE_LOG(LogTemp, Display, TEXT("M3_CAPTURE_START TimeSec=%.6f"), M3StartedAt);
        M3DriverInBytes = GetWorld()->GetNetDriver()->InTotalBytes;
        M3DriverOutBytes = GetWorld()->GetNetDriver()->OutTotalBytes;
        FString CsvDir;
        if (FParse::Value(FCommandLine::Get(), TEXT("M3CsvDir="), CsvDir))
        { FCsvProfiler::Get()->BeginCapture(-1, CsvDir, TEXT("server-m3.csv")); }
    }
    FM3ConnectionSample Sample;
    Sample.Id = ++M3NextConnectionId;
    Sample.InBytes = Connection->InTotalBytes;
    Sample.OutBytes = Connection->OutTotalBytes;
    Sample.JoinedAt = FPlatformTime::Seconds();
    Sample.LastInBytes = Sample.InBytes;
    Sample.LastOutBytes = Sample.OutBytes;
    Sample.LastAt = Sample.JoinedAt;
    M3Connections.Add(Connection, Sample);
}

void ASFGameMode::FinishM3Connection(UNetConnection* Connection)
{
    if (!Connection) { return; }
    const FM3ConnectionSample* Sample = M3Connections.Find(Connection);
    if (!Sample) { return; }
    const double Seconds = FMath::Max(0.001, FPlatformTime::Seconds() - Sample->JoinedAt);
    M3FinishedConnections.Add(FString::Printf(TEXT("%d:%0.3f:%0.1f:%0.1f"), Sample->Id, Seconds,
        double(Connection->InTotalBytes - Sample->InBytes) / Seconds,
        double(Connection->OutTotalBytes - Sample->OutBytes) / Seconds));
    M3Connections.Remove(Connection);
}

void ASFGameMode::FinishM3Capture()
{
    if (bM3CaptureFinished || M3StartedAt == 0 || !GetWorld() || !GetWorld()->GetNetDriver()) { return; }
    bM3CaptureFinished = true;
    for (const auto& Pair : M3Connections)
    {
        const FM3ConnectionSample& Sample = Pair.Value;
        const UNetConnection* Connection = Pair.Key.Get();
        const double EndAt = Connection ? FPlatformTime::Seconds() : Sample.LastAt;
        const double Duration = FMath::Max(0.001, EndAt - Sample.JoinedAt);
        const int32 InBytes = Connection ? Connection->InTotalBytes : Sample.LastInBytes;
        const int32 OutBytes = Connection ? Connection->OutTotalBytes : Sample.LastOutBytes;
        M3FinishedConnections.Add(FString::Printf(TEXT("%d:%0.3f:%0.1f:%0.1f"), Sample.Id, Duration,
            double(InBytes - Sample.InBytes) / Duration, double(OutBytes - Sample.OutBytes) / Duration));
    }
    M3Connections.Empty();
    const double Seconds = FMath::Max(0.001, FPlatformTime::Seconds() - M3StartedAt);
    UNetDriver* Driver = GetWorld()->GetNetDriver();
    UE_LOG(LogTemp, Display, TEXT("M3_SERVER_NET DurationSec=%.3f InBps=%.1f OutBps=%.1f Connections=%s"), Seconds,
        double(Driver->InTotalBytes - M3DriverInBytes) / Seconds,
        double(Driver->OutTotalBytes - M3DriverOutBytes) / Seconds,
        *FString::Join(M3FinishedConnections, TEXT(";")));
    FString CsvDir;
    if (FParse::Value(FCommandLine::Get(), TEXT("M3CsvDir="), CsvDir)) { FCsvProfiler::Get()->EndCapture(); }
}
