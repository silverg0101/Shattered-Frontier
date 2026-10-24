#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Party/SFPartyModel.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPartyLifecycleTest,
    "ShatteredFrontier.Party.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPartyLifecycleTest::RunTest(const FString&)
{
    FSFPartyModel Parties;
    TestTrue(TEXT("Invite another player"), Parties.Invite(1, 2, 0, 60));
    TestTrue(TEXT("Accept creates party"), Parties.Accept(1, 2, 1));
    TestEqual(TEXT("Two members"), Parties.FindParty(2)->Members.Num(), 2);
    TestEqual(TEXT("Inviter leads"), Parties.FindParty(2)->Leader, 1);
    TestTrue(TEXT("Invite third"), Parties.Invite(1, 3, 2, 60));
    TestTrue(TEXT("Third accepts"), Parties.Accept(1, 3, 3));
    TestTrue(TEXT("Assign assistant"), Parties.AssignAssistant(1, 3));
    TestTrue(TEXT("Leader leaves"), Parties.Leave(1));
    TestEqual(TEXT("Assistant takes leadership"), Parties.FindParty(2)->Leader, 3);
    TestTrue(TEXT("Assistant promotes member"), Parties.Promote(3, 2));
    TestEqual(TEXT("Explicit handoff"), Parties.FindParty(3)->Leader, 2);
    TestTrue(TEXT("Leader kicks"), Parties.Kick(2, 3));
    TestFalse(TEXT("Kicked member no longer belongs"), Parties.FindParty(3) != nullptr);
    TestTrue(TEXT("Last member leaves"), Parties.Leave(2));
    TestFalse(TEXT("Party disbanded"), Parties.FindParty(2) != nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPartyAuthorityTest,
    "ShatteredFrontier.Party.Authority", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPartyAuthorityTest::RunTest(const FString&)
{
    FSFPartyModel Parties;
    TestFalse(TEXT("No self invite"), Parties.Invite(1, 1, 0, 60));
    TestFalse(TEXT("No unsolicited accept"), Parties.Accept(1, 2, 0));
    TestTrue(TEXT("Invite can be declined"), Parties.Invite(1, 2, 0, 60));
    TestTrue(TEXT("Only target declines"), Parties.Decline(1, 2));
    TestFalse(TEXT("Declined invite cannot be accepted"), Parties.Accept(1, 2, 1));
    TestTrue(TEXT("Fresh invite"), Parties.Invite(1, 2, 1, 60));
    TestTrue(TEXT("Accept"), Parties.Accept(1, 2, 2));
    TestFalse(TEXT("Member cannot invite"), Parties.Invite(2, 3, 2, 60));
    TestFalse(TEXT("Member cannot kick"), Parties.Kick(2, 1));
    TestFalse(TEXT("Member cannot promote"), Parties.Promote(2, 1));
    TestFalse(TEXT("Already grouped target cannot be invited"), Parties.Invite(4, 2, 2, 60));
    for (int32 Member = 3; Member <= 5; ++Member)
    {
        TestTrue(TEXT("Invite up to five"), Parties.Invite(1, Member, 3, 60));
        TestTrue(TEXT("Accept up to five"), Parties.Accept(1, Member, 4));
    }
    TestEqual(TEXT("Five member cap"), Parties.FindParty(1)->Members.Num(), FSFPartyModel::MaxMembers);
    TestFalse(TEXT("Leader cannot invite beyond cap"), Parties.Invite(1, 6, 5, 60));
    TestTrue(TEXT("Kicking frees a slot"), Parties.Kick(1, 5));
    TestTrue(TEXT("Invite after kick"), Parties.Invite(1, 6, 6, 60));
    TestTrue(TEXT("Accept after kick"), Parties.Accept(1, 6, 7));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPartyExpiryTest,
    "ShatteredFrontier.Party.Expiry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPartyExpiryTest::RunTest(const FString&)
{
    FSFPartyModel Parties;
    TestTrue(TEXT("Invite"), Parties.Invite(1, 2, 10, 60));
    TestFalse(TEXT("Deadline exclusive"), Parties.Accept(1, 2, 70));
    TestTrue(TEXT("Reinvite"), Parties.Invite(1, 2, 71, 60));
    Parties.RemoveConnection(1);
    TestFalse(TEXT("Disconnect invalidates invite"), Parties.Accept(1, 2, 72));
    TestTrue(TEXT("New invite"), Parties.Invite(1, 2, 73, 60));
    Parties.Expire(133);
    TestFalse(TEXT("Expired invite removed"), Parties.Accept(1, 2, 133));
    return true;
}
#endif
