#include "Combat/SFCombatPilot.h"
#include "Combat/SFCommonRespawn.h"
#include "Combat/SFSpiritReleaseRules.h"
#include "Combat/SFResurrectionAcceptRules.h"
#include "Combat/SFFallDamage.h"
#include "Combat/SFTauntRules.h"
#include "Combat/SFBoarDodgeRules.h"
#include "Inventory/SFGear.h"
#include "Framework/SFGameMode.h"
#include "Combat/SFCombatPilotRewards.h"
#include "Characters/SFE1Character.h"
#include "Creatures/SFRestAreaVolume.h"
#include "Framework/SFPlayerController.h"
#include "Party/SFPartySettings.h"
#include "Gathering/SFGatheringPilot.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
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

FString ThreatIdentity(APlayerController* Controller)
{
    if (!IsValid(Controller) || !Controller->HasAuthority()) { return {}; }
    if (USFBackpackComponent* Bag = USFBackpackComponent::FindOrCreate(Controller))
    { return Bag->GetCharacterId(); }
    // The service-disabled combat pilot has no durable character identity. Its threat
    // table is session-only; the authority's distinct controller name is stable for it.
    return TEXT("chr_session_") + Controller->GetName();
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

bool SFCombatPilotRules::LeashAbuseSignal(const TArray<double>& EvadeTimes, double Now)
{
    int32 Recent = 0;
    for (double Time : EvadeTimes) { if (Time >= Now - 120.0 && Time <= Now) { ++Recent; } }
    return Recent == 3;
}

double SFCombatPilotRules::AggroRadiusMetres(double BaseRadius, int32 NpcLevel,
    int32 CharacterLevel, double MinimumRadius, double MaximumBonus)
{
    return FMath::Clamp(BaseRadius - (CharacterLevel - NpcLevel), MinimumRadius,
        BaseRadius + MaximumBonus);
}

bool SFCombatPilotRules::DetectBySight(double DistanceMetres, double FacingDot,
    bool bHasLineOfSight, double AggroRadius, double ProximityRadius, double FieldOfViewDegrees)
{
    return bHasLineOfSight && DistanceMetres <= AggroRadius &&
        (DistanceMetres <= ProximityRadius || FacingDot >=
            FMath::Cos(FMath::DegreesToRadians(FieldOfViewDegrees * 0.5)));
}

void FSFBoarThreatTable::Add(const FString& CharacterId, double Amount, int32 MaxEntries)
{
    if (CharacterId.IsEmpty() || !FMath::IsFinite(Amount) || Amount <= 0 || MaxEntries < 1) { return; }
    for (FEntry& Entry : Entries)
    { if (Entry.Id == CharacterId) { Entry.Threat += Amount; return; } }
    if (Entries.Num() >= MaxEntries)
    {
        int32 Lowest = 0;
        for (int32 I = 1; I < Entries.Num(); ++I)
        {
            if (Entries[I].Threat < Entries[Lowest].Threat ||
                (Entries[I].Threat == Entries[Lowest].Threat && Entries[I].Id > Entries[Lowest].Id)) { Lowest = I; }
        }
        Entries.RemoveAt(Lowest);
    }
    Entries.Add({CharacterId, Amount});
}

void FSFBoarThreatTable::Remove(const FString& CharacterId)
{ Entries.RemoveAll([&](const FEntry& Entry) { return Entry.Id == CharacterId; }); }

bool FSFBoarThreatTable::Contains(const FString& CharacterId) const
{ return Entries.ContainsByPredicate([&](const FEntry& Entry) { return Entry.Id == CharacterId; }); }

double FSFBoarThreatTable::Highest() const
{
    double Top = 0;
    for (const FEntry& Entry : Entries) { Top = FMath::Max(Top, Entry.Threat); }
    return Top;
}

double FSFBoarThreatTable::Get(const FString& CharacterId) const
{
    for (const FEntry& Entry : Entries) { if (Entry.Id == CharacterId) { return Entry.Threat; } }
    return 0;
}

void FSFBoarThreatTable::RaiseTo(const FString& CharacterId, double Amount, int32 MaxEntries)
{ Add(CharacterId, Amount - Get(CharacterId), MaxEntries); }

void FSFRecentAttackerMemory::RecordReset(const TSet<FString>& Attackers, double Now)
{
    for (const FString& Id : Attackers)
    { if (!Id.IsEmpty()) { ExpiresAt.Add(Id, Now + RetentionSeconds); } }
}

bool FSFRecentAttackerMemory::Contains(const FString& CharacterId, double Now) const
{
    const double* Deadline = ExpiresAt.Find(CharacterId);
    return Deadline && Now < *Deadline;
}

FString FSFBoarThreatTable::Select(const FString& CurrentId, const TMap<FString, double>& ReachableDistancesCm,
    double MeleeThreshold, double RangedThreshold) const
{
    const FEntry* Best = nullptr;
    const FEntry* Current = nullptr;
    for (const FEntry& Entry : Entries)
    {
        if (!ReachableDistancesCm.Contains(Entry.Id)) { continue; }
        if (Entry.Id == CurrentId) { Current = &Entry; }
        if (!Best || Entry.Threat > Best->Threat ||
            (Entry.Threat == Best->Threat && Entry.Id < Best->Id)) { Best = &Entry; }
    }
    if (!Best) { return {}; }
    if (!Current) { return Best->Id; }
    const FEntry* Qualified = nullptr;
    for (const FEntry& Entry : Entries)
    {
        if (Entry.Id == Current->Id) { continue; }
        const double* Distance = ReachableDistancesCm.Find(Entry.Id);
        if (!Distance) { continue; }
        const double Threshold = *Distance <= 500.0 ? MeleeThreshold : RangedThreshold;
        if (Entry.Threat <= Current->Threat * Threshold) { continue; }
        if (!Qualified || Entry.Threat > Qualified->Threat ||
            (Entry.Threat == Qualified->Threat && Entry.Id < Qualified->Id)) { Qualified = &Entry; }
    }
    return Qualified ? Qualified->Id : Current->Id;
}

void ASFGameMode::RecordBoarEvade(const FString& Group, const FString& CharacterId)
{
    if (!HasAuthority() || CharacterId.IsEmpty()) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    TArray<double>& Times = LeashEvadesByGroupAndCharacter.FindOrAdd(Group + TEXT(":") + CharacterId);
    Times.RemoveAll([Now](double Time) { return Time < Now - 120.0; });
    Times.Add(Now);
    if (SFCombatPilotRules::LeashAbuseSignal(Times, Now))
    {
        // Structured authority signal; anticheat-service transport is not present in this pilot.
        UE_LOG(LogTemp, Warning, TEXT("LEASH_ABUSE_SIGNAL Type=leash_abuse Group=%s Character=%s Evades=3 WindowSeconds=120"),
            *Group, *CharacterId);
    }
}

bool USFCombatPilotSettings::Validate(FString& Error) const
{
    if (PlayerHealth < 20 || PlayerHealth > 500 || BoarHealth < 10 || BoarHealth > 500 ||
        !FMath::IsWithinInclusive(FallSpeedThresholdMps, 0.0f, 50.0f) ||
        PlayerMeleeDamage < 1 || PlayerMeleeDamage > 100 || BoarMeleeDamage < 1 || BoarMeleeDamage > 100 ||
        !FMath::IsWithinInclusive(BoarDodgeChance, 0.0f, 0.5f) ||
        !FMath::IsWithinInclusive(BoarParryChance, 0.0f, 0.5f) ||
        !FMath::IsWithinInclusive(AvoidanceCap, 0.0f, 0.5f) ||
        !FMath::IsWithinInclusive(FrontArcDegrees, 1.0f, 360.0f) ||
        !FMath::IsWithinInclusive(MissCap, 0.0f, 0.30f) ||
        !FMath::IsWithinInclusive(CritCap, 0.0f, 0.60f) ||
        !FMath::IsWithinInclusive(CritDamageCap, 1.0f, 3.0f) ||
        !FMath::IsWithinInclusive(BlockCap, 0.0f, 0.75f) ||
        !FMath::IsWithinInclusive(BlockReductionCap, 0.0f, 0.40f) ||
        !FMath::IsWithinInclusive(ArmorKLevel, 1.0f, 100000.0f) ||
        !FMath::IsWithinInclusive(ArmorReductionCap, 0.0f, 0.85f) ||
        !FMath::IsWithinInclusive(ResistK, 1.0f, 100000.0f) ||
        !FMath::IsWithinInclusive(ResistReductionCap, 0.0f, 0.75f) ||
        !FMath::IsWithinInclusive(ArmorPenetrationCap, 0.0f, 0.60f) ||
        !FMath::IsWithinInclusive(MagicPenetrationCap, 0.0f, 0.60f) ||
        !FMath::IsWithinInclusive(PlayerMeleeCooldownSeconds, 0.3f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarMeleeCooldownSeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(MeleeRangeMetres, 1.0f, 5.0f) ||
        !FMath::IsWithinInclusive(TabTargetRangeMetres, 10.0f, 60.0f) ||
        !FMath::IsWithinInclusive(TabTargetDropRangeMetres, TabTargetRangeMetres, 100.0f) ||
        !FMath::IsWithinInclusive(TabTargetConeDegrees, 60.0f, 360.0f) ||
        !FMath::IsWithinInclusive(TabTargetCycleResetSeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(PlayerRespawnDelaySeconds, 1.0f, 60.0f) ||
        !FMath::IsNearlyEqual(SpiritResFraction, 0.5f) ||
        !FMath::IsNearlyEqual(PlayerResFraction, 0.35f) ||
        !FMath::IsNearlyEqual(ResAcceptSeconds, 60.0f) ||
        (PlayerRespawnPointRule != ESFPlayerRespawnPointRule::NearestToDeath && PlayerRespawnPointRule != ESFPlayerRespawnPointRule::FirstAuthored) ||
        !FMath::IsWithinInclusive(BoarAggroRadiusMetres, 4.0f, 18.0f) ||
        !FMath::IsWithinInclusive(BoarNearNoticeRadiusMetres, 1.0f, 4.0f) ||
        !FMath::IsWithinInclusive(BoarAggroMinimumMetres, 1.0f, 4.0f) ||
        !FMath::IsWithinInclusive(BoarAggroMaximumBonusMetres, 0.0f, 6.0f) ||
        BoarNearNoticeRadiusMetres > BoarAggroMinimumMetres ||
        !FMath::IsWithinInclusive(ThreatSwitchMelee, 1.01f, 3.0f) ||
        !FMath::IsWithinInclusive(ThreatSwitchRanged, ThreatSwitchMelee, 3.0f) ||
        ThreatTableMax < 1 || ThreatTableMax > 40 ||
        !FMath::IsWithinInclusive(ThreatDecisionSeconds, 0.05f, 1.0f) ||
        !FMath::IsNearlyEqual(TauntThreatMultiplier, 1.10f) ||
        !FMath::IsNearlyEqual(TauntForceSeconds, 3.0f) ||
        !FMath::IsNearlyEqual(TauntDrWindowSeconds, 18.0f) ||
        !FMath::IsWithinInclusive(BoarAssistDelaySeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarAssistRadiusMetres, 2.0f, 15.0f) ||
        !FMath::IsNearlyEqual(CommonRespawnSeconds, 300.0f) ||
        !FMath::IsNearlyEqual(RespawnPlayerSlope, 0.15f) ||
        !FMath::IsNearlyEqual(RespawnPlayerFloor, 0.35f) ||
        !FMath::IsNearlyEqual(RespawnJitter, 0.10f) ||
        !FMath::IsWithinInclusive(BoarLeashRadiusMetres, 20.0f, 100.0f) ||
        !FMath::IsWithinInclusive(BoarFieldOfViewDegrees, 60.0f, 360.0f) ||
        !FMath::IsWithinInclusive(BoarWanderRadiusMetres, 2.0f, 40.0f) ||
        !FMath::IsWithinInclusive(BoarWanderSpeedMetresPerSecond, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarChaseSpeedMetresPerSecond, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(BoarWanderMinSeconds, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(BoarWanderMaxSeconds, BoarWanderMinSeconds, 20.0f) ||
        HidesPerBoar < 1 || HidesPerBoar > 5 ||
        !FMath::IsNearlyEqual(CombatExitSeconds, 5.0f) || !FMath::IsNearlyEqual(RegenDelaySeconds, 5.0f) ||
        !FMath::IsNearlyEqual(HealthRegenOocPct, 0.03f))
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
    DOREPLIFETIME(ASFPilotBoar, SpawnGroup);
    DOREPLIFETIME(ASFPilotBoar, Slot);
    DOREPLIFETIME(ASFPilotBoar, bReturning);
    DOREPLIFETIME(ASFPilotBoar, ReplicatedTarget);
}

bool ASFPilotBoar::HasThreatFor(const ASFE1Character* Character) const
{
    if (!HasAuthority() || !Character) { return false; }
    for (const auto& Pair : ThreatActors)
    { if (Pair.Value.Get() == Character && Threat.Contains(Pair.Key)) { return true; } }
    return false;
}

void ASFPilotBoar::Initialize(ASFGameMode* OwnerMode, const FString& Group, int32 InSlot,
    const TArray<FVector2D>& Area, int32 Health, int32 Serial, bool bInDefensive)
{
    check(HasAuthority());
    PilotMode = OwnerMode; SpawnGroup = Group; Slot = InSlot; Territory = Area; SpawnSerial = Serial;
    bDefensive = bInDefensive;
    Anchor = GetActorLocation(); AnchorFacing = GetActorRotation(); CurrentHealth = Health; OnRep_Health();
    LastDamageAt = GetWorld()->GetTimeSeconds();
    Threat.Clear(); ThreatActors.Empty(); OutsideLeashSince.Empty(); TargetCharacterId.Empty(); ForcedTauntId.Empty(); ForcedTauntUntil = 0;
    LastTauntAt = -1; TauntApplications = 0;
    RecentAttackers = FSFRecentAttackerMemory(); DamageAttackers.Empty(); PendingResetAttackers.Empty();
    ReplicatedTarget = nullptr; NextThreatDecisionAt = 0; EngagedSince = -1;
}

bool ASFPilotBoar::ReceivePackAssist(ASFE1Character* Character, const FString& CharacterId)
{
    if (!HasAuthority() || !PilotMode.IsValid() || !IsValid(Character) || CharacterId.IsEmpty() ||
        CurrentHealth <= 0 || bReturning || !TargetCharacterId.IsEmpty() ||
        Character->CombatHealth() <= 0 || !IsValid(Character->GetController()) ||
        USFCharacterSessionComponent::IsLinkdead(Character->GetController()) ||
        USFCharacterSessionComponent::IsRestoring(Character->GetController()) ||
        PilotMode->IsSafe(Character->GetActorLocation()) ||
        !PilotMode->IsInsideTerritory(Character->GetActorLocation(), Territory) ||
        FVector::Dist2D(Anchor, Character->GetActorLocation()) > PilotMode->CombatSettings()->BoarLeashRadiusMetres * 100.0)
    { return false; }
    Threat.Add(CharacterId, 1.0, PilotMode->CombatSettings()->ThreatTableMax);
    ThreatActors.Add(CharacterId, Character);
    TargetCharacterId = CharacterId;
    Target = Character;
    ReplicatedTarget = Character;
    EngagedSince = GetWorld()->GetTimeSeconds();
    Character->CombatMarkHostile();
    ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("BOAR_PACK_ASSIST Group=%s Slot=%d Target=%s Threat=1"),
        *SpawnGroup, Slot, *CharacterId);
    return true;
}

void ASFPilotBoar::EnterEvade()
{
    if (!HasAuthority() || bReturning || CurrentHealth <= 0) { return; }
    FString TopCharacterId;
    if (ASFE1Character* Victim = Target.Get())
    {
        if (USFBackpackComponent* Bag = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Victim->GetController())))
        { TopCharacterId = Bag->GetCharacterId(); }
    }
    if (PilotMode.IsValid()) { PilotMode->RecordBoarEvade(SpawnGroup, TopCharacterId); }
    PendingResetAttackers = MoveTemp(DamageAttackers);
    DamageAttackers.Empty();
    Target.Reset(); ReplicatedTarget = nullptr; TargetCharacterId.Empty(); ForcedTauntId.Empty(); ForcedTauntUntil = 0;
    EngagedSince = -1;
    Threat.Clear(); ThreatActors.Empty(); OutsideLeashSince.Empty();
    WanderDestination = Anchor; bReturning = true; UnreachableSince = -1;
    UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_START Group=%s Slot=%d X=%.1f Y=%.1f HP=%d"),
        *SpawnGroup, Slot, GetActorLocation().X, GetActorLocation().Y, CurrentHealth);
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ASFE1Character* Character = Cast<ASFE1Character>(It->Get()->GetPawn()))
        { if (Character->GetCombatTarget() == this) { Character->SetCombatTarget(nullptr); } }
    }
    ForceNetUpdate();
}

void ASFPilotBoar::OnRep_Health()
{
    if (!Label) { return; }
    Label->SetText(FText::FromString(FString::Printf(TEXT("Boar L2  %d HP"), CurrentHealth)));
    Label->SetTextRenderColor(CurrentHealth > 0 ? FColor::Yellow : FColor::Red);
}

void ASFPilotBoar::ReceivePilotDodge(ASFE1Character* Attacker)
{
    if (!HasAuthority() || !PilotMode.IsValid() || !IsValid(Attacker) || CurrentHealth <= 0 || bReturning) { return; }
    const FString Id = ThreatIdentity(Cast<APlayerController>(Attacker->GetController()));
    if (Id.IsEmpty()) { return; }
    // A hostile swing starts ordinary detection threat, but gives no damage credit.
    Threat.Add(Id, 1.0, PilotMode->CombatSettings()->ThreatTableMax);
    ThreatActors.Add(Id, Attacker);
    if (TargetCharacterId.IsEmpty())
    {
        TargetCharacterId = Id;
        Target = Attacker;
        ReplicatedTarget = Attacker;
        EngagedSince = GetWorld()->GetTimeSeconds();
    }
    Attacker->CombatMarkHostile();
    ForceNetUpdate();
}

void ASFPilotBoar::ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage)
{
    if (!HasAuthority() || CurrentHealth <= 0 || bReturning || !IsValid(Attacker) || Damage <= 0) { return; }
    const int32 EffectiveDamage = FMath::Min(CurrentHealth, Damage);
    Attacker->CombatMarkHostile();
    const FString AttackerId = ThreatIdentity(Cast<APlayerController>(Attacker->GetController()));
    if (!AttackerId.IsEmpty())
    {
        Threat.Add(AttackerId, EffectiveDamage, PilotMode->CombatSettings()->ThreatTableMax);
        ThreatActors.Add(AttackerId, Attacker);
        DamageAttackers.Add(AttackerId);
    }
    CurrentHealth = SFCombatPilotRules::RemainingHealth(CurrentHealth, Damage);
    LastDamageAt = GetWorld()->GetTimeSeconds(); UnreachableSince = -1;
    if (!Attacker->GetCombatTarget() && CurrentHealth > 0) { Attacker->SetCombatTarget(this); }
    OnRep_Health(); ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_HIT Group=%s Slot=%d HP=%d Attacker=%s"),
        *SpawnGroup, Slot, CurrentHealth, AttackerId.IsEmpty() ? TEXT("none") : *AttackerId);
    if (CurrentHealth == 0 && PilotMode.IsValid())
    { Threat.Clear(); ThreatActors.Empty(); DamageAttackers.Empty(); PendingResetAttackers.Empty(); ForcedTauntId.Empty(); ForcedTauntUntil = 0;
      SightCache.Empty(); OutsideLeashSince.Empty(); ReplicatedTarget = nullptr; Target.Reset(); PilotMode->BoarDied(this, Attacker); }
}

bool ASFPilotBoar::ReceivePilotTaunt(ASFE1Character* Taunter)
{
    if (!HasAuthority() || !IsValid(Taunter) || CurrentHealth <= 0 || bReturning ||
        !PilotMode.IsValid() || Threat.Highest() <= 0) { return false; }
    const FString Id = ThreatIdentity(Cast<APlayerController>(Taunter->GetController()));
    if (Id.IsEmpty()) { return false; }
    const USFCombatPilotSettings* Settings = PilotMode->CombatSettings();
    const double Now = GetWorld()->GetTimeSeconds();
    const double SinceLast = LastTauntAt < 0 ? Settings->TauntDrWindowSeconds : Now - LastTauntAt;
    if (SinceLast >= Settings->TauntDrWindowSeconds) { TauntApplications = 0; }
    const double Duration = SFTauntRules::ForcedDuration(TauntApplications, SinceLast,
        Settings->TauntForceSeconds, Settings->TauntDrWindowSeconds);
    LastTauntAt = Now;
    ++TauntApplications;
    if (Duration <= 0) { UE_LOG(LogTemp, Display, TEXT("BOAR_TAUNT_IMMUNE Group=%s"), *SpawnGroup); return false; }
    const double Raised = SFTauntRules::RaisedThreat(Threat.Get(Id), Threat.Highest(), Settings->TauntThreatMultiplier);
    Threat.RaiseTo(Id, Raised, Settings->ThreatTableMax);
    ThreatActors.Add(Id, Taunter);
    ForcedTauntId = Id; ForcedTauntUntil = Now + Duration;
    Taunter->CombatMarkHostile();
    NextThreatDecisionAt = Now;
    UE_LOG(LogTemp, Display, TEXT("BOAR_TAUNT_APPLIED Group=%s Taunter=%s Threat=%.2f Duration=%.2f HP=%d"),
        *SpawnGroup, *Id, Raised, Duration, CurrentHealth);
    return true;
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
    if (Victim && FVector::Dist2D(Anchor, GetActorLocation()) > Mode->CombatSettings()->BoarLeashRadiusMetres*100)
    { EnterEvade(); Victim = nullptr; }
    if (Victim && UnreachableSince >= 0 &&
        SFCombatPilotRules::UnreachableEvade(Now - UnreachableSince, Now - LastDamageAt))
    { EnterEvade(); Victim = nullptr; }
    if (bReturning && FVector::Dist2D(GetActorLocation(),Anchor) < 100)
    {
        SetActorLocation(Anchor, false);
        SetActorRotation(AnchorFacing);
        CurrentHealth = Mode->CombatSettings()->BoarHealth;
        RecentAttackers.RecordReset(PendingResetAttackers, Now);
        const int32 RememberedCount = PendingResetAttackers.Num();
        PendingResetAttackers.Empty();
        bReturning = false; OnRep_Health(); ForceNetUpdate();
        UE_LOG(LogTemp, Display, TEXT("BOAR_EVADE_RESET Group=%s Slot=%d X=%.1f Y=%.1f HP=%d"),
            *SpawnGroup, Slot, GetActorLocation().X, GetActorLocation().Y, CurrentHealth);
        if (bDefensive)
        { UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_RESET Group=%s Attackers=%d RetentionSeconds=60"),
            *SpawnGroup, RememberedCount); }
    }
    if (!bReturning && Mode->CombatSettings()->bCombatAvailable)
    {
        // Perception enters nearby characters with one initial threat. Damage enters them separately.
        for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            auto* Candidate = Cast<ASFE1Character>(It->Get()->GetPawn());
            const FVector Toward = Candidate ? Candidate->GetActorLocation()-GetActorLocation() : FVector::ZeroVector;
            const double Distance = Toward.Size2D();
            // A linkdead (draining) character stays visible but is never chosen as prey (INT-05).
            if (IsValid(Candidate) && Candidate->CombatHealth() > 0 && !USFCharacterSessionComponent::IsLinkdead(It->Get()) &&
                !USFCharacterSessionComponent::IsRestoring(It->Get()) &&
                !Mode->IsSafe(Candidate->GetActorLocation()) &&
                Mode->IsInsideTerritory(Candidate->GetActorLocation(), Territory))
            {
                const FString Id = ThreatIdentity(Cast<APlayerController>(It->Get()));
                if (!Id.IsEmpty() && !Threat.Contains(Id) &&
                    (!bDefensive || RecentAttackers.Contains(Id, Now)))
                {
                    const USFGatheringPilotComponent* Progression = It->Get()->FindComponentByClass<USFGatheringPilotComponent>();
                    const int32 CharacterLevel = Progression ? Progression->GetCharacterLevel() : 1;
                    const USFCombatPilotSettings* Settings = Mode->CombatSettings();
                    const double Radius = SFCombatPilotRules::AggroRadiusMetres(Settings->BoarAggroRadiusMetres,
                        2, CharacterLevel, Settings->BoarAggroMinimumMetres, Settings->BoarAggroMaximumBonusMetres);
                    const double Facing = FVector::DotProduct(GetActorForwardVector(), Toward.GetSafeNormal2D());
                    if (SpawnGroup == TEXT("aggro_sandbox") && Distance < 1500 && Now >= NextAggroFixtureTraceAt)
                    {
                        NextAggroFixtureTraceAt = Now + 1.0;
                        UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_EVAL Level=%d DistanceCm=%.0f RadiusCm=%.0f Facing=%.2f Yaw=%.0f"),
                            CharacterLevel, Distance, Radius * 100.0, Facing, GetActorRotation().Yaw);
                    }
                    if (Distance <= Radius * 100.0 && (Distance <= Settings->BoarNearNoticeRadiusMetres * 100.0 ||
                        Facing >= FMath::Cos(FMath::DegreesToRadians(Settings->BoarFieldOfViewDegrees * 0.5))))
                    {
                        const TWeakObjectPtr<ASFE1Character> Key(Candidate);
                        TPair<double, bool>* Cached = SightCache.Find(Key);
                        bool bVisible = false;
                        if (Cached && Now - Cached->Key < 0.2) { bVisible = Cached->Value; }
                        else
                        {
                            FCollisionQueryParams Query(SCENE_QUERY_STAT(BoarSight), true);
                            Query.AddIgnoredActor(this);
                            Query.AddIgnoredActor(Candidate);
                            const FVector Eye = GetActorLocation() + FVector(0, 0, 80);
                            const FVector Head = Candidate->GetActorLocation() + FVector(0, 0, 80);
                            const FVector Chest = Candidate->GetActorLocation() + FVector(0, 0, 40);
                            bVisible = !GetWorld()->LineTraceTestByChannel(Eye, Head, ECC_Visibility, Query) ||
                                !GetWorld()->LineTraceTestByChannel(Eye, Chest, ECC_Visibility, Query);
                            SightCache.Add(Key, TPair<double, bool>(Now, bVisible));
                        }
                        if (SFCombatPilotRules::DetectBySight(Distance / 100.0, Facing, bVisible, Radius,
                            Settings->BoarNearNoticeRadiusMetres, Settings->BoarFieldOfViewDegrees))
                        {
                            Threat.Add(Id, 1.0, Settings->ThreatTableMax);
                            ThreatActors.Add(Id, Candidate);
                            Candidate->CombatMarkHostile();
                            if (bDefensive)
                            { UE_LOG(LogTemp, Display, TEXT("BOAR_MEMORY_REACQUIRED Group=%s Character=%s Threat=1"),
                                *SpawnGroup, *Id); }
                            UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_DETECTED Group=%s Character=%s Level=%d DistanceCm=%.0f RadiusCm=%.0f"),
                                *SpawnGroup, *Id, CharacterLevel, Distance, Radius * 100.0);
                        }
                    }
                }
            }
        }
        if (Now >= NextThreatDecisionAt)
        {
            NextThreatDecisionAt = Now + Mode->CombatSettings()->ThreatDecisionSeconds;
            TMap<FString, double> Reachable;
            TArray<FString> Ids;
            ThreatActors.GetKeys(Ids);
            for (const FString& Id : Ids)
            {
                ASFE1Character* Character = ThreatActors.FindRef(Id).Get();
                if (!IsValid(Character) || Character->CombatHealth() <= 0 ||
                    Mode->IsSafe(Character->GetActorLocation()))
                { Threat.Remove(Id); ThreatActors.Remove(Id); OutsideLeashSince.Remove(Id); continue; }
                const double AnchorDistance = FVector::Dist2D(Anchor, Character->GetActorLocation());
                if (AnchorDistance > Mode->CombatSettings()->BoarLeashRadiusMetres * 100.0)
                {
                    double& Since = OutsideLeashSince.FindOrAdd(Id, Now);
                    if (Now - Since >= 5.0)
                    { Threat.Remove(Id); ThreatActors.Remove(Id); OutsideLeashSince.Remove(Id); }
                    else if (Mode->IsInsideTerritory(Character->GetActorLocation(), Territory))
                    { Reachable.Add(Id, FVector::Dist2D(GetActorLocation(), Character->GetActorLocation())); }
                    continue;
                }
                OutsideLeashSince.Remove(Id);
                if (Mode->IsInsideTerritory(Character->GetActorLocation(), Territory))
                { Reachable.Add(Id, FVector::Dist2D(GetActorLocation(), Character->GetActorLocation())); }
            }
            FString NextId = Threat.Select(TargetCharacterId, Reachable,
                Mode->CombatSettings()->ThreatSwitchMelee, Mode->CombatSettings()->ThreatSwitchRanged);
            if (Now < ForcedTauntUntil && Reachable.Contains(ForcedTauntId)) { NextId = ForcedTauntId; }
            else if (!ForcedTauntId.IsEmpty() && Now >= ForcedTauntUntil)
            { UE_LOG(LogTemp, Display, TEXT("BOAR_TAUNT_EXPIRED Group=%s"), *SpawnGroup); ForcedTauntId.Empty(); }
            if (NextId != TargetCharacterId)
            {
                const bool bLostEngagement = NextId.IsEmpty() && !TargetCharacterId.IsEmpty();
                if (bLostEngagement) { EnterEvade(); }
                else
                {
                    UE_LOG(LogTemp, Display, TEXT("BOAR_THREAT_TARGET Group=%s Slot=%d From=%s To=%s"),
                        *SpawnGroup, Slot, TargetCharacterId.IsEmpty() ? TEXT("none") : *TargetCharacterId,
                        NextId.IsEmpty() ? TEXT("none") : *NextId);
                    TargetCharacterId = NextId;
                    Target = ThreatActors.FindRef(NextId);
                    ReplicatedTarget = Target.Get();
                    if (!NextId.IsEmpty() && EngagedSince < 0) { EngagedSince = Now; }
                    ForceNetUpdate();
                }
            }
            Victim = Target.Get();
            if (!Victim && !NextId.IsEmpty()) { TargetCharacterId.Empty(); }
        }
    }
    if (Victim && !bReturning && EngagedSince >= 0 &&
        SFCombatPilotRules::CanAssist(Now - EngagedSince, 0.0,
            Mode->CombatSettings()->BoarAssistDelaySeconds, Mode->CombatSettings()->BoarAssistRadiusMetres))
    {
        for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
        {
            ASFPilotBoar* Packmate = *It;
            if (Packmate != this && Packmate->GroupId() == SpawnGroup &&
                SFCombatPilotRules::CanAssist(Now - EngagedSince,
                    FVector::Dist2D(GetActorLocation(), Packmate->GetActorLocation()) / 100.0,
                    Mode->CombatSettings()->BoarAssistDelaySeconds, Mode->CombatSettings()->BoarAssistRadiusMetres))
            {
                if (Packmate->ReceivePackAssist(Victim, TargetCharacterId))
                { UE_LOG(LogTemp, Display, TEXT("BOAR_PACK_ASSIST_TIMING Group=%s Elapsed=%.3f DistanceCm=%.0f"),
                    *SpawnGroup, Now - EngagedSince, FVector::Dist2D(GetActorLocation(), Packmate->GetActorLocation())); }
            }
        }
    }
    if ((SpawnGroup == TEXT("aggro_sandbox") || SpawnGroup == TEXT("memory_sandbox") ||
        SpawnGroup == TEXT("assist_sandbox") || SpawnGroup == TEXT("respawn_sandbox") ||
        SpawnGroup == TEXT("dodge_sandbox")) && !Victim && !bReturning) { return; }
    FVector Destination = WanderDestination;
    float Speed = Mode->CombatSettings()->BoarWanderSpeedMetresPerSecond*100;
    if (Victim)
    {
        Destination = Victim->GetActorLocation(); Speed = Mode->CombatSettings()->BoarChaseSpeedMetresPerSecond*100;
        if (FVector::Dist2D(GetActorLocation(), Destination) <= Mode->CombatSettings()->MeleeRangeMetres * 100 && Now >= NextSwingAt)
        { NextSwingAt = Now + Mode->CombatSettings()->BoarMeleeCooldownSeconds; Mode->TryBoarAttack(this, Victim); LastDamageAt = Now; UnreachableSince = -1; }
    }
    else if (bReturning)
    { Destination = Anchor; Speed = SFCombatPilotRules::EvadeReturnSpeedCmPerSecond(
        Mode->CombatSettings()->BoarChaseSpeedMetresPerSecond); }
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
    if (!bReturning && (!Mode->IsInsideTerritory(Candidate, Territory) || Mode->IsSafe(Candidate)))
    { EnterEvade(); return; }
    const FVector Ground = Grounded(GetWorld(), Candidate, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const FVector BeforeMove = GetActorLocation();
    // An immune return follows its straight anchor path; sweeping can pin it
    // against Silverbrook fences indefinitely before health/threat reset.
    SetActorLocation(Ground, !bReturning);
    if (Victim && !bReturning)
    {
        const bool bBlocked = FVector::Dist2D(BeforeMove, GetActorLocation()) < 1.0 &&
            FVector::Dist2D(GetActorLocation(), Victim->GetActorLocation()) > Mode->CombatSettings()->MeleeRangeMetres * 100;
        if (bBlocked) { if (UnreachableSince < 0) { UnreachableSince = Now; } }
        else { UnreachableSince = -1; }
    }
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
    const FString MapName = GetWorld()->GetOutermost()->GetName();
    const bool bEvadeSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Evade");
    const bool bThreatSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Threat");
    const bool bTauntSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Taunt");
    const bool bAggroSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Aggro");
    const bool bMemorySandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_AttackerMemory");
    const bool bRegenSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Regeneration");
    const bool bRestRegenSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_RestRegen");
    const bool bFallSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_FallDamage");
    const bool bAssistSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Assist");
    const bool bAutoAttackSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_AutoAttack");
    const bool bDodgeSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Dodge");
    const bool bUnstuckSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Unstuck");
    const bool bSpiritReleaseSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease");
    const bool bResurrectionAcceptSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept");
    const bool bPersonalLootSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_PersonalLoot");
    bRespawnSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Respawn");
    bCompanionSandbox = GetNetMode() != NM_Client &&
        (MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Companions") || bEvadeSandbox || bThreatSandbox || bTauntSandbox || bAggroSandbox || bMemorySandbox || bRegenSandbox || bRestRegenSandbox || bFallSandbox || bAssistSandbox || bRespawnSandbox || bAutoAttackSandbox || bDodgeSandbox || bUnstuckSandbox || bSpiritReleaseSandbox || bResurrectionAcceptSandbox || bPersonalLootSandbox);
    if (bCompanionSandbox)
    {
        CapturedSettings = GetDefault<USFCombatPilotSettings>();
        FString Error;
        if (!CapturedSettings->Validate(Error))
        { UE_LOG(LogTemp, Error, TEXT("COMPANION_SANDBOX_DISABLED Invalid combat settings: %s"), *Error); return; }
        const double Extent = (bEvadeSandbox || bThreatSandbox || bTauntSandbox || bAggroSandbox || bMemorySandbox || bRegenSandbox || bRestRegenSandbox || bAssistSandbox || bRespawnSandbox || bAutoAttackSandbox || bDodgeSandbox || bSpiritReleaseSandbox || bResurrectionAcceptSandbox) ? 85.0 : 45.0;
        const TArray<FVector2D> SandboxArea = { FVector2D(-Extent, -Extent), FVector2D(Extent, -Extent),
            FVector2D(Extent, Extent), FVector2D(-Extent, Extent) };
        int32 Slot = 0;
        for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
        {
            const FName Tag = bEvadeSandbox ? TEXT("SF_EvadeSandboxWildlife") :
                bThreatSandbox ? TEXT("SF_ThreatSandboxWildlife") :
                bTauntSandbox ? TEXT("SF_TauntSandboxWildlife") :
                bAggroSandbox ? TEXT("SF_AggroSandboxWildlife") :
                bMemorySandbox ? TEXT("SF_AttackerMemorySandboxWildlife") :
                bRegenSandbox ? TEXT("SF_RegenerationSandboxWildlife") :
                bRestRegenSandbox ? TEXT("SF_RestRegenSandboxWildlife") :
                bAssistSandbox ? TEXT("SF_AssistSandboxWildlife") :
                bAutoAttackSandbox ? TEXT("SF_AutoAttackSandboxWildlife") :
                bDodgeSandbox ? TEXT("SF_DodgeSandboxWildlife") :
                bSpiritReleaseSandbox ? TEXT("SF_SpiritReleaseSandboxWildlife") :
                bResurrectionAcceptSandbox ? TEXT("SF_ResurrectionAcceptSandboxWildlife") :
                bRespawnSandbox ? TEXT("SF_RespawnSandboxWildlife") :
                bPersonalLootSandbox ? TEXT("SF_PersonalLootSandboxWildlife") : TEXT("SF_CompanionSandboxWildlife");
            if (!It->ActorHasTag(Tag)) { continue; }
            if (bRespawnSandbox) { RespawnSandboxPoints.Add(It->GetActorTransform()); }
            It->Initialize(this, bEvadeSandbox ? TEXT("evade_sandbox") :
                bThreatSandbox ? TEXT("threat_sandbox") :
                bTauntSandbox ? TEXT("taunt_sandbox") :
                bAggroSandbox ? TEXT("aggro_sandbox") :
                bMemorySandbox ? TEXT("memory_sandbox") :
                bRegenSandbox ? TEXT("regeneration_sandbox") :
                bRestRegenSandbox ? TEXT("rest_regen_sandbox") :
                bAssistSandbox ? TEXT("assist_sandbox") :
                bAutoAttackSandbox ? TEXT("auto_attack_sandbox") :
                bDodgeSandbox ? TEXT("dodge_sandbox") :
                bSpiritReleaseSandbox ? TEXT("spirit_release_sandbox") :
                bResurrectionAcceptSandbox ? TEXT("resurrection_accept_sandbox") :
                bRespawnSandbox ? TEXT("respawn_sandbox") :
                bPersonalLootSandbox ? TEXT("personal_loot_sandbox") : TEXT("companions_sandbox"), Slot++, SandboxArea,
                CapturedSettings->BoarHealth, ++NextBoarSerial,
                bMemorySandbox || bRespawnSandbox || bAutoAttackSandbox || bDodgeSandbox || bSpiritReleaseSandbox || bResurrectionAcceptSandbox || bPersonalLootSandbox || (bAssistSandbox && It->ActorHasTag(TEXT("SF_AssistDefensive"))));
        }
        if (bRespawnSandbox)
        {
            for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
            { if (It->ActorHasTag(TEXT("SF_RespawnPoint"))) { RespawnSandboxPoints.Add(It->GetActorTransform()); } }
            if (Slot != 1 || RespawnSandboxPoints.Num() != 2)
            { UE_LOG(LogTemp, Error, TEXT("BOAR_RESPAWN_INVALID_POINTS Boars=%d Points=%d"), Slot, RespawnSandboxPoints.Num()); return; }
        }
        bPilotActive = bFallSandbox || bUnstuckSandbox || Slot > 0;
        RespawnPoints.Add(FVector(0, 0, 110));
        if (bSpiritReleaseSandbox)
        {
            RespawnPoints.Reset();
            for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
            { if (It->ActorHasTag(TEXT("SF_RespawnPoint"))) { RespawnPoints.Add(It->GetActorLocation()); } }
            if (Slot != 1 || RespawnPoints.Num() != 1)
            { UE_LOG(LogTemp, Error, TEXT("SPIRIT_RELEASE_INVALID_FIXTURE Boars=%d Points=%d"), Slot, RespawnPoints.Num()); bPilotActive = false; return; }
        }
        UE_LOG(LogTemp, Display, TEXT("COMBAT_SANDBOX_READY Map=%s Boars=%d"), *MapName, Slot);
        return;
    }
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
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bPilotActive && GetNumPlayers() == 1 &&
        ((GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease") &&
          FParse::Param(FCommandLine::Get(), TEXT("SpiritReleaseScenario"))) ||
         (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept") &&
          FParse::Param(FCommandLine::Get(), TEXT("ResurrectionAcceptScenario")))))
    {
        const TWeakObjectPtr<APlayerController> Weak(NewPlayer);
        FTimerHandle FixtureTimer;
        GetWorldTimerManager().SetTimer(FixtureTimer, FTimerDelegate::CreateWeakLambda(this, [Weak]()
        {
            if (Weak.IsValid())
            {
                if (ASFE1Character* Driver = Cast<ASFE1Character>(Weak->GetPawn()))
                {
                    Driver->CombatSetHealth(35); // Fixture: three real 14-point boar hits can cause death.
                    if (Driver->GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease"))
                    { UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_FIXTURE_HEALTH HP=%d"), Driver->CombatHealth()); }
                    else
                    { UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_FIXTURE_HEALTH HP=%d"), Driver->CombatHealth()); }
                }
            }
        }), 1.0f, false);
    }
    if (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") &&
        (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsBoarNetcheck")) ||
         FParse::Param(FCommandLine::Get(), TEXT("GreenlandsDeathNetcheck"))))
    {
        if (GetNumPlayers() == 1) { GreenlandsDriverController = NewPlayer; }
        else if (GetNumPlayers() == 2) { GreenlandsObserverController = NewPlayer; }
    }
    if (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_Aggro") &&
        FParse::Param(FCommandLine::Get(), TEXT("CombatAggroScenario")))
    {
        const int32 Participant = GetNumPlayers();
        const TWeakObjectPtr<APlayerController> Weak(NewPlayer);
        FTimerHandle AggroFixtureTimer;
        GetWorldTimerManager().SetTimer(AggroFixtureTimer, FTimerDelegate::CreateWeakLambda(this, [this, Weak, Participant]()
        {
            if (!Weak.IsValid()) { return; }
            ASFE1Character* Player = Cast<ASFE1Character>(Weak->GetPawn());
            if (!Player) { return; }
            const FVector XY = Participant == 1 ? FVector(650, 0, 0) : FVector(0, -1200, 0);
            const FVector Place = Grounded(GetWorld(), XY, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            Player->SetActorLocation(Place, false, nullptr, ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_FIXTURE Participant=%d X=%.0f Y=%.0f"), Participant, Place.X, Place.Y);
        }), 1.0, false);
    }
#endif
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
            FVector Offset = bCapacitySlots
                ? FVector(180.0 * FMath::Cos(Angle), 180.0 * FMath::Sin(Angle), 0)
                : (PlayerIndex == 1 ? FVector(180,0,0) :
                    PlayerIndex == 2 ? FVector(180,180,0) : FVector(180,-180,0));
            if (FParse::Param(FCommandLine::Get(), TEXT("PartyXpScenario")))
            {
                auto* Bag = Weak->FindComponentByClass<USFBackpackComponent>();
                if (Bag && Bag->GetCharacterId() == TEXT("chr_dev_gamma")) { Offset = FVector(8000,0,0); }
                else if (Bag && Bag->GetCharacterId() == TEXT("chr_dev_beta")) { Offset = FVector(180,80,0); }
                else { Offset = FVector(180,-80,0); }
            }
            const FVector Near = Grounded(GetWorld(), Boar->GetActorLocation()+Offset,
                Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            Player->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
            if (FParse::Param(FCommandLine::Get(), TEXT("PartyXpScenario")))
            {
                auto* Bag = Weak->FindComponentByClass<USFBackpackComponent>();
                UE_LOG(LogTemp, Display, TEXT("PARTY_XP_RANGE Character=%s DistanceCm=%.0f"),
                    Bag ? *Bag->GetCharacterId() : TEXT("none"), FVector::Dist(Near, Boar->GetActorLocation()));
            }
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
    if (bPilotActive && bRespawnSandbox && FSFCommonRespawn::IsDue(GetWorld()->GetTimeSeconds(), RespawnSandboxDueAt))
    {
        const int32 Preferred = RespawnSandboxCycle % RespawnSandboxPoints.Num();
        int32 NextPoint = INDEX_NONE;
        for (int32 Offset = 0; Offset < RespawnSandboxPoints.Num(); ++Offset)
        {
            const int32 Candidate = (Preferred + Offset) % RespawnSandboxPoints.Num();
            if (!GetWorld()->OverlapAnyTestByChannel(RespawnSandboxPoints[Candidate].GetLocation(), FQuat::Identity,
                ECC_Pawn, FCollisionShape::MakeSphere(110.0))) { NextPoint = Candidate; break; }
        }
        if (NextPoint != INDEX_NONE)
        {
            const FTransform& Point = RespawnSandboxPoints[NextPoint];
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ASFPilotBoar* Boar = GetWorld()->SpawnActor<ASFPilotBoar>(ASFPilotBoar::StaticClass(), Point, Params);
            if (Boar)
            {
                const TArray<FVector2D> Area = { FVector2D(-85, -85), FVector2D(85, -85),
                    FVector2D(85, 85), FVector2D(-85, 85) };
                Boar->Initialize(this, TEXT("respawn_sandbox"), 0, Area,
                    CapturedSettings->BoarHealth, ++NextBoarSerial, true);
                RespawnSandboxDueAt = -1.0;
                UE_LOG(LogTemp, Display, TEXT("BOAR_RESPAWN_ACTIVE Cycle=%d Point=%d T=%.3f HP=%d X=%.0f Y=%.0f"),
                    RespawnSandboxCycle, NextPoint, GetWorld()->GetTimeSeconds(), Boar->Health(),
                    Point.GetLocation().X, Point.GetLocation().Y);
            }
        }
    }
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
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsBoarNetcheck"))) { TickGreenlandsNetcheck(); }
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsDeathNetcheck"))) { TickGreenlandsDeathNetcheck(); }
#endif
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFGameMode::TickGreenlandsDeathNetcheck()
{
    // The reconnect fixture starts both clients together; resolve their durable identities
    // after restoration instead of depending on which editor process joins first.
    GreenlandsDriverController.Reset();
    GreenlandsObserverController.Reset();
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Controller = It->Get();
        const FString Id = ThreatIdentity(Controller);
        if (Id == TEXT("chr_dev_alpha")) { GreenlandsDriverController = Controller; }
        else if (Id == TEXT("chr_dev_beta")) { GreenlandsObserverController = Controller; }
    }
    auto* Driver = Cast<ASFE1Character>(GreenlandsDriverController.IsValid() ? GreenlandsDriverController->GetPawn() : nullptr);
    auto* Observer = Cast<ASFE1Character>(GreenlandsObserverController.IsValid() ? GreenlandsObserverController->GetPawn() : nullptr);
    if (!Driver || !Observer || USFCharacterSessionComponent::IsRestoring(Driver->GetController()) ||
        USFCharacterSessionComponent::IsRestoring(Observer->GetController())) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (GreenlandsDeathStage == 0 && Driver->CombatHealth() == 0)
    {
        Observer->SetActorLocation(Grounded(GetWorld(), Driver->GetActorLocation() + FVector(0, 1600, 0),
            Observer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
        Observer->CombatSetHealth(1000); // Keep the observing fixture in replication range after reconnect.
        GreenlandsDeathStage = 2;
        UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_RESTORED Event=%s"),
            *PlayerDeathEvents.FindRef(TWeakObjectPtr<ASFE1Character>(Driver)));
    }
    if (GreenlandsDeathStage == 0)
    {
        for (const auto& Slot : ActiveBoars)
        {
            ASFPilotBoar* Boar = Slot.Value.Get();
            if (!Slot.Key.StartsWith(TEXT("boar_farm_edge:")) || !Boar || !Boar->IsDefensive()) continue;
            GreenlandsDeathBoar = Boar;
            const FVector Near = Grounded(GetWorld(), Boar->GetActorLocation() + FVector(100, 0, 0),
                Driver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            Driver->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
            Observer->SetActorLocation(Grounded(GetWorld(), Near + FVector(0, 1600, 0),
                Observer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
            Driver->CombatSetHealth(35);
            GreenlandsDeathStage = 1; GreenlandsDeathNextHitAt = Now + 2.0;
            UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_BEGIN Group=%s Slot=%d HP=35"),
                *Boar->GroupId(), Boar->SlotIndex());
            break;
        }
    }
    else if (GreenlandsDeathStage == 1 && Driver->CombatHealth() > 0 && Now >= GreenlandsDeathNextHitAt)
    {
        if (ASFPilotBoar* Boar = GreenlandsDeathBoar.Get())
        { TryBoarAttack(Boar, Driver); GreenlandsDeathNextHitAt = Now + 1.5; }
    }
    else if (GreenlandsDeathStage == 1 && Driver->CombatHealth() == 0)
    {
        GreenlandsDeathStage = 2;
        UE_LOG(LogTemp, Display, TEXT("GREENLANDS_DEATH_BOAR_KILL HP=0 Event=%s"),
            *PlayerDeathEvents.FindRef(TWeakObjectPtr<ASFE1Character>(Driver)));
    }
}

void ASFGameMode::TickGreenlandsNetcheck()
{
    auto* Driver = Cast<ASFE1Character>(GreenlandsDriverController.IsValid() ? GreenlandsDriverController->GetPawn() : nullptr);
    auto* Observer = Cast<ASFE1Character>(GreenlandsObserverController.IsValid() ? GreenlandsObserverController->GetPawn() : nullptr);
    if (!Driver || !Observer || Driver->CombatHealth() <= 0 || Observer->CombatHealth() <= 0) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    auto Move = [this](ASFE1Character* Player, const FVector& XY)
    {
        Player->SetActorLocation(Grounded(GetWorld(), XY, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
            false, nullptr, ETeleportType::TeleportPhysics);
    };
    auto Stage = [this, Driver, Observer, Now](int32 Value, bool bKill = false)
    {
        GreenlandsNetcheckStage = Value;
        GreenlandsStageAt = Now;
        Driver->SetGreenlandsNetcheckStage(Value, GreenlandsLeadSlot, GreenlandsMateSlot, bKill);
        Observer->SetGreenlandsNetcheckStage(Value, GreenlandsLeadSlot, GreenlandsMateSlot, false);
    };
    if (GreenlandsNetcheckStage == 0)
    {
        ASFPilotBoar* BestLead = nullptr;
        ASFPilotBoar* BestMate = nullptr;
        double BestDistance = 800.0;
        for (const auto& First : ActiveBoars)
        {
            ASFPilotBoar* Lead = First.Value.Get();
            if (!First.Key.StartsWith(TEXT("boar_farm_edge:")) || !Lead || Lead->IsDefensive()) { continue; }
            for (const auto& Second : ActiveBoars)
            {
                ASFPilotBoar* Mate = Second.Value.Get();
                if (!Second.Key.StartsWith(TEXT("boar_farm_edge:")) || !Mate || !Mate->IsDefensive()) { continue; }
                const double Distance = FVector::Dist2D(Lead->GetActorLocation(), Mate->GetActorLocation());
                if (Distance < BestDistance)
                { BestDistance = Distance; BestLead = Lead; BestMate = Mate; }
            }
        }
        if (BestLead && BestMate)
        {
            GreenlandsLead = BestLead; GreenlandsMate = BestMate;
            GreenlandsLeadSlot = BestLead->SlotIndex(); GreenlandsMateSlot = BestMate->SlotIndex();
            GreenlandsLeadAnchor = BestLead->SpawnAnchor();
            GreenlandsMateAnchor = BestMate->SpawnAnchor();
            Move(Driver, BestLead->GetActorLocation() + BestLead->GetActorForwardVector() * 700);
            Move(Observer, BestLead->GetActorLocation() - BestLead->GetActorForwardVector() * 3000);
            // Keep the two test pawns alive through several minutes of observed AI transitions.
            Driver->CombatSetHealth(1000);
            Observer->CombatSetHealth(1000);
            Stage(1);
            UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_BEGIN LeadSlot=%d MateSlot=%d DistanceCm=%.0f"),
                GreenlandsLeadSlot, GreenlandsMateSlot, BestDistance);
        }
        return;
    }
    ASFPilotBoar* Lead = GreenlandsLead.Get();
    ASFPilotBoar* Mate = GreenlandsMate.Get();
    if (GreenlandsNetcheckStage <= 8 && (!Lead || !Mate)) { return; }
    // Switch before the 2 s pack-assist timer so the mate assists the observer deterministically.
    if (GreenlandsNetcheckStage == 1 && Lead->ThreatTarget() == Driver &&
        !Mate->ThreatTarget() && Now - GreenlandsStageAt >= 0.5)
    {
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_AGGRO LeadSlot=%d"), GreenlandsLeadSlot);
        // Keep the challenger in the pack clearing so the lead can close back
        // within the common 8 m assist radius after switching targets.
        const FVector TowardLead = (Lead->GetActorLocation() - Mate->GetActorLocation()).GetSafeNormal2D();
        Move(Observer, Mate->GetActorLocation() + TowardLead * 180);
        Lead->ReceivePilotDamage(Observer, 20);
        Stage(2);
    }
    else if (GreenlandsNetcheckStage == 2 && Lead->ThreatTarget() == Observer && Mate->ThreatTarget() == Observer)
    {
        Mate->ReceivePilotDamage(Observer, 1); // accepted damage seeds the defensive boar's later memory
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_SWITCH_ASSIST LeadSlot=%d MateSlot=%d"),
            GreenlandsLeadSlot, GreenlandsMateSlot);
        Move(Driver, GreenlandsLeadAnchor + FVector(-10000, 0, 0));
        Stage(3);
    }
    else if (GreenlandsNetcheckStage == 3 && Now - GreenlandsStageAt >= 2.0)
    {
        FVector2D Centre = FVector2D::ZeroVector;
        const auto& Polygon = Groups[TEXT("boar_farm_edge")].Polygon;
        for (const FVector2D& Corner : Polygon) { Centre += Corner; }
        Centre /= Polygon.Num();
        const FVector TowardCentre = (FVector(Centre.X * 100, Centre.Y * 100, 0) -
            GreenlandsLeadAnchor).GetSafeNormal2D();
        Move(Observer, GreenlandsLeadAnchor + TowardCentre * 5200);
        // Probe the authored 60 m leash with both engaged boars. The farm
        // clearing has collision that otherwise ends pursuit near the anchor.
        const FVector MateTowardCentre = (FVector(Centre.X * 100, Centre.Y * 100, 0) -
            GreenlandsMateAnchor).GetSafeNormal2D();
        Lead->SetActorLocation(Grounded(GetWorld(), GreenlandsLeadAnchor + TowardCentre * 6200,
            Lead->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
        Mate->SetActorLocation(Grounded(GetWorld(), GreenlandsMateAnchor + MateTowardCentre * 6200,
            Mate->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_CHASE_POINT X=%.0f Y=%.0f Inside=%d"),
            Observer->GetActorLocation().X, Observer->GetActorLocation().Y,
            IsInsideTerritory(Observer->GetActorLocation(), Polygon) ? 1 : 0);
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_LEASH_PULL DistanceCm=%.0f"),
            FVector::Dist2D(Lead->GetActorLocation(), GreenlandsLeadAnchor));
        Stage(4);
    }
    else if (GreenlandsNetcheckStage == 4 && Lead->IsLeashing() && Mate->IsLeashing() &&
        Now - GreenlandsStageAt >= 2.0)
    {
        Move(Observer, GreenlandsLeadAnchor + FVector(-10000, 0, 0));
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_EVADE LeadSlot=%d MateSlot=%d"),
            GreenlandsLeadSlot, GreenlandsMateSlot);
        Stage(5);
    }
    else if (GreenlandsNetcheckStage == 5 && !Lead->IsLeashing() && !Mate->IsLeashing() &&
        Lead->Health() == CapturedSettings->BoarHealth && Mate->Health() == CapturedSettings->BoarHealth)
    {
        GreenlandsRegenHealth = FMath::Min(60, Observer->CombatHealth() - 1);
        Observer->CombatSetHealth(GreenlandsRegenHealth);
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_RESET_REGEN_START Health=%d"), GreenlandsRegenHealth);
        Stage(6);
    }
    else if (GreenlandsNetcheckStage == 6 && Observer->CombatHealth() > GreenlandsRegenHealth &&
        !Observer->IsPilotInCombat())
    {
        if (!bGreenlandsRegenObserved)
        {
            bGreenlandsRegenObserved = true;
            GreenlandsStageAt = Now;
            UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_REGEN Health=%d"), Observer->CombatHealth());
        }
        else if (Now - GreenlandsStageAt >= 2.0)
        {
            for (TActorIterator<ASFRestAreaVolume> It(GetWorld()); It; ++It)
            {
                if (It->Kind != ESFRestAreaKind::Inn) { continue; }
                Observer->SetActorLocation(It->GetActorLocation() - FVector(0, 0, 200),
                    false, nullptr, ETeleportType::TeleportPhysics);
                if (!It->Contains(Observer->GetActorLocation())) { return; }
                Stage(12);
                UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_REST_ENTER Health=%d"), Observer->CombatHealth());
                break;
            }
        }
    }
    else if (GreenlandsNetcheckStage == 12 && Now - GreenlandsStageAt >= 3.0 &&
        ASFRestAreaVolume::IsInsideInn(GetWorld(), Observer->GetActorLocation()))
    {
        Move(Observer, Mate->GetActorLocation() + Mate->GetActorForwardVector() * 500);
        Stage(7);
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_REST_EXIT Health=%d"), Observer->CombatHealth());
    }
    else if (GreenlandsNetcheckStage == 7 && Mate->ThreatTarget() == Observer && Now - GreenlandsStageAt >= 2.0)
    {
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_MEMORY MateSlot=%d"), GreenlandsMateSlot);
        Move(Observer, GreenlandsLeadAnchor + FVector(-10000, 0, 0));
        Stage(8);
    }
    else if (GreenlandsNetcheckStage == 8 && !Lead->IsLeashing() && !Mate->IsLeashing() &&
        !Lead->ThreatTarget() && !Mate->ThreatTarget() && Now - GreenlandsStageAt >= 1.0)
    {
        Move(Driver, Lead->GetActorLocation() + Lead->GetActorForwardVector() * 180);
        Driver->SetCombatTarget(Lead);
        Stage(13);
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_AUTO_READY LeadSlot=%d"), GreenlandsLeadSlot);
    }
    else if (GreenlandsNetcheckStage == 13 && Lead->Health() == 20 &&
        Lead->ThreatTarget() == Driver && !Driver->IsAutoAttacking())
    {
        Stage(14);
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_AUTO_STOP Health=%d Target=Driver"), Lead->Health());
    }
    else if (GreenlandsNetcheckStage == 14 && Now - GreenlandsStageAt >= 2.5)
    {
        if (Lead->Health() != 20 || Driver->IsAutoAttacking())
        { UE_LOG(LogTemp, Error, TEXT("BOAR_GREENLANDS_AUTO_FAILED Health=%d Enabled=%d"), Lead->Health(), Driver->IsAutoAttacking() ? 1 : 0); return; }
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_AUTO_DONE Health=%d"), Lead->Health());
        Stage(9, true);
    }
    else if (GreenlandsNetcheckStage == 9 && !GreenlandsLead.IsValid())
    {
        Move(Driver, GreenlandsLeadAnchor + FVector(-10000, 0, 0));
        Stage(10);
        UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_KILL LeadSlot=%d"), GreenlandsLeadSlot);
    }
    else if (GreenlandsNetcheckStage == 10)
    {
        ASFPilotBoar* Fresh = ActiveBoars.FindRef(FString::Printf(TEXT("boar_farm_edge:%d"), GreenlandsLeadSlot)).Get();
        if (Fresh && Fresh != Lead && Fresh->Health() == CapturedSettings->BoarHealth)
        {
            Stage(11);
            UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_RESPAWN LeadSlot=%d HP=%d"), GreenlandsLeadSlot, Fresh->Health());
        }
    }
}
#endif

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
    // Each four-point clearing has two sight-aggressive and two defensive boars.
    // Defensive packmates join through the common assist rule, then remember actual attackers.
    const int32 PointOrdinal = FCString::Atoi(*PointId.Right(2));
    Boar->Initialize(this, Group, Slot, Groups[Group].Polygon, CapturedSettings->BoarHealth,
        ++NextBoarSerial, PointOrdinal % 4 >= 2);
    ActiveBoars.Add(Key, Boar);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_SPAWN Group=%s Slot=%d Cycle=%d Point=%s"), *Group, Slot, Cycle, *PointId);
}

SFCombatResolution::FResult ASFGameMode::ResolveCombatAttack(AActor* Attacker,
    const SFCombatResolution::FAttack& Attack, double FixtureAvoidanceDraw)
{
    if (!IsValid(Attacker) || !Attacker->HasAuthority()) { return {}; }
    FRandomStream* Stream = nullptr;
    if (ASFE1Character* Player = Cast<ASFE1Character>(Attacker))
    {
        Stream = PlayerCombatStreams.Find(Player);
        if (!Stream)
        {
            const FString Id = ThreatIdentity(Cast<APlayerController>(Player->GetController()));
            Stream = &PlayerCombatStreams.Add(Player, FRandomStream(GetTypeHash(Id)));
        }
    }
    else if (ASFPilotBoar* Boar = Cast<ASFPilotBoar>(Attacker))
    {
        Stream = BoarCombatStreams.Find(Boar);
        if (!Stream) { Stream = &BoarCombatStreams.Add(Boar, FRandomStream(GetTypeHash(Boar->DeathId()))); }
    }
    if (!Stream) { return {}; }
    SFCombatResolution::FDraws Draws = SFCombatResolution::Draw(*Stream);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FixtureAvoidanceDraw >= 0) { Draws.Avoid = FixtureAvoidanceDraw; }
#endif
    SFCombatResolution::FAttack Tuned = Attack;
    if (CapturedSettings)
    {
        Tuned.AvoidanceCap = CapturedSettings->AvoidanceCap;
        Tuned.MissCap = CapturedSettings->MissCap;
        Tuned.CritCap = CapturedSettings->CritCap;
        Tuned.CritDamageCap = CapturedSettings->CritDamageCap;
        Tuned.BlockCap = CapturedSettings->BlockCap;
        Tuned.BlockReductionCap = CapturedSettings->BlockReductionCap;
        Tuned.ArmorKLevel = CapturedSettings->ArmorKLevel;
        Tuned.ArmorReductionCap = CapturedSettings->ArmorReductionCap;
        Tuned.ResistK = CapturedSettings->ResistK;
        Tuned.ResistReductionCap = CapturedSettings->ResistReductionCap;
        Tuned.ArmorPenetrationCap = CapturedSettings->ArmorPenetrationCap;
        Tuned.MagicPenetrationCap = CapturedSettings->MagicPenetrationCap;
    }
    const SFCombatResolution::FResult Result = SFCombatResolution::Resolve(Tuned, Draws);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_RESOLUTION_SERVER Source=%s Outcome=%s Damage=%d Draws=%.3f,%.3f,%.3f,%.3f,%.3f"),
        *GetNameSafe(Attacker), SFCombatResolution::Name(Result.Outcome), Result.Damage,
        Draws.Hit, Draws.Avoid, Draws.Crit, Draws.Block, Draws.Spread);
    return Result;
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
        if (!IsValid(*It) || It->IsActorBeingDestroyed() || It->Health() <= 0 || It->IsLeashing() || IsSafe(It->GetActorLocation())
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
    SFCombatResolution::FAttack Attack;
    Attack.RawDamage = CapturedSettings->PlayerMeleeDamage + SFGear::Equipped(Attacker->GetController()).Damage;
    Attack.bFrontArc = SFBoarDodgeRules::IsInFrontArc(Best->GetActorForwardVector(),
        Attacker->GetActorLocation() - Best->GetActorLocation(), CapturedSettings->FrontArcDegrees);
    const FString Map = GetWorld()->GetOutermost()->GetName();
    const bool bBoarAvoidance = Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_Dodge") ||
        Map == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
    Attack.bCanDodge = bBoarAvoidance && Attack.bFrontArc;
    Attack.bCanParry = bBoarAvoidance && Attack.bFrontArc;
    Attack.DodgeChance = CapturedSettings->BoarDodgeChance;
    Attack.ParryChance = CapturedSettings->BoarParryChance;
    double FixtureDraw = -1;
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FParse::Param(FCommandLine::Get(), TEXT("DodgeScenario")) ||
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsBoarNetcheck")))
    {
        const int32 Swing = ++DodgeScenarioSwings.FindOrAdd(Attacker);
        if (Swing <= 3) { FixtureDraw = Swing == 1 ? 0.01 : 0.5; }
    }
#endif
    const auto Result = ResolveCombatAttack(Attacker, Attack, FixtureDraw);
    if (Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_Dodge") && FixtureDraw >= 0)
    { UE_LOG(LogTemp, Display, TEXT("BOAR_DODGE_SERVER Dodge=%d HP=%d Front=%d Draw=%.3f"),
        Result.Outcome == SFCombatResolution::EOutcome::Dodge ? 1 : 0, Best->Health(),
        Attack.bFrontArc ? 1 : 0, FixtureDraw); }
    if (Result.Damage <= 0) { Best->ReceivePilotDodge(Attacker); return; }
    Best->ReceivePilotDamage(Attacker, Result.Damage);
}

bool ASFGameMode::TryPlayerTaunt(ASFE1Character* Taunter, AActor* SelectedTarget)
{
    const auto Deny = [Taunter](const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Display, TEXT("BOAR_TAUNT_DENIED Reason=%s"), Reason);
        if (IsValid(Taunter))
        { if (ASFPlayerController* PC = Cast<ASFPlayerController>(Taunter->GetController())) { PC->ClientPilotHit(FString::Printf(TEXT("Taunt: %s"), Reason)); } }
        return false;
    };
    if (GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_Taunt") ||
        !bPilotActive || !CapturedSettings || !CapturedSettings->bCombatAvailable ||
        !IsValid(Taunter) || Taunter->CombatHealth() <= 0 ||
        USFCharacterSessionComponent::IsRestoring(Taunter->GetController())) { return Deny(TEXT("unavailable")); }
    ASFPilotBoar* Boar = Cast<ASFPilotBoar>(SelectedTarget);
    if (!IsValid(Boar) || Boar->Health() <= 0 || Boar->IsLeashing() ||
        IsSafe(Taunter->GetActorLocation()) || IsSafe(Boar->GetActorLocation())) { return Deny(TEXT("invalid target")); }
    if (FVector::Dist2D(Taunter->GetActorLocation(), Boar->GetActorLocation()) > CapturedSettings->MeleeRangeMetres * 100.0)
    { return Deny(TEXT("out of range")); }
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatPilotTaunt), false, Taunter);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Taunter->GetActorLocation() + FVector(0, 0, 45),
        Boar->GetActorLocation() + FVector(0, 0, 45), ECC_Visibility, Query) && Hit.GetActor() != Boar) { return Deny(TEXT("line of sight")); }
    return Boar->ReceivePilotTaunt(Taunter) ? true : Deny(TEXT("not engaged or immune"));
}

void ASFGameMode::TryBoarAttack(ASFPilotBoar* Boar, ASFE1Character* Victim)
{
    if (!bPilotActive || !CapturedSettings->bCombatAvailable || !IsValid(Boar) || !IsValid(Victim) ||
        Boar->Health() <= 0 || Victim->CombatHealth() <= 0 || IsSafe(Boar->GetActorLocation()) ||
        IsSafe(Victim->GetActorLocation()) ||
        FVector::Dist2D(Boar->GetActorLocation(), Victim->GetActorLocation()) > CapturedSettings->MeleeRangeMetres*100) return;
    SFCombatResolution::FAttack Attack;
    Attack.RawDamage = CapturedSettings->BoarMeleeDamage;
    Attack.bFrontArc = SFBoarDodgeRules::IsInFrontArc(Victim->GetActorForwardVector(),
        Boar->GetActorLocation() - Victim->GetActorLocation(), CapturedSettings->FrontArcDegrees);
    Attack.PilotFlatArmor = SFGear::Equipped(Victim->GetController()).Armor;
    const auto Result = ResolveCombatAttack(Boar, Attack);
    if (Result.Damage > 0) { HitPlayer(Boar, Victim, Result.Damage, true); }
}

void ASFGameMode::ApplyFallDamage(ASFE1Character* Victim, float ImpactSpeedMps)
{
    if (!bPilotActive || !CapturedSettings || !IsValid(Victim) || !Victim->HasAuthority()) { return; }
    const int32 Damage = SFFallDamage::Amount(CapturedSettings->PlayerHealth, ImpactSpeedMps,
        CapturedSettings->FallSpeedThresholdMps);
    if (Damage <= 0) { return; }
    UE_LOG(LogTemp, Display, TEXT("FALL_DAMAGE ImpactMps=%.2f Damage=%d HealthBefore=%d Pawn=%s"),
        ImpactSpeedMps, Damage, Victim->CombatHealth(), *Victim->GetName());
    HitPlayer(nullptr, Victim, Damage, true);
}

void ASFGameMode::HitPlayer(AActor* Source, ASFE1Character* Victim, int32 Damage, bool bIgnoreArmor)
{
    if (!IsValid(Victim) || !Victim->HasAuthority() || Victim->CombatHealth() <= 0 ||
        USFCharacterSessionComponent::IsRestoring(Victim->GetController())) return;
    if (!bIgnoreArmor) { Damage = SFGear::MitigatedDamage(Damage, SFGear::Equipped(Victim->GetController()).Armor); }
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
        if (Victim->GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease") &&
            !USFBackpackComponent::Services())
        {
            // This isolated health-only fixture waits for the dead owner's release intent.
            UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_SERVER_DEAD Pawn=%s X=%.0f Y=%.0f"),
                *Victim->GetName(), Victim->GetActorLocation().X, Victim->GetActorLocation().Y);
        }
        else if (Victim->GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept") &&
            !USFBackpackComponent::Services())
        {
            const double ExpiresAt = Mode->GetWorld()->GetTimeSeconds() + Mode->CapturedSettings->ResAcceptSeconds;
            Victim->BeginResurrectionOffer(ExpiresAt); // Sandbox ability-offer fixture; only the dead owner may accept.
            const uint32 OfferId = Victim->GetResurrectionOfferId();
            UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_SERVER_OFFER Pawn=%s Offer=%u HP=0 ExpiresAt=%.2f"),
                *Victim->GetName(), OfferId, ExpiresAt);
            FTimerHandle Timer;
            Mode->GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(Mode,
                [Mode, Weak, OfferId]()
                {
                    if (Weak.IsValid() && Weak->CombatHealth() == 0 && Weak->GetResurrectionOfferId() == OfferId)
                    {
                        Weak->ClearResurrectionOffer();
                        Mode->RespawnPlayer(Weak);
                    }
                }), Mode->CapturedSettings->ResAcceptSeconds, false);
        }
        else if (USFBackpackComponent::Services() && Pack &&
            (Pack->HasVerifiedIdentity() || Pack->HasDevIdentity()))
        {
            FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::ReportDeath;
            Request.DeathEventId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
            Request.CharacterId = Pack->GetCharacterId(); Request.DeathAtUnixMs = UnixNowMs();
            Request.ZoneId = FPackageName::GetShortName(Victim->GetWorld()->GetOutermost()->GetName()).ToLower();
            Request.Position = Victim->GetActorLocation(); Request.RespawnPosition = Mode->ChooseRespawnPoint(Request.Position);
            const bool bGreenlands = Victim->GetWorld()->GetOutermost()->GetName() ==
                TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
            Request.RespawnDelayMs = FMath::RoundToInt((bGreenlands ? Mode->CapturedSettings->ResAcceptSeconds :
                Mode->CapturedSettings->PlayerRespawnDelaySeconds) * 1000);
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

bool ASFGameMode::AcceptResurrection(ASFE1Character* Player, uint32 OfferId, const FString& DeathEventId)
{
    const FString Map = GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString();
    const bool bSandbox = Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_ResurrectionAccept");
    const bool bGreenlands = Map == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
    if ((!bSandbox && !bGreenlands) || (bSandbox && USFBackpackComponent::Services()) ||
        !HasAuthority() || !bPilotActive || !CapturedSettings ||
        !IsValid(Player) || !Player->CanAcceptResurrectionOffer(OfferId, GetWorld()->GetTimeSeconds()))
    {
        UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_SERVER_DENIED Pawn=%s Offer=%u"), *GetNameSafe(Player), OfferId);
        return false;
    }
    if (bGreenlands)
    {
        const auto Weak = TWeakObjectPtr<ASFE1Character>(Player);
        if (!USFBackpackComponent::Services() || DeathEventId.IsEmpty() ||
            PlayerDeathEvents.FindRef(Weak) != DeathEventId ||
            Player->GetResurrectionOfferDeathEventId() != DeathEventId ||
            PlayerRespawnInFlight.Contains(Weak))
        {
            UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_SERVER_DENIED Pawn=%s Offer=%u Reason=DeathEvent"),
                *Player->GetName(), OfferId);
            return false;
        }
        RespawnPlayer(Weak, FSFResurrectionAcceptRules::RestoredHealth(CapturedSettings->PlayerHealth,
            CapturedSettings->PlayerResFraction), true);
        return true;
    }
    if (!Player->AcceptResurrectionOffer(OfferId, GetWorld()->GetTimeSeconds(),
        CapturedSettings->PlayerHealth, CapturedSettings->PlayerResFraction)) return false;
    UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_SERVER_ACCEPTED Pawn=%s Offer=%u HP=%d"),
        *Player->GetName(), OfferId, Player->CombatHealth());
    return true;
}

bool ASFGameMode::ReleaseSpirit(ASFE1Character* Player)
{
    const FString Map = GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString();
    const bool bSandbox = Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease");
    const bool bGreenlands = Map == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
    if (!FSFSpiritReleaseRules::CanRelease(bSandbox || bGreenlands, HasAuthority(), IsValid(Player) &&
        Player->GetController() != nullptr, IsValid(Player) ? Player->CombatHealth() : -1, !RespawnPoints.IsEmpty()) ||
        !bPilotActive || !CapturedSettings || (bSandbox && USFBackpackComponent::Services()))
    {
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_SERVER_DENIED Pawn=%s"), *GetNameSafe(Player));
        return false;
    }
    if (bGreenlands)
    {
        const auto Weak = TWeakObjectPtr<ASFE1Character>(Player);
        if (!USFBackpackComponent::Services() || !PlayerDeathEvents.Contains(Weak) ||
            PlayerRespawnInFlight.Contains(Weak)) return false;
        RespawnPlayer(Weak, FSFSpiritReleaseRules::RestoredHealth(CapturedSettings->PlayerHealth,
            CapturedSettings->SpiritResFraction));
        return true;
    }
    const FVector At = Grounded(GetWorld(), ChooseRespawnPoint(Player->GetActorLocation()),
        Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    if (!Player->TeleportTo(At, Player->GetActorRotation(), false, false))
    {
        UE_LOG(LogTemp, Warning, TEXT("SPIRIT_RELEASE_SERVER_DENIED Pawn=%s Reason=Teleport"), *GetNameSafe(Player));
        return false;
    }
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->CombatSetHealth(FSFSpiritReleaseRules::RestoredHealth(CapturedSettings->PlayerHealth,
        CapturedSettings->SpiritResFraction));
    UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_SERVER_ACCEPTED Pawn=%s HP=%d X=%.0f Y=%.0f"),
        *Player->GetName(), Player->CombatHealth(), At.X, At.Y);
    return true;
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
        FString Resolution;
        FParse::Value(FCommandLine::Get(), TEXT("DeathResolution="), Resolution);
        if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsDeathNetcheck")) &&
            Resolution == TEXT("Resurrection") && RemainingMs > 0)
        {
            Player->BeginResurrectionOffer(Mode->GetWorld()->GetTimeSeconds() + RemainingMs / 1000.0, Request.DeathEventId);
            UE_LOG(LogTemp, Display, TEXT("COMBAT_DEATH_FIXTURE_OFFER Event=%s Offer=%u"),
                *Request.DeathEventId, Player->GetResurrectionOfferId());
        }
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
    FString Resolution;
    FParse::Value(FCommandLine::Get(), TEXT("DeathResolution="), Resolution);
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsDeathNetcheck")) &&
        Resolution == TEXT("Resurrection") && RemainingMs > 0)
    { Player->BeginResurrectionOffer(GetWorld()->GetTimeSeconds() + RemainingMs / 1000.0, State.DeathEventId); }
    FTimerHandle Timer;
    GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this,
        [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Weak]() { if (ASFGameMode* M = WeakMode.Get()) M->RespawnPlayer(Weak); }), FMath::Max(0.01, RemainingMs / 1000.0), false);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_DEATH_RESTORED Event=%s RemainingMs=%lld"), *State.DeathEventId, RemainingMs);
}

void ASFGameMode::RespawnPlayer(TWeakObjectPtr<ASFE1Character> Player, int32 RestoredHealth, bool bAtDeathPosition)
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
        Clear.bResurrectAtDeathPosition = bAtDeathPosition;
        PlayerRespawnInFlight.Add(Player);
        USFBackpackComponent::Services()->Call(Clear, [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Player, Clear, RestoredHealth, bAtDeathPosition](const FSFGameplayResponse& Reply)
        {
            ASFGameMode* Mode = WeakMode.Get(); if (!Mode) return;
            Mode->PlayerRespawnInFlight.Remove(Player);
            if (!Player.IsValid() || Mode->PlayerDeathEvents.FindRef(Player) != Clear.DeathEventId) return;
            if (Reply.Status == ESFGameplayStatus::Ok && !Reply.bIsDead)
            {
                Mode->PlayerDeathEvents.Remove(Player);
                Mode->RespawnPlayer(Player, RestoredHealth, bAtDeathPosition);
                UE_LOG(LogTemp, Display, TEXT("COMBAT_DEATH_RESOLVED Event=%s HP=%d Mode=%s X=%.0f Y=%.0f"),
                    *Clear.DeathEventId, Player->CombatHealth(),
                    RestoredHealth < 0 ? TEXT("Automatic") : bAtDeathPosition ? TEXT("Resurrection") : TEXT("Release"),
                    Player->GetActorLocation().X, Player->GetActorLocation().Y);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("COMBAT_DEATH_CLEAR_RETRY Character=%s Code=%d"), *Clear.CharacterId, Reply.GrpcCode);
                FTimerHandle Retry; Mode->GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(Mode,
                    [WeakMode, Player, RestoredHealth, bAtDeathPosition]() { if (ASFGameMode* M = WeakMode.Get()) M->RespawnPlayer(Player, RestoredHealth, bAtDeathPosition); }), 3.0f, false);
            }
        });
        return;
    }
    const FVector At = Grounded(GetWorld(), bAtDeathPosition ? Player->GetActorLocation() : Nearest,
        Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    Player->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->CombatSetHealth(RestoredHealth > 0 ? RestoredHealth : CapturedSettings->PlayerHealth);
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
    TArray<APlayerController*> Eligible;
    if (!bProtected && KillerController)
    {
        const FSFPartyModel::FParty* Party = PartyModel.FindParty(KillerController->GetUniqueID());
        const float RadiusCm = FMath::Clamp(GetDefault<USFPartySettings>()->KillXpRadiusMeters, 1.0f, 200.0f) * 100.0f;
        for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* Player = Cast<APlayerController>(It->Get());
            const ASFE1Character* Character = IsValid(Player) ? Cast<ASFE1Character>(Player->GetPawn()) : nullptr;
            if (!Character) { continue; }
            if (Player == KillerController || (Party && Party->Members.Contains(Player->GetUniqueID()) &&
                Character->CombatHealth() > 0 && FVector::Dist(Character->GetActorLocation(), BoarAt) <= RadiusCm))
            { Eligible.Add(Player); }
        }
        if (!Eligible.IsEmpty() && !CreditedXpDeaths.Contains(DeathId))
        {
            int32 ReferenceLevel = 1;
            for (APlayerController* Player : Eligible)
            { if (const USFGatheringPilotComponent* Pilot = Player->FindComponentByClass<USFGatheringPilotComponent>())
                { ReferenceLevel = FMath::Max(ReferenceLevel, Pilot->GetCharacterLevel()); } }
            CreditedXpDeaths.Add(DeathId);
            for (APlayerController* Player : Eligible)
            { SFQuestPilotEvents::OnBoarDefeated().Broadcast(Player, DeathId, Eligible.Num(), ReferenceLevel); }
        }
    }
    if (USFBackpackComponent::Services())
    {
        bool bSeen = false; RewardedDeaths.Add(DeathId, &bSeen);
        if (bSeen) { return 0; }
        USFBackpackComponent* KillerBackpack = USFBackpackComponent::FindOrCreate(KillerController);
        const FString KillerId = KillerBackpack ? KillerBackpack->GetCharacterId() : FString();
        const bool bBoarSafe = IsSafe(BoarAt);
        // The process nonce prevents a restarted server's slot serials from reusing a durable defeat ID.
        static const FString Epoch = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        for (APlayerController* Recipient : Eligible)
        {
            USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Recipient);
            const ASFE1Character* Character = Cast<ASFE1Character>(Recipient->GetPawn());
            RetryDefeatReward(DeathId, Epoch + TEXT(":") + DeathId, KillerId,
                Backpack ? Backpack->GetCharacterId() : FString(), Backpack ? Backpack->GetContainerId() : FString(),
                !Character || IsSafe(Character->GetActorLocation()), bBoarSafe, Recipient);
        }
        return 0;
    }
    int32 Granted = 0;
    for (APlayerController* Recipient : Eligible)
    {
        const FString RecipientDeath = DeathId + TEXT(":") + FString::FromInt(Recipient->GetUniqueID());
        const int32 Hides = SFCombatPilotRewards::HidesFor(RewardedDeaths, RecipientDeath, false, CapturedSettings->HidesPerBoar);
        if (Hides > 0 && SFCombatPilotRewards::GrantHides(Recipient, Hides)) { Granted += Hides; }
    }
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_REWARD Death=%s Hides=%d Granted=%d Total=%d"),
        *DeathId, Granted, Granted > 0, SFCombatPilotRewards::CountHides(KillerController));
    return Granted;
}

void ASFGameMode::RetryDefeatReward(const FString& DeathId, const FString& ServiceEventId, const FString& KillerId, const FString& CharacterId,
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
                [WeakMode = TWeakObjectPtr<ASFGameMode>(this), DeathId, ServiceEventId, KillerId, CharacterId, ContainerId,
                 bKillerSafe, bBoarSafe, Controller]()
                { if (ASFGameMode* Mode = WeakMode.Get()) Mode->RetryDefeatReward(DeathId, ServiceEventId, KillerId, CharacterId, ContainerId, bKillerSafe, bBoarSafe, Controller); }), 3.0f, false);
        }
        return;
    }
    if ((!CharacterId.IsEmpty() && Backpack->GetCharacterId() != CharacterId) ||
        (!ContainerId.IsEmpty() && Backpack->GetContainerId() != ContainerId)) return;
    const FString RecipientId = Backpack->GetCharacterId(), RecipientContainer = Backpack->GetContainerId();
    auto Receipt = MakeShared<FSFGameplayResponse>();
    FSFBackpackMutation Mutation; Mutation.Label = TEXT("defeat_reward"); Mutation.OperationId = ServiceEventId;
    Mutation.bRetryStale = false;
    Mutation.Run = [Receipt, ServiceEventId, KillerId, RecipientId, RecipientContainer, bKillerSafe, bBoarSafe](const FString&, int64 Expected,
        TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        FSFGameplayRequest Q = SFCombatPilotRewards::BuildDefeatRequest(ServiceEventId, KillerId, RecipientId,
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
        KillerId, RecipientId, RecipientContainer, bKillerSafe, bBoarSafe, Controller, Weak = TWeakObjectPtr<USFBackpackComponent>(Backpack)](ESFBackpackOutcome Outcome)
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
            // The host may replay a completed intent after a restart; project the ledger snapshot.
            if (USFBackpackComponent* Owner = Weak.Get()) { Owner->Reconcile(); }
        }
        else if (Outcome == ESFBackpackOutcome::Unavailable || Outcome == ESFBackpackOutcome::Stale)
        {
            UE_LOG(LogTemp, Warning, TEXT("COMBAT_PILOT_REWARD_HELD Death=%s Code=%d"), *DeathId, Receipt->GrpcCode);
            FTimerHandle Retry;
            Mode->GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(Mode,
                [WeakMode, DeathId, ServiceEventId, KillerId, RecipientId, RecipientContainer, bKillerSafe, bBoarSafe, Controller]()
                { if (ASFGameMode* M = WeakMode.Get()) M->RetryDefeatReward(DeathId, ServiceEventId, KillerId, RecipientId, RecipientContainer, bKillerSafe, bBoarSafe, Controller); }), 3.0f, false);
        }
    };
    Backpack->Enqueue(MoveTemp(Mutation));
}

void ASFGameMode::BoarDied(ASFPilotBoar* Boar, ASFE1Character* Killer)
{
    if (!bPilotActive || !IsValid(Boar)) { return; }
    BoarCombatStreams.Remove(Boar);
    if (bCompanionSandbox)
    {
        if (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_PersonalLoot"))
        { RewardKill(Boar->DeathId(), Boar->GetActorLocation(), Killer); }
        if (bRespawnSandbox)
        {
            int32 PlayersInArea = 0;
            const TArray<FVector2D> Area = { FVector2D(-85, -85), FVector2D(85, -85),
                FVector2D(85, 85), FVector2D(-85, 85) };
            for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            { if (const APawn* Pawn = It->Get()->GetPawn())
              { if (IsInsideTerritory(Pawn->GetActorLocation(), Area)) { ++PlayersInArea; } } }
            const double Now = GetWorld()->GetTimeSeconds();
            const double Delay = FSFCommonRespawn::DelaySeconds(CapturedSettings->CommonRespawnSeconds,
                PlayersInArea, CapturedSettings->RespawnPlayerSlope, CapturedSettings->RespawnPlayerFloor,
                CapturedSettings->RespawnJitter, FMath::FRand());
            ++RespawnSandboxCycle;
            RespawnSandboxDueAt = Now + Delay;
            UE_LOG(LogTemp, Display, TEXT("BOAR_RESPAWN_DEAD Cycle=%d Players=%d Delay=%.3f Due=%.3f"),
                RespawnSandboxCycle, PlayersInArea, Delay, RespawnSandboxDueAt);
        }
        UE_LOG(LogTemp, Display, TEXT("COMPANION_SANDBOX_BOAR_DEAD Slot=%d"), Boar->SlotIndex());
        Boar->Destroy();
        return;
    }
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
