#include "Chat/SFChatWidget.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Editor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFChatWidgetTest, "ShatteredFrontier.Chat.Widget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFChatWidgetTest::RunTest(const FString&)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world"), World)) { return false; }
    const TSubclassOf<USFChatWidget> Class = LoadClass<USFChatWidget>(nullptr, USFChatWidget::AssetPath);
    if (!TestNotNull(TEXT("WBP_Chat loads"), Class.Get())) { return false; }
    USFChatWidget* Widget = CreateWidget<USFChatWidget>(World, Class);
    if (!TestNotNull(TEXT("WBP_Chat constructs"), Widget)) { return false; }
    TestNotNull(TEXT("ChatLog bound"), Cast<UScrollBox>(Widget->WidgetTree->FindWidget(TEXT("ChatLog"))));
    TestNotNull(TEXT("MessageInput bound"), Cast<UEditableTextBox>(Widget->WidgetTree->FindWidget(TEXT("MessageInput"))));
    return true;
}
#endif
