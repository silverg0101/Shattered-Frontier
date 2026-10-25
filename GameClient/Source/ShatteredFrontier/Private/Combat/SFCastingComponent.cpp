#include "Combat/SFCastingComponent.h"

#include "Characters/SFE1Character.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Combat/SFCombatPilot.h"
#include "Combat/SFStarterAbilities.h"
#include "Combat/SFCombatPart6.h"
#include "Combat/SFBoarDodgeRules.h"
#include "Framework/SFGameMode.h"
#include "Combat/SFPlayerInterruptRules.h"
#include "Combat/SFCombatEnergy.h"
#include "Inventory/SFGear.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

bool USFCastingSettings::Validate(FString& Error) const
{
    if (!FMath::IsWithinInclusive(GcdBaseSeconds, .75f, 3.f) ||
        !FMath::IsWithinInclusive(GcdShortSeconds, .75f, 2.f) || GcdShortSeconds > GcdBaseSeconds ||
        !FMath::IsWithinInclusive(GcdMinSeconds, .5f, 1.5f) || GcdMinSeconds > GcdShortSeconds || GcdMinSeconds > GcdBaseSeconds ||
        !FMath::IsWithinInclusive(HasteMax, 0.f, .75f) ||
        !FMath::IsWithinInclusive(MoveToleranceMetres, 0.f, 2.f) ||
        !FMath::IsWithinInclusive(PushbackSeconds, 0.f, 1.f) || !FMath::IsWithinInclusive(PushbackMax, 0, 5) ||
        !FMath::IsWithinInclusive(QueueWindowMs, 0, 1000) || !FMath::IsWithinInclusive(CancelRefundPercent, 0, 100) ||
        !FMath::IsWithinInclusive(TrainingCastSeconds, .1f, 10.f) ||
        !FMath::IsWithinInclusive(TrainingChannelSeconds, .1f, 10.f) ||
        !FMath::IsWithinInclusive(TrainingChannelTicks, 1, 20) ||
        !FMath::IsWithinInclusive(TrainingCooldownSeconds, 0.f, 30.f) ||
        !FMath::IsWithinInclusive(TrainingDamagePerTick, 1, 100) ||
        !FMath::IsWithinInclusive(TrainingResourceCost, 0, 100) ||
        !FMath::IsWithinInclusive(TrainingResourceMax, 10, 1000) || TrainingResourceCost > TrainingResourceMax ||
        !FMath::IsWithinInclusive(TrainingTargetHealth, 10, 10000) ||
        !FMath::IsWithinInclusive(TrainingRangeMetres, 1.f, 20.f) ||
        !FMath::IsWithinInclusive(TrainingHasteFraction, 0.f, HasteMax) ||
        !FMath::IsWithinInclusive(TrainingInterruptLockoutSeconds, 0.f, 10.f) ||
        !FMath::IsWithinInclusive(CaptainMendHealthThreshold, .1f, 1.f) ||
        !FMath::IsWithinInclusive(CaptainMendHealFraction, .01f, .3f) ||
        !FMath::IsWithinInclusive(CaptainMendCooldownSeconds, 3.f, 60.f) ||
        !FMath::IsWithinInclusive(NpcSchoolLockoutSeconds, 1.f, 10.f) ||
        !FMath::IsWithinInclusive(CaptainManaMaximum, 20, 1000))
    { Error = TEXT("Combat casting configuration is outside release bounds"); return false; }
    return true;
}

SFCastRules::FTuning USFCastingSettings::Tuning() const
{
    SFCastRules::FTuning Result;
    Result.GcdBase = GcdBaseSeconds;
    Result.GcdShort = GcdShortSeconds;
    Result.GcdMin = GcdMinSeconds;
    Result.HasteMax = HasteMax;
    Result.MoveToleranceCm = MoveToleranceMetres * 100.0;
    Result.Pushback = PushbackSeconds;
    Result.PushbackMax = PushbackMax;
    Result.QueueWindow = QueueWindowMs / 1000.0;
    return Result;
}

USFCastingComponent::USFCastingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void USFCastingComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner()->HasAuthority()) return;
    const USFCastingSettings* Settings = GetDefault<USFCastingSettings>();
    FString Error;
    if (!Settings->Validate(Error)) { UE_LOG(LogTemp, Error, TEXT("CASTING_CONFIG_INVALID %s"), *Error); SetComponentTickEnabled(false); return; }
    Tune = Settings->Tuning();
    CastSeconds = Settings->TrainingCastSeconds;
    ChannelSeconds = Settings->TrainingChannelSeconds;
    ChannelTicks = Settings->TrainingChannelTicks;
    CooldownSeconds = Settings->TrainingCooldownSeconds;
    DamagePerTick = Settings->TrainingDamagePerTick;
    ResourceCost = Settings->TrainingResourceCost;
    ResourceMax = Settings->TrainingResourceMax;
    Resource = ResourceMax;
    RangeMetres = Settings->TrainingRangeMetres;
    HasteFraction = Settings->TrainingHasteFraction;
    LockoutSeconds = Settings->TrainingInterruptLockoutSeconds;
    CancelRefundPercent = Settings->CancelRefundPercent;
    NpcMendHealthThreshold = Settings->CaptainMendHealthThreshold;
    NpcMendHealFraction = Settings->CaptainMendHealFraction;
    NpcMendCooldownSeconds = Settings->CaptainMendCooldownSeconds;
    NpcSchoolLockoutSeconds = Settings->NpcSchoolLockoutSeconds;
    NpcManaMaximum = Settings->CaptainManaMaximum;
    NpcMana = NpcManaMaximum;
    if (InSandbox())
    {
        if (ASFE1Character* Owner = Cast<ASFE1Character>(GetOwner()); Owner && Owner->CombatHealth() <= 0)
        { Owner->CombatSetHealth(GetDefault<class USFCombatPilotSettings>()->PlayerHealth); }
    }
}

bool USFCastingComponent::InSandbox() const
{
    return GetWorld() && (GetWorld()->GetMapName().Contains(TEXT("SB_Casting")) ||
        GetWorld()->GetMapName().Contains(TEXT("SB_Interrupt")));
}

double USFCastingComponent::Now() const
{
    const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    return GameState ? GameState->GetServerWorldTimeSeconds() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

SFCastRules::FAbility USFCastingComponent::Definition(uint8 Id) const
{
    SFCastRules::FAbility Ability;
    Ability.Id = Id;
    Ability.School = 1; // arcane training school; both abilities share its interrupt lockout
    Ability.Gcd = Id == 2 ? SFCastRules::EGcd::Short : SFCastRules::EGcd::Standard;
    Ability.Cast = Id == 1 ? CastSeconds : 0.0;
    Ability.Channel = Id == 2 ? ChannelSeconds : 0.0;
    Ability.Ticks = Id == 2 ? ChannelTicks : 0;
    Ability.Cooldown = CooldownSeconds;
    Ability.bHasteCooldown = true;
    return Ability;
}

void USFCastingComponent::UseTrainingAbility(uint8 AbilityId)
{
    if (GetOwner()->HasAuthority()) TryUse(AbilityId);
    else ServerUseTrainingAbility(AbilityId);
}

void USFCastingComponent::ServerUseTrainingAbility_Implementation(uint8 AbilityId) { TryUse(AbilityId); }

void USFCastingComponent::TryUse(uint8 AbilityId)
{
    ASFE1Character* Owner = Cast<ASFE1Character>(GetOwner());
    if (!Owner || !Owner->HasAuthority() || !InSandbox() || (AbilityId != 1 && AbilityId != 2) ||
        Owner->CombatHealth() <= 0 || (Owner->CrowdControl && Owner->CrowdControl->IsStunned())) return;
    ASFCastingTarget* Best = nullptr;
    double Distance = RangeMetres * 100.0;
    for (TActorIterator<ASFCastingTarget> It(GetWorld()); It; ++It)
    {
        const double D = FVector::Dist(Owner->GetActorLocation(), It->GetActorLocation());
        if (D < Distance && It->GetTrainingHealth() > 0) { Distance = D; Best = *It; }
    }
    if (!Best) return;
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainingCastLOS), false, Owner);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Owner->GetActorLocation() + FVector(0,0,50),
        Best->GetActorLocation() + FVector(0,0,50), ECC_Visibility, Query) && Hit.GetActor() != Best) return;
    if (Resource < ResourceCost) return;
    const auto Result = State.Request(Definition(AbilityId), Now(), HasteFraction, Tune);
    if (Result == SFCastRules::EStart::Started)
    {
        View.bInterrupted = false;
        bCurrentCastInterruptible = true;
        Resource -= ResourceCost;
        StartLocation = Owner->GetActorLocation();
        Target = Best;
        SyncView();
        UE_LOG(LogTemp, Display, TEXT("CASTING_SERVER_START Ability=%d Resource=%d"), AbilityId, Resource);
    }
    else if (Result == SFCastRules::EStart::Queued)
    { UE_LOG(LogTemp, Display, TEXT("CASTING_SERVER_QUEUE Ability=%d"), AbilityId); }
    else if (Result == SFCastRules::EStart::Locked)
    { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_SERVER_LOCKED Ability=%d"), AbilityId); }
}

void USFCastingComponent::RequestTrainingInterrupt()
{
    if (GetOwner()->HasAuthority()) ServerRequestTrainingInterrupt_Implementation();
    else ServerRequestTrainingInterrupt();
}

void USFCastingComponent::RequestPublishedInterrupt(bool bCounterspell, AActor* SelectedTarget)
{
    if (GetOwner()->HasAuthority()) { ServerRequestPublishedInterrupt_Implementation(bCounterspell, SelectedTarget); }
    else { ServerRequestPublishedInterrupt(bCounterspell, SelectedTarget); }
}

void USFCastingComponent::ServerRequestPublishedInterrupt_Implementation(bool bCounterspell, AActor* SelectedTarget)
{
    ASFE1Character* Owner = Cast<ASFE1Character>(GetOwner());
    ASFE1Character* Victim = Cast<ASFE1Character>(SelectedTarget);
    if (!Owner || !Owner->HasAuthority() || !SFNpcZoneContent::IsProductionMap(GetWorld())) return;
    if (SFCombatPart6::Active(GetWorld()) && !Owner->HasCombatSnapshot()) return;
    const double Time = Now();
    const USFCombatEnergySettings* Energy = GetDefault<USFCombatEnergySettings>();
    const double ReachCm = (bCounterspell ? Energy->CounterspellRangeMetres :
        Energy->MeleeReachMetres) * 100.0;
    if (ASFCombatNpc* Npc = Cast<ASFCombatNpc>(SelectedTarget))
    {
        ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>();
        if (!Mode || !Mode->IsCombatActive() || Mode->IsSafe(Owner->GetActorLocation())) return;
        const bool bCasting = Npc->GetCasting() &&
            Npc->GetCasting()->GetCastView().Phase == ESFCastPhase::Casting &&
            Npc->GetCasting()->IsCurrentCastInterruptible();
        const FVector Delta = Npc->GetActorLocation() - Owner->GetActorLocation();
        const bool bValid = Npc->Health() > 0 && !Npc->IsLeashing() && Npc->HasThreatFor(Owner) &&
            Delta.Size2D() <= ReachCm && (bCounterspell ||
            FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(), Delta.GetSafeNormal2D()) >= 0.f);
        if (!SFCombatEnergy::CanUseRankOne(Owner->CombatCharacterLevel(), Owner->CombatHealth() > 0,
            bValid, bCasting, true) || Time < (bCounterspell ? NextCounterspellAt : NextPommelAt)) return;
        FCollisionQueryParams NpcQuery(SCENE_QUERY_STAT(NpcPublishedInterruptLOS), false, Owner);
        NpcQuery.AddIgnoredActor(Npc);
        if (GetWorld()->LineTraceTestByChannel(Owner->GetActorLocation() + FVector(0, 0, 50),
            Npc->GetActorLocation() + FVector(0, 0, 50), ECC_Visibility, NpcQuery)) return;
        const bool bPaid = bCounterspell ? Owner->SpendManaFraction(Energy->CounterspellManaCostFraction) :
            Owner->SpendStaminaFraction(Energy->PommelStaminaCostFraction);
        if (!bPaid) return;
        if (bCounterspell) NextCounterspellAt = Time + Energy->CounterspellCooldownSeconds;
        else NextPommelAt = Time + Energy->PommelCooldownSeconds;
        Npc->GetCasting()->Interrupt();
        if (!bCounterspell)
        {
            if (SFCombatPart6::Active(GetWorld()))
            {
                const auto* CombatTune = GetDefault<USFCombatPilotSettings>();
                const auto Stats = Owner->CombatStats(CombatTune->CritBase, CombatTune->SkillDamageFloor);
                const auto Weapon = SFGear::CombatEquipment(Owner->GetController(), CombatTune->DevPilotArmorUnit);
                const auto* Coefficient = SFCombatPart6::Ability(TEXT("pommel_strike"));
                check(Coefficient);
                auto Attack = SFCombatPart6::AuthoredAbility(Stats, Weapon.Weapon, *Coefficient,
                    CombatTune->WeaponSpread, CombatTune->PowerSpeedReferenceSeconds,
                    CombatTune->UnarmedDpsFactor, CombatTune->WeaponDpsK);
                Attack.bHasArmorSnapshot = true; Attack.Armor = Npc->CombatArmor();
                Attack.bFrontArc = SFBoarDodgeRules::IsInFrontArc(Npc->GetActorForwardVector(),
                    Owner->GetActorLocation() - Npc->GetActorLocation(), CombatTune->FrontArcDegrees);
                const auto Result = Mode->ResolveCombatAttack(Owner, Attack);
                if (Result.Damage > 0) { Npc->ReceivePilotDamage(Owner, Result.Damage); }
            }
            else
            {
                const int32 BaseDamage = GetDefault<USFCombatPilotSettings>()->PlayerMeleeDamage +
                    SFGear::Equipped(Owner->GetController()).Damage;
                const int32 Damage = FMath::Max(1, FMath::RoundToInt(BaseDamage * Energy->PommelPhysicalModifier));
                Npc->ReceivePilotDamage(Owner, Damage);
            }
            Owner->CombatMarkHostile();
        }
        UE_LOG(LogTemp, Display, TEXT("NPC_INTERRUPT_APPLIED Kind=%s Creature=%s Mana=%d Locked=%d"),
            bCounterspell ? TEXT("counterspell") : TEXT("pommel_strike"), *Npc->GetCreatureId(),
            Npc->GetCasting()->GetNpcMana(), Npc->GetCasting()->IsNpcSchoolLocked() ? 1 : 0);
        return;
    }
    const bool bHostile = Victim && Owner->GetCombatTarget() == Victim && Victim->GetCombatTarget() == Owner;
    const bool bCasting = Victim && ((Victim->Casting && Victim->Casting->GetCastView().Phase != ESFCastPhase::Idle) ||
        Victim->IsCombatAbilityCasting());
    const bool bValid = bHostile && Victim->CombatHealth() > 0 &&
        FVector::Dist2D(Owner->GetActorLocation(), Victim->GetActorLocation()) <= ReachCm &&
        (bCounterspell || FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(),
            (Victim->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D()) >= 0.f);
    if (!SFCombatEnergy::CanUseRankOne(Owner->CombatCharacterLevel(), Owner->CombatHealth() > 0,
        bValid, bCasting, true))
    { UE_LOG(LogTemp, Display, TEXT("PUBLISHED_INTERRUPT_DENIED Reason=level_target_or_cast Level=%d Hostile=%d Valid=%d Casting=%d Distance=%.0f Facing=%.2f"),
        Owner->CombatCharacterLevel(), bHostile ? 1 : 0, bValid ? 1 : 0, bCasting ? 1 : 0,
        Victim ? FVector::Dist2D(Owner->GetActorLocation(), Victim->GetActorLocation()) : -1.0,
        Victim ? FVector::DotProduct(Owner->GetActorForwardVector().GetSafeNormal2D(),
            (Victim->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal2D()) : -1.0); return; }
    if (Time < (bCounterspell ? NextCounterspellAt : NextPommelAt))
    { UE_LOG(LogTemp, Display, TEXT("PUBLISHED_INTERRUPT_DENIED Reason=cooldown")); return; }
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PublishedInterruptLOS), false, Owner);
    Query.AddIgnoredActor(Victim);
    if (GetWorld()->LineTraceTestByChannel(Owner->GetActorLocation() + FVector(0, 0, 50),
        Victim->GetActorLocation() + FVector(0, 0, 50), ECC_Visibility, Query))
    { UE_LOG(LogTemp, Display, TEXT("PUBLISHED_INTERRUPT_DENIED Reason=sight")); return; }
    const bool bPaid = bCounterspell ? Owner->SpendManaFraction(Energy->CounterspellManaCostFraction) :
        Owner->SpendStaminaFraction(Energy->PommelStaminaCostFraction);
    if (!bPaid) { UE_LOG(LogTemp, Display, TEXT("PUBLISHED_INTERRUPT_DENIED Reason=combat_resource")); return; }
    if (bCounterspell) { NextCounterspellAt = Time + Energy->CounterspellCooldownSeconds; }
    else { NextPommelAt = Time + Energy->PommelCooldownSeconds; }
    const bool bApplied = Victim->IsCombatAbilityCasting() ||
        (Victim->Casting && Victim->Casting->IsCurrentCastInterruptible());
    if (bApplied)
    {
        Victim->InterruptCombatAbilityCast();
        if (Victim->Casting && Victim->Casting->GetCastView().Phase != ESFCastPhase::Idle)
            Victim->Casting->Interrupt();
    }
    if (!bCounterspell)
    {
        if (SFCombatPart6::Active(GetWorld()))
        {
            const auto* CombatTune = GetDefault<USFCombatPilotSettings>();
            const auto AttackerStats = Owner->CombatStats(CombatTune->CritBase, CombatTune->SkillDamageFloor);
            const auto Weapon = SFGear::CombatEquipment(Owner->GetController(), CombatTune->DevPilotArmorUnit);
            const auto DefenderGear = SFGear::CombatEquipment(Victim->GetController(), CombatTune->DevPilotArmorUnit);
            const auto* Coefficient = SFCombatPart6::Ability(TEXT("pommel_strike"));
            check(Coefficient);
            SFCombatResolution::FAttack Attack = SFCombatPart6::AuthoredAbility(AttackerStats, Weapon.Weapon,
                *Coefficient, CombatTune->WeaponSpread,
                CombatTune->PowerSpeedReferenceSeconds, CombatTune->UnarmedDpsFactor, CombatTune->WeaponDpsK);
            Attack.bFrontArc = SFBoarDodgeRules::IsInFrontArc(Victim->GetActorForwardVector(),
                Owner->GetActorLocation() - Victim->GetActorLocation(), CombatTune->FrontArcDegrees);
            Attack.bHasArmorSnapshot = true; Attack.Armor = DefenderGear.Armor;
            Attack.bCanBlock = DefenderGear.bShield;
            Attack.BlockChance = DefenderGear.bShield ? .10 : 0;
            Attack.BlockReduction = CombatTune->BlockReduction;
            Attack.Absorb = Victim->CombatAbsorb();
            if (ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>())
            {
                const auto Result = Mode->ResolveCombatAttack(Owner, Attack);
                Victim->ConsumeCombatAbsorb(Result.Absorbed);
                if (Result.Damage > 0)
                { Victim->CombatSetHealth(FMath::Max(0, Victim->CombatHealth() - Result.Damage)); }
            }
        }
        else
        {
            const int32 BaseDamage = GetDefault<USFCombatPilotSettings>()->PlayerMeleeDamage +
                SFGear::Equipped(Owner->GetController()).Damage;
            const int32 Damage = FMath::Max(1, FMath::RoundToInt(BaseDamage * Energy->PommelPhysicalModifier));
            Victim->CombatSetHealth(FMath::Max(0, Victim->CombatHealth() - Damage));
        }
        Owner->CombatMarkHostile();
    }
    UE_LOG(LogTemp, Display, TEXT("PUBLISHED_INTERRUPT_APPLIED Kind=%s Applied=%d Level=%d Stamina=%.2f Mana=%.2f"),
        bCounterspell ? TEXT("counterspell") : TEXT("pommel_strike"), bApplied ? 1 : 0, Owner->CombatCharacterLevel(),
        Owner->GetStamina(), Owner->GetMana());
}

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
bool USFCastingComponent::BeginPublishedInterruptFixtureCast(bool bInterruptible)
{
    if (!GetOwner()->HasAuthority() || !GetWorld() ||
        GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook") ||
        !FParse::Param(FCommandLine::Get(), TEXT("GreenlandsEnergyNetcheck"))) return false;
    const auto Result = State.Request(Definition(1), Now(), 0.0, Tune);
    if (Result != SFCastRules::EStart::Started) return false;
    bCurrentCastInterruptible = bInterruptible;
    View.bInterrupted = false;
    Resource -= ResourceCost;
    SyncView();
    return true;
}
#endif

void USFCastingComponent::ServerRequestTrainingInterrupt_Implementation()
{
    ASFE1Character* Owner = Cast<ASFE1Character>(GetOwner());
    if (!Owner || !Owner->HasAuthority() || !GetWorld() ||
        GetWorld()->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Sandbox/SB_Interrupt") ||
        Owner->CombatHealth() <= 0) return;
    const double ReachCm = GetDefault<USFCombatPilotSettings>()->MeleeRangeMetres * 100.0;
    ASFE1Character* Best = nullptr;
    double Distance = ReachCm;
    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
    {
        ASFE1Character* Candidate = *It;
        if (Candidate == Owner || !Candidate->Casting) continue;
        const double D = FVector::Dist2D(Owner->GetActorLocation(), Candidate->GetActorLocation());
        if (SFPlayerInterruptRules::CanInterrupt(Owner->CombatHealth() > 0,
            Candidate->CombatHealth() > 0, Candidate->Casting->GetCastView().Phase != ESFCastPhase::Idle,
            D, Distance, true)) { Distance = D; Best = Candidate; }
    }
    if (!Best) { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_SERVER_DENIED Reason=RangeOrState")); return; }
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(TrainingInterruptLOS), false, Owner);
    Query.AddIgnoredActor(Best);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Owner->GetActorLocation() + FVector(0, 0, 50),
        Best->GetActorLocation() + FVector(0, 0, 50), ECC_Visibility, Query))
    { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_SERVER_DENIED Reason=Sight")); return; }
    Best->Casting->Interrupt();
    UE_LOG(LogTemp, Display, TEXT("INTERRUPT_SERVER_APPLIED Target=%s RangeCm=%.0f Resource=%d"),
        *Best->GetName(), Distance, Best->Casting->GetTrainingResource());
}

void USFCastingComponent::ServerSetInterruptScenarioPosition_Implementation(bool bCaster, bool bNear)
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetWorld() && GetWorld()->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Sandbox/SB_Interrupt") &&
        FParse::Param(FCommandLine::Get(), TEXT("InterruptScenario")))
    {
        if (ASFE1Character* Owner = Cast<ASFE1Character>(GetOwner()))
        { Owner->SetActorLocation(FVector(0, bCaster ? 0 : (bNear ? 200 : 600), 110)); }
    }
#endif
}

void USFCastingComponent::TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(Dt, TickType, TickFunction);
    if (ASFCombatNpc* Npc = Cast<ASFCombatNpc>(GetOwner()))
    {
        if (Npc->HasAuthority() && State.Phase == SFCastRules::EPhase::Casting)
        {
            bool bCompleted = false;
            SFCastRules::FAbility Ability;
            Ability.Id = 3; Ability.School = 2;
            State.Advance(Now(), Ability, bCompleted);
            if (bCompleted)
            {
                Npc->CompleteNpcMend(NpcMendHealFraction);
                View.bInterrupted = false;
                SyncView();
            }
        }
        return;
    }
    ASFE1Character* Owner = Cast<ASFE1Character>(GetOwner());
    if (!Owner || !InSandbox()) return;
    if (!Owner->HasAuthority())
    {
        if (Owner->IsLocallyControlled())
        {
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
            if (FParse::Param(FCommandLine::Get(), TEXT("InterruptScenario")))
            {
                FString Role;
                FParse::Value(FCommandLine::Get(), TEXT("InterruptRole="), Role);
                const bool bCaster = Role == TEXT("Caster");
                if (!InterruptScenarioAt) InterruptScenarioAt = Now();
                if (InterruptScenarioStep == 0 && Now() - InterruptScenarioAt > 2.0)
                { ServerSetInterruptScenarioPosition(bCaster, false); InterruptScenarioStep = 1; InterruptScenarioAt = Now(); }
                else if (bCaster)
                {
                    if (InterruptScenarioStep == 1 && Now() - InterruptScenarioAt > 1.0)
                    {
                        for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
                        {
                            if (*It != Owner && FMath::Abs((*It)->GetActorLocation().Y - 600.0) < 50.0)
                            { UseTrainingAbility(1); InterruptScenarioStep = 2; break; }
                        }
                    }
                    else if (InterruptScenarioStep == 2 && View.bInterrupted)
                    {
                        UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_INTERRUPTED Resource=%d"), Resource);
                        UseTrainingAbility(1); InterruptScenarioStep = 3; InterruptScenarioAt = Now();
                    }
                    else if (InterruptScenarioStep == 3 && Now() - InterruptScenarioAt > 3.3)
                    { UseTrainingAbility(1); InterruptScenarioStep = 4; }
                    else if (InterruptScenarioStep == 4 && View.Phase == ESFCastPhase::Casting)
                    { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_RECAST")); InterruptScenarioStep = 5; }
                    else if (InterruptScenarioStep == 5 && View.Phase == ESFCastPhase::Idle && !View.bInterrupted)
                    { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_COMPLETE Resource=%d"), Resource); InterruptScenarioStep = 6; }
                }
                else
                {
                    ASFE1Character* Caster = nullptr;
                    for (TActorIterator<ASFE1Character> It(GetWorld()); It; ++It)
                    { if (*It != Owner && (*It)->Casting) { Caster = *It; break; } }
                    if (Caster && InterruptScenarioStep == 1 && Caster->Casting->GetCastView().Phase == ESFCastPhase::Casting)
                    { RequestTrainingInterrupt(); UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_RANGE_REQUEST")); InterruptScenarioStep = 2; InterruptScenarioAt = Now(); }
                    else if (InterruptScenarioStep == 2 && Now() - InterruptScenarioAt > 0.15)
                    { ServerSetInterruptScenarioPosition(false, true); InterruptScenarioStep = 3; InterruptScenarioAt = Now(); }
                    else if (InterruptScenarioStep == 3 && Now() - InterruptScenarioAt > 0.15)
                    { RequestTrainingInterrupt(); UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_NEAR_REQUEST")); InterruptScenarioStep = 4; }
                    else if (Caster && InterruptScenarioStep == 4 && Caster->Casting->GetCastView().bInterrupted)
                    { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_OBSERVED_CANCEL")); InterruptScenarioStep = 5; }
                    else if (Caster && InterruptScenarioStep == 5 && Caster->Casting->GetCastView().Phase == ESFCastPhase::Casting)
                    { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_OBSERVED_RECAST")); InterruptScenarioStep = 6; }
                    else if (Caster && InterruptScenarioStep == 6 && Caster->Casting->GetCastView().Phase == ESFCastPhase::Idle && !Caster->Casting->GetCastView().bInterrupted)
                    { UE_LOG(LogTemp, Display, TEXT("INTERRUPT_CLIENT_OBSERVED_COMPLETE")); InterruptScenarioStep = 7; }
                }
            }
            else
            if (FParse::Param(FCommandLine::Get(), TEXT("CastingScenario")))
            {
                if (!ScenarioStartedAt) ScenarioStartedAt = Now();
                if (ScenarioStep == 0 && Now() - ScenarioStartedAt > 3.0)
                { UseTrainingAbility(1); ScenarioCastRequestedAt = Now(); ScenarioStep = 1; }
                else if (ScenarioStep == 1)
                {
                    if (View.Phase == ESFCastPhase::Casting && !bScenarioSawCastView)
                    { bScenarioSawCastView = true; UE_LOG(LogTemp, Display, TEXT("CASTING_CLIENT_CAST_VIEW Progress=%f"), GetCastProgress()); }
                    if (Now() - ScenarioCastRequestedAt >= 1.18)
                    { UseTrainingAbility(2); ScenarioStep = 2; }
                }
                else if (ScenarioStep == 2 && View.Phase == ESFCastPhase::Channeling)
                { UE_LOG(LogTemp, Display, TEXT("CASTING_CLIENT_CHANNEL_STARTED")); ScenarioStep = 3; }
                else if (ScenarioStep == 3 && View.Phase == ESFCastPhase::Idle)
                {
                    for (TActorIterator<ASFCastingTarget> It(GetWorld()); It; ++It)
                    {
                        if (bScenarioSawCastView && It->GetTrainingHealth() <= 475)
                        { UE_LOG(LogTemp, Display, TEXT("CASTING_SCENARIO_PASS Health=%d Resource=%d"), It->GetTrainingHealth(), Resource); ScenarioStep = 4; break; }
                    }
                }
            }
            else
#endif
            if (APlayerController* PC = Cast<APlayerController>(Owner->GetController()))
            {
                if (PC->WasInputKeyJustPressed(EKeys::One)) UseTrainingAbility(1);
                if (PC->WasInputKeyJustPressed(EKeys::Two)) UseTrainingAbility(2);
                if (PC->WasInputKeyJustPressed(EKeys::Three)) CancelCast();
                if (GetWorld()->GetMapName().Contains(TEXT("SB_Interrupt")) && PC->WasInputKeyJustPressed(EKeys::I))
                { RequestTrainingInterrupt(); }
            }
        }
        return;
    }
    const double Time = Now();
    if (State.Phase != SFCastRules::EPhase::Idle)
    {
        if (Owner->CombatHealth() <= 0 || (Owner->CrowdControl && Owner->CrowdControl->IsStunned()) ||
            SFCastRules::MovedBeyond(FVector::Dist(StartLocation, Owner->GetActorLocation()),
                Tune.MoveToleranceCm, Definition(State.ActiveId).bCastWhileMoving))
        {
            Resource = FMath::Min(ResourceMax, Resource + SFCastRules::Refund(ResourceCost, true, CancelRefundPercent));
            View.bInterrupted = true;
            State.Cancel(); State.QueuedId = 0; SyncView();
            UE_LOG(LogTemp, Display, TEXT("CASTING_SERVER_CANCEL Cause=MovementOrControl"));
        }
        else
        {
            const uint8 Id = State.ActiveId;
            const bool bCast = State.Phase == SFCastRules::EPhase::Casting;
            bool bCompleted = false;
            const int32 Due = State.Advance(Time, Definition(Id), bCompleted);
            for (int32 I = 0; I < Due; ++I) ResolveTick(Id);
            if (bCast && bCompleted) ResolveTick(Id);
            if (Due || bCompleted)
            {
                if (bCompleted) View.bInterrupted = false;
                SyncView();
                if (bCompleted) UE_LOG(LogTemp, Display, TEXT("CASTING_SERVER_COMPLETE Ability=%d"), Id);
            }
        }
    }
    if (State.Phase == SFCastRules::EPhase::Idle && State.QueuedId && Time >= State.GcdEndsAt)
    {
        const uint8 Queued = State.QueuedId;
        State.QueuedId = 0;
        TryUse(Queued);
    }
}

void USFCastingComponent::ResolveTick(uint8 AbilityId)
{
    ASFE1Character* Owner = Cast<ASFE1Character>(GetOwner());
    if (Owner && Target.IsValid() && Target->GetTrainingHealth() > 0 &&
        FVector::Dist(Owner->GetActorLocation(), Target->GetActorLocation()) <= RangeMetres * 100.0)
    {
        ASFGameMode* Mode = GetWorld()->GetAuthGameMode<ASFGameMode>();
        if (!Mode) { return; }
        SFCombatResolution::FAttack Attack;
        Attack.RawDamage = DamagePerTick;
        Attack.bSpell = true;
        Attack.bPhysical = false;
        const auto Result = Mode->ResolveCombatAttack(Owner, Attack);
        if (Result.Damage > 0) { Target->ReceiveTrainingDamage(Result.Damage); }
        UE_LOG(LogTemp, Display, TEXT("CASTING_SERVER_TICK Ability=%d Health=%d"), AbilityId, Target->GetTrainingHealth());
    }
}

void USFCastingComponent::CancelCast()
{
    if (GetOwner()->HasAuthority()) ServerCancelCast_Implementation();
    else ServerCancelCast();
}

void USFCastingComponent::ServerCancelCast_Implementation()
{
    if (State.Phase == SFCastRules::EPhase::Idle) return;
    Resource = FMath::Min(ResourceMax, Resource + SFCastRules::Refund(ResourceCost, false, CancelRefundPercent));
    View.bInterrupted = true;
    State.Cancel(); State.QueuedId = 0; SyncView();
}

void USFCastingComponent::Interrupt()
{
    if (!GetOwner()->HasAuthority() || State.Phase == SFCastRules::EPhase::Idle) return;
    const bool bNpc = Cast<ASFCombatNpc>(GetOwner()) != nullptr;
    if (bNpc) { NpcMana = FMath::Min(NpcManaMaximum, NpcMana + NpcCastCost()); }
    else { Resource = FMath::Min(ResourceMax, Resource + SFCastRules::Refund(ResourceCost, true, CancelRefundPercent)); }
    State.Interrupt(Now(), bNpc ? NpcSchoolLockoutSeconds : LockoutSeconds, bNpc ? 2 : 1);
    View.bInterrupted = true;
    SyncView();
}

int32 USFCastingComponent::NpcCastCost() const
{
    const auto* Mend = SFStarterAbilities::Find(TEXT("mend"));
    return Mend ? FMath::RoundToInt(Mend->StaminaCostFraction * NpcManaMaximum) : 0;
}

bool USFCastingComponent::TryBeginNpcMend()
{
    ASFCombatNpc* Npc = Cast<ASFCombatNpc>(GetOwner());
    const auto* Mend = SFStarterAbilities::Find(TEXT("mend"));
    if (!Npc || !Npc->HasAuthority() || !Mend || !Npc->CanCastMend() ||
        Npc->Health() <= 0 || Npc->IsLeashing() || NpcMana < NpcCastCost()) return false;
    SFCastRules::FAbility Ability;
    Ability.Id = 3; Ability.School = 2; Ability.Cast = Mend->CastSeconds;
    Ability.Cooldown = NpcMendCooldownSeconds;
    if (State.Request(Ability, Now(), 0.0, Tune) != SFCastRules::EStart::Started) return false;
    NpcMana -= NpcCastCost();
    bCurrentCastInterruptible = true;
    View.bInterrupted = false;
    SyncView();
    UE_LOG(LogTemp, Display, TEXT("NPC_CAST_START Creature=%s Ability=mend Mana=%d"), *Npc->GetCreatureId(), NpcMana);
    return true;
}

void USFCastingComponent::CancelNpcCast()
{
    if (!Cast<ASFCombatNpc>(GetOwner()) || !GetOwner()->HasAuthority() ||
        State.Phase == SFCastRules::EPhase::Idle) return;
    State.Cancel(); State.QueuedId = 0; View.bInterrupted = true; SyncView();
}

bool USFCastingComponent::IsNpcSchoolLocked() const
{
    const double* Until = State.SchoolLockEnds.Find(2);
    return Until && Now() < *Until;
}

void USFCastingComponent::NotifyDamage()
{
    if (!GetOwner()->HasAuthority() || State.Phase == SFCastRules::EPhase::Idle) return;
    State.Damage(Now(), Tune); SyncView();
}

void USFCastingComponent::SyncView()
{
    View.Phase = static_cast<ESFCastPhase>(State.Phase);
    if (State.Phase != SFCastRules::EPhase::Idle || !View.bInterrupted) View.AbilityId = State.ActiveId;
    View.StartedAt = State.StartedAt;
    View.EndsAt = State.EndsAt;
    View.GcdEndsAt = State.GcdEndsAt;
    View.TicksDone = State.TicksDone;
    GetOwner()->ForceNetUpdate();
}

float USFCastingComponent::GetCastProgress() const
{
    if (View.Phase == ESFCastPhase::Idle) return 0.f;
    return FMath::Clamp(float((Now() - View.StartedAt) / FMath::Max(0.001f, View.EndsAt - View.StartedAt)), 0.f, 1.f);
}

void USFCastingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USFCastingComponent, View);
    DOREPLIFETIME_CONDITION(USFCastingComponent, Resource, COND_OwnerOnly);
}

ASFCastingTarget::ASFCastingTarget()
{
    bReplicates = true;
    Health = GetDefault<USFCastingSettings>()->TrainingTargetHealth;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    RootComponent = Body;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded()) Body->SetStaticMesh(Cube.Object);
    Body->SetWorldScale3D(FVector(1.0, 1.0, 2.0));
    Body->SetCollisionProfileName(TEXT("BlockAll"));
}

void ASFCastingTarget::ReceiveTrainingDamage(int32 Damage)
{
    if (!HasAuthority() || Damage <= 0) return;
    Health = FMath::Max(0, Health - Damage);
    ForceNetUpdate();
}

void ASFCastingTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASFCastingTarget, Health);
}
