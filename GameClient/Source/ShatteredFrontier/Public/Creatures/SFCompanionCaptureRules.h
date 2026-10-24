#pragma once

#include "CoreMinimal.h"

// Companion Gameplay Details §§2-3, §11. Value-only rules; the server supplies world evidence.
enum class ESFCompanionCaptureReject : uint8
{
    None, InvalidSettings, NoAuthority, MissingContent, IneligiblePopulation, OwnedOrReserved, NoStableSlot,
    LevelOrHunting, TooHealthy, MountedOrPetsActive, ForeignEngagement, OutOfRangeOrSight,
    NoSafeOpening, Moved, OwnerDefeated, OwnerOrPetDamaged, TargetDamagedOrDead, ChannelIncomplete
};

struct FSFCompanionCaptureSettings
{
    int32 StableBaseSlots = 2;
    int32 HealthThresholdBp = 3000;
    int32 ChannelMs = 8000;
    double RangeMetres = 5.0;
    int32 TrainingStepMs = 10000;

    bool Valid() const
    {
        return StableBaseSlots >= 2 && StableBaseSlots <= 6 &&
            HealthThresholdBp >= 1000 && HealthThresholdBp <= 4000 && HealthThresholdBp % 100 == 0 &&
            ChannelMs >= 5000 && ChannelMs <= 15000 && ChannelMs % 1000 == 0 &&
            RangeMetres >= 3.0 && RangeMetres <= 5.0 &&
            FMath::IsNearlyZero(FMath::Fmod(RangeMetres * 2.0, 1.0)) &&
            TrainingStepMs >= 5000 && TrainingStepMs <= 30000 && TrainingStepMs % 1000 == 0;
    }
};

struct FSFCompanionCaptureEvidence
{
    // Supplied by the authority's versioned species/trait/kit validator, never by the client.
    bool bAuthority = false, bAuthoredContentCompatible = false, bEligiblePopulation = false;
    bool bUnownedUnreserved = false, bFreeStableSlot = false;
    int32 OwnerLevel = 0, CreatureLevel = 0, Hunting = 0, Difficulty = 0;
    int32 Health = 0, MaxHealth = 0;
    bool bMounted = false, bPetsPassiveRecalled = false, bForeignEngagement = false;
    bool bSafeOpeningActive = false, bStationary = false, bLineOfSight = false, bOwnerDefeated = false;
    bool bOwnerOrPetDamaged = false, bTargetDamagedOrDead = false;
    double RangeMetres = 0;
    int32 ElapsedMs = 0;
};

namespace SFCompanionCaptureRules
{
    inline ESFCompanionCaptureReject Check(const FSFCompanionCaptureSettings& S,
        const FSFCompanionCaptureEvidence& E, bool bCommit)
    {
        if (!S.Valid()) return ESFCompanionCaptureReject::InvalidSettings;
        if (!E.bAuthority) return ESFCompanionCaptureReject::NoAuthority;
        if (!E.bAuthoredContentCompatible) return ESFCompanionCaptureReject::MissingContent;
        if (!E.bEligiblePopulation) return ESFCompanionCaptureReject::IneligiblePopulation;
        if (!E.bUnownedUnreserved) return ESFCompanionCaptureReject::OwnedOrReserved;
        if (!E.bFreeStableSlot) return ESFCompanionCaptureReject::NoStableSlot;
        if (E.CreatureLevel < 1 || E.OwnerLevel < E.CreatureLevel || E.Difficulty < 0 || E.Difficulty > 1000 ||
            E.Hunting < FMath::Max(0, E.Difficulty - 100)) return ESFCompanionCaptureReject::LevelOrHunting;
        if (E.MaxHealth <= 0 || E.Health <= 0 || int64(E.Health) * 10000 > int64(E.MaxHealth) * S.HealthThresholdBp)
            return ESFCompanionCaptureReject::TooHealthy;
        if (E.bMounted || !E.bPetsPassiveRecalled) return ESFCompanionCaptureReject::MountedOrPetsActive;
        if (E.bForeignEngagement) return ESFCompanionCaptureReject::ForeignEngagement;
        if (!E.bLineOfSight || !FMath::IsFinite(E.RangeMetres) ||
            E.RangeMetres > S.RangeMetres || E.RangeMetres < 0)
            return ESFCompanionCaptureReject::OutOfRangeOrSight;
        if (!E.bSafeOpeningActive) return ESFCompanionCaptureReject::NoSafeOpening;
        if (!E.bStationary) return ESFCompanionCaptureReject::Moved;
        if (E.bOwnerDefeated) return ESFCompanionCaptureReject::OwnerDefeated;
        if (E.bOwnerOrPetDamaged) return ESFCompanionCaptureReject::OwnerOrPetDamaged;
        if (E.bTargetDamagedOrDead) return ESFCompanionCaptureReject::TargetDamagedOrDead;
        if (bCommit && E.ElapsedMs < S.ChannelMs) return ESFCompanionCaptureReject::ChannelIncomplete;
        return ESFCompanionCaptureReject::None;
    }
}

enum class ESFCompanionLessonReject : uint8 { None, NotAtRest, InCombat, WrongLesson, ObjectiveIncomplete, Interrupted, TooShort };

struct FSFCompanionLessonEvidence
{
    bool bAtRestArea = false, bInCombat = false, bObjectiveComplete = false, bInterrupted = false;
    int32 Lesson = 0, CompletedLessons = 0, ActiveMs = 0;
};

namespace SFCompanionTrainingRules
{
    inline ESFCompanionLessonReject Check(const FSFCompanionCaptureSettings& S, const FSFCompanionLessonEvidence& E)
    {
        if (!E.bAtRestArea) return ESFCompanionLessonReject::NotAtRest;
        if (E.bInCombat) return ESFCompanionLessonReject::InCombat;
        if (E.Lesson != E.CompletedLessons || E.Lesson < 0 || E.Lesson >= 3)
            return ESFCompanionLessonReject::WrongLesson;
        if (!E.bObjectiveComplete) return ESFCompanionLessonReject::ObjectiveIncomplete;
        if (E.bInterrupted) return ESFCompanionLessonReject::Interrupted;
        if (!S.Valid() || E.ActiveMs < S.TrainingStepMs) return ESFCompanionLessonReject::TooShort;
        return ESFCompanionLessonReject::None;
    }
}
