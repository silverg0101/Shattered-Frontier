#include "FrontEnd/SFFrontEndGateway.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FString Encode(const TSharedRef<FJsonObject>& Object)
{
    FString Body;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Body));
    return Body;
}

TSharedPtr<FJsonObject> Decode(const FString& Body)
{
    TSharedPtr<FJsonObject> Object;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Object);
    return Object;
}

FString Failure(const FSFIdentityHttpReply& Reply)
{
    if (const TSharedPtr<FJsonObject> Body = Decode(Reply.Body))
    {
        FString Reason;
        if (Body->TryGetStringField(TEXT("reason"), Reason) && !Reason.IsEmpty())
        { Reason.ReplaceInline(TEXT("_"), TEXT(" ")); return Reason; }
    }
    return Reply.Status == 0 ? TEXT("Identity gateway is unavailable.") : TEXT("Identity request failed.");
}

bool Successful(const FSFIdentityHttpReply& Reply) { return Reply.Status >= 200 && Reply.Status < 300; }

FSFCharacterSummary Character(const FJsonObject& Object)
{
    FSFCharacterSummary Result;
    Object.TryGetStringField(TEXT("character_id"), Result.Id);
    Object.TryGetStringField(TEXT("name"), Result.Name);
    Result.Location = TEXT("Greenlands");
    FString Encoded;
    TArray<uint8> Packet;
    if (Object.TryGetStringField(TEXT("appearance_v2_base64"), Encoded) && FBase64::Decode(Encoded, Packet) && Packet.Num() == 29 && Packet[0] == 2)
    {
        Result.Look.Body = Packet[2]; Result.Look.Heritage = Packet[3];
        Result.Look.HeightCm = FSFAppearance::MinHeight + FMath::RoundToInt(Packet[4] * float(FSFAppearance::MaxHeight - FSFAppearance::MinHeight) / 255.f);
        Result.Look.Build = FMath::RoundToInt(Packet[5] * 100.f / 255.f);
        Result.Look.Hair = Packet[17] + 1; Result.Look.Beard = Packet[19];
        Result.Look.Skin = Packet[23] + 1; Result.Look.HairColour = Packet[24] + 1; Result.Look.Eyes = Packet[26] + 1;
        const auto Faces = SFFrontEnd::FacesForBody(Result.Look.Body);
        for (int32 Index = 0; Index < Faces.Num(); ++Index) { if (Faces[Index].Index == Packet[28]) { Result.Look.Face = Index + 1; break; } }
        Result.Look.Clamp();
    }
    return Result;
}

class FUnrealIdentityHttp final : public ISFIdentityHttp
{
public:
    explicit FUnrealIdentityHttp(FString InBaseUrl) : BaseUrl(MoveTemp(InBaseUrl)) {}
    void Send(const FString& Verb, const FString& Path, const FString& Body, const FString& Bearer,
        TFunction<void(FSFIdentityHttpReply)> Done) override
    {
        if (BaseUrl.IsEmpty()) { Done({}); return; }
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(BaseUrl + Path);
        Request->SetVerb(Verb);
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        if (!Bearer.IsEmpty()) { Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Bearer); }
        if (Verb == TEXT("POST") || Verb == TEXT("DELETE")) { Request->SetHeader(TEXT("Idempotency-Key"), FGuid::NewGuid().ToString(EGuidFormats::Digits)); }
        if (!Body.IsEmpty()) { Request->SetContentAsString(Body); }
        Request->SetTimeout(5.f);
        TSharedRef<TFunction<void(FSFIdentityHttpReply)>> Completion = MakeShared<TFunction<void(FSFIdentityHttpReply)>>(MoveTemp(Done));
        Request->OnProcessRequestComplete().BindLambda([Completion](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
        {
            if (*Completion) { auto Callback = MoveTemp(*Completion); Callback({bConnected && Response.IsValid() ? Response->GetResponseCode() : 0, Response.IsValid() ? Response->GetContentAsString() : FString()}); }
        });
        if (!Request->ProcessRequest() && *Completion) { Request->OnProcessRequestComplete().Unbind(); auto Callback = MoveTemp(*Completion); Callback({}); }
    }
private:
    FString BaseUrl;
};

class FGatewayIdentity final : public ISFFrontEndIdentity, public TSharedFromThis<FGatewayIdentity>
{
public:
    explicit FGatewayIdentity(TSharedRef<ISFIdentityHttp> InHttp) : Http(MoveTemp(InHttp)) {}
    FString Label() const override { return TEXT("gateway"); }
    FString ChatSessionToken() const override { return SessionToken; }
    FSFRealmStatus Realm() const override { return {GetDefault<USFFrontEndSettings>()->SafeRealmName(), true, TEXT("Online")}; }
    void SignIn(const FString& Account, const FString& Password, TFunction<void(bool, const FString&, const FString&)> Done) override
    {
        FString Error;
        if (!SFFrontEnd::ValidateAccountName(Account, Error) || Password.IsEmpty())
        { Done(false, {}, Error.IsEmpty() ? TEXT("Enter your password.") : Error); return; }
        TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("account_name"), Account); Payload->SetStringField(TEXT("password"), Password);
        Http->Send(TEXT("POST"), TEXT("/v1/login/begin"), Encode(Payload), {},
            [Self = AsShared(), Account, Done = MoveTemp(Done)](FSFIdentityHttpReply Reply) mutable
            {
                const TSharedPtr<FJsonObject> Json = Decode(Reply.Body);
                FString LoginId;
                if (!Successful(Reply) || !Json.IsValid() || !Json->TryGetStringField(TEXT("login_id"), LoginId) || LoginId.IsEmpty())
                { Done(false, {}, Failure(Reply)); return; }
                TSharedRef<FJsonObject> Complete = MakeShared<FJsonObject>(); Complete->SetStringField(TEXT("login_id"), LoginId);
                Self->Http->Send(TEXT("POST"), TEXT("/v1/login/complete"), Encode(Complete), {},
                    [Self, Account, Done = MoveTemp(Done)](FSFIdentityHttpReply Final) mutable
                    {
                        const TSharedPtr<FJsonObject> Result = Decode(Final.Body);
                        FString Token;
                        if (!Successful(Final) || !Result.IsValid() || !Result->TryGetStringField(TEXT("session_token"), Token) || Token.IsEmpty())
                        { Done(false, {}, Failure(Final)); return; }
                        Self->SessionToken = MoveTemp(Token);
                        Done(true, Account.ToLower(), {});
                    });
            });
    }
    void SignOut() override
    {
        if (!SessionToken.IsEmpty()) { Http->Send(TEXT("POST"), TEXT("/v1/sessions/logout"), TEXT("{}"), SessionToken, [](FSFIdentityHttpReply) {}); }
        SessionToken.Reset();
    }
    void ListCharacters(TFunction<void(bool, const TArray<FSFCharacterSummary>&, const FString&)> Done) override
    {
        if (SessionToken.IsEmpty()) { Done(false, {}, TEXT("Sign in first.")); return; }
        Http->Send(TEXT("GET"), TEXT("/v1/characters?realm_id=greenlands"), {}, SessionToken,
            [Done = MoveTemp(Done)](FSFIdentityHttpReply Reply) mutable
            {
                const TSharedPtr<FJsonObject> Json = Decode(Reply.Body);
                const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
                if (!Successful(Reply) || !Json.IsValid() || !Json->TryGetArrayField(TEXT("characters"), Items)) { Done(false, {}, Failure(Reply)); return; }
                TArray<FSFCharacterSummary> List;
                for (const auto& Item : *Items) { if (Item.IsValid() && Item->Type == EJson::Object) { List.Add(Character(*Item->AsObject())); } }
                Done(true, List, {});
            });
    }
    void CreateCharacter(const FString& Name, const FSFAppearance& Look, const FString& ArchetypeId,
        TFunction<void(bool, const FSFCharacterSummary&, const FString&)> Done) override
    {
        FString Error;
        const TArray<uint8> Packet = Look.PackV2();
        if (SessionToken.IsEmpty() || !SFFrontEnd::ValidateCharacterName(Name, Error) || Packet.IsEmpty() || !FSFCreatorState::AvailableArchetypeIds().Contains(ArchetypeId))
        { Done(false, {}, SessionToken.IsEmpty() ? TEXT("Sign in first.") : Error.IsEmpty() ? TEXT("Invalid appearance.") : Error); return; }
        TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
        Payload->SetStringField(TEXT("realm_id"), TEXT("greenlands")); Payload->SetStringField(TEXT("name"), Name);
        Payload->SetStringField(TEXT("appearance_v2_base64"), FBase64::Encode(Packet));
        Payload->SetStringField(TEXT("archetype_id"), ArchetypeId);
        Http->Send(TEXT("POST"), TEXT("/v1/characters"), Encode(Payload), SessionToken,
            [Done = MoveTemp(Done)](FSFIdentityHttpReply Reply) mutable
            {
                const TSharedPtr<FJsonObject> Json = Decode(Reply.Body);
                FString Id;
                if (!Successful(Reply) || !Json.IsValid() || !Json->TryGetStringField(TEXT("character_id"), Id) || Id.IsEmpty())
                { Done(false, {}, Failure(Reply)); return; }
                Done(true, Character(*Json), {});
            });
    }
    void DeleteCharacter(const FString& Id, TFunction<void(bool, const FString&)> Done) override
    {
        if (SessionToken.IsEmpty() || !Id.StartsWith(TEXT("chr_"))) { Done(false, TEXT("Invalid character.")); return; }
        Http->Send(TEXT("DELETE"), TEXT("/v1/characters/") + Id, {}, SessionToken,
            [Done = MoveTemp(Done)](FSFIdentityHttpReply Reply) mutable { Done(Successful(Reply), Successful(Reply) ? FString() : Failure(Reply)); });
    }
    void EnterWorld(const FSFCharacterSummary& Selected, TFunction<void(bool, const FString&, const FString&)> Done) override
    {
        if (SessionToken.IsEmpty() || !Selected.Id.StartsWith(TEXT("chr_"))) { Done(false, {}, TEXT("Invalid character.")); return; }
        Http->Send(TEXT("POST"), TEXT("/v1/characters/") + Selected.Id + TEXT("/enter-world"), TEXT("{}"), SessionToken,
            [Done = MoveTemp(Done)](FSFIdentityHttpReply Reply) mutable
            {
                const TSharedPtr<FJsonObject> Json = Decode(Reply.Body);
                FString Address, Ticket;
                if (!Successful(Reply) || !Json.IsValid() || !Json->TryGetStringField(TEXT("address"), Address) || !Json->TryGetStringField(TEXT("ticket"), Ticket)
                    || Address.IsEmpty() || Ticket.IsEmpty() || Address.Contains(TEXT("?")) || Address.Contains(TEXT("/")) || Address.Contains(TEXT("@"))
                    || Ticket.Contains(TEXT("?")) || Ticket.Contains(TEXT("/")) || Ticket.Contains(TEXT("&")))
                { Done(false, {}, Failure(Reply)); return; }
                Done(true, Address + TEXT("?SFIdentityTicket=") + Ticket, {});
            });
    }
private:
    TSharedRef<ISFIdentityHttp> Http;
    FString SessionToken;
};
}

TSharedRef<ISFIdentityHttp> SFMakeIdentityHttp(const FString& BaseUrl) { return MakeShared<FUnrealIdentityHttp>(BaseUrl); }
TSharedRef<ISFFrontEndIdentity> SFMakeGatewayIdentity(TSharedRef<ISFIdentityHttp> Http) { return MakeShared<FGatewayIdentity>(MoveTemp(Http)); }
