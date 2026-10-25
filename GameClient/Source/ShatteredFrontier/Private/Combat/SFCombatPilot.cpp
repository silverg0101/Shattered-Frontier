#include "Combat/SFCombatPilot.h"
#include "Combat/SFCastingComponent.h"
#include "Combat/SFCombatEnergy.h"
#include "Combat/SFStarterAbilities.h"
#include "Combat/SFCastingComponent.h"
#include "Combat/SFCommonRespawn.h"
#include "Combat/SFSpiritReleaseRules.h"
#include "Combat/SFResurrectionAcceptRules.h"
#include "Combat/SFFallDamage.h"
#include "Combat/SFTauntRules.h"
#include "Combat/SFBoarDodgeRules.h"
#include "Combat/SFCombatPart6.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Combat/SFStatusEffectsComponent.h"
#include "Inventory/SFGear.h"
#include "Inventory/SFInventory.h"
#include "Inventory/SFLootWorldSubsystem.h"
#include "Framework/SFGameMode.h"
#include "Combat/SFCombatPilotRewards.h"
#include "Characters/SFE1Character.h"
#include "Creatures/SFRestAreaVolume.h"
#include "Framework/SFPlayerController.h"
#include "Townsfolk/SFTownsfolk.h"
#include "Inventory/SFRepairStationActor.h"
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
#include "DrawDebugHelpers.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Quest/SFQuestPilotEvents.h"
#include "Quest/SFQuestPilot.h"
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

int32 FixtureRewardNpcCount(const TMap<FString, TWeakObjectPtr<ASFPilotBoar>>& Npcs)
{
    int32 Count = 0;
    for (const auto& Pair : Npcs)
    {
        const auto* Npc = Pair.Value.Get();
        const auto* Profile = Npc ? Npc->GetNpcProfile() : nullptr;
        if (Profile && Profile->bLocalHidesFixture) ++Count;
    }
    return Count;
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

bool SFCombatPilotRules::CanRanged(double DistanceCm, double RangeCm)
{
    return FMath::IsFinite(DistanceCm) && DistanceCm >= 0.0 && DistanceCm <= RangeCm;
}

bool SFCombatPilotRules::HasRangedLineOfSight(UWorld* World, const AActor* Source,
    const AActor* Target, const FVector& Start, const FVector& End)
{
    if (!World || !IsValid(Source) || !IsValid(Target)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatPilotRanged), false);
    Query.AddIgnoredActor(Source);
    Query.AddIgnoredActor(Target);
    return !World->LineTraceTestByChannel(Start, End, ECC_Visibility, Query);
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
        !FMath::IsWithinInclusive(CritBase, 0.0f, 0.15f) ||
        !FMath::IsWithinInclusive(BlockReduction, 0.10f, 0.40f) ||
        !FMath::IsWithinInclusive(SkillDamageFloor, 0.50f, 1.00f) ||
        !FMath::IsWithinInclusive(MissDivisor, 500.0f, 10000.0f) ||
        !FMath::IsWithinInclusive(PowerSpeedReferenceSeconds, 1.5f, 4.0f) ||
        !FMath::IsWithinInclusive(UnarmedDpsFactor, 0.10f, 1.00f) ||
        !FMath::IsWithinInclusive(WeaponSpread, 0.0f, 0.30f) ||
        !FMath::IsWithinInclusive(WeaponDpsK, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(ArmorK, 5.0f, 100.0f) ||
        !FMath::IsWithinInclusive(BeastDpsCoefficient, 0.005f, 0.080f) ||
        !FMath::IsWithinInclusive(BeastSwingSeconds, 1.0f, 3.5f) ||
        !FMath::IsWithinInclusive(NpcCritDamage, 1.0f, 2.0f) ||
        !FMath::IsWithinInclusive(NpcCritCap, 0.05f, 0.40f) ||
        !FMath::IsWithinInclusive(NpcDodge, 0.0f, 0.15f) ||
        !FMath::IsWithinInclusive(NpcParry, 0.0f, 0.15f) ||
        NpcDodge + NpcParry > AvoidanceCap ||
        !FMath::IsWithinInclusive(NpcMediumArmor, 0.05f, 2.0f) ||
        !FMath::IsWithinInclusive(NpcMediumResistance, 0.05f, 2.0f) ||
        !FMath::IsWithinInclusive(DevPilotDamageUnit, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(DevPilotArmorUnit, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(PlayerMeleeCooldownSeconds, 0.3f, 5.0f) ||
        !FMath::IsWithinInclusive(BoarMeleeCooldownSeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(MeleeRangeMetres, 1.0f, 5.0f) ||
        !FMath::IsWithinInclusive(StealthPracticeIntervalSeconds, 0.5f, 10.0f) ||
        !FMath::IsWithinInclusive(StealthPracticeMinimumSpeedCmPerSecond, 1.0f, 10.0f) ||
        !FMath::IsWithinInclusive(TabTargetRangeMetres, 10.0f, 60.0f) ||
        !FMath::IsWithinInclusive(TabTargetDropRangeMetres, TabTargetRangeMetres, 100.0f) ||
        !FMath::IsWithinInclusive(TabTargetConeDegrees, 60.0f, 360.0f) ||
        !FMath::IsWithinInclusive(TabTargetCycleResetSeconds, 0.5f, 5.0f) ||
        !FMath::IsWithinInclusive(PlayerRespawnDelaySeconds, 1.0f, 60.0f) ||
        !FMath::IsNearlyEqual(SpiritResFraction, 0.5f) ||
        !FMath::IsNearlyEqual(PlayerResFraction, 0.35f) ||
        !FMath::IsNearlyEqual(ResAcceptSeconds, 60.0f) ||
        (PlayerRespawnPointRule != ESFPlayerRespawnPointRule::NearestToDeath && PlayerRespawnPointRule != ESFPlayerRespawnPointRule::FirstAuthored) ||
        !FMath::IsWithinInclusive(ThreatSwitchMelee, 1.01f, 3.0f) ||
        !FMath::IsWithinInclusive(ThreatSwitchRanged, ThreatSwitchMelee, 3.0f) ||
        ThreatTableMax < 1 || ThreatTableMax > 40 ||
        !FMath::IsWithinInclusive(ThreatDecisionSeconds, 0.05f, 1.0f) ||
        !FMath::IsNearlyEqual(TauntThreatMultiplier, 1.10f) ||
        !FMath::IsNearlyEqual(TauntForceSeconds, 3.0f) ||
        !FMath::IsNearlyEqual(TauntDrWindowSeconds, 18.0f) ||
        !FMath::IsNearlyEqual(CommonRespawnSeconds, 300.0f) ||
        !FMath::IsNearlyEqual(RespawnPlayerSlope, 0.15f) ||
        !FMath::IsNearlyEqual(RespawnPlayerFloor, 0.35f) ||
        !FMath::IsNearlyEqual(RespawnJitter, 0.10f) ||
        HidesPerBoar < 1 || HidesPerBoar > 5 ||
        !FMath::IsNearlyEqual(CombatExitSeconds, 5.0f) || !FMath::IsNearlyEqual(RegenDelaySeconds, 5.0f) ||
        !FMath::IsNearlyEqual(HealthRegenOocPct, 0.03f))
    { Error = TEXT("Combat pilot balance is outside declared bounds"); return false; }
    return true;
}

const FName ASFPilotBoar::OutOfPlaceNpcTag(TEXT("SF_OutOfPlaceNpc"));

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
    Casting = CreateDefaultSubobject<USFCastingComponent>(TEXT("Casting"));
    StatusEffects = CreateDefaultSubobject<USFStatusEffectsComponent>(TEXT("StatusEffects"));
    Label->SetupAttachment(GetCapsuleComponent());
    Label->SetRelativeLocation(FVector(0, 0, 130));
    Label->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
    Label->SetWorldSize(34);
    SetNetCullDistanceSquared(FMath::Square(80000.0f));
}

const SFNpcProfiles::FBehavior& ASFPilotBoar::GetBehavior() const
{
    if (const auto* Profile = GetNpcProfile()) return Profile->Behavior;
    static const SFNpcProfiles::FBehavior Invalid;
    return Invalid; // Unknown definitions are never configured; fail closed if an old serialized actor is invalid.
}

void ASFPilotBoar::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyPresentation();
}

void ASFPilotBoar::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    ApplyPresentation();
}

void ASFPilotBoar::ApplyPresentation()
{
    const auto* Profile = GetNpcProfile();
    if (!Profile || !Body || !Label) return;
    const auto& Presentation = Profile->Presentation;
    GetCapsuleComponent()->SetCapsuleSize(Presentation.CapsuleRadiusCm, Presentation.CapsuleHalfHeightCm);
    Body->SetRelativeScale3D(Presentation.BodyScale);
    Label->SetRelativeLocation(FVector(0, 0, Presentation.LabelHeightCm));
    Label->SetWorldSize(Presentation.LabelWorldSizeCm);
    // Static-body mode has no animation asset. Asset references are release-authored, never client supplied.
    if (!IsRunningDedicatedServer())
    {
        UStaticMesh* BodyMesh = Cast<UStaticMesh>(Presentation.StaticMeshPath.TryLoad());
        if (!BodyMesh) { UE_LOG(LogTemp, Error, TEXT("NPC_PRESENTATION_UNAVAILABLE Definition=%s Mesh=%s"), *CreatureId, *Presentation.StaticMeshPath.ToString()); }
        Body->SetStaticMesh(BodyMesh);
    }
}

void ASFPilotBoar::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFPilotBoar, CurrentHealth);
    DOREPLIFETIME(ASFPilotBoar, MaximumHealthValue);
    DOREPLIFETIME(ASFPilotBoar, SpawnGroup);
    DOREPLIFETIME(ASFPilotBoar, CreatureId);
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
    Anchor = GetActorLocation(); AnchorFacing = GetActorRotation(); CurrentHealth = Health; MaximumHealthValue = Health; OnRep_Health();
    if (SFCombatPart6::Active(GetWorld()) && OwnerMode)
    {
        const auto* Tune = OwnerMode->CombatSettings();
        const auto Npc = SFCombatPart6::Npc(CreatureId);
        CombatSwingSecondsValue = Npc.SwingSeconds;
        CombatSwingDamageValue = Npc.SwingDamage;
        CombatArmorValue = Npc.Armor;
        CombatResistanceValue = Npc.Resistance;
        CombatDodgeValue = Tune->NpcDodge;
        CombatParryValue = Tune->NpcParry;
    }
    LastDamageAt = GetWorld()->GetTimeSeconds();
    Threat.Clear(); ThreatActors.Empty(); OutsideLeashSince.Empty(); TargetCharacterId.Empty(); ForcedTauntId.Empty(); ForcedTauntUntil = 0;
    LastTauntAt = -1; TauntApplications = 0;
    RecentAttackers = FSFRecentAttackerMemory(); DamageAttackers.Empty(); PendingResetAttackers.Empty();
    ReplicatedTarget = nullptr; NextThreatDecisionAt = 0; EngagedSince = -1;
}

bool ASFPilotBoar::ConfigureCreature(const FString& DefinitionId)
{
    if (!HasAuthority()) return false;
    FString TableId;
    bool bUnique = false;
    if (!ResolveCreatureLoot(DefinitionId, TableId, bUnique))
    { UE_LOG(LogTemp, Error, TEXT("NPC_PROFILE_UNKNOWN Definition=%s"), *DefinitionId); return false; }
    CreatureId = DefinitionId; LootTableId = TableId; bSingleDrop = bUnique;
    ApplyPresentation();
    return true;
}

bool ASFPilotBoar::ResolveCreatureLoot(const FString& DefinitionId, FString& TableId, bool& bUnique)
{
    const auto* Profile = SFNpcProfiles::Find(DefinitionId);
    if (!Profile) return false;
    TableId = Profile->LootTableId; bUnique = Profile->bSingleDrop;
    return true;
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
        FVector::Dist2D(Anchor, Character->GetActorLocation()) > GetBehavior().LeashRadiusMetres * 100.0)
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
    if (StatusEffects) StatusEffects->ClearAll();
    AbilityRootUntil = AbilitySnareUntil = 0;
    AbilitySnareMultiplier = 1;
    if (Casting) { Casting->CancelNpcCast(); }
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
    const FString Name = CreatureId.RightChop(4).Replace(TEXT("_"), TEXT(" "));
    Label->SetText(FText::FromString(FString::Printf(TEXT("%s L%d  %d HP"), *Name,
        SFCombatPart6::Npc(CreatureId).Level, CurrentHealth)));
    Label->SetTextRenderColor(CurrentHealth > 0 ? FColor::Yellow : FColor::Red);
}

void ASFPilotBoar::CompleteNpcMend(float HealthFraction)
{
    if (!HasAuthority() || !CanCastMend() || CurrentHealth <= 0 || bReturning) return;
    const int32 Before = CurrentHealth;
    CurrentHealth = FMath::Min(MaximumHealthValue,
        CurrentHealth + FMath::RoundToInt(MaximumHealthValue * HealthFraction));
    OnRep_Health(); ForceNetUpdate();
    UE_LOG(LogTemp, Display, TEXT("NPC_CAST_COMPLETE Creature=%s Ability=mend Heal=%d HP=%d"),
        *CreatureId, CurrentHealth - Before, CurrentHealth);
}

void ASFPilotBoar::OnRep_CreatureId() { ApplyPresentation(); OnRep_Health(); }

void ASFPilotBoar::MulticastArcherTracer_Implementation(FVector_NetQuantize Start, FVector_NetQuantize End)
{
    DrawDebugLine(GetWorld(), Start, End, FColor(255, 205, 90), false, 0.22f, 0, 2.0f);
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetNetMode() == NM_Client && FParse::Param(FCommandLine::Get(), TEXT("GreenlandsArcherNetcheck")))
    { UE_LOG(LogTemp, Display, TEXT("ARCHER_TRACER_RECEIVED Creature=%s"), *CreatureId); }
#endif
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

bool ASFPilotBoar::HasThreatOn(ASFE1Character* Character) const
{
    const FString Id = IsValid(Character) ? ThreatIdentity(Cast<APlayerController>(Character->GetController())) : FString();
    return !Id.IsEmpty() && Threat.Contains(Id) && CurrentHealth > 0 && !bReturning;
}

void ASFPilotBoar::ReceivePilotHealingThreat(ASFE1Character* Healer, ASFE1Character* Subject, double Amount)
{
    if (!HasAuthority() || !IsValid(Healer) || !HasThreatOn(Subject) || Amount <= 0 || !PilotMode.IsValid()) return;
    const FString Id = ThreatIdentity(Cast<APlayerController>(Healer->GetController()));
    if (Id.IsEmpty()) return;
    Threat.Add(Id, Amount * Healer->ThreatMultiplier(), PilotMode->CombatSettings()->ThreatTableMax);
    ThreatActors.Add(Id, Healer);
    Healer->CombatMarkHostile();
}

void ASFPilotBoar::ApplyAbilitySnare(double Until, double SpeedMultiplier)
{
    if (!HasAuthority() || CurrentHealth <= 0) return;
    AbilitySnareUntil = FMath::Max(AbilitySnareUntil, Until);
    AbilitySnareMultiplier = FMath::Min(AbilitySnareMultiplier, SpeedMultiplier);
}

float ASFPilotBoar::ApplyAbilityRoot(double Until)
{
    if (!HasAuthority() || CurrentHealth <= 0) return 0.f;
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - AbilityRootLastAt > 18.0) AbilityRootApplications = 0;
    const int32 Percent = SFCrowdControlRules::DrStepPercent(AbilityRootApplications++);
    AbilityRootLastAt = Now;
    AbilityRootUntil = FMath::Max(AbilityRootUntil, Now + (Until - Now) * Percent / 100.0);
    return FMath::Max(0.0, (Until - Now) * Percent / 100.0);
}

void ASFPilotBoar::DropAbilityThreat(ASFE1Character* Character)
{
    if (!HasAuthority() || !IsValid(Character)) return;
    for (auto It = ThreatActors.CreateIterator(); It; ++It)
        if (It.Value().Get() == Character)
        { Threat.Remove(It.Key()); OutsideLeashSince.Remove(It.Key()); It.RemoveCurrent(); }
    if (Target.Get() == Character) { Target.Reset(); ReplicatedTarget = nullptr; TargetCharacterId.Empty(); }
}

void ASFPilotBoar::RetaliateAgainst(ASFE1Character* Attacker)
{
    if (!HasAuthority() || !IsValid(Attacker) || !Attacker->HasAuthority() ||
        Attacker->GetWorld() != GetWorld() || Attacker->CombatHealth() <= 0) return;
    ASFGameMode::HitPlayer(this, Attacker, Attacker->CombatHealth(), true, true);
}

float ASFPilotBoar::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
    AController* EventInstigator, AActor* DamageCauser)
{
    if (!ActorHasTag(OutOfPlaceNpcTag))
        return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
    if (FMath::IsFinite(DamageAmount) && DamageAmount > 0)
    {
        APawn* InstigatorPawn = nullptr;
        if (IsValid(EventInstigator)) InstigatorPawn = EventInstigator->GetPawn();
        else if (IsValid(DamageCauser)) InstigatorPawn = DamageCauser->GetInstigator();
        ASFE1Character* Attacker = Cast<ASFE1Character>(InstigatorPawn);
        if (!Attacker) Attacker = Cast<ASFE1Character>(DamageCauser);
        RetaliateAgainst(Attacker);
    }
    return 0; // Engine damage cannot bypass the same immunity as resolved combat hits.
}

void ASFPilotBoar::ReceivePilotDamage(ASFE1Character* Attacker, int32 Damage, double AbilityThreatMultiplier)
{
    if (ActorHasTag(OutOfPlaceNpcTag))
    {
        if (Damage > 0) RetaliateAgainst(Attacker);
        return;
    }
    if (!HasAuthority() || CurrentHealth <= 0 || bReturning || !IsValid(Attacker) || Damage <= 0) { return; }
    const int32 EffectiveDamage = FMath::Min(CurrentHealth, Damage);
    Attacker->CombatMarkHostile();
    const FString AttackerId = ThreatIdentity(Cast<APlayerController>(Attacker->GetController()));
    if (!AttackerId.IsEmpty())
    {
        Threat.Add(AttackerId, EffectiveDamage * AbilityThreatMultiplier * Attacker->ThreatMultiplier(),
            PilotMode->CombatSettings()->ThreatTableMax);
        ThreatActors.Add(AttackerId, Attacker);
        DamageAttackers.Add(AttackerId);
    }
    CurrentHealth = SFCombatPilotRules::RemainingHealth(CurrentHealth, Damage);
    if (CurrentHealth == 0 && StatusEffects) StatusEffects->ClearAll();
    if (CurrentHealth == 0 && Casting) { Casting->CancelNpcCast(); }
    else if (Casting) { Casting->NotifyDamage(); }
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
    if (ActorHasTag(OutOfPlaceNpcTag)) return; // This NPC retaliates; it never hunts players.
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!HasAuthority() && !bClientLoggedNpcCast && CanCastMend() &&
        FParse::Param(FCommandLine::Get(), TEXT("GreenlandsAbilityNetcheck")) && Casting &&
        Casting->GetCastView().Phase == ESFCastPhase::Casting)
    {
        bClientLoggedNpcCast = true;
        UE_LOG(LogTemp, Display, TEXT("NPC_CAST_CLIENT_VIEW Ability=%d EndsAt=%.2f"),
            Casting->GetCastView().AbilityId, Casting->GetCastView().EndsAt);
    }
#endif
    if (!HasAuthority() || CurrentHealth <= 0 || !PilotMode.IsValid() || !PilotMode->IsCombatActive()) { return; }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Keep one shared target alive across the staged client reconnect in this network fixture.
    if (FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario"))) { return; }
#endif
    auto* Mode = PilotMode.Get();
    const double Now = GetWorld()->GetTimeSeconds();
    ASFE1Character* Victim = Target.Get();
    if (Victim && FVector::Dist2D(Anchor, GetActorLocation()) > GetBehavior().LeashRadiusMetres*100)
    { EnterEvade(); Victim = nullptr; }
    if (Victim && UnreachableSince >= 0 &&
        SFCombatPilotRules::UnreachableEvade(Now - UnreachableSince, Now - LastDamageAt))
    { EnterEvade(); Victim = nullptr; }
    if (bReturning && FVector::Dist2D(GetActorLocation(),Anchor) < 100)
    {
        SetActorLocation(Anchor, false);
        SetActorRotation(AnchorFacing);
        CurrentHealth = MaximumHealthValue;
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
            if (IsValid(Candidate) && Candidate->HasCombatStealth(Now) && ThreatTarget())
            { Mode->TryHiddenMovementPractice(Candidate, this, Now); }
            // A linkdead (draining) character stays visible but is never chosen as prey (INT-05).
            if (IsValid(Candidate) && Candidate->CombatHealth() > 0 && !Candidate->HasCombatStealth(Now) &&
                !USFCharacterSessionComponent::IsLinkdead(It->Get()) &&
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
                    const double Radius = SFCombatPilotRules::AggroRadiusMetres(GetBehavior().AggroRadiusMetres,
                        SFCombatPart6::Npc(CreatureId).Level, CharacterLevel,
                        GetBehavior().AggroMinimumMetres, GetBehavior().AggroMaximumBonusMetres);
                    const double Facing = FVector::DotProduct(GetActorForwardVector(), Toward.GetSafeNormal2D());
                    if (SpawnGroup == TEXT("aggro_sandbox") && Distance < 1500 && Now >= NextAggroFixtureTraceAt)
                    {
                        NextAggroFixtureTraceAt = Now + 1.0;
                        UE_LOG(LogTemp, Display, TEXT("BOAR_AGGRO_EVAL Level=%d DistanceCm=%.0f RadiusCm=%.0f Facing=%.2f Yaw=%.0f"),
                            CharacterLevel, Distance, Radius * 100.0, Facing, GetActorRotation().Yaw);
                    }
                    if (Distance <= Radius * 100.0 && (Distance <= GetBehavior().NearNoticeRadiusMetres * 100.0 ||
                        Facing >= FMath::Cos(FMath::DegreesToRadians(GetBehavior().FieldOfViewDegrees * 0.5))))
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
                            GetBehavior().NearNoticeRadiusMetres, GetBehavior().FieldOfViewDegrees))
                        {
                            Threat.Add(Id, 1.0, Mode->CombatSettings()->ThreatTableMax);
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
                if (AnchorDistance > GetBehavior().LeashRadiusMetres * 100.0)
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
            GetBehavior().AssistDelaySeconds, GetBehavior().AssistRadiusMetres))
    {
        for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
        {
            ASFPilotBoar* Packmate = *It;
            if (Packmate != this && Packmate->GroupId() == SpawnGroup &&
                SFCombatPilotRules::CanAssist(Now - EngagedSince,
                    FVector::Dist2D(GetActorLocation(), Packmate->GetActorLocation()) / 100.0,
                    GetBehavior().AssistDelaySeconds, GetBehavior().AssistRadiusMetres))
            {
                if (Packmate->ReceivePackAssist(Victim, TargetCharacterId))
                { UE_LOG(LogTemp, Display, TEXT("BOAR_PACK_ASSIST_TIMING Group=%s Elapsed=%.3f DistanceCm=%.0f"),
                    *SpawnGroup, Now - EngagedSince, FVector::Dist2D(GetActorLocation(), Packmate->GetActorLocation())); }
            }
        }
    }
    if (Casting && CanCastMend() && Victim && !bReturning)
    {
        if (Casting->GetCastView().Phase == ESFCastPhase::Casting) { return; }
        if (CurrentHealth < FMath::RoundToInt(MaximumHealthValue *
            Casting->GetNpcMendHealthThreshold()) && Casting->TryBeginNpcMend())
        { return; }
    }
    if ((SpawnGroup == TEXT("aggro_sandbox") || SpawnGroup == TEXT("memory_sandbox") ||
        SpawnGroup == TEXT("assist_sandbox") || SpawnGroup == TEXT("respawn_sandbox") ||
        SpawnGroup == TEXT("dodge_sandbox")) && !Victim && !bReturning) { return; }
    FVector Destination = WanderDestination;
    float Speed = GetBehavior().WanderSpeedMetresPerSecond*100;
    if (Victim)
    {
        const USFCombatPilotSettings* Settings = Mode->CombatSettings();
        Destination = Victim->GetActorLocation(); Speed = GetBehavior().ChaseSpeedMetresPerSecond*100;
        if (Now < AbilityRootUntil) Speed = 0;
        else if (Now < AbilitySnareUntil) Speed *= AbilitySnareMultiplier;
        else AbilitySnareMultiplier = 1;
        const double Distance = FVector::Dist2D(GetActorLocation(), Destination);
        const bool bRanged = IsRangedArcher();
        const FVector ShotStart = GetActorLocation() + FVector(0, 0, 80);
        const FVector ShotEnd = Victim->GetActorLocation() + FVector(0, 0, 60);
        const bool bClearShot = bRanged && SFCombatPilotRules::HasRangedLineOfSight(
            GetWorld(), this, Victim, ShotStart, ShotEnd);
        if (bRanged && bClearShot)
        {
            const FVector Away = (GetActorLocation() - Destination).GetSafeNormal2D();
            if (Distance < GetBehavior().RetreatRangeMetres * 100.0 && !Away.IsNearlyZero())
            {
                const FVector Retreat = GetActorLocation() + Away *
                    (GetBehavior().PreferredRangeMetres * 100.0 - Distance);
                if (Mode->IsInsideTerritory(Retreat, Territory) && !Mode->IsSafe(Retreat) &&
                    FVector::Dist2D(Anchor, Retreat) <= GetBehavior().LeashRadiusMetres * 100.0)
                { Destination = Retreat; }
                else { Destination = GetActorLocation(); }
            }
            else if (Distance <= GetBehavior().PreferredRangeMetres * 100.0)
            { Destination = GetActorLocation(); }
        }
        const double AttackRange = bRanged ? GetBehavior().RangedRangeMetres : GetBehavior().MeleeRangeMetres;
        if (Distance <= AttackRange * 100.0 && (!bRanged || bClearShot) && Now >= NextSwingAt)
        {
            NextSwingAt = Now + (bRanged ? GetBehavior().RangedSwingSeconds :
                (SFCombatPart6::Active(GetWorld()) ? CombatSwingSecondsValue : Settings->BoarMeleeCooldownSeconds));
            if (bRanged) { SetActorRotation((Victim->GetActorLocation() - GetActorLocation()).Rotation()); }
            Mode->TryBoarAttack(this, Victim); LastDamageAt = Now; UnreachableSince = -1;
        }
    }
    else if (bReturning)
    { Destination = Anchor; Speed = SFCombatPilotRules::EvadeReturnSpeedCmPerSecond(
        GetBehavior().ChaseSpeedMetresPerSecond); }
    else if (Now >= NextWanderAt || Destination.IsNearlyZero() || FVector::Dist2D(GetActorLocation(), Destination) < 80)
    {
        NextWanderAt = Now + FMath::FRandRange(double(GetBehavior().WanderMinSeconds),
            double(GetBehavior().WanderMaxSeconds));
        for (int32 Attempt = 0; Attempt < 12; ++Attempt)
        {
            const double Radius = GetBehavior().WanderRadiusMetres*100;
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
        const double Distance = FVector::Dist2D(GetActorLocation(), Victim->GetActorLocation());
        const bool bOutOfReach = IsRangedArcher() ?
            (Distance > GetBehavior().RangedRangeMetres * 100.0 ||
                !SFCombatPilotRules::HasRangedLineOfSight(GetWorld(), this, Victim,
                    GetActorLocation() + FVector(0, 0, 80), Victim->GetActorLocation() + FVector(0, 0, 60))) :
            Distance > GetBehavior().MeleeRangeMetres * 100.0;
        const bool bBlocked = FVector::Dist2D(BeforeMove, GetActorLocation()) < 1.0 && bOutOfReach;
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
    const FString MapId = GetWorld()->GetOutermost()->GetName();
    const FString Path = SFNpcZoneContent::FindMapFile(MapId);
    if (Path.IsEmpty()) return false;
    SFNpcZoneContent::FZone Zone;
    FString Error;
    if (!SFNpcZoneContent::Load(Path, MapId, Zone, Error))
    { UE_LOG(LogTemp, Error, TEXT("NPC_ZONE_REJECTED Map=%s %s"), *MapId, *Error); return false; }
    CombatZonePath = Path;
    Groups = MoveTemp(Zone.Groups);
    ProtectedAreas = MoveTemp(Zone.ProtectedAreas);
    RespawnPoints = MoveTemp(Zone.RespawnPoints);
    return true;
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
    const bool bOutOfPlaceSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_OutOfPlaceNpc");
    bRespawnSandbox = MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Respawn");
    bCompanionSandbox = GetNetMode() != NM_Client &&
        (MapName == TEXT("/Game/Frontier/Maps/Sandbox/SB_Companions") || bEvadeSandbox || bThreatSandbox || bTauntSandbox || bAggroSandbox || bMemorySandbox || bRegenSandbox || bRestRegenSandbox || bFallSandbox || bAssistSandbox || bRespawnSandbox || bAutoAttackSandbox || bDodgeSandbox || bUnstuckSandbox || bSpiritReleaseSandbox || bResurrectionAcceptSandbox || bPersonalLootSandbox || bOutOfPlaceSandbox);
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
            const FName Tag = bOutOfPlaceSandbox ? TEXT("SF_OutOfPlaceNpcSandboxWildlife") : bEvadeSandbox ? TEXT("SF_EvadeSandboxWildlife") :
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
            It->Initialize(this, bOutOfPlaceSandbox ? TEXT("out_of_place_sandbox") : bEvadeSandbox ? TEXT("evade_sandbox") :
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
    // Any authority with an approved matching zone record uses the common NPC runtime.
    bPilotActive = GetNetMode() != NM_Client && LoadZoneRecord();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bScenario = bPilotActive && (FParse::Param(FCommandLine::Get(), TEXT("CombatPilotScenario")) ||
        FParse::Param(FCommandLine::Get(), TEXT("CombatTargetScenario")) ||
        FParse::Param(FCommandLine::Get(), TEXT("CombatGroupScenario")));
#endif
    if (!bPilotActive) { return; }
    CapturedSettings = GetDefault<USFCombatPilotSettings>();
    FString Error;
    if (!CapturedSettings->Validate(Error))
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
        UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_READY Boars=%d Hostiles=%d Combat=%d HostileSpawning=%d"),
            FixtureRewardNpcCount(ActiveBoars), ActiveBoars.Num(),
            CapturedSettings->bCombatAvailable, CapturedSettings->bSilverbrookHostileSpawning);
    }
}

void ASFGameMode::PostLoginCombat(APlayerController* NewPlayer)
{
    if (bPilotActive && !USFBackpackComponent::Services()) { SFCombatPilotRewards::AddPilotBag(NewPlayer); }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetNumPlayers() == 2 && FParse::Param(FCommandLine::Get(), TEXT("GreenlandsUnstuckNetcheck")) &&
        GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook"))
    {
        TWeakObjectPtr<APlayerController> Driver;
        for (TActorIterator<APlayerController> It(GetWorld()); It; ++It)
        { if (*It != NewPlayer) { Driver = *It; break; } }
        const TWeakObjectPtr<APlayerController> Observer(NewPlayer);
        FTimerHandle UnstuckFixtureTimer;
        GetWorldTimerManager().SetTimer(UnstuckFixtureTimer, FTimerDelegate::CreateWeakLambda(this, [this, Driver, Observer]()
        {
            ASFE1Character* First = Driver.IsValid() ? Cast<ASFE1Character>(Driver->GetPawn()) : nullptr;
            ASFE1Character* Second = Observer.IsValid() ? Cast<ASFE1Character>(Observer->GetPawn()) : nullptr;
            if (!First || !Second) return;
            const FVector DryShore(50000, 37700, 0);
            First->SetActorLocation(Grounded(GetWorld(), DryShore, First->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
                false, nullptr, ETeleportType::TeleportPhysics);
            Second->SetActorLocation(Grounded(GetWorld(), DryShore + FVector(0,200,0),
                Second->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
            First->ForceNetUpdate(); Second->ForceNetUpdate();
            UE_LOG(LogTemp, Display, TEXT("UNSTUCK_FIXTURE_READY Driver=%s Observer=%s"),
                *First->GetActorLocation().ToString(), *Second->GetActorLocation().ToString());
        }), 1.5f, false);
    }
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
         FParse::Param(FCommandLine::Get(), TEXT("GreenlandsDeathNetcheck")) ||
         FParse::Param(FCommandLine::Get(), TEXT("GreenlandsEnergyNetcheck")) ||
         FParse::Param(FCommandLine::Get(), TEXT("GreenlandsAbilityNetcheck"))))
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
    { if (auto* Player = Cast<ASFE1Character>(NewPlayer ? NewPlayer->GetPawn() : nullptr)) Player->CombatSetHealth(Player->CombatMaximumHealth()); }
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
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsRepairNetcheck"))) { TickGreenlandsRepairNetcheck(); }
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsArcherNetcheck"))) { TickGreenlandsArcherNetcheck(); }
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsEnergyNetcheck"))) { TickGreenlandsEnergyNetcheck(); }
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsAbilityNetcheck"))) { TickGreenlandsAbilityNetcheck(); }
#endif
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void ASFGameMode::TickGreenlandsAbilityNetcheck()
{
    auto* Driver = Cast<ASFE1Character>(GreenlandsDriverController.IsValid() ? GreenlandsDriverController->GetPawn() : nullptr);
    auto* Observer = Cast<ASFE1Character>(GreenlandsObserverController.IsValid() ? GreenlandsObserverController->GetPawn() : nullptr);
    if (!Driver || !Observer || !Driver->HasCombatSnapshot() || !Observer->HasCombatSnapshot() ||
        USFCharacterSessionComponent::IsRestoring(Driver->GetController()) ||
        USFCharacterSessionComponent::IsRestoring(Observer->GetController())) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (FParse::Param(FCommandLine::Get(), TEXT("GreenlandsStatusNetcheck")))
    {
        if (GreenlandsAbilityStage == 0)
        {
            auto* Levels = Driver->GetController()->FindComponentByClass<USFGatheringPilotComponent>();
            if (!Levels || RespawnPoints.IsEmpty() || !IsSafe(RespawnPoints[0])) return;
            Levels->RestoreLevel(10); // Development fixture for the otherwise unavailable Healing rank.
            Driver->SetActorLocation(RespawnPoints[0], false, nullptr, ETeleportType::TeleportPhysics);
            Observer->SetActorLocation(RespawnPoints[0] + FVector(0, 100, 0), false, nullptr, ETeleportType::TeleportPhysics);
            Observer->CombatSetHealth(Observer->CombatMaximumHealth());
            Observer->StatusEffects->Apply(TEXT("netcheck/poison"), ESFStatusKind::Poison, ESFDispelType::Poison,
                Driver, 10.f, 0.f, 1, false);
            Observer->StatusEffects->Apply(TEXT("netcheck/curse"), ESFStatusKind::Curse, ESFDispelType::Curse,
                Driver, 10.f, 0.f, 1, false);
            GreenlandsAbilityStage = 1; GreenlandsAbilityStageAt = Now;
            UE_LOG(LogTemp, Display, TEXT("STATUS_NETCHECK_APPLIED Poison=1 Curse=1"));
        }
        else if (GreenlandsAbilityStage == 1 && Now - GreenlandsAbilityStageAt >= 1.0)
        {
            TryPlayerCombatAbility(Driver, TEXT("purify"), Observer);
            if (Observer->StatusEffects->GetStacks(TEXT("netcheck/poison"), Driver) != 0 ||
                Observer->StatusEffects->GetStacks(TEXT("netcheck/curse"), Driver) != 1)
            { UE_LOG(LogTemp, Error, TEXT("STATUS_NETCHECK_FAILED Purify")); GreenlandsAbilityStage = 99; return; }
            GreenlandsAbilityStage = 2; GreenlandsAbilityStageAt = Now;
            UE_LOG(LogTemp, Display, TEXT("STATUS_NETCHECK_PURIFY Poison=0 Curse=1"));
        }
        else if (GreenlandsAbilityStage == 2 && Now - GreenlandsAbilityStageAt >= 1.0)
        {
            GreenlandsAbilityStage = 3;
            UE_LOG(LogTemp, Display, TEXT("STATUS_NETCHECK_PASS Clients=2"));
        }
        return;
    }
    if (GreenlandsAbilityStage >= 9 && GreenlandsAbilityStage < 99)
    {
        Driver->CombatSetHealth(Driver->CombatMaximumHealth());
        Observer->CombatSetHealth(Observer->CombatMaximumHealth());
    }
    if (GreenlandsAbilityStage == 0)
    {
        ASFPilotBoar* Boar = nullptr;
        for (const auto& Slot : ActiveBoars)
        { if (Slot.Key.StartsWith(TEXT("boar_farm_edge:")) && IsValid(Slot.Value.Get()) && Slot.Value->Health() > 0)
          { Boar = Slot.Value.Get(); break; } }
        if (!Boar) return;
        GreenlandsAbilityBoar = Boar;
        const FVector Point = Boar->GetActorLocation() + FVector(180, 0, 0);
        Driver->SetActorLocation(Grounded(GetWorld(), Point, Driver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
            false, nullptr, ETeleportType::TeleportPhysics);
        Driver->SetActorRotation((Boar->GetActorLocation() - Driver->GetActorLocation()).Rotation());
        Driver->SetCombatTarget(Boar);
        if (IsSafe(Driver->GetActorLocation())) { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED SafeLocation")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityBoarHealth = Boar->Health();
        GreenlandsAbilityStamina = Driver->GetStamina();
        TryPlayerCombatAbility(Driver, TEXT("strike"));
        const float Cost = Driver->GetMaxStamina() * .05f * GetDefault<USFCombatEnergySettings>()->AbilityEnergyCostMultiplier;
        if (!FMath::IsNearlyEqual(Driver->GetStamina(), GreenlandsAbilityStamina - Cost, .05f) ||
            Boar->Health() > GreenlandsAbilityBoarHealth)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED Strike Cost=%.2f HP=%d"),
            GreenlandsAbilityStamina - Driver->GetStamina(), Boar->Health()); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStamina = Driver->GetStamina();
        GreenlandsAbilityBoarHealth = Boar->Health();
        TryPlayerCombatAbility(Driver, TEXT("strike"));
        if (Driver->GetStamina() != GreenlandsAbilityStamina || Boar->Health() != GreenlandsAbilityBoarHealth)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED GCD")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 1; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_STRIKE Cost=%.2f HP=%d"), Cost, Boar->Health());
    }
    else if (GreenlandsAbilityStage == 1 && Now - GreenlandsAbilityStageAt >= 1.7)
    {
        auto* Levels = Driver->GetController()->FindComponentByClass<USFGatheringPilotComponent>();
        if (!Levels) return;
        Levels->RestoreLevel(7); // Development fixture for the otherwise unavailable Magic kit.
        Driver->SetCombatArchetypeFixture(TEXT("mage"));
        const float Before = Driver->GetMana();
        TryPlayerCombatAbility(Driver, TEXT("arcane_ward"));
        const float Cost = Driver->GetMaxMana() * .08f * GetDefault<USFCombatEnergySettings>()->AbilityEnergyCostMultiplier;
        if (!FMath::IsNearlyEqual(Driver->GetMana(), Before - Cost, .05f) || Driver->CombatAbsorb() <= 0)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED Ward Mana=%.2f Absorb=%.2f"),
            Driver->GetMana(), Driver->CombatAbsorb()); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityWardAbsorb = Driver->CombatAbsorb();
        GreenlandsAbilityStage = 2;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_WARD Cost=%.2f Absorb=%.2f"), Cost, Driver->CombatAbsorb());
    }
    else if (GreenlandsAbilityStage == 2)
    {
        ASFPilotBoar* Boar = GreenlandsAbilityBoar.Get();
        if (!Boar || Boar->Health() <= 0) return;
        const double Before = GreenlandsAbilityWardAbsorb;
        for (int32 Attempt = 0; Attempt < 8 && Driver->CombatAbsorb() >= Before; ++Attempt)
            TryBoarAttack(Boar, Driver);
        if (Driver->CombatAbsorb() >= Before)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED AbsorbConsumption")); GreenlandsAbilityStage = 99; return; }
        // End the hostile phase before checking an interruptible friendly cast.
        Boar->ReceivePilotDamage(Driver, Boar->Health());
        GreenlandsAbilityStage = 3; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_ABSORB Absorbed=%.2f Remaining=%.2f DriverHP=%d ObserverHP=%d"),
            Before - Driver->CombatAbsorb(), Driver->CombatAbsorb(), Driver->CombatHealth(), Observer->CombatHealth());
    }
    else if (GreenlandsAbilityStage == 3 && Now - GreenlandsAbilityStageAt >= 1.7)
    {
        if (RespawnPoints.IsEmpty() || !IsSafe(RespawnPoints[0]))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED FriendlySafePoint")); GreenlandsAbilityStage = 99; return; }
        Driver->SetActorLocation(RespawnPoints[0], false, nullptr, ETeleportType::TeleportPhysics);
        Driver->SetCombatArchetypeFixture(TEXT("guardian"));
        const float Before = Driver->GetStamina();
        TryPlayerCombatAbility(Driver, TEXT("defensive_stance"));
        const float Cost = Driver->GetMaxStamina() * .05f *
            GetDefault<USFCombatEnergySettings>()->AbilityEnergyCostMultiplier;
        if (!Driver->HasDefensiveStance() || !FMath::IsNearlyEqual(Driver->GetStamina(), Before - Cost, .05f))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED Guardian")); GreenlandsAbilityStage = 99; return; }
        Driver->SetCombatArchetypeFixture(TEXT("fighter")); // hybrid: Fighter pays Mana for Healing.
        Observer->SetActorLocation(Driver->GetActorLocation() + FVector(0, 100, 0), false, nullptr, ETeleportType::TeleportPhysics);
        Observer->CombatSetHealth(FMath::Max(1, Observer->CombatMaximumHealth() - 100));
        GreenlandsAbilityBoarHealth = Observer->CombatHealth();
        GreenlandsAbilityStage = 4; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_GUARDIAN Cost=%.2f"), Cost);
    }
    else if (GreenlandsAbilityStage == 4 && Now - GreenlandsAbilityStageAt >= 1.7)
    {
        const float Before = Driver->GetMana();
        TryPlayerCombatAbility(Driver, TEXT("mend"), Observer);
        const float Cost = Driver->GetMaxMana() * .06f *
            GetDefault<USFCombatEnergySettings>()->AbilityEnergyCostMultiplier;
        if (!FMath::IsNearlyEqual(Driver->GetMana(), Before - Cost, .05f))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED HybridMana")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 5; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_HYBRID_MANA Cost=%.2f"), Cost);
    }
    else if (GreenlandsAbilityStage == 5 && Now - GreenlandsAbilityStageAt >= 2.2)
    {
        if (Observer->CombatHealth() <= GreenlandsAbilityBoarHealth)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED FriendlyHeal")); GreenlandsAbilityStage = 99; return; }
        auto* Levels = Driver->GetController()->FindComponentByClass<USFGatheringPilotComponent>();
        if (!Levels) return;
        Levels->RestoreLevel(20);
        Driver->SetCombatArchetypeFixture(TEXT("fighter"));
        const float ManaBefore = Driver->GetMana();
        TryPlayerCombatAbility(Driver, TEXT("blink"));
        const float Cost = Driver->GetMaxMana() * .08f *
            GetDefault<USFCombatEnergySettings>()->AbilityEnergyCostMultiplier;
        if (!FMath::IsNearlyEqual(Driver->GetMana(), ManaBefore - Cost, .05f))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED HybridMagic")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 6;
        GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_MAGIC Cost=%.2f Healed=%d"), Cost,
            Observer->CombatHealth() - GreenlandsAbilityBoarHealth);
    }
    else if (GreenlandsAbilityStage == 6 && Now - GreenlandsAbilityStageAt >= 1.7)
    {
        const float Before = Driver->GetStamina();
        TryPlayerCombatAbility(Driver, TEXT("poisoned_blade"));
        const float Cost = Driver->GetMaxStamina() * .06f *
            GetDefault<USFCombatEnergySettings>()->AbilityEnergyCostMultiplier;
        if (!FMath::IsNearlyEqual(Driver->GetStamina(), Before - Cost, .05f) || !Driver->HasPoisonedBlade(Now))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED Shadow")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 7; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_SHADOW Cost=%.2f"), Cost);
    }
    else if (GreenlandsAbilityStage == 7 && Now - GreenlandsAbilityStageAt >= 1.2)
    {
        const float Before = Driver->GetStamina();
        TryPlayerCombatAbility(Driver, TEXT("vanish"));
        const float Cost = Driver->GetMaxStamina() * .12f *
            GetDefault<USFCombatEnergySettings>()->AbilityEnergyCostMultiplier;
        if (!FMath::IsNearlyEqual(Driver->GetStamina(), Before - Cost, .05f) || !Driver->HasCombatStealth(Now))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED Vanish")); GreenlandsAbilityStage = 99; return; }
        ASFPilotBoar* BowTarget = nullptr;
        for (const auto& Slot : ActiveBoars)
        { if (Slot.Key.StartsWith(TEXT("boar_farm_edge:")) && IsValid(Slot.Value.Get()) && Slot.Value->Health() > 0)
          { BowTarget = Slot.Value.Get(); break; } }
        if (!BowTarget)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED NoBowTarget")); GreenlandsAbilityStage = 99; return; }
        Driver->SetActorLocation(Grounded(GetWorld(), BowTarget->GetActorLocation() + FVector(180, 0, 0),
            Driver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
        Driver->SetCombatTarget(BowTarget);
        if (IsSafe(Driver->GetActorLocation()))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED NoBowSafeLocation")); GreenlandsAbilityStage = 99; return; }
        const float GateBefore = Driver->GetStamina();
        TryPlayerCombatAbility(Driver, TEXT("quick_shot"));
        if (!FMath::IsNearlyEqual(Driver->GetStamina(), GateBefore, .05f))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED NoBow")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 8; GreenlandsAbilityStageAt = Now;
    }
    else if (GreenlandsAbilityStage == 8 && Now - GreenlandsAbilityStageAt >= 1.7)
    {
        auto* Inventory = Driver->GetController()->FindComponentByClass<USFInventoryComponent>();
        ASFPilotBoar* Boar = nullptr;
        for (const auto& Slot : ActiveBoars)
        { if (Slot.Key.StartsWith(TEXT("boar_farm_edge:")) && IsValid(Slot.Value.Get()) && Slot.Value->Health() > 0)
          { Boar = Slot.Value.Get(); break; } }
        if (!Inventory || !Boar)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED BowSetup")); GreenlandsAbilityStage = 99; return; }
        Driver->SetActorLocation(Grounded(GetWorld(), Boar->GetActorLocation() + FVector(180, 0, 0),
            Driver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
        Driver->SetActorRotation((Boar->GetActorLocation() - Driver->GetActorLocation()).Rotation());
        Driver->SetCombatTarget(Boar);
        FSFEquippedSlot Bow;
        Bow.SlotName = TEXT("MainHand");
        Bow.Item = TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(
            TEXT("/Game/Frontier/Gameplay/Pilot/DA_StarterBow.DA_StarterBow")));
        Inventory->EquippedItems.Add(Bow); // Test fixture: exercise the live server equipment snapshot.
        const float Before = Driver->GetStamina();
        TryPlayerCombatAbility(Driver, TEXT("quick_shot"), Boar);
        if (!SFGear::CombatEquipment(Driver->GetController(), CapturedSettings->DevPilotArmorUnit).bRangedWeapon ||
            !(Driver->GetStamina() < Before))
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED EquippedBow")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 9;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_BOW NoBow=1 EquippedBow=1 Mana=%.2f"), Driver->GetMana());
    }
    else if (GreenlandsAbilityStage == 9)
    {
        ASFPilotBoar* Captain = nullptr;
        for (const auto& Slot : ActiveBoars)
        { if (IsValid(Slot.Value.Get()) && Slot.Value->GetCreatureId() == TEXT("npc/bandit_captain"))
          { Captain = Slot.Value.Get(); break; } }
        if (!Captain || !Captain->GetCasting()) return;
        // Begin the fixture at the camp anchor so pursuit cannot carry the captain out of territory.
        Captain->SetActorLocation(Captain->SpawnAnchor(), false, nullptr, ETeleportType::TeleportPhysics);
        GreenlandsAbilityBoar = Captain;
        bool bInsideCamp = false;
        for (int32 Direction = 0; Direction < 8 && !bInsideCamp; ++Direction)
        {
            const double Angle = Direction * PI / 4.0;
            const FVector Candidate = Grounded(GetWorld(), Captain->GetActorLocation() +
                FVector(FMath::Cos(Angle) * 180, FMath::Sin(Angle) * 180, 0),
                Driver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            if (!IsInsideTerritory(Candidate, Groups[Captain->GroupId()].Polygon) || IsSafe(Candidate)) continue;
            FCollisionQueryParams Sight(SCENE_QUERY_STAT(CaptainNetcheckPosition), false, Driver);
            Sight.AddIgnoredActor(Captain);
            if (GetWorld()->LineTraceTestByChannel(Candidate + FVector(0, 0, 50),
                Captain->GetActorLocation() + FVector(0, 0, 50), ECC_Visibility, Sight)) continue;
            Driver->SetActorLocation(Candidate, false, nullptr, ETeleportType::TeleportPhysics);
            bInsideCamp = true;
        }
        if (!bInsideCamp)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED CaptainPosition")); GreenlandsAbilityStage = 99; return; }
        Observer->SetActorLocation(Driver->GetActorLocation() + FVector(0, 120, 0), false, nullptr, ETeleportType::TeleportPhysics);
        Driver->SetActorRotation((Captain->GetActorLocation() - Driver->GetActorLocation()).Rotation());
        Driver->SetCombatTarget(Captain);
        Captain->ReceivePilotDamage(Driver, Captain->MaximumHealth() / 2);
        GreenlandsAbilityBoarHealth = Captain->Health();
        if (!Captain->GetCasting()->TryBeginNpcMend() || Captain->GetCasting()->GetNpcMana() != 94)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED NpcCastStart")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 10; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_NPC_START Mana=94 HP=%d"), Captain->Health());
    }
    else if (GreenlandsAbilityStage == 10 && Now - GreenlandsAbilityStageAt >= 2.8)
    {
        ASFPilotBoar* Captain = GreenlandsAbilityBoar.Get();
        if (!Captain || Captain->Health() <= GreenlandsAbilityBoarHealth ||
            Captain->GetCasting()->GetCastView().Phase != ESFCastPhase::Idle)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED NpcCastComplete")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 11;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_NPC_COMPLETE Heal=%d"), Captain->Health() - GreenlandsAbilityBoarHealth);
    }
    else if (GreenlandsAbilityStage == 11)
    {
        ASFPilotBoar* Captain = GreenlandsAbilityBoar.Get();
        if (!Captain || !Captain->GetCasting() || Captain->GetCasting()->GetCastView().Phase != ESFCastPhase::Casting) return;
        Driver->SetActorRotation((Captain->GetActorLocation() - Driver->GetActorLocation()).Rotation());
        Driver->Casting->RequestPublishedInterrupt(false, Captain);
        const int32 Mana = Captain->GetCasting()->GetNpcMana();
        if (!Captain->GetCasting()->IsNpcSchoolLocked() || Mana != 94 ||
            Captain->GetCasting()->GetCastView().Phase != ESFCastPhase::Idle)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED PommelInterrupt Mana=%d"), Mana); GreenlandsAbilityStage = 99; return; }
        const float Stamina = Driver->GetStamina();
        Driver->Casting->RequestPublishedInterrupt(false, Captain);
        if (Captain->GetCasting()->GetNpcMana() != Mana || Driver->GetStamina() != Stamina)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED DoubleRefund")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 12;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_NPC_POMMEL Locked=1 Mana=%d"), Mana);
    }
    else if (GreenlandsAbilityStage == 12)
    {
        ASFPilotBoar* Captain = GreenlandsAbilityBoar.Get();
        if (!Captain || !Captain->GetCasting() || Captain->GetCasting()->GetCastView().Phase != ESFCastPhase::Casting) return;
        bool bFound = false;
        for (int32 Direction = 0; Direction < 8 && !bFound; ++Direction)
        {
            const double Angle = Direction * PI / 4.0;
            const FVector Point = Captain->GetActorLocation() + FVector(FMath::Cos(Angle) * 2990,
                FMath::Sin(Angle) * 2990, 0);
            const FVector Ground = Grounded(GetWorld(), Point, Driver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            FCollisionQueryParams Query(SCENE_QUERY_STAT(NpcNetcheckSight), false, Driver);
            Query.AddIgnoredActor(Captain);
            if (!IsSafe(Ground) && !GetWorld()->LineTraceTestByChannel(Ground + FVector(0, 0, 50),
                Captain->GetActorLocation() + FVector(0, 0, 50), ECC_Visibility, Query))
            { Driver->SetActorLocation(Ground, false, nullptr, ETeleportType::TeleportPhysics); bFound = true; }
        }
        if (!bFound)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED CounterspellPosition")); GreenlandsAbilityStage = 99; return; }
        Driver->Casting->RequestPublishedInterrupt(true, Captain);
        if (!Captain->GetCasting()->IsNpcSchoolLocked() ||
            Captain->GetCasting()->GetCastView().Phase != ESFCastPhase::Idle)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED CounterspellInterrupt")); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 13; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_NPC_COUNTERSPELL DistanceCm=%.0f Locked=1"),
            FVector::Dist2D(Driver->GetActorLocation(), Captain->GetActorLocation()));
        Observer->SetActorLocation(Driver->GetActorLocation() + FVector(0, 100, 0), false, nullptr, ETeleportType::TeleportPhysics);
        Observer->StatusEffects->Apply(TEXT("netcheck/poison"), ESFStatusKind::Poison, ESFDispelType::Poison,
            Driver, 10.f, 0.f, 1, false);
        Observer->StatusEffects->Apply(TEXT("netcheck/curse"), ESFStatusKind::Curse, ESFDispelType::Curse,
            Driver, 10.f, 0.f, 1, false);
        UE_LOG(LogTemp, Display, TEXT("STATUS_NETCHECK_APPLIED Poison=1 Curse=1"));
    }
    else if (GreenlandsAbilityStage == 13 && Now - GreenlandsAbilityStageAt >= 1.0)
    {
        const float ManaBefore = Driver->GetMana();
        UE_LOG(LogTemp, Display, TEXT("STATUS_NETCHECK_PURIFY_GATE Cast=%d Level=%d HP=%d Mana=%.2f Distance=%.0f"),
            Driver->CanBeginCombatAbilityCast(true, Now), Driver->CombatCharacterLevel(),
            Observer->CombatHealth(), ManaBefore,
            FVector::Dist2D(Driver->GetActorLocation(), Observer->GetActorLocation()));
        TryPlayerCombatAbility(Driver, TEXT("purify"), Observer);
        if (Observer->StatusEffects->GetStacks(TEXT("netcheck/poison"), Driver) != 0 ||
            Observer->StatusEffects->GetStacks(TEXT("netcheck/curse"), Driver) != 1)
        { UE_LOG(LogTemp, Error, TEXT("ABILITY_NETCHECK_FAILED Purify ManaAfter=%.2f"), Driver->GetMana()); GreenlandsAbilityStage = 99; return; }
        GreenlandsAbilityStage = 14; GreenlandsAbilityStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("STATUS_NETCHECK_PURIFY Poison=0 Curse=1"));
    }
    else if (GreenlandsAbilityStage == 14 && Now - GreenlandsAbilityStageAt >= 1.0)
    {
        GreenlandsAbilityStage = 15;
        UE_LOG(LogTemp, Display, TEXT("ABILITY_NETCHECK_PASS Shadow=1 NoBow=1 EquippedBow=1 NpcCast=1"));
    }
}

void ASFGameMode::TickGreenlandsArcherNetcheck()
{
    if (bGreenlandsArcherPositioned) return;
    ASFE1Character* Driver = nullptr;
    ASFE1Character* Observer = nullptr;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ThreatIdentity(It->Get()) == TEXT("chr_dev_alpha")) { Driver = Cast<ASFE1Character>(It->Get()->GetPawn()); }
        if (ThreatIdentity(It->Get()) == TEXT("chr_dev_beta")) { Observer = Cast<ASFE1Character>(It->Get()->GetPawn()); }
    }
    if (!Driver || !Observer || USFCharacterSessionComponent::IsRestoring(Driver->GetController()) ||
        USFCharacterSessionComponent::IsRestoring(Observer->GetController())) return;
    for (const auto& Slot : ActiveBoars)
    {
        ASFPilotBoar* Archer = Slot.Value.Get();
        if (!IsValid(Archer) || !Archer->IsRangedArcher() || Archer->Health() <= 0) continue;
        const FVector Origin = Archer->GetActorLocation();
        for (int32 DirectionIndex = 0; DirectionIndex < 8; ++DirectionIndex)
        {
            const double Angle = 2.0 * PI * DirectionIndex / 8.0;
            const FVector Offset(1200.0 * FMath::Cos(Angle), 1200.0 * FMath::Sin(Angle), 0);
            const FVector Place = Grounded(GetWorld(), Origin + Offset,
                Driver->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            if (!IsInsideTerritory(Place, Groups[Archer->GroupId()].Polygon) || IsSafe(Place) ||
                !SFCombatPilotRules::HasRangedLineOfSight(GetWorld(), Archer, Driver,
                    Origin + FVector(0, 0, 80), Place + FVector(0, 0, 60))) continue;
            Driver->SetActorLocation(Place, false, nullptr, ETeleportType::TeleportPhysics);
            Observer->SetActorLocation(Grounded(GetWorld(), Place + FVector(0, 150, 0),
                Observer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
            Driver->CombatSetHealth(1000);
            Observer->CombatSetHealth(1000);
            if (!Archer->ReceivePackAssist(Driver, TEXT("chr_dev_alpha"))) continue;
            bGreenlandsArcherPositioned = true;
            UE_LOG(LogTemp, Display, TEXT("ARCHER_NETCHECK_READY Creature=%s DistanceCm=%.0f Clients=2"),
                *Archer->GetCreatureId(), FVector::Dist2D(Origin, Place));
            return;
        }
    }
}

void ASFGameMode::TickGreenlandsEnergyNetcheck()
{
    auto* Driver = Cast<ASFE1Character>(GreenlandsDriverController.IsValid() ? GreenlandsDriverController->GetPawn() : nullptr);
    auto* Observer = Cast<ASFE1Character>(GreenlandsObserverController.IsValid() ? GreenlandsObserverController->GetPawn() : nullptr);
    if (!Driver || !Observer || Driver->CombatHealth() <= 0 || Observer->CombatHealth() <= 0 ||
        USFCharacterSessionComponent::IsRestoring(Driver->GetController()) ||
        USFCharacterSessionComponent::IsRestoring(Observer->GetController())) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (GreenlandsEnergyStage == 0)
    {
        auto* Levels = Driver->GetController()->FindComponentByClass<USFGatheringPilotComponent>();
        if (!Levels) return;
        Levels->RestoreLevel(10); // Development fixture; production level comes from Progression restore.
        GreenlandsEnergyBeforeSprint = Driver->GetStamina();
        Driver->SetSprintIntent(true);
        if (!Driver->IsSprinting()) { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED SprintStart")); return; }
        GreenlandsEnergyStage = 1; GreenlandsEnergyStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_BEGIN Level=%d Stamina=%.2f Mana=%.2f"),
            Driver->CombatCharacterLevel(), Driver->GetStamina(), Driver->GetMana());
    }
    else if (GreenlandsEnergyStage == 1 && Now - GreenlandsEnergyStageAt >= 2.2)
    {
        Driver->SetSprintIntent(false);
        if (Driver->GetStamina() >= GreenlandsEnergyBeforeSprint || Driver->IsSprinting())
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED SprintDrain")); return; }
        GreenlandsEnergyBeforeSprint = Driver->GetStamina();
        GreenlandsEnergyStage = 2; GreenlandsEnergyStageAt = Now;
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_DRAIN Stamina=%.2f"), Driver->GetStamina());
    }
    else if (GreenlandsEnergyStage == 2 && Now - GreenlandsEnergyStageAt >= 1.0)
    {
        if (!FMath::IsNearlyEqual(Driver->GetStamina(), GreenlandsEnergyBeforeSprint, .01f))
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED SprintRegenDelay")); return; }
        GreenlandsEnergyStage = 3;
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_REGEN_PAUSED Stamina=%.2f"), Driver->GetStamina());
    }
    else if (GreenlandsEnergyStage == 3)
    {
        ASFPilotBoar* Boar = nullptr;
        for (const auto& Slot : ActiveBoars)
        { if (Slot.Key.StartsWith(TEXT("boar_farm_edge:")) && IsValid(Slot.Value.Get()) && Slot.Value->Health() > 0)
          { Boar = Slot.Value.Get(); break; } }
        if (!Boar) return;
        GreenlandsEnergyBoar = Boar;
        auto Move = [this](ASFE1Character* Player, const FVector& Point)
        { Player->SetActorLocation(Grounded(GetWorld(), Point, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
            false, nullptr, ETeleportType::TeleportPhysics); };
        Move(Observer, Boar->GetActorLocation() + FVector(140, 0, 0));
        Move(Driver, Boar->GetActorLocation() + FVector(0, 550, 0));
        Driver->CombatSetHealth(1000); Observer->CombatSetHealth(1000);
        Boar->ReceivePilotDamage(Observer, 1); // Development pull, separate from the published ability.
        GreenlandsEnergyStage = 4; GreenlandsEnergyStageAt = Now;
    }
    else if (GreenlandsEnergyStage == 4 && Now - GreenlandsEnergyStageAt >= 0.5)
    {
        ASFPilotBoar* Boar = GreenlandsEnergyBoar.Get();
        if (!Boar || !Boar->ThreatTarget()) return;
        auto* Levels = Driver->GetController()->FindComponentByClass<USFGatheringPilotComponent>();
        if (!Levels) return;
        Levels->RestoreLevel(9);
        const float Before = Driver->GetStamina();
        const bool bBelowLevel = TryPlayerTaunt(Driver, Boar);
        if (bBelowLevel || Driver->GetStamina() != Before)
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED LevelGate")); return; }
        Levels->RestoreLevel(10);
        if (!TryPlayerTaunt(Driver, Boar))
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED Challenge")); return; }
        const float After = Driver->GetStamina();
        const bool bCooldown = TryPlayerTaunt(Driver, Boar);
        if (bCooldown || Driver->GetStamina() != After)
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED ChallengeCooldown")); return; }
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_CHALLENGE Level=10 Cost=%.2f Target=%s"),
            Before - After, *GetNameSafe(Boar->ThreatTarget()));
        Driver->SetActorLocation(Observer->GetActorLocation() + FVector(0, 150, 0), false, nullptr, ETeleportType::TeleportPhysics);
        Driver->SetActorRotation((Observer->GetActorLocation() - Driver->GetActorLocation()).Rotation());
        Driver->SetCombatTarget(Observer); Observer->SetCombatTarget(Driver); // Hostility fixture only.
        GreenlandsEnergyStage = 5;
    }
    else if (GreenlandsEnergyStage == 5)
    {
        Driver->SetActorRotation((Observer->GetActorLocation() - Driver->GetActorLocation()).Rotation());
        const float Before = Driver->GetStamina();
        Driver->Casting->RequestPublishedInterrupt(false, Observer);
        if (Driver->GetStamina() != Before)
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED IdleInterruptCost")); return; }
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_IDLE_REFUSED Stamina=%.2f"), Before);
        if (!Observer->Casting->BeginPublishedInterruptFixtureCast())
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED FixtureCast")); return; }
        const int32 BeforeHealth = Observer->CombatHealth();
        Driver->Casting->RequestPublishedInterrupt(false, Observer);
        if (!Observer->Casting->GetCastView().bInterrupted || Driver->GetStamina() >= Before ||
            Observer->CombatHealth() >= BeforeHealth)
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED Pommel")); return; }
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_POMMEL Cost=%.2f Damage=%d"),
            Before - Driver->GetStamina(), BeforeHealth - Observer->CombatHealth());
        GreenlandsEnergyStage = 6; GreenlandsEnergyStageAt = Now;
    }
    else if (GreenlandsEnergyStage == 6 && Now - GreenlandsEnergyStageAt >= 3.5)
    {
        const float Before = Driver->GetMana();
        if (!Observer->Casting->BeginPublishedInterruptFixtureCast())
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED FixtureRecast")); return; }
        const int32 BeforeHealth = Observer->CombatHealth();
        Driver->Casting->RequestPublishedInterrupt(true, Observer);
        if (!Observer->Casting->GetCastView().bInterrupted || Driver->GetMana() >= Before ||
            Observer->CombatHealth() != BeforeHealth)
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED Counterspell")); return; }
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_COUNTERSPELL Cost=%.2f"), Before - Driver->GetMana());
        GreenlandsEnergyStage = 7;
    }
    else if (GreenlandsEnergyStage == 7 && Now - GreenlandsEnergyStageAt >= 15.2)
    {
        Driver->SetActorRotation((Observer->GetActorLocation() - Driver->GetActorLocation()).Rotation());
        if (!Observer->Casting->BeginPublishedInterruptFixtureCast(false))
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED ImmuneFixtureCast")); return; }
        const float Before = Driver->GetStamina();
        const int32 BeforeHealth = Observer->CombatHealth();
        Driver->Casting->RequestPublishedInterrupt(false, Observer);
        if (Driver->GetStamina() >= Before || Observer->Casting->GetCastView().Phase == ESFCastPhase::Idle ||
            Observer->CombatHealth() >= BeforeHealth)
        { UE_LOG(LogTemp, Error, TEXT("ENERGY_NETCHECK_FAILED ImmuneInterrupt")); return; }
        UE_LOG(LogTemp, Display, TEXT("ENERGY_NETCHECK_IMMUNE Cost=%.2f Damage=%d"),
            Before - Driver->GetStamina(), BeforeHealth - Observer->CombatHealth());
        GreenlandsEnergyStage = 8;
    }
}

void ASFGameMode::TickGreenlandsRepairNetcheck()
{
    if (bGreenlandsRepairPositioned || GetNumPlayers() != 2) { return; }
    APlayerController* Driver = nullptr;
    APlayerController* Observer = nullptr;
    for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ThreatIdentity(It->Get()) == TEXT("chr_dev_alpha")) { Driver = It->Get(); }
        if (ThreatIdentity(It->Get()) == TEXT("chr_dev_beta")) { Observer = It->Get(); }
    }
    if (!Driver || !Observer || !Driver->GetPawn() || !Observer->GetPawn() ||
        USFCharacterSessionComponent::IsRestoring(Driver) || USFCharacterSessionComponent::IsRestoring(Observer)) { return; }
    for (TActorIterator<ASFRepairStationActor> It(GetWorld()); It; ++It)
    {
        const FVector Near = It->GetActorLocation() + FVector(0,-150,0);
        Driver->GetPawn()->SetActorLocation(Near, false, nullptr, ETeleportType::TeleportPhysics);
        Observer->GetPawn()->SetActorLocation(Near + FVector(0, 150, 0), false, nullptr, ETeleportType::TeleportPhysics);
        bGreenlandsRepairPositioned = true;
        UE_LOG(LogTemp, Display, TEXT("GREENLANDS_REPAIR_READY Clients=2 Smith=%s"), *It->GetName());
        return;
    }
}

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
        Lead->Health() == Lead->MaximumHealth() && Mate->Health() == Mate->MaximumHealth())
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
        if (Fresh && Fresh != Lead && Fresh->Health() == Fresh->MaximumHealth())
        {
            Stage(11);
            UE_LOG(LogTemp, Display, TEXT("BOAR_GREENLANDS_RESPAWN LeadSlot=%d HP=%d"), GreenlandsLeadSlot, Fresh->Health());
        }
    }
}
#endif

bool ASFGameMode::ValidateWorldSlotsReply(const FSFGameplayResponse& Reply) const
{
    if (Reply.Status != ESFGameplayStatus::Ok) return false;
    int64 Expected = 0;
    for (const auto& Group : Groups) Expected += Group.Value.ActiveCount;
    if (Expected <= 0 || Reply.BoarSlots.Num() != Expected) return false;
    TSet<FString> SeenSlots, SeenActivePoints;
    for (const auto& Row : Reply.BoarSlots)
    {
        const auto* Group = Groups.Find(Row.GroupId);
        const FString Key = FString::Printf(TEXT("%s:%d"), *Row.GroupId, Row.SlotIndex);
        if (!Group || Row.SlotIndex < 0 || Row.SlotIndex >= Group->ActiveCount || Row.Cycle < 0 ||
            SeenSlots.Contains(Key) || !FMath::IsFinite(Row.XMetres) || !FMath::IsFinite(Row.YMetres) ||
            !FMath::IsFinite(Row.FacingDegrees)) return false;
        SeenSlots.Add(Key);
        if (Row.bActive)
        {
            const FString PointKey = Row.GroupId + TEXT(":") + Row.PointId;
            if (!Group->Definitions.Contains(Row.DefinitionId) || !Group->DefensivePoints.Contains(Row.PointId) ||
                SeenActivePoints.Contains(PointKey)) return false;
            SeenActivePoints.Add(PointKey);
        }
    }
    return true;
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
            if (!Mode->ValidateWorldSlotsReply(Reply))
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
                    Row.XMetres, Row.YMetres, Row.FacingDegrees, Row.DefinitionId); }
            }
            if (!Mode->bSlotReadyLogged)
            {
                Mode->bSlotReadyLogged = true;
                UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_READY Boars=%d Hostiles=%d Combat=%d HostileSpawning=%d"),
                    FixtureRewardNpcCount(Mode->ActiveBoars), Mode->ActiveBoars.Num(),
                    Mode->CapturedSettings->bCombatAvailable, Mode->CapturedSettings->bSilverbrookHostileSpawning);
            }
            if (Mode->PendingSlotDefeats.Num()) { Mode->RunWorldSlots(TEXT("tick")); }
        });
        return true;
    }
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TEXT(".."));
    const FString Dotnet = FPaths::Combine(Root, TEXT("Artifacts/Persistence/Toolchains/dotnet-10.0.402/dotnet.exe"));
    const FString Dll = FPaths::Combine(Root, TEXT("BackendServices/WorldContent.Tool/bin/Debug/net10.0/WorldContent.Tool.dll"));
    const FString Zone = CombatZonePath;
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
        if (Fields.Num() != 10 || Fields[0] != TEXT("SLOT") || !Groups.Contains(Fields[1])) { continue; }
        if (Fields[4] == TEXT("Active") && CapturedSettings->bSilverbrookHostileSpawning)
        { SpawnSlot(Fields[1], FCString::Atoi(*Fields[2]), FCString::Atoi(*Fields[3]), Fields[5],
            FCString::Atod(*Fields[6]), FCString::Atod(*Fields[7]), FCString::Atod(*Fields[8]), Fields[9]); }
    }
    return true;
}

void ASFGameMode::SpawnSlot(const FString& Group, int32 Slot, int32 Cycle, const FString& PointId,
    double X, double Y, double Facing, const FString& DefinitionId)
{
    const FString Key = FString::Printf(TEXT("%s:%d"), *Group, Slot);
    const auto* Record = Groups.Find(Group);
    if (!Record || !Record->Definitions.Contains(DefinitionId) || !Record->DefensivePoints.Contains(PointId) ||
        ActiveBoars.FindRef(Key).IsValid() || Slot < 0 || Slot >= Record->ActiveCount) { return; }
    const FVector Where = Grounded(GetWorld(), FVector(X*100,Y*100,0), 65);
    if (IsSafe(Where) || !IsInsideTerritory(Where, Groups[Group].Polygon)) { return; }
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    auto* Boar = GetWorld()->SpawnActor<ASFPilotBoar>(Where, FRotator(0,Facing,0), Params);
    if (!Boar) { return; }
    if (!Boar->ConfigureCreature(DefinitionId)) { Boar->Destroy(); return; }
    Boar->Initialize(this, Group, Slot, Groups[Group].Polygon,
        SFCombatPart6::Active(GetWorld()) ? SFCombatPart6::Npc(DefinitionId).Health : CapturedSettings->BoarHealth,
        ++NextBoarSerial, Record->DefensivePoints.FindChecked(PointId));
    ActiveBoars.Add(Key, Boar);
    UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_BOAR_SPAWN Group=%s Slot=%d Cycle=%d Point=%s Creature=%s"), *Group, Slot, Cycle, *PointId, *DefinitionId);
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
        if (!Stream) { Stream = &BoarCombatStreams.Add(Boar, FRandomStream(GetTypeHash(Boar->SpawnSequenceId()))); }
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

int32 ASFGameMode::ApplyStarterHeal(ASFE1Character* Healer, ASFE1Character* Subject, int32 Amount)
{
    if (!IsValid(Healer) || !IsValid(Subject) || Subject->CombatHealth() <= 0 || Amount <= 0) return 0;
    const int32 Effective = FMath::Min(Amount, Subject->CombatMaximumHealth() - Subject->CombatHealth());
    if (Effective <= 0) return 0;
    Subject->CombatSetHealth(Subject->CombatHealth() + Effective);
    TArray<ASFPilotBoar*> Engaged;
    for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
        if (It->HasThreatOn(Subject)) Engaged.Add(*It);
    if (!Engaged.IsEmpty())
        for (ASFPilotBoar* Boar : Engaged)
            Boar->ReceivePilotHealingThreat(Healer, Subject,
                SFStarterAbilities::HealingThreatPerNpc(Effective, Engaged.Num()));
    return Engaged.IsEmpty() ? 0 : Effective;
}

static void RecordPilotCombatPractice(ASFE1Character* Attacker, ASFPilotBoar* Target,
    const FString& SkillId, double Seconds, bool bSuccess, const FString& AbilityId, int32 Difficulty = -1)
{
    if (!IsValid(Attacker) || !IsValid(Target) || Target->IsLeashing() || Target->GetCreatureId().IsEmpty()) return;
    const auto Npc = SFCombatPart6::Npc(Target->GetCreatureId());
    if (Npc.Level - Attacker->CombatCharacterLevel() <= -10) return;
    auto* Quest = Attacker->GetController() ? Attacker->GetController()->FindComponentByClass<USFQuestPilotComponent>() : nullptr;
    if (!Quest) return;
    const FVector Location = Target->GetActorLocation();
    const FIntVector Cell(FMath::FloorToInt(Location.X / 10000.0), FMath::FloorToInt(Location.Y / 10000.0), 0);
    const uint64 Context = uint64(HashCombine(GetTypeHash(AbilityId),
        HashCombine(GetTypeHash(Target->GetCreatureId()), GetTypeHash(Cell))));
    Quest->RecordCombatPractice(SkillId, Difficulty >= 0 ? Difficulty : Npc.Skill,
        Seconds, Context, bSuccess, Npc.Level);
}

void ASFGameMode::TryHiddenMovementPractice(ASFE1Character* Character, ASFPilotBoar* Observer, double Now)
{
    if (!IsValid(Character) || !IsValid(Observer) || !CapturedSettings ||
        !SFCombatPart6::Active(GetWorld()) || !Character->HasCombatStealth(Now) ||
        !Observer->ThreatTarget() || Character->CombatHealth() <= 0 || Observer->Health() <= 0 ||
        Observer->IsLeashing() || IsSafe(Character->GetActorLocation()) ||
        USFCharacterSessionComponent::IsLinkdead(Character->GetController()) ||
        USFCharacterSessionComponent::IsRestoring(Character->GetController()) ||
        Character->GetVelocity().Size2D() < CapturedSettings->StealthPracticeMinimumSpeedCmPerSecond ||
        Now < NextStealthPracticeAt.FindRef(Character)) return;
    const auto Npc = SFCombatPart6::Npc(Observer->GetCreatureId());
    const double Radius = SFCombatPilotRules::AggroRadiusMetres(Observer->GetBehavior().AggroRadiusMetres,
        Npc.Level, Character->CombatCharacterLevel(), Observer->GetBehavior().AggroMinimumMetres,
        Observer->GetBehavior().AggroMaximumBonusMetres);
    if (FVector::Dist2D(Character->GetActorLocation(), Observer->GetActorLocation()) > Radius * 100.0) return;
    NextStealthPracticeAt.Add(Character, Now + CapturedSettings->StealthPracticeIntervalSeconds);
    RecordPilotCombatPractice(Character, Observer, TEXT("skill/stealth"),
        CapturedSettings->StealthPracticeIntervalSeconds, true, TEXT("hidden_movement"),
        SFCombatPart6::SkillReference(Npc.Level));
}

void ASFGameMode::RecordIncomingCombatPractice(ASFPilotBoar* Source, ASFE1Character* Victim,
    const SFCombatResolution::FAttack& Attack, const SFCombatResolution::FResult& Result)
{
    if (!IsValid(Source) || !IsValid(Victim) || !SFCombatPart6::Active(GetWorld()) ||
        Source->Health() <= 0 || Victim->CombatHealth() <= 0 ||
        IsSafe(Source->GetActorLocation()) || IsSafe(Victim->GetActorLocation())) return;
    if (SFCombatResolution::GrantsShieldPractice(Attack, Result))
        RecordPilotCombatPractice(Victim, Source, TEXT("skill/shield_defense"),
            Source->CombatSwingSeconds(), true, TEXT("shield_block"));
    if (SFCombatResolution::GrantsResistPractice(Attack, Result))
        RecordPilotCombatPractice(Victim, Source, TEXT("skill/resist_magic"),
            Source->CombatSwingSeconds(), true, TEXT("hostile_spell"));
}

void ASFGameMode::ApplyStarterWeakPoison(ASFE1Character* Source, ASFPilotBoar* Victim,
    const SFCombatResolution::FAttack& Attack)
{
    if (!IsValid(Source) || !IsValid(Victim) || !Victim->GetStatusEffects()) return;
    const auto* StatusTune = GetDefault<USFStatusEffectsSettings>();
    SFCombatResolution::FAttack TickAttack = Attack;
    TickAttack.RawDamage /= StatusTune->WeakPoisonTicks;
    TickAttack.SpreadWeaponDamage /= StatusTune->WeakPoisonTicks;
    TickAttack.bCanMiss = false; TickAttack.bCanBlock = false;
    TWeakObjectPtr<ASFE1Character> Caster = Source;
    TWeakObjectPtr<ASFPilotBoar> Target = Victim;
    TWeakObjectPtr<ASFGameMode> Mode = this;
    Victim->GetStatusEffects()->Apply(TEXT("effect/weak_poison"), ESFStatusKind::Poison, ESFDispelType::Poison,
        Source, StatusTune->WeakPoisonDurationSeconds,
        StatusTune->WeakPoisonDurationSeconds / StatusTune->WeakPoisonTicks,
        StatusTune->WeakPoisonMaxStacks, true, [Mode, Caster, Target, TickAttack]()
        {
            if (!Mode.IsValid() || !Caster.IsValid() || !Target.IsValid() ||
                Target->Health() <= 0 || Target->IsLeashing()) return;
            auto CurrentTick = TickAttack;
            CurrentTick.RawDamage *= Caster->DamageDoneMultiplier();
            CurrentTick.SpreadWeaponDamage *= Caster->DamageDoneMultiplier();
            CurrentTick.Armor = Target->CombatArmor();
            CurrentTick.Resistance = Target->CombatResistance();
            const auto Hit = Mode->ResolveCombatAttack(Caster.Get(), CurrentTick);
            if (Hit.Damage > 0) Target->ReceivePilotDamage(Caster.Get(), Hit.Damage);
        });
}

void ASFGameMode::TryPlayerCombatAbility(ASFE1Character* Attacker, FName AbilityId,
    AActor* SelectedTarget, bool bCastComplete, FVector CastOrigin, FVector GroundLocation)
{
    using namespace SFStarterAbilities;
    const FDefinition* Ability = Find(AbilityId.ToString());
    if (!Ability || !bPilotActive || !CapturedSettings || !CapturedSettings->bCombatAvailable ||
        !SFCombatPart6::Active(GetWorld()) || !IsValid(Attacker) || !Attacker->HasCombatSnapshot() ||
        Attacker->CombatHealth() <= 0 || USFCharacterSessionComponent::IsRestoring(Attacker->GetController())) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (!bCastComplete && !Attacker->CanBeginCombatAbilityCast(
        Ability->Kit == EKit::Healing || Ability->Kit == EKit::Magic, Now)) return;
    if (Ability->Effect == EEffect::Interrupt)
    {
        if (Attacker->Casting)
            Attacker->Casting->RequestPublishedInterrupt(Ability->Kit == EKit::Magic,
                IsValid(SelectedTarget) ? SelectedTarget : Attacker->GetCombatTarget());
        return;
    }
    const bool bSelf = Ability->Target == ETarget::Self;
    const bool bFriendly = IsFriendlyTarget(Ability->Target);
    if (!bSelf && !bFriendly && IsSafe(Attacker->GetActorLocation())) return;
    const auto* Energy = GetDefault<USFCombatEnergySettings>();
    const auto* StatusTune = GetDefault<USFStatusEffectsSettings>();
    const auto Equipment = SFGear::CombatEquipment(Attacker->GetController(), CapturedSettings->DevPilotArmorUnit);
    if (Ability->bRequiresRangedWeapon && !Equipment.bRangedWeapon)
    {
        UE_LOG(LogTemp, Display, TEXT("COMBAT_ABILITY_REFUSED Ability=%s Reason=EquipBow"), *AbilityId.ToString());
        if (ASFPlayerController* PC = Cast<ASFPlayerController>(Attacker->GetController()))
        { PC->ClientPilotHit(TEXT("Equip a bow to use Archery.")); }
        return;
    }
    const FString GoverningSkill = SFStarterAbilities::GoverningSkill(*Ability,
        SFCombatPart6::WeaponSkill(Equipment.Weapon));
    if (!CanUseEquipment(*Ability, Equipment.bShield, Equipment.bRangedWeapon,
        !Equipment.Weapon.bEquipped || Equipment.Weapon.SpeedSeconds == 1.4)) return;
    if (Ability->Effect == EEffect::Hide && !CanHide(Attacker->IsPilotInCombat())) return;
    if (Ability->Effect == EEffect::Backstab && !Attacker->HasCombatStealth(Now)) return;
    const double MaximumRange = Ability->MaximumRangeMetres == 3 ? Energy->MeleeReachMetres : Ability->MaximumRangeMetres;
    const FVector GroundCenter = Ability->Target == ETarget::GroundAlly && !GroundLocation.IsNearlyZero() ?
        GroundLocation : (IsValid(SelectedTarget) ? SelectedTarget->GetActorLocation() : Attacker->GetActorLocation());
    if (Ability->Target == ETarget::GroundAlly)
    {
        if (!IsGroundWithinRange(Attacker->GetActorLocation(), GroundCenter, MaximumRange)) return;
        FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(CombatGroundAbilityLOS), false, Attacker);
        if (GetWorld()->LineTraceTestByChannel(Attacker->GetActorLocation() + FVector(0, 0, 45),
            GroundCenter + FVector(0, 0, 45), ECC_Visibility, GroundQuery)) return;
    }
    ASFE1Character* Friendly = nullptr;
    if (bFriendly)
    {
        Friendly = Cast<ASFE1Character>(SelectedTarget);
        if (!Friendly && Ability->Target != ETarget::Ally && Ability->Target != ETarget::DeadAlly)
            Friendly = Attacker;
        if (!IsValid(Friendly) || (Ability->Target == ETarget::Ally && Friendly == Attacker) ||
            (Ability->Target == ETarget::DeadAlly && (Friendly == Attacker || Friendly->CombatHealth() != 0)) ||
            (Ability->Target != ETarget::DeadAlly && Friendly->CombatHealth() <= 0) ||
            FVector::Dist2D(Attacker->GetActorLocation(), Friendly->GetActorLocation()) > MaximumRange * 100.0) return;
        FCollisionQueryParams FriendQuery(SCENE_QUERY_STAT(CombatFriendlyAbilityLOS), false, Attacker);
        FriendQuery.AddIgnoredActor(Friendly);
        if (GetWorld()->LineTraceTestByChannel(Attacker->GetActorLocation() + FVector(0, 0, 45),
            Friendly->GetActorLocation() + FVector(0, 0, 45), ECC_Visibility, FriendQuery)) return;
    }
    if (Ability->Effect == EEffect::Revive && Attacker->IsPilotInCombat()) return;
    const FVector Facing = Attacker->GetActorForwardVector().GetSafeNormal2D();
    const auto ValidEnemy = [&](ASFPilotBoar* Boar)
    {
        if (!IsValid(Boar) || Boar->IsActorBeingDestroyed() || Boar->Health() <= 0 ||
            Boar->IsLeashing() || IsSafe(Boar->GetActorLocation())) return false;
        const FVector Delta = Boar->GetActorLocation() - Attacker->GetActorLocation();
        const double Distance = Delta.Size2D() / 100.0;
        if (Distance < Ability->MinimumRangeMetres || Distance > MaximumRange ||
            FVector::DotProduct(Facing, Delta.GetSafeNormal2D()) <
                (Ability->Target == ETarget::Cone ?
                    (Ability->Kit == EKit::Archery ? .96592583 : .70710678) : 0.0)) return false;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatAbilityLOS), false, Attacker);
        Query.AddIgnoredActor(Boar);
        if (Ability->Kit == EKit::Archery)
            return SFCombatPilotRules::HasRangedLineOfSight(GetWorld(), Attacker, Boar,
                Attacker->GetActorLocation() + FVector(0, 0, 45),
                Boar->GetActorLocation() + FVector(0, 0, 45));
        return !GetWorld()->LineTraceTestByChannel(Attacker->GetActorLocation() + FVector(0, 0, 45),
            Boar->GetActorLocation() + FVector(0, 0, 45), ECC_Visibility, Query);
    };
    TArray<ASFPilotBoar*> Targets;
    if (!bSelf && !bFriendly)
    {
        if (Ability->Target == ETarget::Cone)
        {
            for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It)
                if (ValidEnemy(*It)) Targets.Add(*It);
            Targets.Sort([Attacker](const ASFPilotBoar& A, const ASFPilotBoar& B)
            { return FVector::DistSquared2D(Attacker->GetActorLocation(), A.GetActorLocation()) <
                FVector::DistSquared2D(Attacker->GetActorLocation(), B.GetActorLocation()); });
            if (Targets.Num() > Ability->MaximumTargets) Targets.SetNum(Ability->MaximumTargets);
        }
        else if (ASFPilotBoar* Boar = Cast<ASFPilotBoar>(Attacker->GetCombatTarget()); ValidEnemy(Boar))
        {
            Targets.Add(Boar);
            if (Ability->Target == ETarget::TargetArea)
                for (TActorIterator<ASFPilotBoar> It(GetWorld()); It && Targets.Num() < Ability->MaximumTargets; ++It)
                    if (*It != Boar && It->Health() > 0 && !It->IsLeashing() &&
                        FVector::Dist2D(It->GetActorLocation(), Boar->GetActorLocation()) <= 600.0 &&
                        !IsSafe(It->GetActorLocation()) &&
                        SFCombatPilotRules::HasRangedLineOfSight(GetWorld(), Attacker, *It,
                            Attacker->GetActorLocation() + FVector(0, 0, 45),
                            It->GetActorLocation() + FVector(0, 0, 45))) Targets.Add(*It);
        }
        if (Targets.IsEmpty()) return;
    }
    if (Ability->Effect == EEffect::Backstab && !IsBehind(Targets[0]->GetActorForwardVector(),
        Attacker->GetActorLocation() - Targets[0]->GetActorLocation())) return;
    FVector ChargeDestination = FVector::ZeroVector;
    if (Ability->Effect == EEffect::DisplaceHit)
    {
        const FVector Toward = (Targets[0]->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
        ChargeDestination = Targets[0]->GetActorLocation() - Toward * 160.0;
        ChargeDestination.Z = Attacker->GetActorLocation().Z;
        FHitResult Sweep;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatCharge), false, Attacker);
        Query.AddIgnoredActor(Targets[0]);
        if (GetWorld()->SweepSingleByChannel(Sweep, Attacker->GetActorLocation(), ChargeDestination,
            FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Attacker->GetCapsuleComponent()->GetScaledCapsuleRadius(),
                Attacker->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), Query)) return;
    }
    if (!bCastComplete && !Attacker->CommitCombatAbility(*Ability, Now)) return;
    if (!bCastComplete && Ability->CastSeconds > 0)
    {
        const uint32 CastId = Attacker->BeginCombatAbilityCast(
            Ability->Kit == EKit::Healing || Ability->Kit == EKit::Magic);
        TWeakObjectPtr<ASFE1Character> Source = Attacker;
        TWeakObjectPtr<AActor> Target = SelectedTarget;
        const FVector Origin = Attacker->GetActorLocation();
        FTimerHandle CastTimer;
        GetWorldTimerManager().SetTimer(CastTimer, FTimerDelegate::CreateWeakLambda(this,
            [this, Source, Target, AbilityId, Origin, CastId, GroundLocation]()
            {
                if (Source.IsValid() && FVector::DistSquared2D(Origin, Source->GetActorLocation()) > FMath::Square(50.0))
                    Source->InterruptCombatAbilityCast();
                else if (Source.IsValid() && Source->CompleteCombatAbilityCast(CastId))
                    TryPlayerCombatAbility(Source.Get(), AbilityId, Target.Get(), true, Origin, GroundLocation);
            }), Ability->CastSeconds / (1.0 + Attacker->CombatHasteFraction(Now)), false);
        return;
    }
    if (bCastComplete && FVector::DistSquared2D(CastOrigin, Attacker->GetActorLocation()) > FMath::Square(50.0)) return;
    if (Ability->Effect == EEffect::Hide)
    {
        if (Attacker->HasCombatStealth(Now)) Attacker->BreakCombatStealth();
        else Attacker->EnterCombatStealth(1, TNumericLimits<double>::Max());
        return;
    }
    if (Ability->Effect == EEffect::Vanish)
    {
        for (TActorIterator<ASFPilotBoar> It(GetWorld()); It; ++It) It->DropAbilityThreat(Attacker);
        Attacker->EnterCombatStealth(2, Now + 6.0);
        return;
    }
    if (Ability->Effect == EEffect::PoisonBlade)
    { Attacker->SetPoisonedBladeUntil(Now + StatusTune->PoisonBladeDurationSeconds); return; }
    if (Ability->Effect == EEffect::HasteBuff) { Attacker->SetSteadyFocusUntil(Now + 10.0); return; }
    if (Ability->Effect == EEffect::SelfDisplace || Ability->Effect == EEffect::Blink)
    {
        const float Distance = Ability->Effect == EEffect::Blink ? 1500.f : -800.f;
        const FVector Destination = Attacker->GetActorLocation() + Facing * Distance;
        Attacker->SetActorLocation(Destination, true);
        if (Ability->Effect == EEffect::Blink && Attacker->CrowdControl)
            Attacker->CrowdControl->ClearRoot();
        return;
    }
    if (Ability->Effect == EEffect::Root)
    {
        const float RootSeconds = Targets[0]->ApplyAbilityRoot(Now + StatusTune->EntangleDurationSeconds);
        if (RootSeconds > 0.f)
            Targets[0]->GetStatusEffects()->Apply(AbilityId, ESFStatusKind::Magic, ESFDispelType::Magic,
                Attacker, RootSeconds, 0.f, 1, false);
        RecordPilotCombatPractice(Attacker, Targets[0], GoverningSkill, 2.0, RootSeconds > 0.f, AbilityId.ToString());
        return;
    }
    if (Ability->Effect == EEffect::Stance) { Attacker->ToggleDefensiveStance(); return; }
    if (Ability->Effect == EEffect::BlockBuff) { Attacker->SetShieldBlockUntil(Now + 6); return; }
    if (Ability->Effect == EEffect::Taunt)
    { Targets[0]->ReceivePilotTaunt(Attacker); RecordPilotCombatPractice(Attacker, Targets[0], GoverningSkill, 1.5, true, AbilityId.ToString()); return; }
    if (Ability->Effect == EEffect::DamageReduction)
    {
        if (AbilityId == TEXT("shield_wall")) Attacker->SetShieldWallUntil(Now + 8);
        else if (Friendly) Friendly->SetBulwarkUntil(Now + 6);
        return;
    }
    if (Ability->Effect == EEffect::Dispel)
    {
        if (Friendly && Friendly->StatusEffects) Friendly->StatusEffects->PurifyNewest();
        return;
    }
    if (Ability->Effect == EEffect::Revive)
    {
        Friendly->BeginResurrectionOffer(Now + CapturedSettings->ResAcceptSeconds,
            Friendly->GetResurrectionOfferDeathEventId());
        return;
    }
    if (Ability->Effect == EEffect::HealOverTime)
    {
        const bool bRenew = AbilityId == TEXT("renew");
        const auto* RenewCoefficient = bRenew ? SFCombatPart6::Ability(TEXT("renew")) : nullptr;
        SFCombatResolution::FAttack TickAttack;
        uint64 Key = 0;
        int32 Generation = 0;
        if (bRenew)
        {
            auto Stats = Attacker->CombatStats(CapturedSettings->CritBase, CapturedSettings->SkillDamageFloor, GoverningSkill);
            Stats.SkillMultiplier = SkillMultiplier(Attacker->HasNativeCombatKit(Ability->Kit),
                Stats.SkillMultiplier, CapturedSettings->SkillDamageFloor);
            TickAttack = SFCombatPart6::AuthoredAbility(Stats, Equipment.Weapon,
                *RenewCoefficient, CapturedSettings->WeaponSpread, CapturedSettings->PowerSpeedReferenceSeconds,
                CapturedSettings->UnarmedDpsFactor, CapturedSettings->WeaponDpsK);
            TickAttack.RawDamage /= 4.0;
            TickAttack.SpreadWeaponDamage /= 4.0;
            Key = (uint64(Attacker->GetUniqueID()) << 32) | Friendly->GetUniqueID();
            Generation = ++RenewGenerations.FindOrAdd(Key);
        }
        TWeakObjectPtr<ASFE1Character> Source = Attacker, Subject = bRenew ? Friendly : Attacker;
        TSharedRef<int32> TickCount = MakeShared<int32>(0);
        TSharedRef<bool> bPracticed = MakeShared<bool>(false);
        TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
        GetWorld()->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(this,
            [this, Source, Subject, TickCount, bPracticed, Handle, bRenew, TickAttack, Key, Generation]()
            {
                ASFE1Character* Player = Subject.Get();
                if (!Source.IsValid() || !Player || Player->CombatHealth() <= 0 ||
                    (bRenew && RenewGenerations.FindRef(Key) != Generation))
                { GetWorld()->GetTimerManager().ClearTimer(*Handle); return; }
                const int32 Heal = bRenew ? ResolveCombatAttack(Source.Get(), TickAttack).Damage :
                    FMath::RoundToInt(Player->CombatMaximumHealth() * .03);
                const int32 Effective = ApplyStarterHeal(Source.Get(), Player, Heal);
                if (bRenew && Effective > 0 && !*bPracticed && Source->GetController())
                {
                    if (auto* Quest = Source->GetController()->FindComponentByClass<USFQuestPilotComponent>())
                        Quest->RecordCombatPractice(TEXT("skill/healing"),
                            SFCombatPart6::SkillReference(Source->CombatCharacterLevel()), 3.0,
                            GetTypeHash(FString(TEXT("renew"))), true, Source->CombatCharacterLevel());
                    *bPracticed = true;
                }
                if (++*TickCount >= (bRenew ? 4 : 5))
                {
                    if (bRenew) RenewGenerations.Remove(Key);
                    GetWorld()->GetTimerManager().ClearTimer(*Handle);
                }
            }), bRenew ? 3.0f : 2.0f, true);
        return;
    }
    const auto* Coefficient = SFCombatPart6::Ability(AbilityId.ToString());
    if (!Coefficient) return;
    auto Stats = Attacker->CombatStats(CapturedSettings->CritBase, CapturedSettings->SkillDamageFloor, GoverningSkill);
    Stats.SkillMultiplier = SkillMultiplier(Attacker->HasNativeCombatKit(Ability->Kit),
        Stats.SkillMultiplier, CapturedSettings->SkillDamageFloor);
    SFCombatResolution::FAttack Base = SFCombatPart6::AuthoredAbility(Stats, Equipment.Weapon,
        *Coefficient, CapturedSettings->WeaponSpread, CapturedSettings->PowerSpeedReferenceSeconds,
        CapturedSettings->UnarmedDpsFactor, CapturedSettings->WeaponDpsK);
    if (!Coefficient->bHeal && !Coefficient->bAbsorb)
    {
        Base.RawDamage *= Attacker->DamageDoneMultiplier();
        Base.SpreadWeaponDamage *= Attacker->DamageDoneMultiplier();
    }
    if (Ability->Effect == EEffect::Heal)
    {
        const int32 Effective = ApplyStarterHeal(Attacker, Friendly, ResolveCombatAttack(Attacker, Base).Damage);
        if (Effective > 0)
            if (auto* Quest = Attacker->GetController()->FindComponentByClass<USFQuestPilotComponent>())
                Quest->RecordCombatPractice(TEXT("skill/healing"), SFCombatPart6::SkillReference(Stats.Level),
                    Coefficient->ReferenceSeconds, GetTypeHash(AbilityId), true, Stats.Level);
        return;
    }
    if (Ability->Effect == EEffect::HealingCircle)
    {
        TArray<ASFE1Character*> Allies;
        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
            if (It->CombatHealth() > 0 && IsInsideHealingCircle(GroundCenter, It->GetActorLocation()))
                Allies.Add(*It);
        Allies.Sort([GroundCenter](const ASFE1Character& A, const ASFE1Character& B)
        { return FVector::DistSquared2D(A.GetActorLocation(), GroundCenter) <
            FVector::DistSquared2D(B.GetActorLocation(), GroundCenter); });
        bool bEffective = false;
        for (int32 Index = 0; Index < FMath::Min(Ability->MaximumTargets, Allies.Num()); ++Index)
            bEffective |= ApplyStarterHeal(Attacker, Allies[Index], ResolveCombatAttack(Attacker, Base).Damage) > 0;
        if (bEffective)
            if (auto* Quest = Attacker->GetController()->FindComponentByClass<USFQuestPilotComponent>())
                Quest->RecordCombatPractice(TEXT("skill/healing"), SFCombatPart6::SkillReference(Stats.Level),
                    Coefficient->ReferenceSeconds, GetTypeHash(AbilityId), true, Stats.Level);
        return;
    }
    if (Ability->Effect == EEffect::Absorb)
    {
        Base.bCanCrit = false;
        const auto Result = ResolveCombatAttack(Attacker, Base);
        Attacker->GrantCombatAbsorb(Result.Damage, 20.0, Attacker);
        return;
    }
    if (Ability->Effect == EEffect::DisplaceHit) Attacker->SetActorLocation(ChargeDestination, true);
    bool bRecordedPractice = false;
    for (ASFPilotBoar* Boar : Targets)
    {
        SFCombatResolution::FAttack Attack = Base;
        const auto Npc = SFCombatPart6::Npc(Boar->GetCreatureId());
        Attack.bCanMiss = Ability->Effect != EEffect::DamageOverTime;
        Attack.MissChance = FMath::Clamp((Npc.Skill - (Stats.Skill + .10 * Stats.Tactics)) /
            CapturedSettings->MissDivisor, 0.0, double(CapturedSettings->MissCap));
        Attack.bHasArmorSnapshot = true; Attack.Armor = Boar->CombatArmor();
        Attack.bHasResistanceSnapshot = true; Attack.Resistance = Boar->CombatResistance();
        Attack.bFrontArc = SFBoarDodgeRules::IsInFrontArc(Boar->GetActorForwardVector(),
            Attacker->GetActorLocation() - Boar->GetActorLocation(), CapturedSettings->FrontArcDegrees);
        Attack.bCanDodge = Attack.bCanMiss && Attack.bFrontArc;
        Attack.bCanParry = Attack.bCanMiss && Attack.bFrontArc;
        Attack.DodgeChance = Boar->CombatDodge(); Attack.ParryChance = Boar->CombatParry();
        if (Ability->Effect == EEffect::DamageOverTime)
        {
            if (!bRecordedPractice)
            {
                RecordPilotCombatPractice(Attacker, Boar, GoverningSkill, Coefficient->ReferenceSeconds, true, AbilityId.ToString());
                bRecordedPractice = true;
            }
            const bool bSerpent = AbilityId == TEXT("serpent_sting");
            const int32 Ticks = bSerpent ? StatusTune->SerpentTicks : StatusTune->RendTicks;
            const float Duration = bSerpent ? StatusTune->SerpentDurationSeconds : StatusTune->RendDurationSeconds;
            Attack.RawDamage /= Ticks; Attack.SpreadWeaponDamage /= Ticks;
            Attack.bCanBlock = false;
            TWeakObjectPtr<ASFE1Character> Source = Attacker;
            TWeakObjectPtr<ASFPilotBoar> Victim = Boar;
            TWeakObjectPtr<ASFGameMode> Mode = this;
            const double AppliedDamageDone = Attacker->DamageDoneMultiplier();
            Boar->GetStatusEffects()->Apply(AbilityId,
                bSerpent ? ESFStatusKind::Poison : ESFStatusKind::Physical,
                bSerpent ? ESFDispelType::Poison : ESFDispelType::None,
                Attacker, Duration, Duration / Ticks, 1, false,
                [Mode, Source, Victim, Attack, AppliedDamageDone]()
                {
                    if (!Mode.IsValid() || !Source.IsValid() || !Victim.IsValid() ||
                        Victim->Health() <= 0 || Victim->IsLeashing()) return;
                    auto CurrentTick = Attack;
                    const double DamageRatio = Source->DamageDoneMultiplier() / AppliedDamageDone;
                    CurrentTick.RawDamage *= DamageRatio;
                    CurrentTick.SpreadWeaponDamage *= DamageRatio;
                    CurrentTick.Armor = Victim->CombatArmor();
                    CurrentTick.Resistance = Victim->CombatResistance();
                    const auto Result = Mode->ResolveCombatAttack(Source.Get(), CurrentTick);
                    if (Result.Damage > 0) Victim->ReceivePilotDamage(Source.Get(), Result.Damage);
                });
            continue;
        }
        const auto Result = ResolveCombatAttack(Attacker, Attack);
        if (!bRecordedPractice)
        {
            RecordPilotCombatPractice(Attacker, Boar, GoverningSkill, Coefficient->ReferenceSeconds,
                Result.Damage > 0, AbilityId.ToString());
            if (Equipment.Weapon.bEquipped &&
                (Ability->Kit == EKit::Weapon || Ability->Kit == EKit::Archery || Ability->Effect == EEffect::Backstab))
                RecordPilotCombatPractice(Attacker, Boar, TEXT("skill/tactics"), Coefficient->ReferenceSeconds,
                    Result.Damage > 0, AbilityId.ToString());
            bRecordedPractice = true;
        }
        if (Result.Damage > 0) Boar->ReceivePilotDamage(Attacker, Result.Damage,
            AbilityId == TEXT("shield_bash") ? 1.5 : 1.0);
        else Boar->ReceivePilotDodge(Attacker);
        if (Result.Damage > 0 && Ability->Effect == EEffect::SlowHit)
        {
            const float SlowDuration = AbilityId == TEXT("frost_shard") ?
                StatusTune->FrostSlowDurationSeconds : StatusTune->CripplingSlowDurationSeconds;
            Boar->ApplyAbilitySnare(Now + SlowDuration,
                AbilityId == TEXT("frost_shard") ? StatusTune->FrostSlowMultiplier : StatusTune->CripplingSlowMultiplier);
            Boar->GetStatusEffects()->Apply(AbilityId,
                AbilityId == TEXT("frost_shard") ? ESFStatusKind::Magic : ESFStatusKind::Physical,
                ESFDispelType::None,
                Attacker, SlowDuration, 0.f, 1, false);
        }
        if (Result.Damage > 0 && Coefficient->bPhysical && Attacker->TryPoisonProc(Now))
        {
            const auto* Poison = SFCombatPart6::Ability(TEXT("effect/weak_poison"));
            auto PoisonStats = Stats;
            PoisonStats.SkillMultiplier = SkillMultiplier(Attacker->HasNativeCombatKit(EKit::Shadow),
                Stats.SkillMultiplier, CapturedSettings->SkillDamageFloor);
            auto PoisonAttack = SFCombatPart6::AuthoredAbility(PoisonStats, Equipment.Weapon, *Poison,
                CapturedSettings->WeaponSpread, CapturedSettings->PowerSpeedReferenceSeconds,
                CapturedSettings->UnarmedDpsFactor, CapturedSettings->WeaponDpsK);
            ApplyStarterWeakPoison(Attacker, Boar, PoisonAttack);
        }
    }
    if (Ability->Effect == EEffect::Backstab) Attacker->BreakCombatStealth();
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
    const bool bPart6 = SFCombatPart6::Active(GetWorld());
    if (bPart6 && !Attacker->HasCombatSnapshot()) return;
    const SFGear::FCombatEquipment Equipment = bPart6 ?
        SFGear::CombatEquipment(Attacker->GetController(), CapturedSettings->DevPilotArmorUnit) :
        SFGear::FCombatEquipment();
    PlayerNextSwing.Add(Attacker, Now + (bPart6 ?
        (Equipment.Weapon.bEquipped ? Equipment.Weapon.SpeedSeconds : 2.0) :
        CapturedSettings->PlayerMeleeCooldownSeconds) / (1.0 + Attacker->CombatHasteFraction(Now)));
    Attacker->BreakCombatStealth();
    SFCombatResolution::FAttack Attack;
    if (bPart6)
    {
        const FString WeaponSkill = SFCombatPart6::WeaponSkill(Equipment.Weapon);
        const auto Stats = Attacker->CombatStats(CapturedSettings->CritBase, CapturedSettings->SkillDamageFloor, WeaponSkill);
        Attack = SFCombatPart6::PlayerAuto(Stats, Equipment.Weapon,
            CapturedSettings->WeaponSpread, CapturedSettings->PowerSpeedReferenceSeconds,
            CapturedSettings->UnarmedDpsFactor, CapturedSettings->WeaponDpsK);
        const auto Npc = SFCombatPart6::Npc(Best->GetCreatureId());
        Attack.bCanMiss = true;
        Attack.MissChance = FMath::Clamp((Npc.Skill - (Stats.Skill + .10 * Stats.Tactics)) /
            CapturedSettings->MissDivisor, 0.0, double(CapturedSettings->MissCap));
        Attack.bHasArmorSnapshot = true;
        Attack.Armor = Best->CombatArmor();
        Attack.bHasResistanceSnapshot = true;
        Attack.Resistance = Best->CombatResistance();
    }
    else { Attack.RawDamage = CapturedSettings->PlayerMeleeDamage + SFGear::Equipped(Attacker->GetController()).Damage; }
    Attack.RawDamage *= Attacker->DamageDoneMultiplier();
    Attack.SpreadWeaponDamage *= Attacker->DamageDoneMultiplier();
    Attack.bFrontArc = SFBoarDodgeRules::IsInFrontArc(Best->GetActorForwardVector(),
        Attacker->GetActorLocation() - Best->GetActorLocation(), CapturedSettings->FrontArcDegrees);
    const FString Map = GetWorld()->GetOutermost()->GetName();
    const bool bBoarAvoidance = SFNpcZoneContent::IsProductionMap(GetWorld()) ||
        Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_Dodge");
    Attack.bCanDodge = bBoarAvoidance && Attack.bFrontArc;
    Attack.bCanParry = bBoarAvoidance && Attack.bFrontArc;
    Attack.DodgeChance = bPart6 ? Best->CombatDodge() : CapturedSettings->BoarDodgeChance;
    Attack.ParryChance = bPart6 ? Best->CombatParry() : CapturedSettings->BoarParryChance;
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
    if (bPart6)
    {
        const FString WeaponSkill = SFCombatPart6::WeaponSkill(Equipment.Weapon);
        RecordPilotCombatPractice(Attacker, Best, WeaponSkill,
            Equipment.Weapon.bEquipped ? Equipment.Weapon.SpeedSeconds : 2.0, Result.Damage > 0, TEXT("auto"));
        if (Equipment.Weapon.bEquipped)
            RecordPilotCombatPractice(Attacker, Best, TEXT("skill/tactics"), Equipment.Weapon.SpeedSeconds,
                Result.Damage > 0, TEXT("auto"));
    }
    if (Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_Dodge") && FixtureDraw >= 0)
    { UE_LOG(LogTemp, Display, TEXT("BOAR_DODGE_SERVER Dodge=%d HP=%d Front=%d Draw=%.3f"),
        Result.Outcome == SFCombatResolution::EOutcome::Dodge ? 1 : 0, Best->Health(),
        Attack.bFrontArc ? 1 : 0, FixtureDraw); }
    if (Result.Damage <= 0) { Best->ReceivePilotDodge(Attacker); return; }
    Best->ReceivePilotDamage(Attacker, Result.Damage);
    if (bPart6 && Attacker->TryPoisonProc(Now))
    {
        auto Stats = Attacker->CombatStats(CapturedSettings->CritBase, CapturedSettings->SkillDamageFloor,
            TEXT("skill/stealth"));
        Stats.SkillMultiplier = SFStarterAbilities::SkillMultiplier(
            Attacker->HasNativeCombatKit(SFStarterAbilities::EKit::Shadow),
            Stats.SkillMultiplier, CapturedSettings->SkillDamageFloor);
        const auto* Poison = SFCombatPart6::Ability(TEXT("effect/weak_poison"));
        const auto PoisonAttack = SFCombatPart6::AuthoredAbility(Stats, Equipment.Weapon, *Poison,
            CapturedSettings->WeaponSpread, CapturedSettings->PowerSpeedReferenceSeconds,
            CapturedSettings->UnarmedDpsFactor, CapturedSettings->WeaponDpsK);
        ApplyStarterWeakPoison(Attacker, Best, PoisonAttack);
    }
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
    const bool bPublished = SFNpcZoneContent::IsProductionMap(GetWorld());
    if ((!bPublished && GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_Taunt")) ||
        !bPilotActive || !CapturedSettings || !CapturedSettings->bCombatAvailable ||
        !IsValid(Taunter) || Taunter->CombatHealth() <= 0 ||
        USFCharacterSessionComponent::IsRestoring(Taunter->GetController())) { return Deny(TEXT("unavailable")); }
    const double Now = GetWorld()->GetTimeSeconds();
    if (bPublished && !Taunter->CanUseChallenge(Now)) { return Deny(TEXT("level or cooldown")); }
    ASFPilotBoar* Boar = Cast<ASFPilotBoar>(SelectedTarget);
    if (!IsValid(Boar) || Boar->Health() <= 0 || Boar->IsLeashing() || (bPublished && !Boar->ThreatTarget()) ||
        IsSafe(Taunter->GetActorLocation()) || IsSafe(Boar->GetActorLocation())) { return Deny(TEXT("invalid target")); }
    const double RangeMetres = bPublished ? GetDefault<USFCombatEnergySettings>()->ChallengeRangeMetres : CapturedSettings->MeleeRangeMetres;
    if (FVector::Dist2D(Taunter->GetActorLocation(), Boar->GetActorLocation()) > RangeMetres * 100.0)
    { return Deny(TEXT("out of range")); }
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CombatPilotTaunt), false, Taunter);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Taunter->GetActorLocation() + FVector(0, 0, 45),
        Boar->GetActorLocation() + FVector(0, 0, 45), ECC_Visibility, Query) && Hit.GetActor() != Boar) { return Deny(TEXT("line of sight")); }
    if (bPublished)
    {
        if (!Taunter->SpendStaminaFraction(GetDefault<USFCombatEnergySettings>()->ChallengeStaminaCostFraction))
        { return Deny(TEXT("combat_resource")); }
        Taunter->CommitChallenge(Now);
    }
    const bool bApplied = Boar->ReceivePilotTaunt(Taunter);
    UE_LOG(LogTemp, Display, TEXT("CHALLENGE_SERVER_RESULT Applied=%d Level=%d Stamina=%.2f"),
        bApplied ? 1 : 0, Taunter->CombatCharacterLevel(), Taunter->GetStamina());
    return bApplied;
}

void ASFGameMode::TryBoarAttack(ASFPilotBoar* Boar, ASFE1Character* Victim)
{
    if (!bPilotActive || !CapturedSettings->bCombatAvailable || !IsValid(Boar) || !IsValid(Victim) ||
        Boar->Health() <= 0 || Victim->CombatHealth() <= 0 || IsSafe(Boar->GetActorLocation()) ||
        IsSafe(Victim->GetActorLocation()) ||
        !SFCombatPilotRules::CanRanged(FVector::Dist2D(Boar->GetActorLocation(), Victim->GetActorLocation()),
            (Boar->IsRangedArcher() ? Boar->GetBehavior().RangedRangeMetres : Boar->GetBehavior().MeleeRangeMetres) * 100.0)) return;
    if (Boar->IsRangedArcher())
    {
        const FVector Start = Boar->GetActorLocation() + FVector(0, 0, 80);
        const FVector End = Victim->GetActorLocation() + FVector(0, 0, 60);
        if (!SFCombatPilotRules::HasRangedLineOfSight(GetWorld(), Boar, Victim, Start, End)) return;
        Boar->MulticastArcherTracer(Start, End);
        UE_LOG(LogTemp, Display, TEXT("ARCHER_RANGED_SHOT Creature=%s DistanceCm=%.0f"),
            *Boar->GetCreatureId(), FVector::Dist2D(Boar->GetActorLocation(), Victim->GetActorLocation()));
    }
    SFCombatResolution::FAttack Attack;
    const bool bPart6 = SFCombatPart6::Active(GetWorld());
    if (bPart6)
    {
        if (!Victim->HasCombatSnapshot()) return;
        const auto Npc = SFCombatPart6::Npc(Boar->GetCreatureId());
        const auto Defender = Victim->CombatStats(CapturedSettings->CritBase, CapturedSettings->SkillDamageFloor);
        const auto Equipment = SFGear::CombatEquipment(Victim->GetController(), CapturedSettings->DevPilotArmorUnit);
        Attack.RawDamage = Boar->CombatSwingDamage();
        Attack.AttackerLevel = Npc.Level;
        Attack.bCanMiss = true;
        Attack.MissChance = FMath::Clamp((Defender.Skill - Npc.Skill) / CapturedSettings->MissDivisor,
            0.0, double(CapturedSettings->MissCap));
        Attack.bCanCrit = true;
        Attack.CritChance = FMath::Min(double(CapturedSettings->NpcCritCap), Npc.CritChance);
        Attack.CritDamage = CapturedSettings->NpcCritDamage;
        Attack.bHasArmorSnapshot = true;
        Attack.Armor = Equipment.Armor;
        Attack.bHasResistanceSnapshot = true;
        Attack.Resistance = Defender.Resistance;
        Attack.bCanBlock = Equipment.bShield;
        Attack.BlockChance = Equipment.bShield ?
            FMath::Min(double(CapturedSettings->BlockCap), .10 + Victim->BlockBonus(GetWorld()->GetTimeSeconds())) : 0;
        Attack.BlockReduction = FMath::Min(.40, double(CapturedSettings->BlockReduction) +
            FMath::Min(.10, FMath::Max(0, Defender.Strength - 10) / 4000.0));
        Attack.Absorb = Victim->CombatAbsorb();
    }
    else { Attack.RawDamage = CapturedSettings->BoarMeleeDamage; }
    Attack.bFrontArc = SFBoarDodgeRules::IsInFrontArc(Victim->GetActorForwardVector(),
        Boar->GetActorLocation() - Victim->GetActorLocation(), CapturedSettings->FrontArcDegrees);
    // The physical avoidance stages stay active for ranged attacks. Starter gear currently
    // supplies no player dodge/parry rating, so their chances remain zero until ratings ship.
    Attack.bCanDodge = Attack.bFrontArc;
    Attack.bCanParry = Attack.bFrontArc;
    if (!bPart6) { Attack.PilotFlatArmor = SFGear::Equipped(Victim->GetController()).Armor; }
    const auto Result = ResolveCombatAttack(Boar, Attack);
    RecordIncomingCombatPractice(Boar, Victim, Attack, Result);
    if (Result.Absorbed > 0) { Victim->ConsumeCombatAbsorb(Result.Absorbed); }
    if (Result.Damage > 0) { HitPlayer(Boar, Victim, Result.Damage, true); }
}

void ASFGameMode::ApplyFallDamage(ASFE1Character* Victim, float ImpactSpeedMps)
{
    if (!bPilotActive || !CapturedSettings || !IsValid(Victim) || !Victim->HasAuthority()) { return; }
    const int32 Damage = SFFallDamage::Amount(Victim->CombatMaximumHealth(), ImpactSpeedMps,
        CapturedSettings->FallSpeedThresholdMps);
    if (Damage <= 0) { return; }
    UE_LOG(LogTemp, Display, TEXT("FALL_DAMAGE ImpactMps=%.2f Damage=%d HealthBefore=%d Pawn=%s"),
        ImpactSpeedMps, Damage, Victim->CombatHealth(), *Victim->GetName());
    HitPlayer(nullptr, Victim, Damage, true);
}

void ASFGameMode::HitPlayer(AActor* Source, ASFE1Character* Victim, int32 Damage, bool bIgnoreArmor,
    bool bLethalRetaliation)
{
    if (!IsValid(Victim) || !Victim->HasAuthority() || Victim->CombatHealth() <= 0 ||
        USFCharacterSessionComponent::IsRestoring(Victim->GetController())) return;
    const bool bRetaliation = bLethalRetaliation && IsValid(Source) && Source->HasAuthority() &&
        Source->GetWorld() == Victim->GetWorld() && Source->IsA<ASFPilotBoar>() &&
        Source->ActorHasTag(ASFPilotBoar::OutOfPlaceNpcTag);
    if (bRetaliation) Damage = Victim->CombatHealth();
    else
    {
        if (!bIgnoreArmor) { Damage = SFGear::MitigatedDamage(Damage, SFGear::Equipped(Victim->GetController()).Armor); }
        Damage = FMath::Max(1, FMath::RoundToInt(Damage * Victim->DamageTakenMultiplier(Victim->GetWorld()->GetTimeSeconds())));
    }
    Victim->InterruptCombatAbilityCast();
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
            const bool bProduction = SFNpcZoneContent::IsProductionMap(Victim->GetWorld());
            Request.RespawnDelayMs = FMath::RoundToInt((bProduction ? Mode->CapturedSettings->ResAcceptSeconds :
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
    const bool bProduction = SFNpcZoneContent::IsProductionMap(GetWorld());
    if ((!bSandbox && !bProduction) || (bSandbox && USFBackpackComponent::Services()) ||
        !HasAuthority() || !bPilotActive || !CapturedSettings ||
        !IsValid(Player) || !Player->CanAcceptResurrectionOffer(OfferId, GetWorld()->GetTimeSeconds()))
    {
        UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_SERVER_DENIED Pawn=%s Offer=%u"), *GetNameSafe(Player), OfferId);
        return false;
    }
    if (bProduction)
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
        RespawnPlayer(Weak, FSFResurrectionAcceptRules::RestoredHealth(Player->CombatMaximumHealth(),
            CapturedSettings->PlayerResFraction), true);
        return true;
    }
    if (!Player->AcceptResurrectionOffer(OfferId, GetWorld()->GetTimeSeconds(),
        Player->CombatMaximumHealth(), CapturedSettings->PlayerResFraction)) return false;
    UE_LOG(LogTemp, Display, TEXT("RES_ACCEPT_SERVER_ACCEPTED Pawn=%s Offer=%u HP=%d"),
        *Player->GetName(), OfferId, Player->CombatHealth());
    return true;
}

bool ASFGameMode::ReleaseSpirit(ASFE1Character* Player)
{
    const FString Map = GetWorld() ? GetWorld()->GetOutermost()->GetName() : FString();
    const bool bSandbox = Map == TEXT("/Game/Frontier/Maps/Sandbox/SB_SpiritRelease");
    const bool bProduction = SFNpcZoneContent::IsProductionMap(GetWorld());
    if (!FSFSpiritReleaseRules::CanRelease(bSandbox || bProduction, HasAuthority(), IsValid(Player) &&
        Player->GetController() != nullptr, IsValid(Player) ? Player->CombatHealth() : -1, !RespawnPoints.IsEmpty()) ||
        !bPilotActive || !CapturedSettings || (bSandbox && USFBackpackComponent::Services()))
    {
        UE_LOG(LogTemp, Display, TEXT("SPIRIT_RELEASE_SERVER_DENIED Pawn=%s"), *GetNameSafe(Player));
        return false;
    }
    if (bProduction)
    {
        const auto Weak = TWeakObjectPtr<ASFE1Character>(Player);
        if (!USFBackpackComponent::Services() || !PlayerDeathEvents.Contains(Weak) ||
            PlayerRespawnInFlight.Contains(Weak)) return false;
        RespawnPlayer(Weak, FSFSpiritReleaseRules::RestoredHealth(Player->CombatMaximumHealth(),
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
    Player->CombatSetHealth(FSFSpiritReleaseRules::RestoredHealth(Player->CombatMaximumHealth(),
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

bool ASFGameMode::TryNearestRespawnPoint(const FVector& From, float PawnHalfHeight, FVector& OutLanding) const
{
    if (RespawnPoints.IsEmpty()) return false;
    const FVector* Nearest = &RespawnPoints[0];
    double Best = FVector::DistSquared2D(From, *Nearest);
    for (const FVector& Point : RespawnPoints)
    {
        const double Distance = FVector::DistSquared2D(From, Point);
        if (Distance < Best) { Best = Distance; Nearest = &Point; }
    }
    OutLanding = Grounded(GetWorld(), *Nearest, PawnHalfHeight);
    return true;
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
        if (AController* Controller = Player->GetController())
            if (USFBackpackComponent* Backpack = Controller->FindComponentByClass<USFBackpackComponent>())
                Backpack->Reconcile();
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
    if (AController* Controller = Player->GetController())
        if (USFBackpackComponent* Backpack = Controller->FindComponentByClass<USFBackpackComponent>())
            Backpack->Reconcile();
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
    Player->CombatSetHealth(RestoredHealth > 0 ? RestoredHealth : Player->CombatMaximumHealth());
    if (bAtDeathPosition) Player->RestoreCombatManaFraction(CapturedSettings->PlayerResFraction);
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
            const int32 TownReward = RewardKill(FixtureBoar, Player.Get());
            UE_LOG(LogTemp, Display, TEXT("COMBAT_PILOT_SAFE_CHECK Protected=%d PlayerHP=%d BoarHP=%d Reward=%d Passed=%d"),
                IsSafe(Town), FixturePlayer->CombatHealth(), FixtureBoar->Health(), TownReward,
                IsSafe(Town) && Before == FixturePlayer->CombatHealth() && FixtureBoar->Health() == CapturedSettings->BoarHealth && TownReward == 0);
        }
        if (FixtureBoar) { FixtureBoar->Destroy(); }
        if (FixturePlayer) { FixturePlayer->Destroy(); }
    }
}

int32 ASFGameMode::RewardKill(ASFPilotBoar* Boar, ASFE1Character* Killer)
{
    if (!CapturedSettings || !HasAuthority() || !IsValid(Boar)) { return 0; }
    const FString DeathId = Boar->DeathId();
    const FVector BoarAt = Boar->GetActorLocation();
    const bool bProtected = IsSafe(BoarAt) || !IsValid(Killer) || IsSafe(Killer->GetActorLocation());
    auto* KillerController = IsValid(Killer) ? Cast<APlayerController>(Killer->GetController()) : nullptr;
    USFBackpackComponent* KillerIdentity = KillerController ? KillerController->FindComponentByClass<USFBackpackComponent>() : nullptr;
    const FString RaidId = KillerIdentity ? RaidIdByCharacter.FindRef(KillerIdentity->GetCharacterId()) : FString();
    const TArray<FString>* RaidMembers = RaidId.IsEmpty() ? nullptr : RaidMembersById.Find(RaidId);
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
            USFBackpackComponent* Identity = Player->FindComponentByClass<USFBackpackComponent>();
            const bool bRaidMember = RaidMembers && Identity && RaidMembers->Contains(Identity->GetCharacterId());
            if (Player == KillerController || (((Party && Party->Members.Contains(Player->GetUniqueID())) || bRaidMember) &&
                Character->CombatHealth() > 0 && !IsSafe(Character->GetActorLocation()) &&
                FVector::Dist(Character->GetActorLocation(), BoarAt) <= RadiusCm))
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
        FSFLootDefeatSource Source;
        Source.EventId = Epoch + TEXT(":") + DeathId;
        Source.CreatureId = Boar->GetCreatureId(); Source.TableId = Boar->GetLootTableId();
        Source.KillerCharacterId = KillerId; Source.RaidGroupId = RaidId;
        Source.Position = BoarAt; Source.bShared = Boar->HasSingleDrop();
        Source.bProtected = bProtected || bBoarSafe;
        for (APlayerController* Recipient : Eligible)
        {
            USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Recipient);
            if (Backpack && !Backpack->GetCharacterId().IsEmpty())
            { Source.EligibleCharacters.AddUnique(Backpack->GetCharacterId()); }
        }
        // Admission transfers retry ownership. Never also execute the legacy direct payout.
        if (USFLootWorldSubsystem* Loot = GetWorld()->GetSubsystem<USFLootWorldSubsystem>())
        { if (Loot->RegisterDefeat(Boar, Source)) { return 0; } }
        if (Boar->HasSingleDrop())
        {
            FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::CreateSharedDefeatDrop;
            Request.OperationId = Epoch + TEXT(":") + DeathId;
            Request.KillerCharacterId = KillerId; Request.CreatureId = Boar->GetCreatureId();
            Request.TableId = Boar->GetLootTableId(); Request.bKillerProtected = bProtected;
            Request.RaidGroupId = RaidId;
            Request.bBoarProtected = bBoarSafe;
            TArray<TWeakObjectPtr<ASFPlayerController>> Players;
            for (APlayerController* Recipient : Eligible)
            {
                USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Recipient);
                if (Backpack && !Backpack->GetCharacterId().IsEmpty())
                { Request.EligibleCharacterIds.AddUnique(Backpack->GetCharacterId()); Players.Add(Cast<ASFPlayerController>(Recipient)); }
            }
            if (!Request.EligibleCharacterIds.IsEmpty()) { RetrySharedDrop(Request, Players); }
            return 0;
        }
        for (APlayerController* Recipient : Eligible)
        {
            USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Recipient);
            const ASFE1Character* Character = Cast<ASFE1Character>(Recipient->GetPawn());
            RetryDefeatReward(DeathId, Epoch + TEXT(":") + DeathId, KillerId,
                Backpack ? Backpack->GetCharacterId() : FString(), Backpack ? Backpack->GetContainerId() : FString(),
                Boar->GetCreatureId(), Boar->GetLootTableId(),
                !Character || IsSafe(Character->GetActorLocation()), bBoarSafe, Recipient);
        }
        return 0;
    }
    if (!Boar->GetNpcProfile() || !Boar->GetNpcProfile()->bLocalHidesFixture) { return 0; }
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
    const FString& ContainerId, const FString& CreatureId, const FString& TableId,
    bool bKillerSafe, bool bBoarSafe, TWeakObjectPtr<APlayerController> Controller)
{
    USFBackpackComponent* Backpack = Controller.IsValid() ? USFBackpackComponent::FindOrCreate(Controller.Get()) : nullptr;
    if (!Backpack)
    {
        UE_LOG(LogTemp, Warning, TEXT("COMBAT_PILOT_REWARD_HELD Death=%s Reason=BackpackUnavailable"), *DeathId);
        if (Controller.IsValid())
        {
            FTimerHandle Retry;
            GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(this,
                [WeakMode = TWeakObjectPtr<ASFGameMode>(this), DeathId, ServiceEventId, KillerId, CharacterId, ContainerId, CreatureId, TableId,
                 bKillerSafe, bBoarSafe, Controller]()
                { if (ASFGameMode* Mode = WeakMode.Get()) Mode->RetryDefeatReward(DeathId, ServiceEventId, KillerId, CharacterId, ContainerId, CreatureId, TableId, bKillerSafe, bBoarSafe, Controller); }), 3.0f, false);
        }
        return;
    }
    if ((!CharacterId.IsEmpty() && Backpack->GetCharacterId() != CharacterId) ||
        (!ContainerId.IsEmpty() && Backpack->GetContainerId() != ContainerId)) return;
    const FString RecipientId = Backpack->GetCharacterId(), RecipientContainer = Backpack->GetContainerId();
    auto Receipt = MakeShared<FSFGameplayResponse>();
    FSFBackpackMutation Mutation; Mutation.Label = TEXT("defeat_reward"); Mutation.OperationId = ServiceEventId;
    Mutation.bRetryStale = false;
    Mutation.Run = [Receipt, ServiceEventId, KillerId, RecipientId, RecipientContainer, CreatureId, TableId, bKillerSafe, bBoarSafe](const FString&, int64 Expected,
        TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        FSFGameplayRequest Q = SFCombatPilotRewards::BuildDefeatRequest(ServiceEventId, KillerId, RecipientId,
            RecipientContainer, Expected, bKillerSafe, bBoarSafe, CreatureId, TableId);
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
        KillerId, RecipientId, RecipientContainer, CreatureId, TableId, bKillerSafe, bBoarSafe, Controller, Weak = TWeakObjectPtr<USFBackpackComponent>(Backpack)](ESFBackpackOutcome Outcome)
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
                [WeakMode, DeathId, ServiceEventId, KillerId, RecipientId, RecipientContainer, CreatureId, TableId, bKillerSafe, bBoarSafe, Controller]()
                { if (ASFGameMode* M = WeakMode.Get()) M->RetryDefeatReward(DeathId, ServiceEventId, KillerId, RecipientId, RecipientContainer, CreatureId, TableId, bKillerSafe, bBoarSafe, Controller); }), 3.0f, false);
        }
    };
    Backpack->Enqueue(MoveTemp(Mutation));
}

void ASFGameMode::RetrySharedDrop(const FSFGameplayRequest& Request,
    const TArray<TWeakObjectPtr<ASFPlayerController>>& Players)
{
    if (!USFBackpackComponent::Services()) { return; }
    USFBackpackComponent::Services()->Call(Request,
        [WeakMode = TWeakObjectPtr<ASFGameMode>(this), Request, Players](const FSFGameplayResponse& Reply)
    {
        ASFGameMode* Mode = WeakMode.Get(); if (!Mode) { return; }
        if (Reply.Status == ESFGameplayStatus::Ok)
        {
            UE_LOG(LogTemp, Display, TEXT("SHARED_LOOT_CREATED Event=%s Creature=%s Eligible=%d Items=%lld"),
                *Request.OperationId, *Request.CreatureId, Request.EligibleCharacterIds.Num(), Reply.Quantity);
            for (const TWeakObjectPtr<ASFPlayerController>& Player : Players)
            { if (Player.IsValid()) { Player->ClientSharedDropAvailable(Request.OperationId); } }
        }
        else if (Reply.Status == ESFGameplayStatus::Unavailable)
        {
            FTimerHandle Retry;
            Mode->GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(Mode,
                [WeakMode, Request, Players]()
                { if (ASFGameMode* M = WeakMode.Get()) { M->RetrySharedDrop(Request, Players); } }), 3.0f, false);
        }
        else { UE_LOG(LogTemp, Warning, TEXT("SHARED_LOOT_CREATE_DENIED Event=%s Code=%d"), *Request.OperationId, Reply.GrpcCode); }
    });
}

void ASFGameMode::ClaimSharedDrop(ASFPlayerController* Player, const FString& EventId)
{
    if (!IsValid(Player) || !HasAuthority() || EventId.IsEmpty() || !USFBackpackComponent::Services()) { return; }
    USFBackpackComponent* Backpack = USFBackpackComponent::FindOrCreate(Player);
    if (!Backpack || Backpack->GetCharacterId().IsEmpty() || Backpack->GetContainerId().IsEmpty()) { return; }
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::ClaimSharedDefeatDrop;
    Request.OperationId = EventId; Request.CharacterId = Backpack->GetCharacterId();
    Request.ContainerId = Backpack->GetContainerId();
    USFBackpackComponent::Services()->Call(Request,
        [WeakMode = TWeakObjectPtr<ASFGameMode>(this), WeakPlayer = TWeakObjectPtr<ASFPlayerController>(Player),
         WeakBackpack = TWeakObjectPtr<USFBackpackComponent>(Backpack), Request](const FSFGameplayResponse& Reply)
    {
        if (!WeakMode.IsValid()) { return; }
        const bool bWon = Reply.Status == ESFGameplayStatus::Ok && Reply.ClaimantCharacterId == Request.CharacterId;
        UE_LOG(LogTemp, Display, TEXT("SHARED_LOOT_CLAIM Event=%s Character=%s Won=%d Code=%d Items=%lld"),
            *Request.OperationId, *Request.CharacterId, bWon, Reply.GrpcCode, Reply.Quantity);
        if (bWon)
        { if (USFBackpackComponent* Owner = WeakBackpack.Get()) { Owner->Reconcile(); } }
        else if (Reply.Status == ESFGameplayStatus::Unavailable && WeakPlayer.IsValid())
        {
            ASFGameMode* Mode = WeakMode.Get(); FTimerHandle Retry;
            Mode->GetWorldTimerManager().SetTimer(Retry, FTimerDelegate::CreateWeakLambda(Mode,
                [WeakMode, WeakPlayer, RetryEvent = Request.OperationId]()
                { if (ASFGameMode* M = WeakMode.Get()) { M->ClaimSharedDrop(WeakPlayer.Get(), RetryEvent); } }), 3.0f, false);
        }
    });
}

void ASFPlayerController::ClientSharedDropAvailable_Implementation(const FString& EventId)
{
    UE_LOG(LogTemp, Display, TEXT("SHARED_LOOT_AVAILABLE Event=%s"), *EventId);
    if (FParse::Param(FCommandLine::Get(), TEXT("PersonalLootScenario")))
    {
        int32 ScenarioRole = 0; FParse::Value(FCommandLine::Get(), TEXT("PartyXpScenarioRole="), ScenarioRole);
        FTimerHandle ClaimTimer;
        GetWorldTimerManager().SetTimer(ClaimTimer, FTimerDelegate::CreateWeakLambda(this,
            [Weak = TWeakObjectPtr<ASFPlayerController>(this), EventId]()
            { if (ASFPlayerController* Player = Weak.Get()) { Player->ServerClaimSharedDrop(EventId); } }),
            ScenarioRole == 1 ? 0.2f : 1.5f, false);
    }
}

void ASFPlayerController::ServerClaimSharedDrop_Implementation(const FString& EventId)
{
    if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>()) { Mode->ClaimSharedDrop(this, EventId); }
}

void ASFPlayerController::SFClaimDrop(const FString& EventId)
{
    ServerClaimSharedDrop(EventId);
}

void ASFGameMode::BoarDied(ASFPilotBoar* Boar, ASFE1Character* Killer)
{
    if (!bPilotActive || !IsValid(Boar)) { return; }
    BoarCombatStreams.Remove(Boar);
    if (bCompanionSandbox)
    {
        if (GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_PersonalLoot"))
        { RewardKill(Boar, Killer); }
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
    RewardKill(Boar, Killer);
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
