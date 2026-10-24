#pragma once

#include "CoreMinimal.h"

/** UObject-free server clock rules. Seconds are world server time; client timestamps never enter this state. */
namespace SFCastRules
{
inline bool MovedBeyond(double DistanceCm, double ToleranceCm, bool bCastWhileMoving)
{ return !bCastWhileMoving && DistanceCm > ToleranceCm; }
inline int32 Refund(int32 Cost, bool bInterrupted, int32 CancelPercent)
{ return bInterrupted ? Cost : FMath::RoundToInt(Cost * FMath::Clamp(CancelPercent, 0, 100) / 100.0); }
enum class EPhase : uint8 { Idle, Casting, Channeling };
enum class EStart : uint8 { Started, Queued, Busy, Cooldown, Locked };
enum class EGcd : uint8 { None, Short, Standard };

struct FTuning
{
    double GcdBase = 1.5, GcdShort = 1.0, GcdMin = 0.75;
    double HasteMax = 0.75, MoveToleranceCm = 25.0, Pushback = 0.25;
    int32 PushbackMax = 2;
    double QueueWindow = 0.4;
};

struct FAbility
{
    uint8 Id = 0;
    double Cast = 0, Channel = 0, Cooldown = 0;
    int32 Ticks = 0;
    EGcd Gcd = EGcd::Standard;
    bool bCastWhileMoving = false, bHasteCooldown = false;
    uint8 School = 0;
};

struct FState
{
    EPhase Phase = EPhase::Idle;
    uint8 ActiveId = 0, QueuedId = 0;
    double StartedAt = 0, EndsAt = 0, GcdEndsAt = 0, NextTickAt = 0, TickInterval = 0;
    int32 TicksDone = 0, Pushbacks = 0;
    TMap<uint8, double> CooldownEnds;
    TMap<uint8, double> SchoolLockEnds;

    EStart Request(const FAbility& Ability, double Now, double Haste, const FTuning& Tune);
    /** Returns number of due channel ticks; cast completion is reported by bCompleted. */
    int32 Advance(double Now, const FAbility& Ability, bool& bCompleted);
    void Damage(double Now, const FTuning& Tune);
    void Cancel() { Phase = EPhase::Idle; ActiveId = 0; TicksDone = 0; }
    void Interrupt(double Now, double LockoutSeconds, uint8 School);
    double Progress(double Now) const;
};
}
