#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Framework/SFGameUserSettings.h"
#include "Framework/SFGraphicsMenu.h"
#include "Characters/SFCombatFramingLogic.h"
#include "Characters/SFE1Character.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/TextBlock.h"
#include "Components/PanelWidget.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPlayerControlPersistenceTest, "ShatteredFrontier.Camera.Controls.PersistenceAndInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPlayerControlPersistenceTest::RunTest(const FString&)
{
    FSFPlayerControlOptions Options;
    TestEqual(TEXT("New player default doubles former mouse response"), Options.MouseDegreesPerCount(.25f), FVector2D(.5, .5));
    Options.MouseHorizontalPercent = 800; Options.MouseVerticalPercent = 25; Options.bInvertMouseY = true;
    TestEqual(TEXT("Axes are independent and inversion is mouse-only"), Options.MouseDegreesPerCount(.25f), FVector2D(2, -.0625));
    // Equal raw counts give equal rotation whether split across 30 or 120 frames: mouse is displacement, not a rate.
    const auto Gain = Options.MouseDegreesPerCount(.25f);
    TestEqual(TEXT("Raw mouse displacement is frame-rate independent"), Gain.X * (120.0 / 30) * 30, Gain.X * (120.0 / 120) * 120);
    Options.MouseHorizontalPercent = -1; Options.MouseVerticalPercent = MAX_int32;
    TestEqual(TEXT("Low config clamps safely"), Options.Clamped().MouseHorizontalPercent, FSFPlayerControlOptions::MinMousePercent);
    TestEqual(TEXT("High config clamps safely"), Options.Clamped().MouseVerticalPercent, FSFPlayerControlOptions::MaxMousePercent);
    Options.MouseHorizontalPercent = 350; Options.MouseVerticalPercent = 150;
    Options.bCombatFraming = false; Options.bCombatDepthOfField = false; Options.Shoulder = ESFCombatShoulderSide::Left;
    const FString Ini = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("SFPlayerControlsTest.ini"));
    auto* Writer = NewObject<USFGameUserSettings>(); Writer->StoreControls(Options); Writer->SaveConfig(CPF_Config, *Ini);
    auto* Reader = NewObject<USFGameUserSettings>(); Reader->LoadConfig(USFGameUserSettings::StaticClass(), *Ini);
    TestTrue(TEXT("Every personal control survives local config reload"), Reader->CaptureControls() == Options);
    IFileManager::Get().Delete(*Ini);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPlayerControlsMenuTest, "ShatteredFrontier.Camera.Controls.SettingsApplyReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPlayerControlsMenuTest::RunTest(const FString&)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const auto Class = LoadClass<USFGraphicsMenu>(nullptr, USFGraphicsMenu::AssetPath);
    auto* Menu = Class && World ? CreateWidget<USFGraphicsMenu>(World, Class) : nullptr;
    auto* Live = GEngine ? Cast<USFGameUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
    if (!TestNotNull(TEXT("Existing owner WBP constructs"), Menu) || !TestNotNull(TEXT("Local preferences exist"), Live)) return false;
    const auto Original = Live->CaptureControls();
    Menu->HandleTab(2);
    TestEqual(TEXT("Controls uses seven canonical rows"), Menu->ControlRows.Num(), 7);
    TestNotEqual(TEXT("Existing row host shows Controls"), Menu->GameplayRows->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("Gameplay row hides on Controls"), Menu->InterfaceRows[0]->GetVisibility(), ESlateVisibility::Collapsed);
    Menu->PendingControls.MouseHorizontalPercent = Original.MouseHorizontalPercent == 400 ? 300 : 400;
    TestTrue(TEXT("Pending edit does not change runtime input"), Live->CaptureControls() == Original);
    Menu->DoApply();
    TestEqual(TEXT("Apply immediately updates camera input source"), Live->CaptureControls().MouseHorizontalPercent, Menu->PendingControls.MouseHorizontalPercent);
    Menu->DoRevert();
    TestTrue(TEXT("Reset stages declared defaults"), Menu->PendingControls == FSFPlayerControlOptions());
    TestTrue(TEXT("Reset waits for Apply"), Live->CaptureControls() == Menu->AppliedControls);
    Menu->PendingControls.MouseVerticalPercent = 775;
    Menu->Reload();
    TestTrue(TEXT("Reopening discards unapplied changes"), Menu->PendingControls == Live->CaptureControls());
    Menu->HandleTab(3);
    TestEqual(TEXT("Controls hides on Gameplay"), Menu->ControlRows[0]->GetVisibility(), ESlateVisibility::Collapsed);
    TestNotEqual(TEXT("Original Gameplay row is retained"), Menu->InterfaceRows[0]->GetVisibility(), ESlateVisibility::Collapsed);
    Live->ApplyControls(Original);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFCombatViewpointAimTest, "ShatteredFrontier.Camera.CombatFraming.RenderedViewpoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFCombatViewpointAimTest::RunTest(const FString&)
{
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::GameRPC) || !Fixture.BeginPlayInTestWorld()) return false;
    auto* Hero = Fixture.GetTestWorld()->SpawnActor<ASFE1Character>();
    if (!TestNotNull(TEXT("Real player camera components spawn"), Hero)) return false;
    auto* Arm = Hero->CameraBoom.Get();
    Arm->bUsePawnControlRotation = false; Arm->SetUsingAbsoluteRotation(true);
    Arm->bDoCollisionTest = false; Arm->bEnableCameraLag = false;
    Arm->SetWorldRotation(FRotator::ZeroRotator); Arm->TargetArmLength = 350;
    Arm->SocketOffset = FVector(0, 55, -15); Arm->TickComponent(.016f, LEVELTICK_All, nullptr);
    const FVector Pivot = Arm->GetComponentLocation(), Camera = Hero->FollowCamera->GetComponentLocation();
    const FVector Target = Pivot + FVector(280, 0, -88);
    TestTrue(TEXT("Runtime viewpoint includes nonzero arm and shoulder"), (Camera - Pivot).Equals(FVector(-350, 55, -15), .01));
    const auto Aim = SFCombatFramingLogic::TargetAim(Camera, Target);
    TestTrue(TEXT("Rendered ray points at target with a shoulder offset"), Aim.Vector().Equals((Target - Camera).GetSafeNormal(), .00001));
    TestTrue(TEXT("Elevated pivot was pitching farther into ground"), (Target - Pivot).Rotation().Pitch < Aim.Pitch - 5);
    const auto Near = SFCombatFramingLogic::TargetAim(Camera, Pivot + FVector(20, 0, -88));
    TestTrue(TEXT("Very close small target does not pitch straight down"), Near.Pitch > -20);
    const auto Moved = SFCombatFramingLogic::TargetAim(Camera, Target + FVector(0, 180, 0));
    TestTrue(TEXT("Moving target changes view aim"), Moved.Yaw > Aim.Yaw);
    TestEqual(TEXT("Stationary target has no angle drift"), SFCombatFramingLogic::TargetAim(Camera, Target), Aim);
    Arm->TargetArmLength = 1200; Arm->TickComponent(.016f, LEVELTICK_All, nullptr);
    const auto FarZoom = SFCombatFramingLogic::TargetAim(Hero->FollowCamera->GetComponentLocation(), Target);
    TestTrue(TEXT("Zoomed-out angle remains above steep ground view"), FarZoom.Pitch > Near.Pitch);
    TestFalse(TEXT("Zoom-out still releases assist"), SFCombatFramingLogic::ShouldEngage(true, true, true, false, 1200, 450));
    TestFalse(TEXT("First-person still releases assist"), SFCombatFramingLogic::ShouldEngage(true, true, true, true, 60, 450));
    TestEqual(TEXT("Manual camera remains untouched at zero assist blend"), SFCombatFramingLogic::AssistAxis(15, Aim.Yaw, 8, 2.5f, 0, .016f), 15.f);
    return true;
}
#endif
