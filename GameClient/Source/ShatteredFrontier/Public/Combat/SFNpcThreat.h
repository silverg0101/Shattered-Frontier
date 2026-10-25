#pragma once

#include "CoreMinimal.h"

// Shared threat/memory algorithms. Names retained for source compatibility.
class SHATTEREDFRONTIER_API FSFBoarThreatTable
{
public:
    void Add(const FString& CharacterId, double Amount, int32 MaxEntries);
    void Remove(const FString& CharacterId);
    void Clear() { Entries.Empty(); }
    bool Contains(const FString& CharacterId) const;
    double Highest() const;
    double Get(const FString& CharacterId) const;
    void RaiseTo(const FString& CharacterId, double Amount, int32 MaxEntries);
    FString Select(const FString& CurrentId, const TMap<FString, double>& ReachableDistancesCm,
        double MeleeThreshold, double RangedThreshold) const;
private:
    struct FEntry { FString Id; double Threat = 0; };
    TArray<FEntry> Entries;
};

/** AI NPC Behavior §9.1: damage attackers stay in one brain for 60 s after reset. */
class SHATTEREDFRONTIER_API FSFRecentAttackerMemory
{
public:
    static constexpr double RetentionSeconds = 60.0;
    void RecordReset(const TSet<FString>& Attackers, double Now);
    bool Contains(const FString& CharacterId, double Now) const;
private:
    TMap<FString, double> ExpiresAt;
};
