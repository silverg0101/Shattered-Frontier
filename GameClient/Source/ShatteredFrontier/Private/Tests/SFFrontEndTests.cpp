#include "FrontEnd/SFFrontEnd.h"
#include "FrontEnd/SFFrontEndGateway.h"
#include "Services/SFGameplayServicesClient.h"
#include "FrontEnd/SFFrontEndWidgets.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// Front-end model: the screen flow state machine, the name rules, the creator and the dev identity (non-shipping).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFrontEndFlowTest, "ShatteredFrontier.FrontEnd.Flow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFrontEndFlowTest::RunTest(const FString& Parameters)
{
    FSFFrontEndFlow Flow;
    TestEqual(TEXT("Starts on the title"), Flow.Screen, ESFFrontEndScreen::Title);
    TestFalse(TEXT("Esc on the title goes nowhere"), Flow.Back());
    Flow.Play();
    TestEqual(TEXT("Play asks for the account when signed out"), Flow.Screen, ESFFrontEndScreen::Login);
    TestFalse(TEXT("No account panel before sign-in"), Flow.OpenAccount());
    TestFalse(TEXT("No creator from the login screen"), Flow.BeginCreate());
    TestTrue(TEXT("Esc leaves the login screen"), Flow.Back());
    TestEqual(TEXT("Back on the title"), Flow.Screen, ESFFrontEndScreen::Title);
    Flow.Play();
    Flow.SignedIn();
    TestEqual(TEXT("Sign-in lands on character select"), Flow.Screen, ESFFrontEndScreen::CharacterSelect);
    TestTrue(TEXT("The account panel opens"), Flow.OpenAccount());
    TestTrue(TEXT("Esc closes the account panel first"), Flow.Back());
    TestEqual(TEXT("Still on character select"), Flow.Screen, ESFFrontEndScreen::CharacterSelect);
    TestFalse(TEXT("Account panel closed"), Flow.bAccountOpen);
    TestTrue(TEXT("Create opens the creator"), Flow.BeginCreate());
    TestTrue(TEXT("Esc leaves the creator for character select"), Flow.Back());
    TestEqual(TEXT("Character select"), Flow.Screen, ESFFrontEndScreen::CharacterSelect);
    TestTrue(TEXT("Esc from select goes to the title"), Flow.Back());
    TestTrue(TEXT("Still signed in"), Flow.bSignedIn);
    Flow.Play();
    TestEqual(TEXT("Play skips the login when signed in"), Flow.Screen, ESFFrontEndScreen::CharacterSelect);
    TestTrue(TEXT("Enter World"), Flow.Enter());
    TestEqual(TEXT("Entering"), Flow.Screen, ESFFrontEndScreen::EnteringWorld);
    TestFalse(TEXT("Esc does not interrupt entering"), Flow.Back());
    Flow.SignOut();
    TestEqual(TEXT("Sign out returns to the title"), Flow.Screen, ESFFrontEndScreen::Title);
    TestFalse(TEXT("Signed out"), Flow.bSignedIn);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFrontEndNameTest, "ShatteredFrontier.FrontEnd.Names", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFrontEndNameTest::RunTest(const FString& Parameters)
{
    FString Error;
    for (const TCHAR* Good : {TEXT("Aldren"), TEXT("Mira"), TEXT("Ann-Marie"), TEXT("D'arcy"), TEXT("Bob"), TEXT("Abcdefghijklmnop")})
    { TestTrue(FString::Printf(TEXT("%s is valid"), Good), SFFrontEnd::ValidateCharacterName(Good, Error)); }
    struct FBad { const TCHAR* Name; const char* Why; };
    const FBad Bad[] = {{TEXT(""), "empty"}, {TEXT("Al"), "too short"}, {TEXT("Abcdefghijklmnopq"), "17 letters"}, {TEXT("1Aldren"), "starts with a digit"},
        {TEXT("-Aldren"), "starts with a hyphen"}, {TEXT("Al dren"), "space"}, {TEXT("Al-dr-en"), "two marks"}, {TEXT("Al'dr-en"), "apostrophe and hyphen"},
        {TEXT("Aldren-"), "ends with a mark"}, {TEXT("Aldren2"), "digit"}, {TEXT("GMBob"), "staff prefix"}, {TEXT("Adminx"), "staff prefix"},
        {TEXT("\x0391rwen"), "Greek alpha mixed with Latin"}};
    for (const FBad& Case : Bad) { TestFalse(FString::Printf(TEXT("%s is rejected (%s)"), Case.Name, ANSI_TO_TCHAR(Case.Why)), SFFrontEnd::ValidateCharacterName(Case.Name, Error)); TestFalse(TEXT("A reason is given"), Error.IsEmpty()); }
    TestTrue(TEXT("Account name"), SFFrontEnd::ValidateAccountName(TEXT("greg_01"), Error));
    TestFalse(TEXT("Account with a space"), SFFrontEnd::ValidateAccountName(TEXT("greg 01"), Error));
    TestFalse(TEXT("Short account"), SFFrontEnd::ValidateAccountName(TEXT("ab"), Error));
    TestFalse(TEXT("Account starts with a digit"), SFFrontEnd::ValidateAccountName(TEXT("1alice"), Error));
    TestTrue(TEXT("Gateway accepts 32 characters"), SFFrontEnd::ValidateAccountName(TEXT("abcdefghabcdefghabcdefghabcdefgh"), Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFrontEndCreatorStateTest, "ShatteredFrontier.FrontEnd.CreatorState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFrontEndCreatorStateTest::RunTest(const FString& Parameters)
{
    FSFCreatorState State;
    TestEqual(TEXT("Seven steps"), FSFCreatorState::NumSteps, 7);
    TestEqual(TEXT("Step line"), State.Progress(), FString(TEXT("Step 1 of 7")));
    TestFalse(TEXT("Prev on the first step leaves the creator"), State.Prev());
    for (int32 i = 0; i < 6; ++i) { TestTrue(TEXT("Next advances"), State.Next()); }
    TestTrue(TEXT("On the last step"), State.OnLastStep());
    TestEqual(TEXT("Last step is the name"), FString(FSFCreatorState::StepName(State.Step)), FString(TEXT("Name")));
    TestFalse(TEXT("Next on the last step creates"), State.Next());
    TestTrue(TEXT("Jump to a step"), State.Jump(2));
    TestEqual(TEXT("Face"), FString(FSFCreatorState::StepName(State.Step)), FString(TEXT("Face")));
    TestFalse(TEXT("Jump out of range"), State.Jump(9));

    FSFAppearance Look;
    TestTrue(TEXT("Default look is valid"), Look.IsValid());
    Look.SetFromGenderTile(4);
    TestEqual(TEXT("Tile 4 is female"), Look.Body, 1);
    TestEqual(TEXT("Tile 4 is Northkin"), Look.Heritage, 2);
    TestEqual(TEXT("Tile round trip"), Look.GenderTile(), 4);
    Look.HeightCm = 400; Look.Face = 0;
    TestFalse(TEXT("Out-of-range look is invalid"), Look.IsValid());
    Look.Clamp();
    TestTrue(TEXT("Clamp repairs it"), Look.IsValid());
    FRandomStream Random(7);
    for (int32 i = 0; i < 20; ++i) { Look.Randomize(Random); TestTrue(TEXT("Random looks are valid"), Look.IsValid()); }
    TestEqual(TEXT("Randomize keeps the gender choice"), Look.GenderTile(), 4);
    TestTrue(TEXT("Json round trip"), FSFAppearance::FromJson(*Look.ToJson()) == Look);
    FSFAppearance Tall; Tall.HeightCm = 200;
    FSFAppearance Short; Short.HeightCm = 150;
    TestTrue(TEXT("Taller is scaled up"), Tall.PreviewScale().Z > Short.PreviewScale().Z);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFrontEndAppearancePacketTest, "ShatteredFrontier.FrontEnd.AppearancePacket", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFrontEndAppearancePacketTest::RunTest(const FString& Parameters)
{
    const TArray<SFFrontEnd::FFaceChoice> Male = SFFrontEnd::FacesForBody(0);
    const TArray<SFFrontEnd::FFaceChoice> Female = SFFrontEnd::FacesForBody(1);
    TestEqual(TEXT("Male face count"), Male.Num(), 5);
    TestEqual(TEXT("Female face count"), Female.Num(), 4);
    if (Male.Num() != 5 || Female.Num() != 4) { return false; }
    const TCHAR* MaleIds[] = {TEXT("Orlando"), TEXT("Godric"), TEXT("Edwin"), TEXT("Wulfric"), TEXT("Osric")};
    const TCHAR* FemaleIds[] = {TEXT("Hilda"), TEXT("Aelfwyn"), TEXT("Eadgyth"), TEXT("Ebba")};
    for (int32 i = 0; i < Male.Num(); ++i) { TestEqual(TEXT("Male face order"), Male[i].Id, FString(MaleIds[i])); TestEqual(TEXT("Male packet index"), Male[i].Index, i); }
    for (int32 i = 0; i < Female.Num(); ++i) { TestEqual(TEXT("Female face order"), Female[i].Id, FString(FemaleIds[i])); TestEqual(TEXT("Female packet index"), Female[i].Index, i + 5); }
    FSFAppearance Look;
    Look.Face = 4;
    Look.SetFromGenderTile(3);
    TestEqual(TEXT("Switching body resets face"), Look.Face, 1);
    Look.Face = 2; Look.Hair = 3; Look.Beard = 1; Look.Skin = 2; Look.HairColour = 3; Look.Eyes = 4;
    Look.HeightCm = 200; Look.Build = 100;
    // README v2: header, eight zero morphs, four LE16 parts, four zero-based palette bytes, zero cosmetics, face index 6 (Aelfwyn).
    const TArray<uint8> Expected = {2, 0, 1, 1, 255, 255, 8,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 2, 0, 1, 0, 0, 0,
        1, 2, 2, 3, 0, 6};
    TestTrue(TEXT("Known v2 packet matches backend byte layout"), Look.PackV2() == Expected);
    return true;
}

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFrontEndCreateWireTest, "ShatteredFrontier.FrontEnd.CreateWire", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFrontEndCreateWireTest::RunTest(const FString& Parameters)
{
    USFGameplayServicesSettings* Settings = GetMutableDefault<USFGameplayServicesSettings>();
    const bool WasEnabled = Settings->bEnabled;
    Settings->bEnabled = true;
    FSFGameplayRequest Sent;
    int32 Calls = 0;
    FSFGameplayServicesClient::SetSendOverrideForTests([&](const FSFGameplayRequest& Request, float, TFunction<void(const FSFGameplayResponse&)> Done)
    {
        Sent = Request; ++Calls;
        FSFGameplayResponse Response; Response.Status = ESFGameplayStatus::Ok; Response.AppearanceRevision = 1;
        Done(Response);
    });
    TSharedRef<ISFFrontEndIdentity> Identity = SFMakeFrontEndIdentity();
    const FString Account = TEXT("tst") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10);
    FString Name = TEXT("Tstwire");
    for (const TCHAR Digit : FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(7))
    {
        const TCHAR Lower = FChar::ToLower(Digit);
        Name.AppendChar(static_cast<TCHAR>(TEXT('a') + (Lower >= TEXT('a') ? Lower - TEXT('a') + 10 : Lower - TEXT('0'))));
    }
    Identity->SignIn(Account, TEXT("pw"), [](bool, const FString&, const FString&) {});
    FSFAppearance Look; Look.SetFromGenderTile(3); Look.Face = 2;
    bool Created = false;
    FSFCharacterSummary Made;
    Identity->CreateCharacter(Name, Look, [&](bool Ok, const FSFCharacterSummary& Character, const FString&) { Created = Ok; Made = Character; });
    TestTrue(TEXT("Create succeeds after service reply"), Created);
    TestEqual(TEXT("One CreateCharacter RPC"), Calls, 1);
    TestEqual(TEXT("RPC method"), Sent.Rpc, ESFGameplayRpc::CreateCharacter);
    TestEqual(TEXT("RPC uses the entering character id"), Sent.CharacterId, Made.Id);
    TestTrue(TEXT("RPC carries the chosen face packet"), Sent.AppearancePacket == Look.PackV2());
    if (Created) { Identity->DeleteCharacter(Made.Id, [](bool, const FString&) {}); }
    Identity->SignOut();
    FSFGameplayServicesClient::SetSendOverrideForTests(FSFGameplaySend());
    Settings->bEnabled = WasEnabled;
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFrontEndIdentityTest, "ShatteredFrontier.FrontEnd.DevIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFrontEndIdentityTest::RunTest(const FString& Parameters)
{
    TSharedRef<ISFFrontEndIdentity> Identity = SFMakeFrontEndIdentity();
    TestEqual(TEXT("Development builds use the dev identity"), Identity->Label(), FString(TEXT("dev")));
    const FString Account = TEXT("tst") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10);
    bool bOk = true; FString Error;
    Identity->SignIn(Account, TEXT(""), [&](bool Ok, const FString&, const FString& E) { bOk = Ok; Error = E; });
    TestFalse(TEXT("An empty password is refused"), bOk);
    Identity->SignIn(TEXT("a b"), TEXT("x"), [&](bool Ok, const FString&, const FString&) { bOk = Ok; });
    TestFalse(TEXT("An invalid account name is refused"), bOk);
    Identity->ListCharacters([&](bool Ok, const TArray<FSFCharacterSummary>&, const FString&) { bOk = Ok; });
    TestFalse(TEXT("No listing before sign-in"), bOk);
    FString Signed;
    Identity->SignIn(Account, TEXT("pw"), [&](bool Ok, const FString& Name, const FString&) { bOk = Ok; Signed = Name; });
    TestTrue(TEXT("Sign-in works"), bOk);
    TestEqual(TEXT("Account name comes back lowercase"), Signed, Account.ToLower());

    FSFCharacterSummary Made;
    FSFAppearance Look;
    const FString Name = TEXT("Tsthero");
    Identity->CreateCharacter(TEXT("Ab"), Look, [&](bool Ok, const FSFCharacterSummary&, const FString& E) { bOk = Ok; Error = E; });
    TestFalse(TEXT("A bad name is refused"), bOk);
    Identity->CreateCharacter(Name, Look, [&](bool Ok, const FSFCharacterSummary& C, const FString& E) { bOk = Ok; Made = C; Error = E; });
    TestTrue(TEXT("Create works"), bOk);
    Identity->CreateCharacter(Name, Look, [&](bool Ok, const FSFCharacterSummary&, const FString& E) { bOk = Ok; Error = E; });
    TestFalse(TEXT("The same name is taken"), bOk);
    TArray<FSFCharacterSummary> List;
    Identity->ListCharacters([&](bool Ok, const TArray<FSFCharacterSummary>& L, const FString&) { bOk = Ok; List = L; });
    TestEqual(TEXT("One character listed"), List.Num(), 1);
    FString Url;
    Identity->EnterWorld(Made, [&](bool Ok, const FString& U, const FString&) { bOk = Ok; Url = U; });
    TestTrue(TEXT("Enter World gives a travel URL carrying the dev character"), Url.Contains(TEXT("?SFDevCharacter=")));
    Identity->DeleteCharacter(Made.Id, [&](bool Ok, const FString&) { bOk = Ok; });
    TestTrue(TEXT("Delete works"), bOk);
    Identity->ListCharacters([&](bool Ok, const TArray<FSFCharacterSummary>& L, const FString&) { List = L; });
    TestEqual(TEXT("Nothing left"), List.Num(), 0);
    Identity->SignOut();
    IFileManager::Get().Delete(*FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FrontEnd"), TEXT("Dev_") + Account.ToLower() + TEXT(".json")));
    return true;
}
#endif

namespace
{
class FFakeIdentityHttp final : public ISFIdentityHttp
{
public:
    TArray<FSFIdentityHttpReply> Replies;
    TArray<FString> Paths;
    TArray<FString> Bodies;
    TArray<FString> Bearers;
    void Send(const FString&, const FString& Path, const FString& Body, const FString& Bearer,
        TFunction<void(FSFIdentityHttpReply)> Done) override
    {
        Paths.Add(Path); Bodies.Add(Body); Bearers.Add(Bearer);
        FSFIdentityHttpReply Reply;
        if (!Replies.IsEmpty()) { Reply = Replies[0]; Replies.RemoveAt(0); }
        Done(MoveTemp(Reply));
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFrontEndGatewayTest, "ShatteredFrontier.FrontEnd.Gateway", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFFrontEndGatewayTest::RunTest(const FString& Parameters)
{
    TSharedRef<FFakeIdentityHttp> Http = MakeShared<FFakeIdentityHttp>();
    TSharedRef<ISFFrontEndIdentity> Identity = SFMakeGatewayIdentity(Http);
    Http->Replies = {{200, TEXT("{\"login_id\":\"login_1\"}")}, {200, TEXT("{\"session_token\":\"opaque-session\"}")},
        {200, TEXT("{\"characters\":[{\"character_id\":\"chr_1\",\"name\":\"Aldren\"}]}")},
        {201, TEXT("{\"character_id\":\"chr_2\",\"name\":\"Mira\"}")},
        {200, TEXT("{\"address\":\"127.0.0.1:7777\",\"ticket\":\"opaque.ticket\"}")}};
    bool Ok = false;
    Identity->SignIn(TEXT("alice"), TEXT("private-password"), [&Ok](bool bOk, const FString&, const FString&) { Ok = bOk; });
    TestTrue(TEXT("Two-step login succeeds"), Ok);
    TestEqual(TEXT("Login begin route"), Http->Paths[0], FString(TEXT("/v1/login/begin")));
    TestEqual(TEXT("Login complete route"), Http->Paths[1], FString(TEXT("/v1/login/complete")));
    TestFalse(TEXT("Password absent from complete request"), Http->Bodies[1].Contains(TEXT("private-password")));
    TArray<FSFCharacterSummary> Characters;
    Identity->ListCharacters([&](bool bOk, const TArray<FSFCharacterSummary>& List, const FString&) { Ok = bOk; Characters = List; });
    TestTrue(TEXT("List succeeds"), Ok);
    TestEqual(TEXT("One character"), Characters.Num(), 1);
    TestEqual(TEXT("Session sent as bearer"), Http->Bearers[2], FString(TEXT("opaque-session")));
    FSFCharacterSummary Made;
    Identity->CreateCharacter(TEXT("Mira"), FSFAppearance(), [&](bool bOk, const FSFCharacterSummary& Created, const FString&) { Ok = bOk; Made = Created; });
    TestTrue(TEXT("Creation succeeds"), Ok);
    TestEqual(TEXT("New character ID"), Made.Id, FString(TEXT("chr_2")));
    FString Url;
    Identity->EnterWorld(Made, [&](bool bOk, const FString& Travel, const FString&) { Ok = bOk; Url = Travel; });
    TestTrue(TEXT("Enter succeeds"), Ok);
    TestEqual(TEXT("Travel carries ticket"), Url, FString(TEXT("127.0.0.1:7777?SFIdentityTicket=opaque.ticket")));
    Http->Replies = {{401, TEXT("{\"reason\":\"SESSION_EXPIRED\"}")}};
    Identity->ListCharacters([&](bool bOk, const TArray<FSFCharacterSummary>&, const FString&) { Ok = bOk; });
    TestFalse(TEXT("Expired session fails closed"), Ok);
    Http->Replies = {{503, TEXT("{\"reason\":\"UNAVAILABLE\"}")}};
    Identity->EnterWorld(Made, [&](bool bOk, const FString&, const FString&) { Ok = bOk; });
    TestFalse(TEXT("Ticket issue failure does not travel"), Ok);
    Http->Replies = {{401, TEXT("{\"reason\":\"TICKET_EXPIRED\"}")}};
    Identity->EnterWorld(Made, [&](bool bOk, const FString&, const FString&) { Ok = bOk; });
    TestFalse(TEXT("Expired ticket response does not travel"), Ok);
    Identity->SignOut();
    return true;
}

#if WITH_EDITOR
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableText.h"
#include "Editor.h"

// The front-end Widget Blueprints (py -3 tools/Run-InEditor.py tools/ui/create_widgets.py): constructed in the editor world, driven through their public API.
namespace
{
template <class T> T* MakePage(const TCHAR* Path)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const TSubclassOf<T> Class = LoadClass<T>(nullptr, Path);
    return Class && World ? CreateWidget<T>(World, Class) : nullptr;
}
bool IsMasked(UUserWidget* Field)
{
    const UEditableText* Input = Field && Field->WidgetTree ? Cast<UEditableText>(Field->WidgetTree->FindWidget(TEXT("Input"))) : nullptr;
    if (!Input) { return false; }
    for (const TCHAR* Name : {TEXT("IsPassword"), TEXT("bIsPassword")})
    {
        if (const FBoolProperty* Prop = CastField<FBoolProperty>(UEditableText::StaticClass()->FindPropertyByName(Name))) { return Prop->GetPropertyValue_InContainer(Input); }
    }
    return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuFrontEndLoginTest, "ShatteredFrontier.Hud.Menus.FrontEndLogin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuFrontEndLoginTest::RunTest(const FString& Parameters)
{
    USFFrontEndLogin* Login = MakePage<USFFrontEndLogin>(USFFrontEndLogin::AssetPath);
    if (!TestNotNull(TEXT("WBP_FrontEndLogin constructs"), Login)) { return false; }
    FString GotAccount, GotPassword; bool bGotRemember = false; int32 Calls = 0;
    Login->OnSignIn.BindLambda([&](const FString& A, const FString& P, bool R) { GotAccount = A; GotPassword = P; bGotRemember = R; ++Calls; });
    Login->SetAccount(TEXT("greg_01"), true);
    Login->SetPasswordForTest(TEXT("secret"));
    Login->Submit();
    TestEqual(TEXT("Sign-in raised once"), Calls, 1);
    TestEqual(TEXT("Account"), GotAccount, FString(TEXT("greg_01")));
    TestEqual(TEXT("Password passed through"), GotPassword, FString(TEXT("secret")));
    TestTrue(TEXT("Remember flag"), bGotRemember);
    Login->SetBusy(true);
    Login->Submit();
    TestEqual(TEXT("No second sign-in while busy"), Calls, 1);
    if (UWidget* Field = Login->GetWidgetFromName(TEXT("PasswordField"))) { TestTrue(TEXT("The password field masks its text"), IsMasked(Cast<UUserWidget>(Field))); }
    else { AddError(TEXT("PasswordField missing")); }
    if (UWidget* Field = Login->GetWidgetFromName(TEXT("AccountField"))) { TestFalse(TEXT("The account field does not mask"), IsMasked(Cast<UUserWidget>(Field))); }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuFrontEndSelectTest, "ShatteredFrontier.Hud.Menus.FrontEndCharacters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuFrontEndSelectTest::RunTest(const FString& Parameters)
{
    USFFrontEndSelect* Page = MakePage<USFFrontEndSelect>(USFFrontEndSelect::AssetPath);
    if (!TestNotNull(TEXT("WBP_FrontEndCharacters constructs"), Page)) { return false; }
    FSFCharacterSummary A; A.Id = TEXT("a"); A.Name = TEXT("Aldren"); A.Level = 18; A.Location = TEXT("Silverbrook");
    FSFCharacterSummary B; B.Id = TEXT("b"); B.Name = TEXT("Mira"); B.Level = 7; B.Location = TEXT("Greenlands");
    Page->SetCharacters({A, B}, 8, 0);
    TestEqual(TEXT("Two characters and two empty slots"), Page->NumRows(), 4);
    if (Page->NumRows() < 4) { return false; }
    TestTrue(TEXT("The first character is selected"), Page->GetRow(0)->IsSelected());
    int32 Selected = -1, Deleted = -1, Entered = -1, Created = 0;
    Page->OnSelected.BindLambda([&](int32 I) { Selected = I; });
    Page->OnDelete.BindLambda([&](int32 I) { Deleted = I; });
    Page->OnEnter.BindLambda([&](int32 I) { Entered = I; });
    Page->OnCreate.BindLambda([&]() { ++Created; });
    Page->GetRow(1)->OnClicked.Broadcast(Page->GetRow(1));
    TestEqual(TEXT("Clicking a character selects it"), Selected, 1);
    TestTrue(TEXT("The gold select frame moves"), Page->GetRow(1)->IsSelected() && !Page->GetRow(0)->IsSelected());
    Page->GetRow(2)->OnClicked.Broadcast(Page->GetRow(2));
    TestEqual(TEXT("An empty slot opens the creator"), Created, 1);
    if (USFButtonWidget* Delete = Page->GetDeleteButton())
    {
        TestFalse(TEXT("Delete needs the hold"), Delete->AdvanceHold(0.5f, true));
        TestEqual(TEXT("Nothing deleted yet"), Deleted, -1);
        TestTrue(TEXT("The hold completes"), Delete->AdvanceHold(1.5f, true));
        TestEqual(TEXT("The selected character is deleted"), Deleted, 1);
    }
    else { AddError(TEXT("DeleteButton missing")); }
    if (USFButtonWidget* Enter = Page->GetEnterButton()) { Enter->OnActivated.Broadcast(Enter); TestEqual(TEXT("Enter World enters the selected character"), Entered, 1); }
    Page->SetCharacters({}, 8, INDEX_NONE);
    TestEqual(TEXT("No characters: two empty slots"), Page->NumRows(), 2);
    TestFalse(TEXT("Nothing to enter"), Page->GetEnterButton()->IsButtonEnabled());
    TArray<FSFCharacterSummary> Full;
    for (int32 i = 0; i < 8; ++i) { FSFCharacterSummary C; C.Id = FString::FromInt(i); C.Name = TEXT("Hero"); Full.Add(C); }
    Page->SetCharacters(Full, 8, 3);
    TestEqual(TEXT("Eight characters fill the slots"), Page->NumRows(), 8);
    TestFalse(TEXT("No Create New with every slot used"), Page->GetCreateButton()->IsButtonEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFMenuFrontEndCreatorTest, "ShatteredFrontier.Hud.Menus.FrontEndCreator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFMenuFrontEndCreatorTest::RunTest(const FString& Parameters)
{
    USFFrontEndCreator* Creator = MakePage<USFFrontEndCreator>(USFFrontEndCreator::AssetPath);
    if (!TestNotNull(TEXT("WBP_FrontEndCreator constructs"), Creator)) { return false; }
    int32 Changes = 0, Leaves = 0, Creates = 0; FString CreatedName;
    Creator->OnChanged.BindLambda([&](const FSFCreatorState&) { ++Changes; });
    Creator->OnLeave.BindLambda([&]() { ++Leaves; });
    Creator->OnCreate.BindLambda([&](const FString& N, const FSFAppearance&) { ++Creates; CreatedName = N; });
    Creator->Begin();
    TestEqual(TEXT("Starts on step 1"), Creator->GetState().Step, 0);
    TestEqual(TEXT("The Gender step has no option rows"), Creator->NumOptionRows(), 0);
    Creator->Next();
    TestEqual(TEXT("Body step: height and build"), Creator->NumOptionRows(), 2);
    Creator->GoToStep(3);
    TestEqual(TEXT("Hair step: style, beard, colour"), Creator->NumOptionRows(), 3);
    Creator->Randomize();
    TestTrue(TEXT("Randomize keeps the look valid"), Creator->GetState().Look.IsValid());
    TestTrue(TEXT("Changes were reported to the scene"), Changes > 0);
    Creator->GoToStep(6);
    Creator->Next();
    TestEqual(TEXT("An empty name does not create"), Creates, 0);
    Creator->SetNameForTest(TEXT("Al"));
    Creator->Next();
    TestEqual(TEXT("A short name does not create"), Creates, 0);
    Creator->SetNameForTest(TEXT("Aldren"));
    Creator->Next();
    TestEqual(TEXT("A valid name creates"), Creates, 1);
    TestEqual(TEXT("With that name"), CreatedName, FString(TEXT("Aldren")));
    Creator->GoToStep(0);
    Creator->Back();
    TestEqual(TEXT("Back on the first step leaves the creator"), Leaves, 1);
    return true;
}
#endif

#endif
