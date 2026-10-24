#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "SFChatWidget.generated.h"

class UScrollBox;
class UEditableTextBox;

// Designer owns layout. This base only binds the log and input to the game-instance chat connection.
UCLASS()
class SHATTEREDFRONTIER_API USFChatWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_Chat.WBP_Chat_C");
    UFUNCTION(BlueprintCallable, Category="Chat") void SetWhisperTarget(const FString& CharacterId) { WhisperTarget = CharacterId; }
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UScrollBox> ChatLog;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UEditableTextBox> MessageInput;
private:
    UFUNCTION() void Receive(const FString& Channel, const FString& Sender, const FString& Body);
    UFUNCTION() void Rejected(const FString& Reason);
    UFUNCTION() void Commit(const FText& Text, ETextCommit::Type Method);
    void Append(const FString& Line);
    FString WhisperTarget;
};
