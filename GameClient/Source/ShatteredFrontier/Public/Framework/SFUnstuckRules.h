#pragma once

#include "CoreMinimal.h"

// World Cells, Movement and Handoffs § Teleport tokens / Unstuck.
namespace SFUnstuckRules
{
inline bool CanRequest(bool bAlive, bool bInCombat, bool bStunned, bool bInInstance,
    double Now, double LastAccepted, int32 CooldownSeconds)
{
    return bAlive && !bInCombat && !bStunned && !bInInstance &&
        (LastAccepted < 0.0 || Now - LastAccepted >= CooldownSeconds);
}

inline bool IsNearbyDestination(const FVector& From, const FVector& To, float MinimumMetres, float MaximumMetres)
{
    const float Distance = FVector::Dist2D(From, To);
    return Distance >= MinimumMetres * 100.0f && Distance <= MaximumMetres * 100.0f;
}

// Evaluate every projected nav candidate. A ring's first hit can be farther than a hit in a later ring.
inline void ConsiderNavigable(const FVector& From, const FVector& Candidate, float MinimumMetres,
    float MaximumMetres, FVector& Best, bool& bFound)
{
    if (!IsNearbyDestination(From, Candidate, MinimumMetres, MaximumMetres)) return;
    const double Distance = FVector::DistSquared2D(From, Candidate);
    if (!bFound || Distance < FVector::DistSquared2D(From, Best) ||
        (Distance == FVector::DistSquared2D(From, Best) &&
            (Candidate.X < Best.X || (Candidate.X == Best.X && Candidate.Y < Best.Y))))
    { Best = Candidate; bFound = true; }
}

// Recast polygons are convex. The closest permitted point is either the closest point on a
// polygon outside the exclusion disc, or its intersection with the disc's 2 m boundary.
inline bool ClosestPermittedOnPolygon(const FVector2D& From, const TArray<FVector>& Vertices,
    float MinimumCm, float MaximumCm, FVector2D& Out)
{
    if (Vertices.Num() < 3) return false;
    bool bPositive = false, bNegative = false;
    FVector2D Nearest; double BestSquared = TNumericLimits<double>::Max();
    for (int32 I = 0; I < Vertices.Num(); ++I)
    {
        const FVector2D A(Vertices[I].X, Vertices[I].Y);
        const FVector2D B(Vertices[(I + 1) % Vertices.Num()].X, Vertices[(I + 1) % Vertices.Num()].Y);
        const FVector2D Edge = B - A;
        const double Cross = Edge.X * (From.Y - A.Y) - Edge.Y * (From.X - A.X);
        bPositive |= Cross > 0.01; bNegative |= Cross < -0.01;
        const double T = Edge.SizeSquared() > 0 ? FMath::Clamp(FVector2D::DotProduct(From - A, Edge) / Edge.SizeSquared(), 0.0, 1.0) : 0.0;
        const FVector2D OnEdge = A + Edge * T;
        const double Squared = (OnEdge - From).SizeSquared();
        if (Squared < BestSquared) { BestSquared = Squared; Nearest = OnEdge; }
    }
    const FVector2D Inner = bPositive && bNegative ? Nearest : From;
    const double InnerSquared = (Inner - From).SizeSquared();
    if (InnerSquared > FMath::Square(static_cast<double>(MaximumCm))) return false;
    if (InnerSquared >= FMath::Square(static_cast<double>(MinimumCm)))
    { Out = Inner; return true; }
    for (const FVector& Vertex : Vertices)
    {
        const FVector2D End(Vertex.X, Vertex.Y);
        if ((End - From).SizeSquared() < FMath::Square(static_cast<double>(MinimumCm))) continue;
        const FVector2D Ray = End - Inner;
        const double A = Ray.SizeSquared();
        const double B = 2.0 * FVector2D::DotProduct(Inner - From, Ray);
        const double C = InnerSquared - FMath::Square(static_cast<double>(MinimumCm));
        const double T = (-B + FMath::Sqrt(B * B - 4.0 * A * C)) / (2.0 * A);
        Out = Inner + Ray * T;
        return true;
    }
    return false;
}

// In-zone token stays on the issuing server. A client only sees a displacement notice sequence.
struct FToken
{
    FString Id, CharacterId;
    uint32 Sequence = 0;
    FVector Origin = FVector::ZeroVector, Destination = FVector::ZeroVector;
    int64 ExpiresAtUnixMs = 0;
    float DestinationToleranceCm = 150.0f;
    bool bOutstanding = false;

    bool Consume(uint32 NoticeSequence, const FString& RequestCharacterId, const FVector& ServerPosition, int64 NowUnixMs)
    {
        if (!bOutstanding || RequestCharacterId != CharacterId || NoticeSequence != Sequence || NowUnixMs > ExpiresAtUnixMs ||
            FVector::Dist2D(ServerPosition, Destination) > DestinationToleranceCm ||
            FVector::Dist2D(ServerPosition, Origin) > FVector::Dist2D(Origin, Destination) + DestinationToleranceCm)
            return false;
        bOutstanding = false;
        return true;
    }
};
}
