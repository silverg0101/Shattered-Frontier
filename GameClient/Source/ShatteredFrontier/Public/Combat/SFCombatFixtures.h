#pragma once
#include "CoreMinimal.h"

/** Explicit authority-only development policy; group IDs never activate fixture behavior. */
enum class ESFNpcFixtureFlags : uint8 { None = 0, HoldIdle = 1 << 0, TraceAggro = 1 << 1 };
ENUM_CLASS_FLAGS(ESFNpcFixtureFlags)

namespace SFCombatFixtures
{
    enum class EKind : uint8 { Standard, EmptyAllowed, Respawn, SpiritRelease, PersonalLoot };
    struct FSpec
    {
        const TCHAR* MapSuffix;
        const TCHAR* WildlifeTag;
        const TCHAR* GroupId;
        double ExtentMetres;
        bool bDefensive;
        bool bAssistDefensiveTag;
        ESFNpcFixtureFlags NpcFlags;
        EKind Kind;
    };
    /** Exact named sandbox admission only. Approved production maps never use this table. */
    SHATTEREDFRONTIER_API const FSpec* Find(const FString& MapName);
}
