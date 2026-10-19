#include "Gathering/SFGatheringProgression.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGatheringProgressionFormulaTest,
    "ShatteredFrontier.Gathering.Pilot.ProgressionFormula",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FSFGatheringProgressionFormulaTest::RunTest(const FString& Parameters)
{
    auto* C=NewObject<USFGatheringProgressionSettings>();
    TestTrue(TEXT("Shipped typed defaults validate"),C->Validate());
    TestTrue(TEXT("Optimum six-second novice success has base probability"),FMath::IsNearlyEqual(FSFGatheringProgressionSession::GainChance(0,25,6,1,1,*C),0.15,1e-7));
    TestTrue(TEXT("Three-second practice has half attempt weight"),FMath::IsNearlyEqual(FSFGatheringProgressionSession::GainChance(0,25,3,1,1,*C),0.075,1e-7));
    TestEqual(TEXT("Trivial common material yields no practice gain above150"),FSFGatheringProgressionSession::GainChance(151,0,6,1,1,*C),0.0);
    TestEqual(TEXT("Impossible difficulty yields zero"),FSFGatheringProgressionSession::GainChance(0,251,6,1,1,*C),0.0);
    TestTrue(TEXT("Expert band and skill factor both apply"),FMath::IsNearlyEqual(FSFGatheringProgressionSession::GainChance(500,525,6,1,1,*C),0.15/5*0.9,1e-7));
    TestEqual(TEXT("Uncommon unlock remains100"),FSFGatheringProgressionSession::RequiredSkill(ESFGatheringMaterialTier::Uncommon),100);
    TestEqual(TEXT("Legendary difficulty remains800"),FSFGatheringProgressionSession::Difficulty(ESFGatheringMaterialTier::Legendary),800);
    TestEqual(TEXT("Canonical logging ID"),FString(FSFGatheringProgressionSession::SkillId(ESFGatheringSkill::Logging)),FString(TEXT("skill/logging")));
    FSFGatheringProgressionSession Kernel; Kernel.TestRoll=0;
    FSFGatheringSkillSnapshot State;
    Kernel.Practice(State,ESFGatheringSkill::Logging,0,3,10,0,3,*C);
    TestEqual(TEXT("Successful roll grants precisely one logging point"),State.Get(ESFGatheringSkill::Logging),1);
    TestEqual(TEXT("Unrelated mining unchanged"),State.Get(ESFGatheringSkill::Mining),0);
    State.Locks[1]=ESFGatheringSkillLock::Locked;
    Kernel.Practice(State,ESFGatheringSkill::Logging,0,3,10,4,7,*C);
    TestEqual(TEXT("Locked practice counted without a gain"),State.PracticeCount,int64(2));
    TestEqual(TEXT("Locked skill remains unchanged"),State.Get(ESFGatheringSkill::Logging),1);
    State.Locks[1]=ESFGatheringSkillLock::Up; State.Values[1]=900;
    Kernel.Practice(State,ESFGatheringSkill::Logging,900,3,10,8,11,*C);
    TestEqual(TEXT("Unspecialized cap preserved"),State.Get(ESFGatheringSkill::Logging),900);

    FSFGatheringProgressionSession Capped; Capped.TestRoll=0;
    FSFGatheringSkillSnapshot Full; Full.Values={700,500,700,700,700,700};
    Capped.Practice(Full,ESFGatheringSkill::Logging,700,6,1,0,6,*C);
    TestEqual(TEXT("Track cap blocks gain without down donor"),Full.Get(ESFGatheringSkill::Logging),500);
    Full.Locks[0]=ESFGatheringSkillLock::Down; Full.Locks[2]=ESFGatheringSkillLock::Down;
    Capped.Practice(Full,ESFGatheringSkill::Logging,500,6,2,7,13,*C);
    TestEqual(TEXT("Valid down donor permits gain at track cap"),Full.Get(ESFGatheringSkill::Logging),501);
    TestEqual(TEXT("Equal donor tie uses canonical lexical skill id"),Full.Get(ESFGatheringSkill::Herbalism),699);
    TestEqual(TEXT("Other equal donor not consumed"),Full.Get(ESFGatheringSkill::Mining),700);
    FSFGatheringProgressionSession Trivial; Trivial.TestRoll=0;
    FSFGatheringSkillSnapshot TrivialState; TrivialState.Values[1]=151;
    Trivial.Practice(TrivialState,ESFGatheringSkill::Logging,0,6,1,0,6,*C);
    TestEqual(TEXT("Zero roll cannot gain from zero-chance trivial practice"),TrivialState.Get(ESFGatheringSkill::Logging),151);
    FSFGatheringProgressionSession Rate; Rate.TestRoll=0; FSFGatheringSkillSnapshot RateState;
    for (int32 I=0;I<550;++I)
    {
        RateState.Values[1]=0; // Isolate the S=0 hourly hard limit from changing skill.
        Rate.Practice(RateState,ESFGatheringSkill::Logging,25,6,I,I*6,I*6+6,*C);
    }
    TestEqual(TEXT("Hourly hard rate rejects gain above540 at novice baseline"),RateState.GainCount,int64(540));

    auto* RepeatConfig=DuplicateObject<USFGatheringProgressionSettings>(C,GetTransientPackage()); RepeatConfig->RepetitionThreshold=2;
    FSFGatheringProgressionSession Repeated; Repeated.TestRoll=0.06;
    FSFGatheringSkillSnapshot RepeatState;
    for (int32 I=0;I<3;++I) { Repeated.Practice(RepeatState,ESFGatheringSkill::Logging,0,3,55,I*4,I*4+3,*RepeatConfig); }
    TestEqual(TEXT("Same-context third practice dampens the otherwise passing roll"),RepeatState.Get(ESFGatheringSkill::Logging),2);

    FSFGatheringProgressionSession Timing; Timing.TestRoll=1;
    FSFGatheringSkillSnapshot TimingState;
    for (int32 I=0;I<199;++I) { Timing.Practice(TimingState,ESFGatheringSkill::Logging,25,6,I,I*7,I*7+6,*C); }
    Timing.TestRoll=0.05;
    Timing.Practice(TimingState,ESFGatheringSkill::Logging,25,6,200,199*7,199*7+6,*C);
    TestEqual(TEXT("Two hundred regular input records apply timing damping"),TimingState.GainCount,int64(0));

    auto* FatigueConfig=DuplicateObject<USFGatheringProgressionSettings>(C,GetTransientPackage()); FatigueConfig->FatigueFreeMinutes=0; FatigueConfig->FatigueWindowMinutes=120;
    FSFGatheringProgressionSession Fatigue; Fatigue.TestRoll=1; FSFGatheringSkillSnapshot FatiguedState;
    for (int32 I=0;I<119;++I) { Fatigue.Practice(FatiguedState,ESFGatheringSkill::Logging,25,6,I,I*60,I*60+6,*FatigueConfig); }
    Fatigue.TestRoll=0.05; Fatigue.Practice(FatiguedState,ESFGatheringSkill::Logging,25,6,120,119*60,119*60+6,*FatigueConfig);
    TestEqual(TEXT("Full fatigue window applies floor and prevents passing unmodified roll"),FatiguedState.GainCount,int64(0));
    C->AttemptWeightSeconds=0;
    TestFalse(TEXT("Invalid release configuration rejected"),C->Validate());
    TestEqual(TEXT("Invalid configuration cannot generate gain chance"),FSFGatheringProgressionSession::GainChance(0,25,6,1,1,*C),0.0);
    return true;
}
#endif
