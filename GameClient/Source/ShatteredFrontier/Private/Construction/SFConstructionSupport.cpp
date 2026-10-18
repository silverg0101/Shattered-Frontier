#include "Construction/SFConstructionSupport.h"
#include "Construction/SFConstructionGeometry.h"

namespace
{
constexpr int32 MaxSupportMembers = 4096;
constexpr double NumericalGuard = 1.e-12; // Conservative equality guard for binary geometry arithmetic.
struct FPoly2 { TArray<FVector2D> P; };
struct FNode
{
    FSFConstructionSupportMember Input;
    FSFConstructionSolid Solid;
    TArray<FPoly2> Sides[6];
    double BottomArea = 0.0, Fraction = 0.0, Load = 0.0;
    int32 Distance = INDEX_NONE;
    bool bAnchored = false, bFailed = false;
    TArray<int32> Parents;
    TArray<double> ParentAreas;
    ESFConstructionSupportReason Reason = ESFConstructionSupportReason::None;
    int32 LimitingParent = INDEX_NONE;
    double LoadRatio = 0.0, ReachRatio = 0.0;
};

bool LessAddress(const FIntVector& A, const FIntVector& B)
{ return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z; }
int32 SideIndex(int32 Axis, bool bPositive) { return Axis * 2 + (bPositive ? 1 : 0); }
FVector2D Project(const FVector& V, int32 Axis)
{ return Axis == 0 ? FVector2D(V.Y,V.Z) : Axis == 1 ? FVector2D(V.X,V.Z) : FVector2D(V.X,V.Y); }
double Cross2(const FVector2D& A, const FVector2D& B, const FVector2D& C)
{ return (B.X-A.X)*(C.Y-A.Y) - (B.Y-A.Y)*(C.X-A.X); }
double SignedArea(const FPoly2& P)
{
    double A=0; for (int32 I=0; I<P.P.Num(); ++I) { const auto& X=P.P[I]; const auto& Y=P.P[(I+1)%P.P.Num()]; A += X.X*Y.Y-X.Y*Y.X; }
    return A*0.5;
}
FPoly2 Intersect(FPoly2 Subject, const FPoly2& Clip)
{
    const double Sign = SignedArea(Clip) >= 0 ? 1.0 : -1.0;
    for (int32 E=0; E<Clip.P.Num() && Subject.P.Num(); ++E)
    {
        const FVector2D A=Clip.P[E], B=Clip.P[(E+1)%Clip.P.Num()];
        FPoly2 Next; FVector2D Prev=Subject.P.Last(); double DP=Cross2(A,B,Prev)*Sign;
        for (const FVector2D& Cur : Subject.P)
        {
            const double DC=Cross2(A,B,Cur)*Sign; const bool IP=DP >= -NumericalGuard, IC=DC >= -NumericalGuard;
            if (IP != IC) { const double T=DP/(DP-DC); Next.P.Add(Prev+(Cur-Prev)*T); }
            if (IC) Next.P.Add(Cur); Prev=Cur; DP=DC;
        }
        Subject=MoveTemp(Next);
    }
    return Subject;
}
void MakeSides(FNode& N)
{
    for (const auto& Face : N.Solid.Faces)
    {
        for (int32 Side=0; Side<6; ++Side)
        {
            const int32 Axis=Side/2; const double Plane=(Side%2) ? SFConstructionGeometry::CellCentimetres : 0.0;
            bool bBoundary=Face.Num()>=3; FPoly2 Poly;
            for (int32 VI : Face) { const FVector& V=N.Solid.Vertices[VI]; bBoundary &= V[Axis] == Plane; Poly.P.Add(Project(V,Axis)); }
            if (bBoundary) { const double Area=FMath::Abs(SignedArea(Poly)); N.Sides[Side].Add(Poly); if (Side==4) N.BottomArea += Area; }
        }
    }
    N.Fraction = FMath::Abs(SFConstructionGeometry::SignedVolume(N.Solid)) / FMath::Pow(SFConstructionGeometry::CellCentimetres,3);
}
double ContactArea(const FNode& A, int32 SideA, const FNode& B, int32 SideB)
{
    double Area=0;
    for (const FPoly2& PA : A.Sides[SideA]) for (const FPoly2& PB : B.Sides[SideB])
        Area += FMath::Abs(SignedArea(Intersect(PA,PB)));
    return Area;
}
}

bool SFConstructionSupport::Evaluate(const TArray<FSFConstructionSupportMember>& Members,
    const FSFConstructionSupportProfile& Profile, TArray<FSFConstructionSupportResult>& Out)
{
    if (Members.Num()>MaxSupportMembers || Profile.ProfileId != TEXT("rough-timber-v1")
        || !FMath::IsFinite(Profile.WeightPerFullCell) || Profile.WeightPerFullCell<=0 || Profile.WeightPerFullCell>1.e6
        || !FMath::IsFinite(Profile.Capacity) || Profile.Capacity<=0 || Profile.Capacity>1.e6
        || !FMath::IsFinite(Profile.VerticalReachCm) || Profile.VerticalReachCm<SFConstructionGeometry::CellCentimetres || Profile.VerticalReachCm>1.e6
        || !FMath::IsFinite(Profile.HorizontalReachCm) || Profile.HorizontalReachCm<SFConstructionGeometry::CellCentimetres || Profile.HorizontalReachCm>1.e6) return false;
    if (Members.IsEmpty()) { Out.Reset(); return true; }
    TArray<FSFConstructionSupportMember> SortedMembers=Members;
    SortedMembers.Sort([](const auto& A,const auto& B){ return LessAddress(A.Address,B.Address); });
    TArray<FNode> Nodes; Nodes.SetNum(SortedMembers.Num());
    TMap<FIntVector,int32> ByAddress;
    for (int32 I=0; I<SortedMembers.Num(); ++I)
    {
        const auto& M=SortedMembers[I];
        if (M.Shape<0 || M.Shape>=SFConstructionGeometry::ShapeCount || M.Rotation<0 || M.Rotation>=SFConstructionGeometry::RotationCount
            || !FMath::IsFinite(M.CombatHealth) || M.CombatHealth<0 || M.CombatHealth>1
            || !FMath::IsFinite(M.AttachedLoad) || M.AttachedLoad<0 || M.AttachedLoad>1.e6 || ByAddress.Contains(M.Address)) return false;
        FNode& N=Nodes[I]; N.Input=M; if (!SFConstructionGeometry::Build(M.Shape,M.Rotation,N.Solid)) return false;
        MakeSides(N); N.Load=Profile.WeightPerFullCell*N.Fraction+M.AttachedLoad; N.bAnchored=M.bAnchored && N.BottomArea>NumericalGuard;
        ByAddress.Add(M.Address,I);
    }
    // Retain deterministic positive contacts for graph traversal.
    TArray<TArray<TPair<int32,double>>> Contacts; Contacts.SetNum(Nodes.Num());
    for (int32 I=0; I<Nodes.Num(); ++I) for (int32 Axis=0; Axis<3; ++Axis) for (int32 Dir : {-1,1})
    {
        FIntVector Adj=Nodes[I].Input.Address; int32& C=Adj[Axis]; if ((Dir<0&&C==MIN_int32)||(Dir>0&&C==MAX_int32)) continue; C+=Dir;
        const int32* Found=ByAddress.Find(Adj); if (!Found) continue; const int32 J=*Found;
        const double A=ContactArea(Nodes[I],SideIndex(Axis,Dir>0),Nodes[J],SideIndex(Axis,Dir<0));
        if (A>NumericalGuard) Contacts[I].Add(TPair<int32,double>(J,A/(SFConstructionGeometry::CellCentimetres*SFConstructionGeometry::CellCentimetres)));
    }
    for (auto& C : Contacts) C.Sort([&](const auto& A,const auto& B){ return LessAddress(Nodes[A.Key].Input.Address,Nodes[B.Key].Input.Address); });
    TArray<int32> Queue;
    for (int32 I=0; I<Nodes.Num(); ++I) if (Nodes[I].bAnchored) { Nodes[I].Distance=0; Queue.Add(I); }
    for (int32 H=0; H<Queue.Num(); ++H) for (const auto& E : Contacts[Queue[H]])
    { FNode& N=Nodes[E.Key]; if (N.Distance==INDEX_NONE) { N.Distance=Nodes[Queue[H]].Distance+1; Queue.Add(E.Key); } }
    for (int32 I=0; I<Nodes.Num(); ++I) if (Nodes[I].Distance>0)
        for (const auto& E : Contacts[I]) if (Nodes[E.Key].Distance>=0 && Nodes[E.Key].Distance<Nodes[I].Distance)
        { Nodes[I].Parents.Add(E.Key); Nodes[I].ParentAreas.Add(E.Value); }
    TArray<int32> Order; for (int32 I=0; I<Nodes.Num(); ++I) Order.Add(I);
    Order.Sort([&](int32 A,int32 B){ return Nodes[A].Distance==Nodes[B].Distance ? LessAddress(Nodes[A].Input.Address,Nodes[B].Input.Address) : Nodes[A].Distance>Nodes[B].Distance; });
    // Load shares flow from the farthest members inward over the frozen parent DAG.
    for (int32 I : Order)
    {
        FNode& N=Nodes[I]; const double H=N.Input.CombatHealth;
        if (N.Distance>0)
        {
            double Sum=0; for (double A:N.ParentAreas) Sum+=A;
            if (Sum>0) for (int32 K=0; K<N.Parents.Num(); ++K)
                Nodes[N.Parents[K]].Load += N.Load*N.ParentAreas[K]/Sum;
        }
        if (H<=0) { N.bFailed=true; N.Reason=ESFConstructionSupportReason::ZeroHealth; continue; }
        if (N.Distance==INDEX_NONE) { N.bFailed=true; N.Reason=ESFConstructionSupportReason::NoAnchorPath; continue; }
        const double AggregateRatio=N.Load/(Profile.Capacity*H);
        N.LoadRatio=AggregateRatio;
        if (AggregateRatio>=1.0-NumericalGuard) { N.bFailed=true; N.Reason=ESFConstructionSupportReason::AggregateCapacity; }
        if (N.Distance>0)
        {
            double Sum=0; for (double A:N.ParentAreas) Sum+=A;
            for (int32 K=0; K<N.Parents.Num(); ++K)
            {
                const int32 P=N.Parents[K]; const double A=N.ParentAreas[K], Share=N.Load*A/Sum;
                const double ParentHealth=Nodes[P].Input.CombatHealth;
                // A zero-health recipient already fails; skip its undefined ratio
                // and let the frozen-parent closure mark this child below.
                if (ParentHealth<=0) continue;
                const double Edge=Share/(Profile.Capacity*ParentHealth*A);
                if (Edge>N.LoadRatio) { N.LoadRatio=Edge; N.LimitingParent=P; }
                if (Edge>=1.0-NumericalGuard && !N.bFailed) { N.bFailed=true; N.Reason=ESFConstructionSupportReason::ContactCapacity; N.LimitingParent=P; }
            }
        }
    }
    // Reach accumulates from anchors outward. This same order propagates fixed
    // parent failures without removing parents or redistributing their shares.
    Order.Sort([&](int32 A,int32 B){ return Nodes[A].Distance==Nodes[B].Distance ? LessAddress(Nodes[A].Input.Address,Nodes[B].Input.Address) : Nodes[A].Distance<Nodes[B].Distance; });
    for (int32 I : Order)
    {
        FNode& N=Nodes[I]; const double H=N.Input.CombatHealth;
        if (H<=0 || N.Distance==INDEX_NONE) continue;
        double Worst=0; int32 WorstParent=INDEX_NONE;
        for (int32 P : N.Parents)
        {
            const int32 AxisDelta=FMath::Abs(N.Input.Address.Z-Nodes[P].Input.Address.Z);
            const double Reach=Nodes[P].ReachRatio + (AxisDelta*SFConstructionGeometry::CellCentimetres/(Profile.VerticalReachCm*H))
                + (SFConstructionGeometry::CellCentimetres*(FMath::Abs(N.Input.Address.X-Nodes[P].Input.Address.X)+FMath::Abs(N.Input.Address.Y-Nodes[P].Input.Address.Y))) /(Profile.HorizontalReachCm*H);
            if (Reach>Worst) { Worst=Reach; WorstParent=P; }
            if (Nodes[P].bFailed && !N.bFailed) { N.bFailed=true; N.Reason=ESFConstructionSupportReason::FailedParent; N.LimitingParent=P; }
        }
        N.ReachRatio=Worst;
        if (!N.bFailed && Worst>N.LoadRatio) N.LimitingParent=WorstParent;
        if (Worst>=1.0-NumericalGuard && !N.bFailed) { N.bFailed=true; N.Reason=ESFConstructionSupportReason::Reach; }
    }
    TArray<int32> Stable; for (int32 I=0; I<Nodes.Num(); ++I) Stable.Add(I);
    Stable.Sort([&](int32 A,int32 B){ return LessAddress(Nodes[A].Input.Address,Nodes[B].Input.Address); });
    TArray<FSFConstructionSupportResult> Candidate; Candidate.Reserve(Nodes.Num());
    for (int32 I:Stable)
    {
        const FNode& N=Nodes[I]; FSFConstructionSupportResult R; R.Address=N.Input.Address; R.AnchorDistance=N.Distance;
        R.LoadRatio=N.LoadRatio; R.ReachRatio=N.ReachRatio; R.Reason=N.Reason;
        if (N.LimitingParent!=INDEX_NONE) { R.bHasLimitingParent=true; R.LimitingParent=Nodes[N.LimitingParent].Input.Address; }
        if (!FMath::IsFinite(R.LoadRatio) || !FMath::IsFinite(R.ReachRatio)) return false;
        R.State=N.bFailed ? ESFConstructionSupportState::Unsupported : (R.LoadRatio>=0.7 || R.ReachRatio>=0.7 ? ESFConstructionSupportState::Strained : ESFConstructionSupportState::Stable);
        Candidate.Add(R);
    }
    Out=MoveTemp(Candidate); return true;
}
