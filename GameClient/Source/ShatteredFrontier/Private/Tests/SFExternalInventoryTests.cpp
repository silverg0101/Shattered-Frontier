#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Framework/SFPlayerController.h"
#include "Gathering/SFGatheringPilot.h"
#include "UI/SFInventoryWidget.h"
#include "UI/SFWindowStack.h"
#include "Exchange/SFExchangeWidgets.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExternalInventoryCouplingTest, "ShatteredFrontier.Inventory.ExternalCoupling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFExternalInventoryCouplingTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    UWorld* World = Fixture.GetTestWorld(); World->GetWorldSettings()->NotifyBeginPlay();
    auto* Player = World->SpawnActor<ASFPlayerController>();
    auto* Local = NewObject<ULocalPlayer>(GEngine); Player->Player = Local; Local->PlayerController = Player;
    auto* Pilot = Player->FindComponentByClass<USFGatheringPilotComponent>();
    TestFalse(TEXT("Fixture deliberately has no admitted gathering feature"), Pilot && Pilot->IsEnabled());
    auto* Stack = Player->GetWindowStack();
    Player->OpenExternalInventoryPanel(nullptr);
    TestEqual(TEXT("Missing target does not open windows"), Stack->Num(), 0);
    const TArray<UClass*> Classes{
        LoadClass<USFInventoryWidget>(nullptr, USFInventoryWidget::AssetPath),
        LoadClass<USFTradeWindow>(nullptr, USFTradeWindow::AssetPath),
        LoadClass<USFMarketBoard>(nullptr, USFMarketBoard::AssetPath)};
    for (UClass* Class : Classes)
    {
        if (!TestNotNull(TEXT("Supported existing target WBP class"), Class)) continue;
        UUserWidget* Target = CreateWidget<UUserWidget>(Player, Class);
        if (!TestNotNull(TEXT("Supported existing target constructs"), Target)) continue;
        Player->OpenExternalInventoryPanel(Target);
        UUserWidget* Own = nullptr;
        for (UUserWidget* Window : Stack->GetOrder()) if (Window != Target && Cast<USFInventoryWidget>(Window)) Own = Window;
        TestNotNull(TEXT("Own bag opens independently of the gathering gate"), Own);
        TestTrue(TEXT("Admitted target remains open"), Stack->IsOpen(Target));
        TestEqual(TEXT("One own bag plus one external window"), Stack->Num(), 2);
        Stack->Close(Own);
        Player->OpenExternalInventoryPanel(Target);
        TestTrue(TEXT("Opening the external target restores a manually closed own bag"), Stack->IsOpen(Own));
        TestEqual(TEXT("Repeated open creates no duplicate bag or target"), Stack->Num(), 2);
        Stack->CloseAll();
    }
    Player->Player = nullptr; Local->PlayerController = nullptr;
    return true;
}
#endif
