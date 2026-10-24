#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Creatures/SFCompanionCaptureRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCompanionCaptureRulesTest,
    "ShatteredFrontier.Creatures.Companion.CaptureRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCompanionCaptureRulesTest::RunTest(const FString& Parameters)
{
    FSFCompanionCaptureSettings S;
    FSFCompanionCaptureEvidence E;
    E.bAuthority = E.bEligiblePopulation = E.bUnownedUnreserved = E.bFreeStableSlot = true;
    E.OwnerLevel = E.CreatureLevel = 2; E.Difficulty = 100; E.Hunting = 0;
    E.MaxHealth = 60; E.Health = 18; E.bPetsPassiveRecalled = E.bStationary = E.bLineOfSight = true;
    E.RangeMetres = 5; E.ElapsedMs = 8000;
    auto Check = [this, &S](const TCHAR* Label, const FSFCompanionCaptureEvidence& Value, bool Commit,
        ESFCompanionCaptureReject Expected)
    { TestEqual(Label, int32(SFCompanionCaptureRules::Check(S, Value, Commit)), int32(Expected)); };
    TestTrue(TEXT("shipped controls are valid"), S.Valid());
    Check(TEXT("exact health, range, time thresholds admit"), E, true, ESFCompanionCaptureReject::None);
    E.Health = 19; Check(TEXT("one HP above threshold refuses"), E, false, ESFCompanionCaptureReject::TooHealthy); E.Health = 18;
    E.ElapsedMs = 7999; Check(TEXT("channel cannot complete early"), E, true, ESFCompanionCaptureReject::ChannelIncomplete); E.ElapsedMs = 8000;
    E.RangeMetres = 5.01; Check(TEXT("range cannot drift"), E, true, ESFCompanionCaptureReject::OutOfRangeOrSight); E.RangeMetres = 5;
    E.bForeignEngagement = true; Check(TEXT("foreign fight refuses"), E, false, ESFCompanionCaptureReject::ForeignEngagement); E.bForeignEngagement = false;
    E.bOwnerOrPetDamaged = true; Check(TEXT("pet taking damage interrupts"), E, true, ESFCompanionCaptureReject::OwnerOrPetDamaged); E.bOwnerOrPetDamaged = false;
    E.bTargetDamagedOrDead = true; Check(TEXT("new damage to target interrupts"), E, true, ESFCompanionCaptureReject::TargetDamagedOrDead); E.bTargetDamagedOrDead = false;
    E.bFreeStableSlot = false; Check(TEXT("reserved roster refuses"), E, false, ESFCompanionCaptureReject::NoStableSlot); E.bFreeStableSlot = true;
    E.CreatureLevel = 3; Check(TEXT("owner level gate"), E, false, ESFCompanionCaptureReject::LevelOrHunting); E.CreatureLevel = 2;
    E.Difficulty = 301; E.Hunting = 200; Check(TEXT("required Hunting is difficulty minus 100"), E, false, ESFCompanionCaptureReject::LevelOrHunting);
    E.Hunting = 201; Check(TEXT("exact Hunting admission"), E, false, ESFCompanionCaptureReject::None);
    E.bAuthority = false; Check(TEXT("client cannot capture"), E, false, ESFCompanionCaptureReject::NoAuthority);
    S.ChannelMs = 4500; TestFalse(TEXT("unsupported tuning fails closed"), S.Valid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCompanionTrainingRulesTest,
    "ShatteredFrontier.Creatures.Companion.TrainingRules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCompanionTrainingRulesTest::RunTest(const FString& Parameters)
{
    FSFCompanionCaptureSettings S;
    FSFCompanionLessonEvidence E;
    E.bAtRestArea = E.bObjectiveComplete = true; E.ActiveMs = 10000;
    auto Check = [this, &S](const TCHAR* Label, const FSFCompanionLessonEvidence& Value, ESFCompanionLessonReject Expected)
    { TestEqual(Label, int32(SFCompanionTrainingRules::Check(S, Value)), int32(Expected)); };
    for (int32 Lesson = 0; Lesson < 3; ++Lesson)
    { E.Lesson = E.CompletedLessons = Lesson; Check(TEXT("ordered objective completes"), E, ESFCompanionLessonReject::None); }
    E.Lesson = 2; E.CompletedLessons = 1; Check(TEXT("cannot skip lesson"), E, ESFCompanionLessonReject::WrongLesson);
    E.CompletedLessons = 3; Check(TEXT("completed lesson gives no repeat credit"), E, ESFCompanionLessonReject::WrongLesson);
    E.Lesson = E.CompletedLessons = 1; E.bObjectiveComplete = false;
    Check(TEXT("elapsed time without route is insufficient"), E, ESFCompanionLessonReject::ObjectiveIncomplete);
    E.bObjectiveComplete = true; E.ActiveMs = 9999; Check(TEXT("lesson needs full interaction"), E, ESFCompanionLessonReject::TooShort);
    E.ActiveMs = 10000; E.bInCombat = true; Check(TEXT("combat ends projection"), E, ESFCompanionLessonReject::InCombat);
    return true;
}
#endif
