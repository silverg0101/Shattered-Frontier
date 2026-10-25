#pragma once

/** Sandbox unarmed swing timing from Combat's 2.0 s unarmed weapon speed. */
namespace SFAutoAttackRules
{
    constexpr double UnarmedSwingSeconds = 2.0;

    inline bool CanSwing(bool bEnabled, bool bHasLiveTarget, bool bAlive, bool bCasting, double Now, double NextSwingAt)
    {
        return bEnabled && bHasLiveTarget && bAlive && !bCasting && Now >= NextSwingAt;
    }
}
