#include "Combat/SFCastingComponent.h"

#include "Characters/SFE1Character.h"
#include "Combat/SFCrowdControlComponent.h"
#include "Combat/SFCombatPilot.h"
#include "Combat/SFPlayerInterruptRules.h"
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
        !FMath::IsWithinInclusive(TrainingInterruptLockoutSeconds, 0.f, 10.f))
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
        Target->ReceiveTrainingDamage(DamagePerTick);
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
    Resource = FMath::Min(ResourceMax, Resource + SFCastRules::Refund(ResourceCost, true, CancelRefundPercent));
    State.Interrupt(Now(), LockoutSeconds, 1);
    View.bInterrupted = true;
    SyncView();
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
