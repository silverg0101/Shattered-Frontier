#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Creatures/SFCompanionMilestones.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCompanionMilestoneTest, "ShatteredFrontier.Creatures.Companion.MilestoneChoice",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCompanionMilestoneTest::RunTest(const FString& Parameters)
{
    using namespace SFCompanionMilestones;
    const FName VoidreignFamily(TEXT("voidreign")), EquineFamily(TEXT("equine")), Bolt(TEXT("equine.up70.bolt"));
    const FName Trampling(TEXT("equine.up70.trampling_charge")), Surefooted(TEXT("equine.passive.surefooted"));
    const TSet<FName> Unlocked = { LongShadow, RiftCollapse, Surefooted };
    FSFMilestoneEvidence Rest; Rest.bAtRestArea = true; Rest.bOwnerInCombat = false; Rest.bCompanionEngaged = false;
    auto Check = [this](const TCHAR* What, ESFMilestoneReject Got, ESFMilestoneReject Want) { TestEqual(What, (int32)Got, (int32)Want); };

    TestEqual(TEXT("Voidreign level-70 pool has its own three options"), Pool(VoidreignFamily, 70).Num(), 3);
    TestTrue(TEXT("Voidreign keeps the Equine level-50 passives"), Pool(VoidreignFamily, 50).Contains(Surefooted));
    TestFalse(TEXT("Equine cannot take a Voidreign upgrade"), Pool(EquineFamily, 70).Contains(LongShadow));

    Check(TEXT("valid at a rest area"), Validate(VoidreignFamily, 70, 70, LongShadow, Unlocked, Rest), ESFMilestoneReject::None);
    Check(TEXT("milestone 60 does not exist"), Validate(VoidreignFamily, 70, 60, LongShadow, Unlocked, Rest), ESFMilestoneReject::UnknownMilestone);
    Check(TEXT("level 69 cannot pick a 70 option"), Validate(VoidreignFamily, 69, 70, LongShadow, Unlocked, Rest), ESFMilestoneReject::LevelTooLow);
    Check(TEXT("other family's option"), Validate(VoidreignFamily, 70, 70, Trampling, Unlocked, Rest), ESFMilestoneReject::NotInPool);
    Check(TEXT("untrained option"), Validate(VoidreignFamily, 70, 70, Bolt, Unlocked, Rest), ESFMilestoneReject::NotUnlocked);
    FSFMilestoneEvidence Field = Rest; Field.bAtRestArea = false;
    Check(TEXT("no field swap"), Validate(VoidreignFamily, 70, 70, LongShadow, Unlocked, Field), ESFMilestoneReject::NotAtRestArea);
    FSFMilestoneEvidence Fight = Rest; Fight.bOwnerInCombat = true;
    Check(TEXT("no in-combat swap"), Validate(VoidreignFamily, 70, 70, LongShadow, Unlocked, Fight), ESFMilestoneReject::InCombat);
    FSFMilestoneEvidence Busy = Rest; Busy.bCompanionEngaged = true;
    Check(TEXT("not while the companion is engaged or pending"), Validate(VoidreignFamily, 70, 70, LongShadow, Unlocked, Busy), ESFMilestoneReject::CompanionBusy);

    FSFMilestoneState State;
    const FGuid First = FGuid::NewGuid(), Second = FGuid::NewGuid();
    Check(TEXT("first selection commits"), Select(State, First, VoidreignFamily, 70, 70, LongShadow, Unlocked, Rest), ESFMilestoneReject::None);
    TestEqual(TEXT("active option recorded"), State.Active(70), LongShadow);
    TestEqual(TEXT("one revision"), State.Revision, 1);
    Check(TEXT("retry of the same operation"), Select(State, First, VoidreignFamily, 70, 70, RiftCollapse, Unlocked, Rest), ESFMilestoneReject::None);
    TestEqual(TEXT("retry changes nothing"), State.Active(70), LongShadow);
    TestEqual(TEXT("retry adds no revision"), State.Revision, 1);
    Check(TEXT("rejected change"), Select(State, Second, VoidreignFamily, 70, 70, RiftCollapse, Unlocked, Field), ESFMilestoneReject::NotAtRestArea);
    TestEqual(TEXT("rejection keeps the active option"), State.Active(70), LongShadow);
    Check(TEXT("rest-area change"), Select(State, Second, VoidreignFamily, 70, 70, RiftCollapse, Unlocked, Rest), ESFMilestoneReject::None);
    TestEqual(TEXT("one option active per milestone"), State.Active(70), RiftCollapse);
    TestEqual(TEXT("level-50 choice untouched"), State.Active(50), FName(NAME_None));
    return true;
}
#endif
