#include "Chat/SFChatSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Chat/SFChatWidget.h"
#include "Dom/JsonObject.h"
#include "Framework/SFPlayerController.h"
#include "FrontEnd/SFFrontEnd.h"
#include "HttpModule.h"
#include "IWebSocket.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WebSocketsModule.h"

namespace
{
FString Encode(const TSharedRef<FJsonObject>& Object)
{
    FString Result;
    FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Result));
    return Result;
}
TSharedPtr<FJsonObject> Decode(const FString& Text)
{
    TSharedPtr<FJsonObject> Result;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result);
    return Result;
}
}

FString USFChatSettings::SafeEndpoint() const
{
    const FString Url = Endpoint.TrimStartAndEnd();
    if (Url.Len() > 256 || Url.Contains(TEXT("?")) || Url.Contains(TEXT("#")) || Url.Contains(TEXT("@")) || Url.Contains(TEXT("\\")) || Url.Contains(TEXT(" ")))
    { return {}; }
    if (Url.StartsWith(TEXT("wss://"))) { return Url; }
    if (Url.StartsWith(TEXT("ws://127.0.0.1:")) || Url.StartsWith(TEXT("ws://localhost:"))) { return Url; }
    return {};
}

void USFChatSubsystem::Start(const FString& InCharacterId, const FString& InSessionToken)
{
    Stop();
    if (!InCharacterId.StartsWith(TEXT("chr_")) || InSessionToken.IsEmpty()) { OnRejected.Broadcast(TEXT("identity_unavailable")); return; }
    Endpoint = GetDefault<USFChatSettings>()->SafeEndpoint();
    if (Endpoint.IsEmpty()) { OnRejected.Broadcast(TEXT("endpoint_unavailable")); return; }
    CharacterId = InCharacterId;
    SessionToken = InSessionToken;
    NextAttemptAt = 0;
    OnStatus.Broadcast(TEXT("connecting"));
}

void USFChatSubsystem::Stop()
{
    ++Generation;
    CharacterId.Reset();
    SessionToken.Reset();
    Endpoint.Reset();
    bConnecting = false;
    bAuthenticated = false;
    if (Socket.IsValid()) { Socket->Close(); Socket.Reset(); }
    if (Widget.IsValid()) { Widget->RemoveFromParent(); Widget.Reset(); }
}

void USFChatSubsystem::Deinitialize()
{
    Stop();
    Super::Deinitialize();
}

void USFChatSubsystem::Tick(float)
{
    if (SessionToken.IsEmpty()) { return; }
    CreateWidgetIfNeeded();
    if (!bConnecting && !bAuthenticated && FPlatformTime::Seconds() >= NextAttemptAt) { RequestTicket(); }
}

void USFChatSubsystem::RequestTicket()
{
    bConnecting = true;
    const uint64 Attempt = ++Generation;
    const FString Base = GetDefault<USFFrontEndSettings>()->SafeGatewayBaseUrl();
    if (Base.IsEmpty()) { Retry(TEXT("identity_unavailable")); return; }
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Base + TEXT("/v1/characters/") + CharacterId + TEXT("/contacts-ticket"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + SessionToken);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(TEXT("{}"));
    Request->SetTimeout(5.f);
    TWeakObjectPtr<USFChatSubsystem> Weak(this);
    Request->OnProcessRequestComplete().BindLambda([Weak, Attempt](FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
    {
        if (!Weak.IsValid() || Weak->Generation != Attempt) { return; }
        const int32 Code = bOk && Response.IsValid() ? Response->GetResponseCode() : 0;
        if (Code == 401 || Code == 403) { Weak->OnRejected.Broadcast(TEXT("identity_expired")); Weak->Stop(); return; }
        FString Ticket;
        const TSharedPtr<FJsonObject> Json = Response.IsValid() ? Decode(Response->GetContentAsString()) : nullptr;
        if (Code < 200 || Code >= 300 || !Json.IsValid() || !Json->TryGetStringField(TEXT("ticket"), Ticket) || Ticket.IsEmpty())
        { Weak->Retry(TEXT("ticket_unavailable")); return; }
        Weak->Connect(Ticket, Attempt);
    });
    if (!Request->ProcessRequest()) { Request->OnProcessRequestComplete().Unbind(); Retry(TEXT("ticket_unavailable")); }
}

void USFChatSubsystem::Connect(const FString& Ticket, uint64 Attempt)
{
    FWebSocketsModule& Module = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
    Socket = Module.CreateWebSocket(Endpoint);
    TWeakObjectPtr<USFChatSubsystem> Weak(this);
    Socket->OnConnected().AddLambda([Weak, Attempt, Ticket]()
    {
        if (!Weak.IsValid() || Weak->Generation != Attempt) { return; }
        TSharedRef<FJsonObject> Auth = MakeShared<FJsonObject>();
        Auth->SetStringField(TEXT("type"), TEXT("auth"));
        Auth->SetStringField(TEXT("ticket"), Ticket);
        Weak->Socket->Send(Encode(Auth));
    });
    Socket->OnMessage().AddLambda([Weak, Attempt](const FString& Packet)
    { if (Weak.IsValid() && Weak->Generation == Attempt) { Weak->HandlePacket(Packet); } });
    Socket->OnConnectionError().AddLambda([Weak, Attempt](const FString&)
    { if (Weak.IsValid() && Weak->Generation == Attempt) { Weak->Retry(TEXT("connection_unavailable")); } });
    Socket->OnClosed().AddLambda([Weak, Attempt](int32, const FString& Reason, bool)
    { if (Weak.IsValid() && Weak->Generation == Attempt) { Weak->Retry(Reason == TEXT("auth_rejected") ? TEXT("auth_rejected") : TEXT("disconnected")); } });
    Socket->Connect();
}

void USFChatSubsystem::Retry(const FString& Reason)
{
    ++Generation;
    bConnecting = false;
    bAuthenticated = false;
    if (Socket.IsValid()) { Socket->Close(); Socket.Reset(); }
    if (Reason == TEXT("auth_rejected")) { OnRejected.Broadcast(Reason); }
    OnStatus.Broadcast(Reason);
    NextAttemptAt = FPlatformTime::Seconds() + RetrySeconds;
    RetrySeconds = FMath::Min(RetrySeconds * 2.f, 30.f);
}

void USFChatSubsystem::HandlePacket(const FString& Packet)
{
    if (Packet.Len() > 65536) { Retry(TEXT("invalid_frame")); return; }
    const TSharedPtr<FJsonObject> Json = Decode(Packet);
    if (!Json.IsValid()) { return; }
    FString Type;
    if (!Json->TryGetStringField(TEXT("type"), Type)) { return; }
    if (Type == TEXT("status"))
    {
        FString Status;
        if (!Json->TryGetStringField(TEXT("status"), Status)) { return; }
        if (Status == TEXT("authenticated")) { bConnecting = false; bAuthenticated = true; RetrySeconds = 1.f; }
        else if (Status == TEXT("invalid") || Status == TEXT("rate_limited") || Status == TEXT("unavailable")
            || Status == TEXT("blocked") || Status == TEXT("not_in_party") || Status == TEXT("dropped"))
        { OnRejected.Broadcast(Status); }
        OnStatus.Broadcast(Status);
    }
    else if (Type == TEXT("message") && bAuthenticated)
    {
        FString Channel, Sender, Body;
        if (Json->TryGetStringField(TEXT("channel"), Channel) && (Channel == TEXT("party") || Channel == TEXT("whisper"))
            && Json->TryGetStringField(TEXT("from"), Sender) && Json->TryGetStringField(TEXT("body"), Body))
        { OnMessage.Broadcast(Channel, Sender, Body); }
    }
}

bool USFChatSubsystem::Send(const FString& Channel, const FString& Target, const FString& Body)
{
    if (!bAuthenticated || !Socket.IsValid() || Body.TrimStartAndEnd().IsEmpty()) { return false; }
    TSharedRef<FJsonObject> Packet = MakeShared<FJsonObject>();
    Packet->SetStringField(TEXT("type"), TEXT("send"));
    Packet->SetStringField(TEXT("channel"), Channel);
    Packet->SetStringField(TEXT("body"), Body);
    if (!Target.IsEmpty()) { Packet->SetStringField(TEXT("target"), Target); }
    const FString Text = Encode(Packet);
    if (FTCHARToUTF8(*Text).Length() > 65536) { OnRejected.Broadcast(TEXT("frame_too_large")); return false; }
    Socket->Send(Text);
    return true;
}

bool USFChatSubsystem::SendParty(const FString& Body) { return Send(TEXT("party"), {}, Body); }
bool USFChatSubsystem::SendWhisper(const FString& TargetCharacterId, const FString& Body)
{
    if (!TargetCharacterId.StartsWith(TEXT("chr_")) || TargetCharacterId == CharacterId) { OnRejected.Broadcast(TEXT("invalid_target")); return false; }
    return Send(TEXT("whisper"), TargetCharacterId, Body);
}

void USFChatSubsystem::CreateWidgetIfNeeded()
{
    if (Widget.IsValid() || !GetWorld()) { return; }
    ASFPlayerController* Controller = Cast<ASFPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!Controller || !Controller->IsLocalController()) { return; }
    const TSubclassOf<USFChatWidget> Class = LoadClass<USFChatWidget>(nullptr, USFChatWidget::AssetPath);
    if (!Class) { return; }
    USFChatWidget* Created = CreateWidget<USFChatWidget>(Controller, Class);
    if (Created) { Created->AddToViewport(9); Widget = Created; }
}
