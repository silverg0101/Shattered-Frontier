#pragma once

#include "CoreMinimal.h"

enum class ESFE1LocalInteractionState : uint8 { Ready, Pending, Cooldown, Coalescing };

// Presentation/input state only. The server independently decides every outcome.
class FSFE1InteractionFeedback
{
public:
    ESFE1LocalInteractionState GetState(double LocalNow) const
    {
        if (bPending) { return ESFE1LocalInteractionState::Pending; }
        if (LocalNow < CooldownUntil) { return ESFE1LocalInteractionState::Cooldown; }
        if (!FMath::IsFinite(LocalNow) || LocalNow < NextInput) { return ESFE1LocalInteractionState::Coalescing; }
        return ESFE1LocalInteractionState::Ready;
    }

    bool TryBeginRequest(double LocalNow)
    {
        if (GetState(LocalNow) != ESFE1LocalInteractionState::Ready) { return false; }
        bPending = true;
        // Fixed input event coalescing, not the configurable accepted-action cooldown.
        NextInput = LocalNow + 0.1;
        return true;
    }

    void ReceiveServerResult(double LocalNow, float ServerCooldownRemainingSeconds)
    {
        bPending = false;
        // Countdown starts at delivery, conservatively accounting for response travel time.
        // It is advisory; no client timestamp or duration is sent back as authority.
        CooldownUntil = LocalNow + FMath::Max(0.0f, ServerCooldownRemainingSeconds);
    }

private:
    bool bPending = false;
    double NextInput = 0.0;
    double CooldownUntil = 0.0;
};
