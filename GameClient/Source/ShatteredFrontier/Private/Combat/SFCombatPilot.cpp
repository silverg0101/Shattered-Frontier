#include "Combat/SFCombatPilot.h"
#include "Inventory/SFGear.h"
#include "Framework/SFGameMode.h"
#include "Combat/SFCombatPilotRewards.h"
#include "Characters/SFE1Character.h"
#include "Framework/SFPlayerController.h"
#include "Gathering/SFGatheringPilot.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Quest/SFQuestPilotEvents.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFCharacterSession.h"
#include "Services/SFGameplayServicesClient.h"

namespace
{
int64 UnixNowMs()
{
    const FDateTime Now = FDateTime::UtcNow();
    return Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();
}
TArray<FVector2D> ReadPolygon(const TArray<TSharedPtr<FJsonValue>>& Values)
{
    TArray<FVector2D> Result;
    for (const auto& Value : Values)
    {
        const auto& Pair = Value->AsArray();
        if (Pair.Num() >= 2) { Result.Emplace(Pair[0]->AsNumber(), Pair[1]->AsNumber()); }
    }
    return Result;
}

FVector ZonePoint(const TArray<TSharedPtr<FJsonValue>>& Values)
{
    return FVector(Values[0]->AsNumber() * 100.0, Values[1]->AsNumber() * 100.0,
        Values.Num() > 2 ? Values[2]->AsNumber() * 100.0 : 0.0);
}

FVector Grounded(UWorld* World, const FVector& XY, float HalfHeight)
{
    FHitResult Hit;
    const FVector Start(XY.X, XY.Y, 20000);
    const FVector End(XY.X, XY.Y, -20000);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatPilotGround), false);
    if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
    { return FVector(XY.X, XY.Y, Hit.ImpactPoint.Z + HalfHeight + 4); }
    return FVector(XY.X, XY.Y, XY.Z + HalfHeight + 4);
}
}

bool SFCombatPilotRules::CanMelee(double DistanceCm, double FacingDot, double Now,
    double NextAllowed, double RangeCm)
{
    return FMath::IsFinite(DistanceCm) && FMath::IsFinite(FacingDot) &&
        DistanceCm <= RangeCm && FacingDot >= 0.35 && Now >= NextAllowed;
}

int32 SFCombatPilotRules::RemainingHealth(int32 Current, int32 Damage)
{
    return Damage > 0 ? FMath::Max(0, Current - Damage) : Current;
}

bool USFCombatPilotSettings::Validate(FString& Error) const
{
    if (PlayerHealth < 20 || PlayerHealth > 500 || BoarHealth < 10 || BoarHealth > 500 ||
        PlayerMeleeDamage < 1 || PlayerMeleeDamage > 100 || BoarMeleeDamage < 1 || BoarMeleeDamage > 100 ||
        !FMath::IsWithinInclusive(PlayerMeleeCooldownSeconds, 0.3f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarMeleeCooldownSeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(MeleeRangeMetres, 1.0f, 5.0f) ||
        !FMath::IsWithinInclusive(TabTargetRangeMetres, 10.0f, 60.0f) ||
        !FMath::IsWithinInclusive(TabTargetDropRangeMetres, TabTargetRangeMetres, 100.0f) ||
        !FMath::IsWithinInclusive(TabTargetConeDegrees, 60.0f, 360.0f) ||
        !FMath::IsWithinInclusive(TabTargetCycleResetSeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(PlayerRespawnDelaySeconds, 1.0f, 60.0f) ||
        (PlayerRespawnPointRule != ESFPlayerRespawnPointRule::NearestToDeath && PlayerRespawnPointRule != ESFPlayerRespawnPointRule::FirstAuthored) ||
        !FMath::IsWithinInclusive(BoarAggroRadiusMetres, 4.0f, 18.0f) ||
        !FMath::IsWithinInclusive(BoarNearNoticeRadiusMetres, 1.0f, 10.0f) ||
        BoarNearNoticeRadiusMetres > BoarAggroRadiusMetres ||
        !FMath::IsWithinInclusive(BoarLeashRadiusMetres, 20.0f, 100.0f) ||
        !FMath::IsWithinInclusive(BoarFieldOfViewDegrees, 60.0f, 360.0f) ||
        !FMath::IsWithinInclusive(BoarWanderRadiusMetres, 2.0f, 40.0f) ||
        !FMath::IsWithinInclusive(BoarWanderSpeedMetresPerSecond, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarChaseSpeedMetresPerSecond, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(BoarWanderMinSeconds, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(BoarWanderMaxSeconds, BoarWanderMinSeconds, 20.0f) ||
        HidesPerBoar < 1 || HidesPerBoar > 5)
    { Error = TEXT("Combat pilot balance is outside declared bounds"); return false; }
    return true;
}

ASFPilotBoar::ASFPilotBoar()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(55, 65);
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoarBody"));
    Body->SetupAttachment(GetCapsuleComponent());
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetRelativeScale3D(FVector(1.3, 0.7, 0.7));
    if (!IsRunningDedicatedServer())
    {
        static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
        Body->SetStaticMesh(Cube.Object);
    }
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthLabel"));
    Label->SetupAttachment(GetCapsuleComponent());
    Label->SetRelativeLocation(FVector(0, 0, 130));
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetWorldSize(34);
    SetNetCullDistanceSquared(FMath::Square(80000.0f));
}

void ASFPilotBoar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFPilotBoar, CurrentHealth);
}

void ASFPilotBoar::Initialize(ASFGameMode* OwnerMode, const FString& Group, int32 InSlot,
    const TArray<FVector2D>& Area, int32 Health, int32 Serial)
{
    check(HasAuthority());
    PilotMode = OwnerMode; SpawnGroup = Group; Slot = InSlot; Territory = Area; SpawnSerial = Serial;
    Anchor = GetActorLocation(); CurrentHealth = Health; OnRep_Health();
}

void ASFPilotBoar::OnRep_Health()
{
    Label->SetText(FText::FromString(FString::Printf(TEXT("Boar L2  %d HP"), CurrentHealth)));
    Label->SetTextRenderColor(CurrentHealth > 0 ? FColor::Yellow : FColor::Red);
}

void ASFPilotBoar::ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage)
{
    if (!HasAuthority() || CurrentHealth <= 0 || !IsValid(Attacker) || Damage <= 0) { return; }
    Target = Attacker;
    bReturning = false;
    CurrentHealth = SFCombatPilotRules::RemainingHealth(CurrentHealth, Damage);
    if (!Attacker->GetCombatTarget() && CurrentHealth > 0) { Attacker->SetCombatTarget(this); }
    OnRep_Health(); ForceNetUpdate();
    USFBackpackComponent* AttackerBag = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Attacker->GetController()));
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_HIT Group=%s Slot=%d HP=%d Attacker=%s"),
        *SpawnGroup, Slot, CurrentHealth, AttackerBag ? *AttackerBag->GetCharacterId() : TEXT("none"));
    if (CurrentHealth == 0 && PilotMode.IsValid()) { PilotMode->BoarDied(this, Attacker); }
}

void ASFPilotBoar::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || CurrentHealth <= 0 || !PilotMode.IsValid() || !PilotMode->IsCombatActive()) { return; }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Keep one shared target alive across the staged client reconnect in this network fixture.
    if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario"))) { return; }
#endif
    auto* Mode = PilotMode.Get();
    const double Now = GetWorld()->GetTimeSeconds();
    ASFE1Character* Victim = Target.Get();
    if (Victim && (Victim->CombatHealth() <= 0 || Mode->IsSafe(Victim->GetActorLocation()) ||
        !Mode->IsInsideTerritory(Victim->GetActorLocation(), Territory) ||
        FVector::Dist2D(Anchor, GetActorLocation()) > Mode->CombatSettings()->BoarLeashRadiusMetres*100))
    { Target.Reset(); Victim = nullptr; WanderDestination = Anchor; bReturning = true; }
    if (bReturning && FVector::Dist2D(GetActorLocation(),Anchor) < 100)
    { bReturning = false; CurrentHealth = Mode->CombatSettings()->BoarHealth; OnRep_Health(); ForceNetUpdate(); }
    if (!Victim && !bReturning && Mode->CombatSettings()->bCombatAvailable)
    {
        for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            auto* Candidate = Cast<ASFE1Character>(It->Get()->GetPawn());
            const FVector Toward = Candidate ? Candidate->GetActorLocation()-GetActorLocation() : FVector::ZeroVector;
            const double Distance = Toward.Size2D();
            // A linkdead (draining) character stays visible but is never chosen as prey (INT-05).
            if (IsValid(Candidate) && Candidate->CombatHealth() > 0 && !USFCharacterSessionComponent::IsLinkdead(It->Get()) &&
                !USFCharacterSessionComponent::IsRestoring(It->Get()) &&
                !Mode->IsSafe(Candidate->GetActorLocation()) &&
                Mode->IsInsideTerritory(Candidate->GetActorLocation(), Territory) &&
                Distance <= Mode->CombatSettings()->BoarAggroRadiusMetres*100 &&
                (Distance <= Mode->CombatSettings()->BoarNearNoticeRadiusMetres*100 ||
                    FVector::DotProduct(GetActorForwardVector(), Toward.GetSafeNormal2D()) >=
                    FMath::Cos(FMath::DegreesToRadians(Mode->CombatSettings()->BoarFieldOfViewDegrees*0.5f))))
            { Target = Candidate; Victim = Candidate; break; }
        }
    }
    FVector Destination = WanderDestination;
    float Speed = Mode->CombatSettings()->BoarWanderSpeedMetresPerSecond*100;
    if (Victim)
    {
        Destination = Victim->GetActorLocation(); Speed = Mode->CombatSettings()->BoarChaseSpeedMetresPerSecond*100;
        if (FVector::Dist2D(GetActorLocation(), Destination) <= Mode->CombatSettings()->MeleeRangeMetres * 100 && Now >= NextSwingAt)
        { NextSwingAt = Now + Mode->CombatSettings()->BoarMeleeCooldownSeconds; Mode->TryBoarAttack(this, Victim); }
    }
    else if (bReturning)
    { Destination = Anchor; Speed = Mode->CombatSettings()->BoarChaseSpeedMetresPerSecond*100; }
    else if (Now >= NextWanderAt || Destination.IsNearlyZero() || FVector::Dist2D(GetActorLocation(), Destination) < 80)
    {
        NextWanderAt = Now + FMath::FRandRange(double(Mode->CombatSettings()->BoarWanderMinSeconds),
            double(Mode->CombatSettings()->BoarWanderMaxSeconds));
        for (int32 Attempt = 0; Attempt < 12; ++Attempt)
        {
            const double Radius = Mode->CombatSettings()->BoarWanderRadiusMetres*100;
            const FVector Candidate = Anchor + FVector(FMath::FRandRange(-Radius, Radius), FMath::FRandRange(-Radius, Radius), 0);
            if (Mode->IsInsideTerritory(Candidate, Territory) && !Mode->IsSafe(Candidate))
            { WanderDestination = Candidate; break; }
        }
        Destination = WanderDestination;
    }
    FVector Direction = Destination - GetActorLocation(); Direction.Z = 0;
    if (Direction.SizeSquared2D() < 2500) { return; }
    const FVector Candidate = GetActorLocation() + Direction.GetSafeNormal2D() * FMath::Min(Speed * DeltaSeconds, Direction.Size2D());
    if (!Mode->IsInsideTerritory(Candidate, Territory) || Mode->IsSafe(Candidate)) { Target.Reset(); return; }
    const FVector Ground = Grounded(GetWorld(), Candidate, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    SetActorLocation(Ground, true);
    SetActorRotation(Direction.Rotation());
}

bool ASFGameMode::IsInsideTerritory(const FVector& Location, const TArray<FVector2D>& Polygon) const
{
    if (Polygon.Num() < 3) { return false; }
    const FVector2D P(Location.X / 100.0, Location.Y / 100.0);
    bool Inside = false;
    for (int32 I = 0, J = Polygon.Num()-1; I < Polygon.Num(); J = I++)
    {
        const FVector2D& A = Polygon[I]; const FVector2D& B = Polygon[J];
        const double Cross = (P.X-A.X)*(B.Y-A.Y) - (P.Y-A.Y)*(B.X-A.X);
        if (FMath::Abs(Cross) < 1e-7 &&
            P.X >= FMath::Min(A.X,B.X)-1e-7 && P.X <= FMath::Max(A.X,B.X)+1e-7 &&
            P.Y >= FMath::Min(A.Y,B.Y)-1e-7 && P.Y <= FMath::Max(A.Y,B.Y)+1e-7) return true;
        if ((A.Y > P.Y) != (B.Y > P.Y) && P.X < (B.X-A.X) * (P.Y-A.Y) / (B.Y-A.Y) + A.X) { Inside = !Inside; }
    }
    return Inside;
}

bool ASFGameMode::IsSafe(const FVector& Location) const
{
    for (const auto& Polygon : ProtectedAreas)
    { if (IsInsideTerritory(Location, Polygon)) { return true; } }
    return false;
}

bool ASFGameMode::LoadZoneRecord()
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/world/zones/greenlands_silverbrook.json"));
    FString Source; TSharedPtr<FJsonObject> Zone;
    if (!FFileHelper::LoadFileToString(Source, *Path) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Zone) || !Zone.IsValid() ||
        Zone->GetStringField(TEXT("map_id")) != TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook"))
    { return false; }
    for (const auto& Value : Zone->GetArrayField(TEXT("spawn_groups")))
    {
        const auto Group = Value->AsObject();
        const FString Id = Group->GetStringField(TEXT("group_id"));
        if (Id == TEXT("boar_farm_edge") || Id == TEXT("boar_forest_edge"))
        {
            if (Group->GetStringField(TEXT("spawn_kind")) != TEXT("npc") ||
                Group->GetArrayField(TEXT("definitions"))[0]->AsObject()->GetStringField(TEXT("definition_id")) != TEXT("npc/boar")) return false;
            Groups.Add(Id, { ReadPolygon(Group->GetArrayField(TEXT("area"))) });
        }
    }
    for (const auto& Value : Zone->GetArrayField(TEXT("areas")))
    {
        const auto Area = Value->AsObject();
        if (Area->GetStringField(TEXT("safety")) == TEXT("protected"))
        { ProtectedAreas.Add(ReadPolygon(Area->GetArrayField(TEXT("polygon")))); }
    }
    TSet<FString> LevelTwoAreas;
    for (const auto& Value : Zone->GetArrayField(TEXT("level_areas")))
    {
        const auto Area = Value->AsObject();
        if (Area->GetIntegerField(TEXT("content_level")) == 2)
        { LevelTwoAreas.Add(Area->GetStringField(TEXT("area_id"))); }
    }
    for (const auto& Value : Zone->GetArrayField(TEXT("respawn_points")))
    { RespawnPoints.Add(ZonePoint(Value->AsObject()->GetArrayField(TEXT("position")))); }
    return Groups.Num() == 2 && LevelTwoAreas.Contains(TEXT("boar_farm_edge")) &&
        LevelTwoAreas.Contains(TEXT("boar_forest_edge")) && ProtectedAreas.Num() > 0 && RespawnPoints.Num() > 0;
}

void ASFGameMode::StartCombat()
{
    // GM-01: on wherever the zone record has boar groups (Silverbrook), on any authority (dedicated, listen, standalone).
    bPilotActive = GetNetMode() != NM_Client &&
        GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bScenario = bPilotActive && (FParse::Param(FCommandLine::Get(), TEXT("CombatPilotScenario")) ||
        FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario")) ||
        FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")));
#endif
    if (!bPilotActive) { return; }
    CapturedSettings = GetDefault<USFCombatPilotSettings>();
    FString Error;
    if (!CapturedSettings->Validate(Error) || !LoadZoneRecord())
    { UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_DISABLED Invalid settings or zone record: %s"), *Error); bPilotActive = false; return; }
    bSlotServiceMode = GetDefault<USFGameplayServicesSettings>()->bEnabled;
    if (!bSlotServiceMode)
    {
        SlotStatePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CombatPilot/boar-slots.json"));
        IFileManager::Get().Delete(*SlotStatePath);
    }
    bSlotBridgeHealthy = RunWorldSlots(TEXT("tick"));
    if (!bSlotBridgeHealthy && !bSlotServiceMode) { bPilotActive = false; UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_DISABLED WorldContent bridge failed")); return; }
    if (!bSlotServiceMode)
    {
        bSlotReadyLogged = true;
        UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_READY Boars=%d Combat=%d HostileSpawning=%d"), ActiveBoars.Num(), CapturedSettings->bCombatAvailable, CapturedSettings->bSilverbrookHostileSpawning);
    }
}

void ASFGameMode::PostLoginCombat(APlayerController* NewPlayer)
{
    if (bPilotActive && !USFBackpackComponent::Services()) { SFCombatPilotRewards::AddPilotBag(NewPlayer); }
    if (!bPilotActive || !bScenario ||
        (!FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")) && GetNumPlayers() != 1)) { return; }
    const TWeakObjectPtr<APlayerController> Weak(NewPlayer);
    const bool bCapacitySlots = FParse::Param(FCommandLine::Get(), TEXT("CombatGroupCapacitySlots"));
    const int32 CapacitySlot = M3NextConnectionId;
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, Weak, bCapacitySlots, CapacitySlot]()
    {
        if (!Weak.IsValid()) { return; }
        auto* Player = Cast<ASFE1Character>(Weak->GetPawn());
        auto* Boar = ActiveBoars.FindRef(TEXT("boar_farm_edge:0")).Get();
        if (!Player || !Boar) { return; }
        if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")))
        {
            const int32 PlayerIndex = GetNumPlayers();
            const double Angle = 2.0 * PI * double(CapacitySlot - 1) / 12.0;
            const FVector Offset = bCapacitySlots
                ? FVector(180.0 * FMath::Cos(Angle), 180.0 * FMath::Sin(Angle), 0)
                : (PlayerIndex == 1 ? FVector(180,0,0) :
                    PlayerIndex == 2 ? FVector(180,180,0) : FVector(180,-180,0));
            const FVector Near = Grounded(GetWorld(), Boar->GetActorLocation()+Offset,
                Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            Player->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Display, TEXT("COMBAT_GROUP_READY Pawn=%s"), *Player->GetName());
            return;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario")))
        {
            ASFPilotBoar* Second = nullptr;
            double Nearest = TNumericLimits<double>::Max();
            for (const auto& Pair : ActiveBoars)
            {
                if (!Pair.Key.StartsWith(TEXT("boar_farm_edge:"))) { continue; }
                ASFPilotBoar* Candidate = Pair.Value.Get();
                if (!Candidate || Candidate == Boar) { continue; }
                const double Distance = FVector::DistSquared2D(Boar->GetActorLocation(), Candidate->GetActorLocation());
                if (Distance < Nearest) { Nearest = Distance; Second = Candidate; }
            }
            if (!Second) { return; }
            const FVector Mid = (Boar->GetActorLocation() + Second->GetActorLocation()) * 0.5;
            const FVector Side = FVector::CrossProduct(FVector::UpVector,
                (Second->GetActorLocation() - Boar->GetActorLocation()).GetSafeNormal2D()) * 1500.0;
            FVector Where = Mid + Side;
            if (!IsInsideTerritory(Where, Groups[TEXT("boar_farm_edge")].Polygon) || IsSafe(Where)) { Where = Mid - Side; }
            Where = Grounded(GetWorld(), Where, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            Player->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Display, TEXT("TARGET_PILOT_READY First=%s Second=%s"), *Boar->GetName(), *Second->GetName());
            return;
        }
        const bool bUiEvidence = FParse::Param(FCommandLine::Get(), TEXT("UiEvidenceHud")) || FParse::Param(FCommandLine::Get(), TEXT("UiEvidenceDefeat"));
        const FVector Near = Grounded(GetWorld(), Boar->GetActorLocation()+FVector(bUiEvidence ? 600 : 180,0,0), Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        Player->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_SCENARIO_START Pawn=%s"), *Player->GetName());
    }), 2.0f, false);
}

void ASFGameMode::RestartCombat(AController* NewPlayer)
{
    if (bPilotActive)
    { if (auto* Player = Cast<ASFE1Character>(NewPlayer ? NewPlayer->GetPawn() : nullptr)) Player->CombatSetHealth(CapturedSettings->PlayerHealth); }
}

void ASFGameMode::TickCombat()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bPilotActive && !bReconnectDeathTriggered && FParse::Param(FCommandLine::Get(), TEXT("ReconnectDeathScenario")))
    {
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* Controller = It->Get();
            auto* Pack = Controller ? Controller->FindComponentByClass<USFBackpackComponent>() : nullptr;
            const auto* Gathering = Controller ? Controller->FindComponentByClass<USFGatheringPilotComponent>() : nullptr;
            auto* Player = Controller ? Cast<ASFE1Character>(Controller->GetPawn()) : nullptr;
            if (!Pack || Pack->GetCharacterId() != TEXT("chr_dev_alpha") || !Gathering || !Player ||
                Gathering->GetSkillSnapshot().Get(ESFGatheringSkill::Logging) < 1 || Player->CombatHealth() <= 0 ||
                USFCharacterSessionComponent::IsRestoring(Controller)) continue;
            bReconnectDeathTriggered = true;
            UE_LOG(LogTemp, Display, TEXT("RECONNECT_DEATH_TRIGGERED Character=chr_dev_alpha Logging=%d Face=%s"),
                Gathering->GetSkillSnapshot().Get(ESFGatheringSkill::Logging), *Player->GetFaceId().ToString());
            HitPlayer(nullptr, Player, 100000);
            break;
        }
    }
#endif
    if (!bPilotActive || !CapturedSettings->bSilverbrookHostileSpawning || (!bSlotServiceMode && !bSlotBridgeHealthy)) { return; }
    if (GetWorld()->GetTimeSeconds() >= NextSlotTick)
    { NextSlotTick = GetWorld()->GetTimeSeconds() + 2.0; bSlotBridgeHealthy = RunWorldSlots(TEXT("tick")); }
}

bool ASFGameMode::RunWorldSlots(const FString& Command, const FString& Group, int32 Slot)
{
    if (bSlotServiceMode)
    {
        if (Command == TEXT("defeat")) { PendingSlotDefeats.Emplace(Group, Slot); }
        if (bSlotRequestInFlight) { return true; }
        const auto Client = USFBackpackComponent::Services();
        if (!Client) { UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_SLOT_ERROR Gameplay Services client missing")); return false; }
        FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::AdvanceWorldSlots;
        Request.bHostileSpawning = CapturedSettings->bSilverbrookHostileSpawning;
        if (PendingSlotDefeats.Num())
        {
            Request.SlotCommand = TEXT("defeat"); Request.SlotGroupId = PendingSlotDefeats[0].Key;
            Request.SlotIndex = PendingSlotDefeats[0].Value; Request.PlayersInArea = GetNumPlayers();
        }
        else { Request.SlotCommand = TEXT("tick"); }
        bSlotRequestInFlight = true;
        Client->Call(Request, [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Request](const FSFGameplayResponse& Reply)
        {
            ASFGameMode* Mode = WeakMode.Get();
            if (!Mode) { return; }
            Mode->bSlotRequestInFlight = false;
            if (Reply.Status != ESFGameplayStatus::Ok || Reply.BoarSlots.Num() != 10)
            {
                Mode->bSlotBridgeHealthy = false;
                UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_SLOT_ERROR Gameplay.Host Code=%d Slots=%d"), Reply.GrpcCode, Reply.BoarSlots.Num());
                return;
            }
            Mode->bSlotBridgeHealthy = true;
            if (Request.SlotCommand == TEXT("defeat") && Mode->PendingSlotDefeats.Num())
            { Mode->PendingSlotDefeats.RemoveAt(0); }
            for (const auto& Row : Reply.BoarSlots)
            {
                // An older poll must not recreate a boar while its defeat is queued.
                const bool bDefeatPending = Mode->PendingSlotDefeats.ContainsByPredicate([&Row](const TPair<FString, int32>& Pending)
                { return Pending.Key == Row.GroupId && Pending.Value == Row.SlotIndex; });
                if (Row.bActive && !bDefeatPending && Mode->CapturedSettings->bSilverbrookHostileSpawning && Mode->Groups.Contains(Row.GroupId))
                { Mode->SpawnSlot(Row.GroupId, Row.SlotIndex, Row.Cycle, Row.PointId,
                    Row.XMetres, Row.YMetres, Row.FacingDegrees); }
            }
            if (!Mode->bSlotReadyLogged)
            {
                Mode->bSlotReadyLogged = true;
                UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_READY Boars=%d Combat=%d HostileSpawning=%d"),
                    Mode->ActiveBoars.Num(), Mode->CapturedSettings->bCombatAvailable, Mode->CapturedSettings->bSilverbrookHostileSpawning);
            }
            if (Mode->PendingSlotDefeats.Num()) { Mode->RunWorldSlots(TEXT("tick")); }
        });
        return true;
    }
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TEXT(".."));
    const FString Dotnet = FPaths::Combine(Root, TEXT("Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe"));
    const FString Dll = FPaths::Combine(Root, TEXT("BackendServices/WorldContent.Tool/bin/Debug/net10.0/WorldContent.Tool.dll"));
    const FString Zone = FPaths::Combine(Root, TEXT("content/world/zones/greenlands_silverbrook.json"));
    if (!FPaths::FileExists(Dotnet) || !FPaths::FileExists(Dll)) { return false; }
    FString Args = FString::Printf(TEXT("\"%s\" combat-pilot \"%s\" \"%s\" %s"), *Dll, *Zone, *SlotStatePath, *Command);
    if (Command == TEXT("defeat"))
    { Args += FString::Printf(TEXT(" %s %d %d"), *Group, Slot, GetNumPlayers()); }
    Args += CapturedSettings->bSilverbrookHostileSpawning ? TEXT(" true") : TEXT(" false");
    int32 Code = -1; FString Stdout, Stderr;
    FPlatformProcess::ExecProcess(*Dotnet, *Args, &Code, &Stdout, &Stderr);
    if (Code != 0) { UE_LOG(LogTemp, Error, TEXT("COMBAT_PILOT_SLOT_ERROR Code=%d %s"), Code, *Stderr); return false; }
    TArray<FString> Lines; Stdout.ParseIntoArrayLines(Lines);
    for (const FString& Line : Lines)
    {
        TArray<FString> Fields; Line.ParseIntoArray(Fields, TEXT("|"), false);
        if (Fields.Num() != 9 || Fields[0] != TEXT("SLOT") || !Groups.Contains(Fields[1])) { continue; }
        if (Fields[4] == TEXT("Active") && CapturedSettings->bSilverbrookHostileSpawning)
        { SpawnSlot(Fields[1], FCString::Atoi(*Fields[2]), FCString::Atoi(*Fields[3]), Fields[5],
            FCString::Atod(*Fields[6]), FCString::Atod(*Fields[7]), FCString::Atod(*Fields[8])); }
    }
    return true;
}

void ASFGameMode::SpawnSlot(const FString& Group, int32 Slot, int32 Cycle, const FString& PointId,
    double X, double Y, double Facing)
{
    const FString Key = FString::Printf(TEXT("%s:%d"), *Group, Slot);
    if (ActiveBoars.FindRef(Key).IsValid() || PointId == TEXT("-")) { return; }
    const FVector Where = Grounded(GetWorld(), FVector(X*100,Y*100,0), 65);
    if (IsSafe(Where) || !IsInsideTerritory(Where, Groups[Group].Polygon)) { return; }
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    auto* Boar = GetWorld()->SpawnActor<ASFPilotBoar>(Where, FRotator(0,Facing,0), Params);
    if (!Boar) { return; }
    Boar->Initialize(this, Group, Slot, Groups[Group].Polygon, CapturedSettings->BoarHealth, ++NextBoarSerial);
    ActiveBoars.Add(Key, Boar);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_SPAWN Group=%s Slot=%d Cycle=%d Point=%s"), *Group, Slot, Cycle, *PointId);
}

void ASFGameMode::TryPlayerAttack(ASFE1Character* Attacker, AActor* SelectedTarget)
{
    if (!bPilotActive || !CapturedSettings->bCombatAvailable || !IsValid(Attacker) || Attacker->CombatHealth() <= 0 ||
        USFCharacterSessionComponent::IsRestoring(Attacker->GetController()) || IsSafe(Attacker->GetActorLocation())) { return; }
    SelectedTarget = Attacker->GetCombatTarget();
    if (SelectedTarget && FVector::Dist2D(Attacker->GetActorLocation(), SelectedTarget->GetActorLocation()) > CapturedSettings->MeleeRangeMetres * 100.0)
    {
        if (ASFPlayerController* PC = Cast<ASFPlayerController>(Attacker->GetController())) { PC->ClientPilotHit(TEXT("Out of range")); }
        return;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now < PlayerNextSwing.FindRef(Attacker)) { return; }
    ASFPilotBoar* Best = nullptr; double BestDistance = CapturedSettings->MeleeRangeMetres * 100;
    const FVector Facing = Attacker->GetActorForwardVector().GetSafeNormal2D();
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
    {
        if (!IsValid(*It) || It->IsActorBeingDestroyed() || It->Health() <= 0 || IsSafe(It->GetActorLocation())
            || (SelectedTarget && SelectedTarget != *It)) { continue; }
        const FVector Delta = It->GetActorLocation() - Attacker->GetActorLocation();
        const double Distance = Delta.Size2D();
        if (!SFCombatPilotRules::CanMelee(Distance, SelectedTarget ? 1.0 : FVector::DotProduct(Facing,Delta.GetSafeNormal2D()),
            Now, PlayerNextSwing.FindRef(Attacker), BestDistance)) { continue; }
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatPilotMelee), false, Attacker);
        if (GetWorld()->LineTraceSingleByChannel(Hit, Attacker->GetActorLocation()+FVector(0,0,45),
            It->GetActorLocation()+FVector(0,0,45), ECC_Visibility, Query) && Hit.GetActor() != *It) { continue; }
        Best = *It; BestDistance = Distance;
    }
    if (!Best) { return; }
    PlayerNextSwing.Add(Attacker, Now + CapturedSettings->PlayerMeleeCooldownSeconds);
    Best->ReceivePilotDamage(Attacker, CapturedSettings->PlayerMeleeDamage + SFGear::Equipped(Attacker->GetController()).Damage);
}

void ASFGameMode::TryBoarAttack(ASFPilotBoar* Boar, ASFE1Character* Victim)
{
    if (!bPilotActive || !CapturedSettings->bCombatAvailable || !IsValid(Boar) || !IsValid(Victim) ||
        Boar->Health() <= 0 || Victim->CombatHealth() <= 0 || IsSafe(Boar->GetActorLocation()) ||
        IsSafe(Victim->GetActorLocation()) ||
        FVector::Dist2D(Boar->GetActorLocation(), Victim->GetActorLocation()) > CapturedSettings->MeleeRangeMetres*100) return;
    HitPlayer(Boar, Victim, CapturedSettings->BoarMeleeDamage);
}

void ASFGameMode::HitPlayer(AActor* Source, ASFE1Character* Victim, int32 Damage)
{
    if (!IsValid(Victim) || !Victim->HasAuthority() || Victim->CombatHealth() <= 0 ||
        USFCharacterSessionComponent::IsRestoring(Victim->GetController())) return;
    Damage = SFGear::MitigatedDamage(Damage, SFGear::Equipped(Victim->GetController()).Armor);
    Victim->CombatSetHealth(SFCombatPilotRules::RemainingHealth(Victim->CombatHealth(), Damage));
    if (ASFPlayerController* PC = Cast<ASFPlayerController>(Victim->GetController())) { PC->ClientPilotHit(TEXT("Hit")); }
    if (!Victim->GetCombatTarget() && Victim->CombatHealth() > 0) { Victim->SetCombatTarget(Source); }
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_PLAYER_HIT HP=%d By=%s"), Victim->CombatHealth(), *GetNameSafe(Source));
    if (Victim->CombatHealth() == 0)
    {
        Victim->GetCharacterMovement()->DisableMovement();
        UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_PLAYER_DEAD Pawn=%s"), *Victim->GetName());
        ASFGameMode* Mode = Victim->GetWorld()->GetAuthGameMode<ASFGameMode>();
        if (!Mode || !Mode->bPilotActive) { return; }
        const auto Weak = TWeakObjectPtr<ASFE1Character>(Victim);
        ASFPlayerController* Controller = Cast<ASFPlayerController>(Victim->GetController());
        USFBackpackComponent* Pack = Controller ? USFBackpackComponent::FindOrCreate(Controller) : nullptr;
        if (USFBackpackComponent::Services() && Pack && Pack->HasDevIdentity())
        {
            FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::ReportDeath;
            Request.DeathEventId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
            Request.CharacterId = Pack->GetCharacterId(); Request.DeathAtUnixMs = UnixNowMs();
            Request.ZoneId = FPackageName::GetShortName(Victim->GetWorld()->GetOutermost()->GetName()).ToLower();
            Request.Position = Victim->GetActorLocation(); Request.RespawnPosition = Mode->ChooseRespawnPoint(Request.Position);
            Request.RespawnDelayMs = FMath::RoundToInt(Mode->CapturedSettings->PlayerRespawnDelaySeconds * 1000);
            Mode->PlayerDeathEvents.Add(Weak, Request.DeathEventId);
            Mode->PlayerDeathRespawnPoints.Add(Weak, Request.RespawnPosition);
            Mode->PendingDeathReports.Add(Request.CharacterId);
            Mode->ReportPlayerDeath(Weak, Request);
        }
        else
        {
            FTimerHandle Timer;
            Mode->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(Mode,
                [Mode, Weak]() { Mode->RespawnPlayer(Weak); }), Mode->CapturedSettings->PlayerRespawnDelaySeconds, false);
        }
    }
}

FVector ASFGameMode::ChooseRespawnPoint(const FVector& DeathLocation) const
{
    if (RespawnPoints.IsEmpty()) return DeathLocation;
    FVector Nearest = RespawnPoints[0];
    if (CapturedSettings->PlayerRespawnPointRule == ESFPlayerRespawnPointRule::NearestToDeath)
    {
        double Distance = FVector::DistSquared2D(DeathLocation, Nearest);
        for (const FVector& Point : RespawnPoints)
        { const double D = FVector::DistSquared2D(DeathLocation, Point); if (D < Distance) { Nearest = Point; Distance = D; } }
    }
    return Nearest;
}

void ASFGameMode::ReportPlayerDeath(TWeakObjectPtr<ASFE1Character> Player, const FSFGameplayRequest& Request)
{
    if (!USFBackpackComponent::Services()) return;
    USFBackpackComponent::Services()->Call(Request, [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Player, Request](const FSFGameplayResponse& Reply)
    {
        ASFGameMode* Mode = WeakMode.Get();
        if (!Mode || !Mode->PendingDeathReports.Contains(Request.CharacterId)) return;
        if (Reply.Status != ESFGameplayStatus::Ok)
        {
            UE_LOG(LogTemp, Warning, TEXT("COMBAT_DEATH_REPORT_RETRY Character=%s Code=%d"), *Request.CharacterId, Reply.GrpcCode);
            FTimerHandle Retry;
            Mode->GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(Mode,
                [WeakMode, Player, Request]() { if (ASFGameMode* M = WeakMode.Get()) M->ReportPlayerDeath(Player, Request); }), 3.0f, false);
            return;
        }
        Mode->PendingDeathReports.Remove(Request.CharacterId);
        if (!Player.IsValid())
        { Mode->PlayerDeathEvents.Remove(Player); Mode->PlayerDeathRespawnPoints.Remove(Player); return; }
        if (Mode->PlayerDeathEvents.FindRef(Player) != Request.DeathEventId) return;
        const int64 RemainingMs = FMath::Max<int64>(0, Reply.RespawnAtUnixMs - UnixNowMs());
        UE_LOG(LogTemp, Display, TEXT("COMBAT_DEATH_RECORDED Character=%s Event=%s RemainingMs=%lld"), *Request.CharacterId, *Request.DeathEventId, RemainingMs);
        FTimerHandle Timer;
        Mode->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(Mode,
            [WeakMode, Player]() { if (ASFGameMode* M = WeakMode.Get()) M->RespawnPlayer(Player); }), FMath::Max(0.01, RemainingMs / 1000.0), false);
    });
}

bool ASFGameMode::HasPendingDeathReport(const FString& CharacterId) const
{
    return PendingDeathReports.Contains(CharacterId);
}

void ASFGameMode::RestoreDeadPlayer(ASFE1Character* Player, const FSFGameplayResponse& State)
{
    if (!bPilotActive || !IsValid(Player) || !State.bHasDeath || !State.bIsDead ||
        State.DeathZoneId != FPackageName::GetShortName(GetWorld()->GetOutermost()->GetName()).ToLower() ||
        State.DeathPosition.ContainsNaN() || State.RespawnPosition.ContainsNaN()) return;
    const auto Weak = TWeakObjectPtr<ASFE1Character>(Player);
    Player->SetActorLocation(State.DeathPosition, false, nullptr, ETeleportType::TeleportPhysics);
    Player->CombatSetHealth(0); Player->GetCharacterMovement()->DisableMovement();
    PlayerDeathEvents.Add(Weak, State.DeathEventId); PlayerDeathRespawnPoints.Add(Weak, State.RespawnPosition);
    const int64 RemainingMs = FMath::Max<int64>(0, State.RespawnAtUnixMs - UnixNowMs());
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this,
        [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Weak]() { if (ASFGameMode* M = WeakMode.Get()) M->RespawnPlayer(Weak); }), FMath::Max(0.01, RemainingMs / 1000.0), false);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_DEATH_RESTORED Event=%s RemainingMs=%lld"), *State.DeathEventId, RemainingMs);
}

void ASFGameMode::RespawnPlayer(TWeakObjectPtr<ASFE1Character> Player)
{
    if (!Player.IsValid())
    { PlayerDeathEvents.Remove(Player); PlayerDeathRespawnPoints.Remove(Player); PlayerRespawnInFlight.Remove(Player); return; }
    if (!bPilotActive || Player->CombatHealth() != 0 || RespawnPoints.IsEmpty()) { return; }
    const FVector Nearest = PlayerDeathRespawnPoints.Contains(Player) ? PlayerDeathRespawnPoints.FindRef(Player) : ChooseRespawnPoint(Player->GetActorLocation());
    if (const FString* EventId = PlayerDeathEvents.Find(Player))
    {
        if (PlayerRespawnInFlight.Contains(Player)) return;
        ASFPlayerController* Controller = Cast<ASFPlayerController>(Player->GetController());
        USFBackpackComponent* Pack = Controller ? USFBackpackComponent::FindOrCreate(Controller) : nullptr;
        if (!Pack || !USFBackpackComponent::Services()) return;
        FSFGameplayRequest Clear; Clear.Rpc = ESFGameplayRpc::ClearDeath; Clear.CharacterId = Pack->GetCharacterId(); Clear.DeathEventId = *EventId;
        PlayerRespawnInFlight.Add(Player);
        USFBackpackComponent::Services()->Call(Clear, [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Player, Clear](const FSFGameplayResponse& Reply)
        {
            ASFGameMode* Mode = WeakMode.Get(); if (!Mode) return;
            Mode->PlayerRespawnInFlight.Remove(Player);
            if (!Player.IsValid() || Mode->PlayerDeathEvents.FindRef(Player) != Clear.DeathEventId) return;
            if (Reply.Status == ESFGameplayStatus::Ok && !Reply.bIsDead)
            {
                Mode->PlayerDeathEvents.Remove(Player);
                Mode->RespawnPlayer(Player);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("COMBAT_DEATH_CLEAR_RETRY Character=%s Code=%d"), *Clear.CharacterId, Reply.GrpcCode);
                FTimerHandle Retry; Mode->GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(Mode,
                    [WeakMode, Player]() { if (ASFGameMode* M = WeakMode.Get()) M->RespawnPlayer(Player); }), 3.0f, false);
            }
        });
        return;
    }
    const FVector At = Grounded(GetWorld(), Nearest, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Player->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->CombatSetHealth(CapturedSettings->PlayerHealth);
    PlayerDeathRespawnPoints.Remove(Player);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_PLAYER_RESPAWN X=%.0f Y=%.0f HP=%d"), At.X,At.Y,Player->CombatHealth());
    if (bScenario)
    {
        const FVector Town = Grounded(GetWorld(), FVector(0,-3000,0), Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* FixturePlayer = GetWorld()->SpawnActor<ASFE1Character>(Town, FRotator::ZeroRotator, Spawn);
        auto* FixtureBoar = GetWorld()->SpawnActor<ASFPilotBoar>(Town+FVector(150,0,0), FRotator::ZeroRotator, Spawn);
        if (FixturePlayer && FixtureBoar)
        {
            FixturePlayer->CombatSetHealth(CapturedSettings->PlayerHealth);
            FixtureBoar->Initialize(this, TEXT("boar_farm_edge"), -1, Groups[TEXT("boar_farm_edge")].Polygon, CapturedSettings->BoarHealth, ++NextBoarSerial);
            const int32 Before = FixturePlayer->CombatHealth();
            TryBoarAttack(FixtureBoar, FixturePlayer);
            TryPlayerAttack(FixturePlayer);
            // A kill credited inside the town (a replayed or forged death) grants nothing.
            const int32 TownReward = RewardKill(FixtureBoar->DeathId(), Town, Player.Get());
            UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_SAFE_CHECK Protected=%d PlayerHP=%d BoarHP=%d Reward=%d Passed=%d"),
                IsSafe(Town), FixturePlayer->CombatHealth(), FixtureBoar->Health(), TownReward,
                IsSafe(Town) && Before == FixturePlayer->CombatHealth() && FixtureBoar->Health() == CapturedSettings->BoarHealth && TownReward == 0);
        }
        if (FixtureBoar) { FixtureBoar->Destroy(); }
        if (FixturePlayer) { FixturePlayer->Destroy(); }
    }
}

int32 ASFGameMode::RewardKill(const FString& DeathId, const FVector& BoarAt, ASFE1Character* Killer)
{
    if (!CapturedSettings || !HasAuthority()) { return 0; }
    const bool bProtected = IsSafe(BoarAt) || !IsValid(Killer) || IsSafe(Killer->GetActorLocation());
    auto* KillerController = IsValid(Killer) ? Cast<APlayerController>(Killer->GetController()) : nullptr;
    if (USFBackpackComponent::Services())
    {
        bool bSeen = false; RewardedDeaths.Add(DeathId, &bSeen);
        if (bSeen) { return 0; }
        USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(KillerController);
        const bool bKillerSafe = !IsValid(Killer) || IsSafe(Killer->GetActorLocation()), bBoarSafe = IsSafe(BoarAt);
        // The process nonce prevents a restarted server's slot serials from reusing a durable defeat ID.
        static const FString Epoch = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        RetryDefeatReward(DeathId, Epoch + TEXT(":") + DeathId, Backpack ? Backpack->GetCharacterId() : FString(),
            Backpack ? Backpack->GetContainerId() : FString(), bKillerSafe, bBoarSafe, KillerController);
        return 0;
    }
    const int32 Hides = SFCombatPilotRewards::HidesFor(RewardedDeaths, DeathId, bProtected, CapturedSettings->HidesPerBoar);
    AActor* Bag = IsValid(Killer) ? Killer->GetController() : nullptr;
    const bool bGranted = Hides > 0 && SFCombatPilotRewards::GrantHides(Bag, Hides);
    if (Hides > 0) { SFQuestPilotEvents::OnBoarDefeated().Broadcast(Cast<APlayerController>(Bag), DeathId); }  // credited once per death
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_REWARD Death=%s Hides=%d Granted=%d Total=%d"),
        *DeathId, Hides, bGranted, SFCombatPilotRewards::CountHides(Bag));
    return bGranted ? Hides : 0;
}

void ASFGameMode::RetryDefeatReward(const FString& DeathId, const FString& ServiceEventId, const FString& CharacterId,
    const FString& ContainerId, bool bKillerSafe, bool bBoarSafe, TWeakObjectPtr<APlayerController> Controller)
{
    USFBackpackComponent* Backpack = Controller.IsValid() ? USFBackpackComponent::FindOrCreate(Controller.Get()) : nullptr;
    if (!Backpack)
    {
        UE_LOG(LogTemp, Warning, TEXT("COMBAT_PILOT_REWARD_HELD Death=%s Reason=BackpackUnavailable"), *DeathId);
        if (Controller.IsValid())
        {
            FTimerHandle Retry;
            GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this,
                [WeakMode = TWeakObjectPtr<ASFGameMode>(this), DeathId, ServiceEventId, CharacterId, ContainerId,
                 bKillerSafe, bBoarSafe, Controller]()
                { if (ASFGameMode* Mode = WeakMode.Get()) Mode->RetryDefeatReward(DeathId, ServiceEventId, CharacterId, ContainerId, bKillerSafe, bBoarSafe, Controller); }), 3.0f, false);
        }
        return;
    }
    if ((!CharacterId.IsEmpty() && Backpack->GetCharacterId() != CharacterId) ||
        (!ContainerId.IsEmpty() && Backpack->GetContainerId() != ContainerId)) return;
    const FString RecipientId = Backpack->GetCharacterId(), RecipientContainer = Backpack->GetContainerId();
    auto Receipt = MakeShared<FSFGameplayResponse>();
    FSFBackpackMutation Mutation; Mutation.Label = TEXT("defeat_reward"); Mutation.OperationId = ServiceEventId;
    Mutation.bRetryStale = false;
    Mutation.Run = [Receipt, ServiceEventId, RecipientId, RecipientContainer, bKillerSafe, bBoarSafe](const FString&, int64 Expected,
        TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        FSFGameplayRequest Q = SFCombatPilotRewards::BuildDefeatRequest(ServiceEventId, RecipientId,
            RecipientContainer, Expected, bKillerSafe, bBoarSafe);
        USFBackpackComponent::Services()->Call(Q, [Receipt, Report](const FSFGameplayResponse& R)
        {
            *Receipt = R;
            Report(R.Status == ESFGameplayStatus::Ok ? ESFBackpackOutcome::Committed :
                R.Status == ESFGameplayStatus::Unavailable ? ESFBackpackOutcome::Unavailable :
                R.GrpcCode == 9 ? ESFBackpackOutcome::Stale : ESFBackpackOutcome::Rejected,
                R.Status == ESFGameplayStatus::Ok && R.bGranted ? R.ContainerRevision : -1);
        });
    };
    Mutation.Finished = [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Receipt, DeathId, ServiceEventId,
        RecipientId, RecipientContainer, bKillerSafe, bBoarSafe, Controller, Weak = TWeakObjectPtr<USFBackpackComponent>(Backpack)](ESFBackpackOutcome Outcome)
    {
        ASFGameMode* Mode = WeakMode.Get(); if (!Mode) return;
        const bool bGranted = Outcome == ESFBackpackOutcome::Committed && Receipt->bGranted;
        UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_REWARD Death=%s Source=service Outcome=%d Granted=%d Code=%s Item=%s Quantity=%lld"),
            *DeathId, int32(Outcome), bGranted, *Receipt->SourceCode, *Receipt->DefinitionId, Receipt->Quantity);
        if (bGranted && FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")))
        {
            FTimerHandle MetricsTimer;
            Mode->GetWorldTimerManager().SetTimer(MetricsTimer, FTimerDelegate::CreateWeakLambda(Mode,
                [WeakMode]() { if (ASFGameMode* M = WeakMode.Get()) { M->FinishM3Capture(); } }), 3.0f, false);
        }
        if (bGranted)
        {
            if (USFBackpackComponent* Owner = Weak.Get()) { Owner->ApplyToBag({{Receipt->DefinitionId, Receipt->Quantity}}); }
            SFQuestPilotEvents::OnBoarDefeated().Broadcast(Controller.Get(), DeathId);
        }
        else if (Outcome == ESFBackpackOutcome::Unavailable || Outcome == ESFBackpackOutcome::Stale)
        {
            UE_LOG(LogTemp, Warning, TEXT("COMBAT_PILOT_REWARD_HELD Death=%s Code=%d"), *DeathId, Receipt->GrpcCode);
            FTimerHandle Retry;
            Mode->GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(Mode,
                [WeakMode, DeathId, ServiceEventId, RecipientId, RecipientContainer, bKillerSafe, bBoarSafe, Controller]()
                { if (ASFGameMode* M = WeakMode.Get()) M->RetryDefeatReward(DeathId, ServiceEventId, RecipientId, RecipientContainer, bKillerSafe, bBoarSafe, Controller); }), 3.0f, false);
        }
    };
    Backpack->Enqueue(MoveTemp(Mutation));
}

void ASFGameMode::BoarDied(ASFPilotBoar* Boar, ASFE1Character* Killer)
{
    if (!bPilotActive || !IsValid(Boar)) { return; }
    const FString Key = FString::Printf(TEXT("%s:%d"), *Boar->GroupId(), Boar->SlotIndex());
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_DEAD Group=%s Slot=%d"), *Boar->GroupId(), Boar->SlotIndex());
    RewardKill(Boar->DeathId(), Boar->GetActorLocation(), Killer);
    bSlotBridgeHealthy = RunWorldSlots(TEXT("defeat"), Boar->GroupId(), Boar->SlotIndex());
    ActiveBoars.Remove(Key);
    Boar->Destroy();
    if (bScenario && !FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")) && !bScenarioFirstBoarDied)
    {
        bScenarioFirstBoarDied = true;
        FTimerHandle Timer;
        GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this]()
        {
            auto* Other = ActiveBoars.FindRef(TEXT("boar_farm_edge:1")).Get();
            auto* PC = GetWorld()->GetFirstPlayerController();
            auto* Player = Cast<ASFE1Character>(PC ? PC->GetPawn() : nullptr);
            if (!Other || !Player) { return; }
            const FVector Near = Grounded(GetWorld(), Other->GetActorLocation()+FVector(180,0,0), Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            Player->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_SCENARIO_BOAR_AGGRO"));
        }), 2.0f, false);
    }
}
