#include "Combat/SFCastRules.h"

namespace SFCastRules
{
EStart FState::Request(const FAbility& Ability, double Now, double Haste, const FTuning& Tune)
{
    if (Now < SchoolLockEnds.FindRef(Ability.School)) return EStart::Locked;
    const double ReadyAt = FMath::Max(Phase == EPhase::Idle ? Now : EndsAt, Ability.Gcd == EGcd::None ? Now : GcdEndsAt);
    if (Now < CooldownEnds.FindRef(Ability.Id) && CooldownEnds.FindRef(Ability.Id) > ReadyAt) return EStart::Cooldown;
    if (Now < ReadyAt)
    {
        const bool bNearCastEnd = Phase != EPhase::Idle && Now <= EndsAt && EndsAt - Now <= Tune.QueueWindow;
        const bool bNearGcdEnd = Ability.Gcd != EGcd::None && Now <= GcdEndsAt && GcdEndsAt - Now <= Tune.QueueWindow;
        if (!QueuedId && (bNearCastEnd || bNearGcdEnd)) { QueuedId = Ability.Id; return EStart::Queued; }
        return EStart::Busy;
    }
    const double H = FMath::Clamp(Haste, 0.0, Tune.HasteMax);
    ActiveId = Ability.Id;
    StartedAt = Now;
    Pushbacks = 0;
    TicksDone = 0;
    Phase = Ability.Cast > 0 ? EPhase::Casting : (Ability.Channel > 0 ? EPhase::Channeling : EPhase::Idle);
    EndsAt = Now + (Ability.Cast > 0 ? Ability.Cast : Ability.Channel) * (1.0 - H);
    TickInterval = Ability.Ticks > 0 ? (EndsAt - Now) / Ability.Ticks : 0.0;
    NextTickAt = Ability.Ticks > 0 ? Now + TickInterval : EndsAt;
    const double Base = Ability.Gcd == EGcd::Standard ? Tune.GcdBase : Tune.GcdShort;
    if (Ability.Gcd != EGcd::None) GcdEndsAt = Now + FMath::Max(Tune.GcdMin, Base * (1.0 - H));
    CooldownEnds.Add(Ability.Id, Now + Ability.Cooldown * (Ability.bHasteCooldown && Ability.Cooldown <= 30.0 ? 1.0 - H : 1.0));
    return EStart::Started;
}

int32 FState::Advance(double Now, const FAbility& Ability, bool& bCompleted)
{
    bCompleted = false;
    if (Phase == EPhase::Idle) return 0;
    if (Phase == EPhase::Casting)
    {
        if (Now >= EndsAt) { bCompleted = true; Cancel(); }
        return 0;
    }
    int32 Due = 0;
    while (Ability.Ticks > 0 && TicksDone < Ability.Ticks && Now >= NextTickAt)
    {
        ++Due; ++TicksDone; NextTickAt += TickInterval;
    }
    // A shortened channel ends immediately after its new deadline; no final catch-up burst.
    if (Now >= EndsAt || TicksDone >= Ability.Ticks) { bCompleted = true; Cancel(); }
    return Due;
}

void FState::Damage(double Now, const FTuning& Tune)
{
    if (Phase == EPhase::Casting && Pushbacks < Tune.PushbackMax) { EndsAt += Tune.Pushback; ++Pushbacks; }
    else if (Phase == EPhase::Channeling) { EndsAt = FMath::Max(Now, EndsAt - Tune.Pushback); }
}

void FState::Interrupt(double Now, double LockoutSeconds, uint8 School)
{
    if (Phase == EPhase::Idle) return;
    Cancel();
    QueuedId = 0;
    SchoolLockEnds.Add(School, Now + FMath::Max(0.0, LockoutSeconds));
}

double FState::Progress(double Now) const
{
    if (Phase == EPhase::Idle) return 0.0;
    return FMath::Clamp((Now - StartedAt) / FMath::Max(EndsAt - StartedAt, 0.001), 0.0, 1.0);
}
}
