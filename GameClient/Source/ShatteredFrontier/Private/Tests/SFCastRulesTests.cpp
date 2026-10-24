#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Combat/SFCastRules.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCastTimingRulesTest, "ShatteredFrontier.Combat.Casting.Timing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCastTimingRulesTest::RunTest(const FString& Parameters)
{
    using namespace SFCastRules;
    FTuning Tune;
    FAbility Cast; Cast.Id = 1; Cast.Cast = 2; Cast.Cooldown = 8; Cast.bHasteCooldown = true;
    FState State;
    TestTrue(TEXT("server starts cast"), State.Request(Cast, 10, .25, Tune) == EStart::Started);
    TestEqual(TEXT("haste scales cast"), State.EndsAt, 11.5);
    TestEqual(TEXT("standard GCD"), State.GcdEndsAt, 11.125);
    TestEqual(TEXT("haste scales authored cooldown"), State.CooldownEnds.FindRef(1), 16.0);
    bool Completed = false;
    State.Advance(11.49, Cast, Completed);
    TestFalse(TEXT("server has not completed early"), Completed);
    State.Advance(11.5, Cast, Completed);
    TestTrue(TEXT("server completes at deadline"), Completed);
    TestTrue(TEXT("per ability cooldown blocks replay"), State.Request(Cast, 12, 0, Tune) == EStart::Cooldown);
    FAbility LongCd; LongCd.Id = 3; LongCd.Cooldown = 40; LongCd.bHasteCooldown = true;
    State.Request(LongCd, 20, .75, Tune);
    TestEqual(TEXT("long cooldown never scales"), State.CooldownEnds.FindRef(3), 60.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCastQueueRulesTest, "ShatteredFrontier.Combat.Casting.QueueAndGcd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCastQueueRulesTest::RunTest(const FString& Parameters)
{
    using namespace SFCastRules;
    FTuning Tune;
    FAbility Cast; Cast.Id = 1; Cast.Cast = 1.5;
    FAbility Channel; Channel.Id = 2; Channel.Channel = 2; Channel.Ticks = 4; Channel.Gcd = EGcd::Short;
    FState State;
    TestTrue(TEXT("cast accepted"), State.Request(Cast, 1, 0, Tune) == EStart::Started);
    TestTrue(TEXT("early input rejected"), State.Request(Channel, 1.5, 0, Tune) == EStart::Busy);
    TestTrue(TEXT("input within 400 ms queued"), State.Request(Channel, 2.2, 0, Tune) == EStart::Queued);
    TestTrue(TEXT("only one queued slot"), State.Request(Channel, 2.3, 0, Tune) == EStart::Busy);
    bool Completed = false;
    State.Advance(2.5, Cast, Completed);
    TestTrue(TEXT("cast finishes"), Completed);
    State.QueuedId = 0;
    TestTrue(TEXT("queued channel starts"), State.Request(Channel, 2.5, 0, Tune) == EStart::Started);
    TestEqual(TEXT("short GCD"), State.GcdEndsAt, 3.5);
    TestEqual(TEXT("first channel tick"), State.Advance(3, Channel, Completed), 1);
    TestEqual(TEXT("all due ticks caught up"), State.Advance(4.5, Channel, Completed), 3);
    TestTrue(TEXT("channel ends"), Completed);
    FAbility Instant; Instant.Id = 3; Instant.Gcd = EGcd::Standard;
    State.Request(Instant, 5, .75, Tune);
    TestEqual(TEXT("GCD floor"), State.GcdEndsAt, 5.75);
    FState ShortCast;
    Cast.Cast = 1.0;
    ShortCast.Request(Cast, 10, 0, Tune);
    TestTrue(TEXT("cast-end window queues before later GCD end"), ShortCast.Request(Channel, 10.8, 0, Tune) == EStart::Queued);
    ShortCast.Advance(11.0, Cast, Completed);
    TestTrue(TEXT("queued ability still waits for GCD"), ShortCast.Request(Channel, 11.0, 0, Tune) == EStart::Busy);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCastInterruptRulesTest, "ShatteredFrontier.Combat.Casting.Interruptions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCastInterruptRulesTest::RunTest(const FString& Parameters)
{
    using namespace SFCastRules;
    FTuning Tune;
    FAbility Cast; Cast.Id = 1; Cast.Cast = 1.5; Cast.School = 1;
    FState State;
    State.Request(Cast, 1, 0, Tune);
    State.Damage(1.2, Tune); State.Damage(1.3, Tune); State.Damage(1.4, Tune);
    TestEqual(TEXT("only two cast pushbacks"), State.EndsAt, 3.0);
    TestTrue(TEXT("movement past 25 cm interrupts"), MovedBeyond(25.01, Tune.MoveToleranceCm, false));
    TestFalse(TEXT("boundary does not interrupt"), MovedBeyond(25.0, Tune.MoveToleranceCm, false));
    TestFalse(TEXT("moving cast exemption"), MovedBeyond(50, Tune.MoveToleranceCm, true));
    State.Interrupt(1.5, 3, 1);
    TestTrue(TEXT("school locked for three seconds"), State.Request(Cast, 4.49, 0, Tune) == EStart::Locked);
    TestTrue(TEXT("school unlocks at deadline"), State.Request(Cast, 4.5, 0, Tune) == EStart::Started);
    TestEqual(TEXT("interrupt refunds in full"), Refund(10, true, 50), 10);
    TestEqual(TEXT("caster cancellation refunds half"), Refund(10, false, 50), 5);
    FAbility Channel; Channel.Id = 2; Channel.Channel = 2; Channel.Ticks = 4;
    State.Cancel(); State.Request(Channel, 7, 0, Tune);
    State.Damage(7.2, Tune);
    TestEqual(TEXT("damage shortens channel by 250 ms"), State.EndsAt, 8.75);
    return true;
}
#endif
