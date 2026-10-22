#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Dialogue/SFDialogue.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFDialogueModelTest, "ShatteredFrontier.Dialogue.Model", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFDialogueModelTest::RunTest(const FString&)
{
    FSFDialogueTree Tree; FString Error;
    TestTrue(TEXT("shipped Millwarden tree loads"), FSFDialogueTree::Load(TEXT("millwarden_oswin.json"), Tree, Error));
    TestEqual(TEXT("speaker"), Tree.Speaker, FString(TEXT("Millwarden Oswin")));
    TestTrue(TEXT("offer"), Tree.bHasOffer);
    TestEqual(TEXT("two objectives"), Tree.Offer.Objectives.Num(), 2);

    const TSharedRef<FSFDialogueTree> Shared = MakeShared<FSFDialogueTree>(Tree);
    FSFDialogueSession Session; Session.Begin(Shared);
    TestEqual(TEXT("starts at greeting"), Session.NodeId, FString(TEXT("greeting")));
    TestTrue(TEXT("choice 0 moves on"), Session.Choose(0) == ESFDialogueStep::Stay);
    TestEqual(TEXT("tell me more"), Session.NodeId, FString(TEXT("more")));
    TestTrue(TEXT("out-of-range choice is ignored"), Session.Choose(9) == ESFDialogueStep::Stay && Session.NodeId == TEXT("more"));
    TestTrue(TEXT("accept"), Session.Choose(0) == ESFDialogueStep::Accepted);
    TestTrue(TEXT("accepted flag"), Session.bAccepted);
    TestEqual(TEXT("accepted node"), Session.NodeId, FString(TEXT("accepted")));
    TestTrue(TEXT("accept twice does nothing"), Session.Accept() == ESFDialogueStep::Stay);
    TestTrue(TEXT("farewell closes"), Session.Choose(1) == ESFDialogueStep::Closed);
    Session.Begin(Shared);
    TestTrue(TEXT("decline"), Session.Decline() == ESFDialogueStep::Declined);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFDialogueValidationTest, "ShatteredFrontier.Dialogue.Validation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFDialogueValidationTest::RunTest(const FString&)
{
    FSFDialogueTree Tree; FString Error;
    TestFalse(TEXT("missing start"), FSFDialogueTree::Parse(TEXT(R"({"speaker":"A","start":"x","nodes":{"y":{"line":"l","choices":[{"label":"b","action":"end"}]}}})"), Tree, Error));
    TestFalse(TEXT("dangling target"), FSFDialogueTree::Parse(TEXT(R"({"speaker":"A","start":"y","nodes":{"y":{"line":"l","choices":[{"label":"b","next":"nowhere"}]}}})"), Tree, Error));
    TestFalse(TEXT("five choices"), FSFDialogueTree::Parse(TEXT(R"({"speaker":"A","start":"y","nodes":{"y":{"line":"l","choices":[{"label":"1","action":"end"},{"label":"2","action":"end"},{"label":"3","action":"end"},{"label":"4","action":"end"},{"label":"5","action":"end"}]}}})"), Tree, Error));
    TestFalse(TEXT("accept without offer"), FSFDialogueTree::Parse(TEXT(R"({"speaker":"A","start":"y","nodes":{"y":{"line":"l","choices":[{"label":"b","action":"accept"}]}}})"), Tree, Error));
    TestFalse(TEXT("path in file name"), FSFDialogueTree::Load(TEXT("../x.json"), Tree, Error));
    TestTrue(TEXT("minimal tree"), FSFDialogueTree::Parse(TEXT(R"({"speaker":"A","start":"y","nodes":{"y":{"line":"l","choices":[{"label":"b","action":"end"}]}}})"), Tree, Error));
    return true;
}

#endif
