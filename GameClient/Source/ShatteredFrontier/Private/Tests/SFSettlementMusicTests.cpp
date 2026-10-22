#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Audio/SFSettlementMusic.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSettlementMusicTest,
    "ShatteredFrontier.Audio.SettlementMusic.Selection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSettlementMusicTest::RunTest(const FString&)
{
    const auto* Settings = GetDefault<USFSettlementMusicSettings>();
    FString Error;
    TestTrue(TEXT("Shipped music settings validate"), Settings->Validate(Error));
    TestEqual(TEXT("Nine zone tracks"), Settings->Tracks.Num(), 9);
    TArray<FSFSettlementMusicArea> Areas;
    FSFSettlementMusicArea Silverbrook;
    Silverbrook.MapName = TEXT("Greenlands_Silverbrook");
    Silverbrook.ZoneId = TEXT("zone/greenlands_silverbrook");
    Areas.Add(Silverbrook);
    TestEqual(TEXT("Town centre selects settlement"), FSFSettlementMusicSelection::At(Areas, TEXT("Greenlands_Silverbrook"), FVector2D::ZeroVector), Silverbrook.ZoneId);
    TestTrue(TEXT("Wilderness is silent"), FSFSettlementMusicSelection::At(Areas, TEXT("Greenlands_Silverbrook"), FVector2D(401, 0)).IsEmpty());
    TestTrue(TEXT("Other map is silent"), FSFSettlementMusicSelection::At(Areas, TEXT("OtherMap"), FVector2D::ZeroVector).IsEmpty());
    FSFSettlementMusicSelection Selection;
    auto Transition = Selection.Select(Silverbrook.ZoneId, true);
    TestTrue(TEXT("Enter fades in"), Transition.bChanged && Transition.bFadeIn && !Transition.bFadeOut);
    Transition = Selection.Select(Silverbrook.ZoneId, true);
    TestFalse(TEXT("Same settlement does not restart"), Transition.bChanged);
    Transition = Selection.Select(TEXT("zone/greenlands_valemere"), true);
    TestTrue(TEXT("Settlement change crossfades"), Transition.bChanged && Transition.bFadeOut && Transition.bFadeIn);
    Transition = Selection.Select(TEXT(""), false);
    TestTrue(TEXT("Exit fades out"), Transition.bChanged && Transition.bFadeOut && !Transition.bFadeIn);
    return true;
}
#endif
