#pragma once

#include "CoreMinimal.h"

// Deterministic per-member support-loss deadlines. Callers supply authoritative
// world time; this helper owns no world, actor, or replication state.
class SHATTEREDFRONTIER_API FSFConstructionInstability
{
public:
    static constexpr int32 MaximumTrackedMembers = 4096;

    // Reconciles unsupported members. Existing episodes retain their deadline;
    // recovered members are removed and newly unsupported members start now.
    // Invalid input leaves both deadlines and clock state unchanged.
    bool Refresh(const TSet<FIntVector>& Unsupported, double Now, double GraceSeconds);

    // Lists overdue members in lexicographic coordinate order. Does not consume
    // deadlines, so a failed caller-side staging operation can retry the wave.
    // Successful reads advance only the shared observed-clock watermark.
    bool Due(double Now, TArray<FIntVector>& Out);

    bool GetDeadline(const FIntVector& Cell, double& OutDeadline) const;
    int32 Num() const { return Deadlines.Num(); }
    bool GetNextDeadline(double& OutDeadline) const;

private:
    TMap<FIntVector, double> Deadlines;
    double LastObservedTime = 0.0;
    bool bHasObservedTime = false;
};
