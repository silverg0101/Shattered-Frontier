#include "Chat/SFChatSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
struct FChatRelayState
{
    TWeakObjectPtr<USFChatSubsystem> Chat;
    FString Role;
    FString Root;
    int32 Step = 0;
    double Started = FPlatformTime::Seconds();
};

class FChatRelayStep final : public IAutomationLatentCommand
{
public:
    FChatRelayStep(const TSharedRef<FChatRelayState>& InState, FAutomationTestBase* InTest)
        : State(InState), Test(InTest) { }

    virtual bool Update() override
    {
        USFChatSubsystem* Chat = State->Chat.Get();
        if (!Chat || FPlatformTime::Seconds() - State->Started > 45.0)
        {
            Test->AddError(TEXT("Chat relay client timed out or lost its subsystem."));
            if (Chat) { Chat->Stop(); }
            return true;
        }
        const bool bA = State->Role == TEXT("A");
        const FString Own = bA ? TEXT("chr_chat_a") : TEXT("chr_chat_b");
        const FString Peer = bA ? TEXT("chr_chat_b") : TEXT("chr_chat_a");
        const FString Ready = FPaths::Combine(State->Root, State->Role + TEXT("-ready"));
        const FString PeerReady = FPaths::Combine(State->Root, bA ? TEXT("B-ready") : TEXT("A-ready"));
        const FString Done = FPaths::Combine(State->Root, TEXT("A-done"));
        switch (State->Step)
        {
        case 0:
            if (!Chat->IsReady()) { return false; }
            if (!FFileHelper::SaveStringToFile(TEXT("ready"), *Ready)) { Test->AddError(TEXT("Cannot write chat fixture gate.")); return true; }
            State->Step = 1;
            return false;
        case 1:
            if (!IFileManager::Get().FileExists(*PeerReady)) { return false; }
            if (bA && !Chat->SendParty(TEXT("party-from-a"))) { Test->AddError(TEXT("A could not send party message.")); return true; }
            State->Step = 2;
            return false;
        case 2:
            if (!Chat->HasFixtureMessage(TEXT("party"), Peer, bA ? TEXT("party-from-b") : TEXT("party-from-a"))) { return false; }
            if (!bA)
            {
                if (!Chat->SendParty(TEXT("party-from-b"))) { Test->AddError(TEXT("B could not reply to party message.")); return true; }
                State->Step = 3;
                return false;
            }
            if (!Chat->HasFixtureMessage(TEXT("party"), Own, TEXT("party-from-a"))) { Test->AddError(TEXT("A did not receive its party echo.")); return true; }
            if (!Chat->SendWhisper(Peer, TEXT("blocked-probe"))) { Test->AddError(TEXT("A could not send whisper probe.")); return true; }
            State->Step = 3;
            return false;
        case 3:
            if (bA)
            {
                if (!Chat->HasFixtureRejection(TEXT("not_delivered"))) { return false; }
                if (!FFileHelper::SaveStringToFile(TEXT("done"), *Done)) { Test->AddError(TEXT("Cannot write chat completion gate.")); return true; }
            }
            else
            {
                if (!Chat->HasFixtureMessage(TEXT("party"), Own, TEXT("party-from-b")) || !IFileManager::Get().FileExists(*Done)) { return false; }
            }
            UE_LOG(LogTemp, Display, TEXT("CHAT_UNREAL_CLIENT_PASS %s"), *State->Role);
            Chat->Stop();
            return true;
        }
        return false;
    }
private:
    TSharedRef<FChatRelayState> State;
    FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFChatRelayTest, "ShatteredFrontier.Chat.TwoClientRelay",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FSFChatRelayTest::RunTest(const FString&)
{
    FString Role, Endpoint, Root;
    FParse::Value(FCommandLine::Get(), TEXT("ChatRelayRole="), Role);
    FParse::Value(FCommandLine::Get(), TEXT("ChatRelayEndpoint="), Endpoint);
    FParse::Value(FCommandLine::Get(), TEXT("ChatRelayRoot="), Root);
    if ((Role != TEXT("A") && Role != TEXT("B")) || !Endpoint.StartsWith(TEXT("ws://127.0.0.1:")) || Root.IsEmpty())
    { AddError(TEXT("Two-client chat fixture arguments are missing.")); return false; }
    UGameInstance* GameInstance = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game && Context.OwningGameInstance) { GameInstance = Context.OwningGameInstance; break; }
    }
    if (!TestNotNull(TEXT("Game client instance"), GameInstance)) { return false; }
    USFChatSubsystem* Chat = GameInstance->GetSubsystem<USFChatSubsystem>();
    if (!TestNotNull(TEXT("Chat subsystem"), Chat)) { return false; }
    auto State = MakeShared<FChatRelayState>();
    State->Chat = Chat;
    State->Role = Role;
    State->Root = Root;
    Chat->StartFixtureForTest(Role == TEXT("A") ? TEXT("chr_chat_a") : TEXT("chr_chat_b"),
        Endpoint, Role == TEXT("A") ? TEXT("fixture_a") : TEXT("fixture_b"));
    ADD_LATENT_AUTOMATION_COMMAND(FChatRelayStep(State, this));
    return true;
}
#endif
