#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Framework/SFGameUserSettings.h"

namespace
{
const ESFGraphicsPreset AllPresets[5] = {ESFGraphicsPreset::Low, ESFGraphicsPreset::Medium, ESFGraphicsPreset::High, ESFGraphicsPreset::Epic, ESFGraphicsPreset::Ultra};

// Each lambda changes exactly one quality option by one step.
TArray<TFunction<void(FSFGraphicsOptions&)>> SingleEdits()
{
    return {
        [](FSFGraphicsOptions& O) { O.ViewDistance = (O.ViewDistance + 1) % 5; }, [](FSFGraphicsOptions& O) { O.Shadows = (O.Shadows + 1) % 5; },
        [](FSFGraphicsOptions& O) { O.GlobalIllumination = (O.GlobalIllumination + 1) % 5; }, [](FSFGraphicsOptions& O) { O.Reflections = (O.Reflections + 1) % 5; },
        [](FSFGraphicsOptions& O) { O.Textures = (O.Textures + 1) % 5; }, [](FSFGraphicsOptions& O) { O.Effects = (O.Effects + 1) % 5; },
        [](FSFGraphicsOptions& O) { O.Foliage = (O.Foliage + 1) % 5; }, [](FSFGraphicsOptions& O) { O.PostProcessing = (O.PostProcessing + 1) % 5; },
        [](FSFGraphicsOptions& O) { O.AntiAliasing = (O.AntiAliasing + 1) % 5; }, [](FSFGraphicsOptions& O) { O.NaniteTessellation = (O.NaniteTessellation + 1) % 5; },
        [](FSFGraphicsOptions& O) { O.RenderScale = O.RenderScale == 100 ? 99 : O.RenderScale + 1; }, [](FSFGraphicsOptions& O) { O.FrameRateLimit += 1; },
        [](FSFGraphicsOptions& O) { O.bVSync = !O.bVSync; }, [](FSFGraphicsOptions& O) { O.bMotionBlur = !O.bMotionBlur; }};
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGraphicsPresetApplyTest, "ShatteredFrontier.Graphics.Presets.Apply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFGraphicsPresetApplyTest::RunTest(const FString&)
{
    const USFGraphicsPresetSettings* Presets = GetDefault<USFGraphicsPresetSettings>();
    USFGameUserSettings* Settings = NewObject<USFGameUserSettings>();
    FSFGraphicsOptions Previous;
    for (const ESFGraphicsPreset Preset : AllPresets)
    {
        const FSFGraphicsOptions Row = Presets->PresetOptions(Preset);
        Settings->StoreGraphics(Presets->WithPreset(Settings->CaptureGraphics(), Preset));
        const FSFGraphicsOptions Live = Settings->CaptureGraphics();
        TestTrue(FString::Printf(TEXT("%s: every option equals its row"), USFGraphicsPresetSettings::PresetName(Preset)), Live.SameQuality(Row));
        TestEqual(FString::Printf(TEXT("%s: label"), USFGraphicsPresetSettings::PresetName(Preset)), static_cast<int32>(Settings->GetGraphicsPreset()), static_cast<int32>(Preset));
        TestEqual(TEXT("level matches the preset index"), Live.ViewDistance, static_cast<int32>(Preset));
        if (Preset != ESFGraphicsPreset::Low) { TestTrue(TEXT("presets differ"), !Live.SameQuality(Previous)); }
        Previous = Live;
    }
    // Presets never touch the display choice.
    FSFGraphicsOptions Current = Settings->CaptureGraphics();
    Current.Resolution = FIntPoint(2560, 1440); Current.WindowMode = EWindowMode::Windowed;
    const FSFGraphicsOptions After = Presets->WithPreset(Current, ESFGraphicsPreset::Low);
    TestEqual(TEXT("resolution kept"), After.Resolution, FIntPoint(2560, 1440));
    TestEqual(TEXT("window mode kept"), static_cast<int32>(After.WindowMode), static_cast<int32>(EWindowMode::Windowed));
    TestEqual(TEXT("Low turns tessellation off"), Presets->DicingRateForLevel(Presets->PresetOptions(ESFGraphicsPreset::Low).NaniteTessellation), 0.f);
    TestTrue(TEXT("Ultra tessellation is finer than High"), Presets->DicingRateForLevel(4) < Presets->DicingRateForLevel(2));

    // The console variables follow the stored options; restore them afterwards.
    IConsoleVariable* Tess = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Nanite.Tessellation"));
    IConsoleVariable* Blur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.MotionBlur"));
    if (TestNotNull(TEXT("r.Nanite.Tessellation exists"), Tess) && TestNotNull(TEXT("r.DefaultFeature.MotionBlur exists"), Blur))
    {
        const int32 OldTess = Tess->GetInt(), OldBlur = Blur->GetInt();
        Settings->NaniteTessellationLevel = 0; Settings->bMotionBlur = false; Settings->ApplyGraphicsCVars();
        TestEqual(TEXT("level 0 disables tessellation"), Tess->GetInt(), 0);
        TestEqual(TEXT("blur off"), Blur->GetInt(), 0);
        Settings->NaniteTessellationLevel = 3; Settings->bMotionBlur = true; Settings->ApplyGraphicsCVars();
        TestEqual(TEXT("level 3 enables tessellation"), Tess->GetInt(), 1);
        TestEqual(TEXT("blur on"), Blur->GetInt(), 1);
        Tess->Set(OldTess, ECVF_SetByCode); Blur->Set(OldBlur, ECVF_SetByCode);
    }

    // Out-of-range declarations are clamped.
    FSFGraphicsOptions Wild; Wild.ViewDistance = 99; Wild.RenderScale = 5; Wild.FrameRateLimit = -4;
    const FSFGraphicsOptions Tame = Wild.Clamped();
    TestEqual(TEXT("level clamps"), Tame.ViewDistance, 4); TestEqual(TEXT("scale clamps"), Tame.RenderScale, 67); TestEqual(TEXT("cap clamps"), Tame.FrameRateLimit, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGraphicsCustomFlagTest, "ShatteredFrontier.Graphics.Presets.Custom", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFGraphicsCustomFlagTest::RunTest(const FString&)
{
    const USFGraphicsPresetSettings* Presets = GetDefault<USFGraphicsPresetSettings>();
    for (const ESFGraphicsPreset Preset : AllPresets)
    {
        int32 Index = 0;
        for (const TFunction<void(FSFGraphicsOptions&)>& Edit : SingleEdits())
        {
            FSFGraphicsEditSession Session(Presets->PresetOptions(Preset), [](const FSFGraphicsOptions&) {}, 15.f);
            TestEqual(TEXT("starts as its preset"), static_cast<int32>(Session.PendingPreset()), static_cast<int32>(Preset));
            Edit(Session.EditPending());
            TestEqual(FString::Printf(TEXT("%s with edit %d reads Custom"), USFGraphicsPresetSettings::PresetName(Preset), Index), static_cast<int32>(Session.PendingPreset()), static_cast<int32>(ESFGraphicsPreset::Custom));
            Session.RevertPending();
            TestEqual(TEXT("revert restores the label"), static_cast<int32>(Session.PendingPreset()), static_cast<int32>(Preset));
            ++Index;
        }
    }
    // Resolution and window mode are display choices and never make a preset Custom.
    FSFGraphicsEditSession Session(Presets->PresetOptions(ESFGraphicsPreset::High), [](const FSFGraphicsOptions&) {}, 15.f);
    Session.EditPending().Resolution = FIntPoint(1280, 720); Session.EditPending().WindowMode = EWindowMode::Windowed;
    TestEqual(TEXT("display change keeps High"), static_cast<int32>(Session.PendingPreset()), static_cast<int32>(ESFGraphicsPreset::High));
    Session.SelectPreset(ESFGraphicsPreset::Ultra);
    TestEqual(TEXT("selecting a preset labels it"), static_cast<int32>(Session.PendingPreset()), static_cast<int32>(ESFGraphicsPreset::Ultra));
    TestEqual(TEXT("selecting a preset keeps the resolution"), Session.Pending().Resolution, FIntPoint(1280, 720));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGraphicsPersistenceTest, "ShatteredFrontier.Graphics.Presets.Persistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFGraphicsPersistenceTest::RunTest(const FString&)
{
    const USFGraphicsPresetSettings* Presets = GetDefault<USFGraphicsPresetSettings>();
    const FString Ini = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("SFGraphicsPersistenceTest.ini"));
    IFileManager::Get().Delete(*Ini);

    USFGameUserSettings* Writer = NewObject<USFGameUserSettings>();
    FSFGraphicsOptions Chosen = Presets->PresetOptions(ESFGraphicsPreset::Epic);
    Chosen.bVSync = true; Chosen.FrameRateLimit = 144; Chosen.RenderScale = 91; Chosen.NaniteTessellation = 1; Chosen.bMotionBlur = true;
    Chosen.Resolution = FIntPoint(2560, 1440); Chosen.WindowMode = EWindowMode::Fullscreen;
    Writer->StoreGraphics(Chosen);
    TestTrue(TEXT("the same options read back"), Writer->CaptureGraphics() == Chosen);
    TestEqual(TEXT("one changed option reads Custom"), static_cast<int32>(Writer->GetGraphicsPreset()), static_cast<int32>(ESFGraphicsPreset::Custom));

    // The options this class owns survive a save and reload of its ini section; the rest persist in the engine's own settings.
    Writer->SaveConfig(CPF_Config, *Ini);
    USFGameUserSettings* Reader = NewObject<USFGameUserSettings>();
    Reader->NaniteTessellationLevel = 4; Reader->bMotionBlur = false;
    Reader->LoadConfig(USFGameUserSettings::StaticClass(), *Ini);
    TestEqual(TEXT("tessellation level persists"), Reader->NaniteTessellationLevel, 1);
    TestTrue(TEXT("motion blur persists"), Reader->bMotionBlur);
    IFileManager::Get().Delete(*Ini);

    // The shipped DefaultGame.ini declares every preset inside its bounds.
    for (const ESFGraphicsPreset Preset : AllPresets)
    {
        const FSFGraphicsOptions Row = Presets->PresetOptions(Preset);
        TestTrue(FString::Printf(TEXT("%s is inside its bounds"), USFGraphicsPresetSettings::PresetName(Preset)), Row == Row.Clamped());
    }
    TestEqual(TEXT("keep timer is 15 s"), Presets->KeepSecondsClamped(), 15.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGraphicsKeepRevertTest, "ShatteredFrontier.Graphics.Presets.KeepRevert", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFGraphicsKeepRevertTest::RunTest(const FString&)
{
    const USFGraphicsPresetSettings* Presets = GetDefault<USFGraphicsPresetSettings>();
    TArray<FSFGraphicsOptions> Applied;
    FSFGraphicsEditSession Session(Presets->PresetOptions(ESFGraphicsPreset::High), [&Applied](const FSFGraphicsOptions& O) { Applied.Add(O); }, 15.f);
    const FSFGraphicsOptions Original = Session.Applied();

    // A quality change applies at once with no countdown.
    Session.EditPending().Shadows = 4;
    TestFalse(TEXT("quality change starts no countdown"), Session.Apply());
    TestFalse(TEXT("not awaiting"), Session.IsAwaitingKeep());
    TestEqual(TEXT("applied once"), Applied.Num(), 1);

    // A resolution change waits 15 s, then puts the previous display back.
    Session.EditPending().Resolution = FIntPoint(3840, 2160);
    TestTrue(TEXT("display change starts the countdown"), Session.Apply());
    TestTrue(TEXT("awaiting keep"), Session.IsAwaitingKeep());
    Session.Tick(14.9f);
    TestTrue(TEXT("still waiting at 14.9 s"), Session.IsAwaitingKeep());
    Session.Tick(0.2f);
    TestFalse(TEXT("reverted at 15 s"), Session.IsAwaitingKeep());
    TestEqual(TEXT("previous resolution restored"), Session.Applied().Resolution, Original.Resolution);
    TestEqual(TEXT("last applied is the restore"), Applied.Last().Resolution, Original.Resolution);

    // Keep commits it.
    Session.EditPending().WindowMode = EWindowMode::Windowed;
    TestTrue(TEXT("window mode change starts the countdown"), Session.Apply());
    Session.Keep();
    Session.Tick(30.f);
    TestEqual(TEXT("kept after the timer"), static_cast<int32>(Session.Applied().WindowMode), static_cast<int32>(EWindowMode::Windowed));

    // Revert drops unapplied edits.
    Session.EditPending().bMotionBlur = !Session.Applied().bMotionBlur;
    TestTrue(TEXT("dirty"), Session.IsDirty());
    Session.RevertPending();
    TestFalse(TEXT("clean after revert"), Session.IsDirty());

    // Auto-detect maps the engine benchmark to a preset, never above the declared cap.
    TestEqual(TEXT("level 0 is Low"), static_cast<int32>(Presets->PresetForBenchmark(0)), static_cast<int32>(ESFGraphicsPreset::Low));
    TestEqual(TEXT("level 2 is High"), static_cast<int32>(Presets->PresetForBenchmark(2)), static_cast<int32>(ESFGraphicsPreset::High));
    TestEqual(TEXT("level 4 is capped at Epic"), static_cast<int32>(Presets->PresetForBenchmark(4)), static_cast<int32>(ESFGraphicsPreset::Epic));
    TestEqual(TEXT("no result is Low"), static_cast<int32>(Presets->PresetForBenchmark(-1)), static_cast<int32>(ESFGraphicsPreset::Low));
    return true;
}

#endif
