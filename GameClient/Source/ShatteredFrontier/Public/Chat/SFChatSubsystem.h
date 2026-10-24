#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "SFChatSubsystem.generated.h"

class IWebSocket;
class UUserWidget;

// Release endpoint. The gateway routes /v1/chat to Chat.Host; use wss outside loopback development.
UCLASS(Config=Game, DefaultConfig)
class SHATTEREDFRONTIER_API USFChatSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Chat", meta=(ToolTip="Gateway WebSocket URL for the party and whisper relay."))
    FString Endpoint;
    FString SafeEndpoint() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSFChatMessageEvent, const FString&, Channel, const FString&, Sender, const FString&, Body);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFChatStatusEvent, const FString&, Status);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFChatRejectionEvent, const FString&, Reason);

// One instance survives front-end to world travel. Credentials stay in memory and are never exposed to Blueprint.
UCLASS()
class SHATTEREDFRONTIER_API USFChatSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="Chat") FSFChatMessageEvent OnMessage;
    UPROPERTY(BlueprintAssignable, Category="Chat") FSFChatStatusEvent OnStatus;
    UPROPERTY(BlueprintAssignable, Category="Chat") FSFChatRejectionEvent OnRejected;

    void Start(const FString& InCharacterId, const FString& InSessionToken);
    UFUNCTION(BlueprintCallable, Category="Chat") bool SendParty(const FString& Body);
    UFUNCTION(BlueprintCallable, Category="Chat") bool SendWhisper(const FString& TargetCharacterId, const FString& Body);
    UFUNCTION(BlueprintCallable, Category="Chat") void Stop();
    UFUNCTION(BlueprintPure, Category="Chat") bool IsReady() const { return bAuthenticated; }

    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USFChatSubsystem, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate(); }

private:
    bool Send(const FString& Channel, const FString& Target, const FString& Body);
    void RequestTicket();
    void Connect(const FString& Ticket, uint64 Attempt);
    void HandlePacket(const FString& Packet);
    void Retry(const FString& Reason);
    void CreateWidgetIfNeeded();

    FString CharacterId;
    FString SessionToken;
    FString Endpoint;
    TSharedPtr<IWebSocket> Socket;
    TWeakObjectPtr<UUserWidget> Widget;
    uint64 Generation = 0;
    double NextAttemptAt = 0;
    float RetrySeconds = 1.f;
    bool bConnecting = false;
    bool bAuthenticated = false;
};
