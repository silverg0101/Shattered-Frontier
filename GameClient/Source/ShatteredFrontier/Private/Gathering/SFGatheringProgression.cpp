#include "Gathering/SFGatheringProgression.h"

bool USFGatheringProgressionSettings::Validate() const
{
    const auto In=[](double V,double Low,double High) { return FMath::IsFinite(V)&&V>=Low&&V<=High; };
    return In(BaseGainRate,0.001,0.9)&&In(MaximumGainChance,0.01,0.9)&&In(AttemptWeightSeconds,1,30)
        &&In(DifficultyOptimum,-100,100)&&In(DifficultyWidth,1,300)&&In(ExpertGainFactor,0,1)
        &&In(MasterGainFactor,0,1)&&In(LegendaryGainFactor,0,1)&&FatigueWindowMinutes>=120&&FatigueWindowMinutes<=480
        &&FatigueFreeMinutes>=0&&FatigueFreeMinutes<=120&&FatigueFreeMinutes<FatigueWindowMinutes
        &&In(FatigueFloor,0.1,1)&&RepetitionThreshold>=1&&RepetitionThreshold<=1000
        &&In(TimingCoefficientMinimum,0,0.2)&&In(RegularTimingGainFactor,0,1)&&In(SkillLockChangeCooldownSeconds,5,60);
}

const TCHAR* FSFGatheringProgressionSession::SkillId(ESFGatheringSkill Skill)
{
    switch (Skill)
    {
    case ESFGatheringSkill::Mining: return TEXT("skill/mining");
    case ESFGatheringSkill::Logging: return TEXT("skill/logging");
    case ESFGatheringSkill::Herbalism: return TEXT("skill/herbalism");
    case ESFGatheringSkill::Farming: return TEXT("skill/farming");
    case ESFGatheringSkill::Fishing: return TEXT("skill/fishing");
    case ESFGatheringSkill::Hunting: return TEXT("skill/hunting");
    default: return TEXT("");
    }
}
const TCHAR* FSFGatheringProgressionSession::SkillName(ESFGatheringSkill Skill)
{
    switch (Skill)
    {
    case ESFGatheringSkill::Mining: return TEXT("Mining");
    case ESFGatheringSkill::Logging: return TEXT("Logging");
    case ESFGatheringSkill::Herbalism: return TEXT("Herbalism");
    case ESFGatheringSkill::Farming: return TEXT("Farming");
    case ESFGatheringSkill::Fishing: return TEXT("Fishing");
    case ESFGatheringSkill::Hunting: return TEXT("Hunting");
    default: return TEXT("Unknown");
    }
}
int32 FSFGatheringProgressionSession::Difficulty(ESFGatheringMaterialTier Tier)
{ return int32(Tier)>=0&&int32(Tier)<=4 ? 200*int32(Tier) : MAX_int32; }
int32 FSFGatheringProgressionSession::RequiredSkill(ESFGatheringMaterialTier Tier)
{ return FMath::Max(0,Difficulty(Tier)-100); }

namespace
{
    double Band(int32 S,const USFGatheringProgressionSettings& C)
    { return S>=900 ? C.LegendaryGainFactor : S>=700 ? C.MasterGainFactor : S>=500 ? C.ExpertGainFactor : 1.0; }
}
double FSFGatheringProgressionSession::GainChance(int32 S,int32 D,double Duration,double Fatigue,double AntiMacro,const USFGatheringProgressionSettings& C)
{
    if (!C.Validate()||S<0||S>1000||D<0||D>1100||!FMath::IsFinite(Duration)||Duration<=0
        ||!FMath::IsFinite(Fatigue)||!FMath::IsFinite(AntiMacro)||Fatigue<0||Fatigue>1||AntiMacro<0||AntiMacro>1) { return 0; }
    const double Delta=D-S;
    if (Delta< -150||Delta>250) { return 0; }
    const double DifficultyFactor=FMath::Exp(-0.5*FMath::Square((Delta-C.DifficultyOptimum)/C.DifficultyWidth));
    return FMath::Min(double(C.MaximumGainChance),C.BaseGainRate*FMath::Clamp(Duration/C.AttemptWeightSeconds,0.2,3.0)
        /FMath::Max(1.0,S/100.0)*Band(S,C)*DifficultyFactor)*Fatigue*AntiMacro;
}

void FSFGatheringProgressionSession::Practice(FSFGatheringSkillSnapshot& State,ESFGatheringSkill Skill,int32 D,double Duration,uint64 Context,double InputTime,double Now,const USFGatheringProgressionSettings& C)
{
    const int32 K=int32(Skill);
    if (K<0||K>=6||State.Values.Num()!=6||State.Locks.Num()!=6||!C.Validate()
        ||!FMath::IsFinite(Now)||!FMath::IsFinite(InputTime)||InputTime>Now||!FMath::IsFinite(Duration)||Duration<=0||D<0||D>1100) { return; }
    int32 Total=0;
    for (int32 I=0;I<6;++I)
    {
        if (State.Values[I]<0||State.Values[I]>900||uint8(State.Locks[I])>uint8(ESFGatheringSkillLock::Locked)) { return; }
        Total+=State.Values[I];
    }
    auto& History=Records[K];
    History.RemoveAll([Now](const FRecord& R) { return R.At<=Now-3600; });
    History.Add({Now,Context,false});
    auto& Minutes=ActiveMinutes[K];
    const int64 Minute=FMath::FloorToInt64(Now/60);
    Minutes.RemoveAll([Minute,&C](int64 M) { return M<=Minute-C.FatigueWindowMinutes; });
    if (Minutes.IsEmpty()||Minutes.Last()!=Minute) { Minutes.Add(Minute); }
    InputTimes.Add(InputTime);
    if (InputTimes.Num()>200) { InputTimes.RemoveAt(0,InputTimes.Num()-200,EAllowShrinking::No); }
    if (State.PracticeCount<MAX_int64) { ++State.PracticeCount; }
    // Locked/capped practice still enters fatigue and repetition windows, without a roll.
    if (State.Locks[K]!=ESFGatheringSkillLock::Up||State.Values[K]>=900) { return; }
    int32 Same=0,Gains=0;
    for (const FRecord& R:History) { Same+=R.Context==Context ? 1 : 0; Gains+=R.bGain ? 1 : 0; }
    const double Fatigue=Minutes.Num()<=C.FatigueFreeMinutes ? 1.0
        : FMath::Max(double(C.FatigueFloor),1.0-0.75*(Minutes.Num()-C.FatigueFreeMinutes)/(C.FatigueWindowMinutes-C.FatigueFreeMinutes));
    const double Repetition=Same<=C.RepetitionThreshold ? 1.0 : FMath::Max(0.2,double(C.RepetitionThreshold)/Same);
    double Timing=1;
    if (InputTimes.Num()==200)
    {
        double Sum=0,SquareSum=0;
        for (int32 I=1;I<InputTimes.Num();++I) { const double Interval=InputTimes[I]-InputTimes[I-1]; Sum+=Interval; SquareSum+=Interval*Interval; }
        const double Mean=Sum/199;
        const double CV=Mean>0 ? FMath::Sqrt(FMath::Max(0.0,SquareSum/199-Mean*Mean))/Mean : 0;
        if (CV<C.TimingCoefficientMinimum) { Timing=C.RegularTimingGainFactor; }
    }
    // Six skills share gathering; no fabricated other-track projection in this pilot.
    const int32 HardLimit=FMath::CeilToInt(2*600*C.BaseGainRate/FMath::Max(1.0,State.Values[K]/100.0)*Band(State.Values[K],C)*3.0);
    if (Gains>=HardLimit) { return; }
    const double Chance=GainChance(State.Values[K],D,Duration,Fatigue,Repetition*Timing,C);
    double Roll=FMath::FRand();
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    if (TestRoll.IsSet()) { Roll=TestRoll.GetValue(); }
#endif
    if (Roll>=Chance) { return; }
    if (Total+1>4000||Total+1>7200)
    {
        int32 Donor=INDEX_NONE;
        for (int32 I=0;I<6;++I)
        {
            if (I==K||State.Locks[I]!=ESFGatheringSkillLock::Down||State.Values[I]<=0) { continue; }
            if (Donor==INDEX_NONE||State.Values[I]>State.Values[Donor]
                ||(State.Values[I]==State.Values[Donor]&&FCString::Strcmp(SkillId(ESFGatheringSkill(I)),SkillId(ESFGatheringSkill(Donor)))<0)) { Donor=I; }
        }
        if (Donor==INDEX_NONE) { return; }
        --State.Values[Donor];
    }
    ++State.Values[K]; History.Last().bGain=true;
    if (State.GainCount<MAX_int64) { ++State.GainCount; } State.LastGainedSkill=Skill;
}
