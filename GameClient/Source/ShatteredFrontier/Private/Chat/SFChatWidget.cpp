#include "Chat/SFChatWidget.h"
#include "Chat/SFChatSubsystem.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"

void USFChatWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (USFChatSubsystem* Chat = GetGameInstance() ? GetGameInstance()->GetSubsystem<USFChatSubsystem>() : nullptr)
    {
        Chat->OnMessage.AddUniqueDynamic(this, &USFChatWidget::Receive);
        Chat->OnRejected.AddUniqueDynamic(this, &USFChatWidget::Rejected);
    }
    if (MessageInput) { MessageInput->OnTextCommitted.AddUniqueDynamic(this, &USFChatWidget::Commit); }
}

void USFChatWidget::NativeDestruct()
{
    if (USFChatSubsystem* Chat = GetGameInstance() ? GetGameInstance()->GetSubsystem<USFChatSubsystem>() : nullptr)
    {
        Chat->OnMessage.RemoveDynamic(this, &USFChatWidget::Receive);
        Chat->OnRejected.RemoveDynamic(this, &USFChatWidget::Rejected);
    }
    if (MessageInput) { MessageInput->OnTextCommitted.RemoveDynamic(this, &USFChatWidget::Commit); }
    Super::NativeDestruct();
}

void USFChatWidget::Receive(const FString& Channel, const FString& Sender, const FString& Body)
{
    Append(FString::Printf(TEXT("[%s] %s: %s"), *Channel, *Sender, *Body));
}

void USFChatWidget::Rejected(const FString& Reason) { Append(FString::Printf(TEXT("Chat: %s"), *Reason)); }

void USFChatWidget::Commit(const FText& Text, ETextCommit::Type Method)
{
    if (Method != ETextCommit::OnEnter) { return; }
    USFChatSubsystem* Chat = GetGameInstance() ? GetGameInstance()->GetSubsystem<USFChatSubsystem>() : nullptr;
    if (!Chat) { return; }
    const FString Body = Text.ToString().TrimStartAndEnd();
    if (Body.IsEmpty()) { return; }
    const bool bSent = WhisperTarget.IsEmpty() ? Chat->SendParty(Body) : Chat->SendWhisper(WhisperTarget, Body);
    if (bSent && MessageInput) { MessageInput->SetText(FText::GetEmpty()); }
}

void USFChatWidget::Append(const FString& Line)
{
    if (!ChatLog) { return; }
    UTextBlock* Row = NewObject<UTextBlock>(ChatLog);
    Row->SetText(FText::FromString(Line));
    ChatLog->AddChild(Row);
    while (ChatLog->GetChildrenCount() > 100) { ChatLog->RemoveChildAt(0); }
    ChatLog->ScrollToEnd();
}
