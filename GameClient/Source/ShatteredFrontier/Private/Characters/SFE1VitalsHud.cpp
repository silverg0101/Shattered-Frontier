#include "Characters/SFE1VitalsHud.h"

#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Framework/SFPlayerController.h"
#include "Framework/SFScreenArt.h"
#include "UI/SFHudWidgets.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void FSFE1VitalsModel::Observe(int32 InHealth, double Now)
{
    if (InHealth > 0) { bActive = true; DefeatedAt = -1; }
    else if (bActive && Health > 0) { DefeatedAt = Now; }
    Health = InHealth;
}

float FSFE1VitalsModel::HealthFraction(int32 InHealth, int32 MaxHealth)
{
    return MaxHealth > 0 ? FMath::Clamp(float(InHealth) / float(MaxHealth), 0.f, 1.f) : 0.f;
}

float FSFE1VitalsModel::RemainingSeconds(double DefeatedAt, double Now, float DelaySeconds)
{
    return DefeatedAt < 0 ? 0.f : FMath::Max(0.f, float(DefeatedAt + DelaySeconds - Now));
}

FString FSFE1VitalsModel::Countdown(float RemainingSeconds)
{
    const int32 Whole = FMath::CeilToInt(FMath::Max(0.f, RemainingSeconds));
    return FString::Printf(TEXT("%02d:%02d"), Whole / 60, Whole % 60);
}

FString FSFE1VitalsModel::RespawnTitle(const FString& ZoneName, const FString& PointId)
{
    TArray<FString> Words;
    PointId.ParseIntoArray(Words, TEXT("_"));
    for (FString& Word : Words) { Word = Word.Left(1).ToUpper() + Word.Mid(1); }
    Words.Insert(ZoneName, 0);
    Words.RemoveAll([](const FString& Word) { return Word.IsEmpty(); });
    return FString::Join(Words, TEXT(" "));
}

void USFE1VitalsHud::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    if (RespawnButton) { RespawnButton->SetLabel(FText::FromString(TEXT("Respawn"))); }  // the server respawns; the button never takes input, its fill counts the delay down
    DefeatedLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void USFE1VitalsHud::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFE1VitalsHud::Present(const FSFE1VitalsModel& Model, float RemainingSeconds, const FSFE1DefeatView& Defeat)
{
    bDefeatShown = Model.IsDefeated();
    DefeatedLayer->SetVisibility(bDefeatShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bDefeatShown) { return; }
    if (CountdownText) { CountdownText->SetText(FText::FromString(RemainingSeconds > 0.f ? TEXT("Respawning in ") + FSFE1VitalsModel::Countdown(RemainingSeconds) + TEXT(" · dropped items remain at the defeat site") : FString(TEXT("Respawning")))); }
    if (CauseText) { CauseText->SetText(FText::FromString(Defeat.Cause.IsEmpty() ? FString(TEXT("You have fallen")) : TEXT("Defeated by ") + Defeat.Cause)); }
    if (PointText && !Defeat.RespawnPoint.IsEmpty()) { PointText->SetText(FText::FromString(Defeat.RespawnPoint)); }
    RespawnProgress = Defeat.DelaySeconds > 0.f ? FMath::Clamp(1.f - RemainingSeconds / Defeat.DelaySeconds, 0.f, 1.f) : 1.f;
    if (RespawnButton) { RespawnButton->SetProgress(RespawnProgress); }
}

namespace
{
struct FVitalsRespawnPoint { FString Id; FVector Location; };
struct FVitalsZone { FString Name; TArray<FVitalsRespawnPoint> Points; };

// Zone name and respawn points from the content file the server's combat pilot reads (positions in metres).
const FVitalsZone& VitalsZone()
{
    static const FVitalsZone Zone = []()
    {
        FVitalsZone Out;
        FString Source; TSharedPtr<FJsonObject> Root;
        const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/world/zones/greenlands_silverbrook.json"));
        if (!FFileHelper::LoadFileToString(Source, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Source), Root) || !Root) { return Out; }
        Root->TryGetStringField(TEXT("display_name"), Out.Name);
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Root->TryGetArrayField(TEXT("respawn_points"), Values)) { return Out; }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject> Point = Value->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
            if (Point && Point->TryGetArrayField(TEXT("position"), Position) && Position->Num() >= 2)
            { Out.Points.Add({Point->GetStringField(TEXT("point_id")), FVector((*Position)[0]->AsNumber() * 100.0, (*Position)[1]->AsNumber() * 100.0, 0.0)}); }
        }
        return Out;
    }();
    return Zone;
}
}

void USFE1VitalsHudSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    const ASFE1Character* Pawn = PC && PC->IsLocalController() ? Cast<ASFE1Character>(PC->GetPawn()) : nullptr;
    if (!Pawn) { return; }
    const double Now = World->GetTimeSeconds();
    if (bStagedDefeat) { if (Model.DefeatedAt < 0) { Model.bActive = true; Model.Health = 0; Model.DefeatedAt = Now; } }
    else { Model.Observe(Pawn->CombatHealth(), Now); }
    // Always on for the local E1 pawn (the master HUD's vitals). Before combat health first replicates (no combat pilot
    // here), nothing can have hurt the player, so the bar reads full.
    if (!Hud)
    {
        const TSubclassOf<USFE1VitalsHud> DefeatedClass = LoadClass<USFE1VitalsHud>(nullptr, USFE1VitalsHud::AssetPath);
        if (!DefeatedClass) { UE_LOG(LogTemp, Error, TEXT("DEFEATED_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFE1VitalsHud::AssetPath); return; }
        Hud = CreateWidget<USFE1VitalsHud>(PC, DefeatedClass);
        if (!Hud) { return; }
        Hud->AddToViewport(15);
        UE_LOG(LogTemp, Display, TEXT("DEFEATED_HUD_CREATED"));
        Hud->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    // Client reads the shipped release defaults; the server applies the same captured values.
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    if (Model.IsDefeated() && DefeatSeenAt != Model.DefeatedAt)
    {
        // Once per defeat: a boar nearby names the cause (boars are the pilot's only damage source), and the respawn
        // card names the nearest zone point by the server's own rule (2D distance to the defeat spot).
        DefeatSeenAt = Model.DefeatedAt;
        Defeat = FSFE1DefeatView();
        Defeat.DelaySeconds = Settings->PlayerRespawnDelaySeconds;
        const FVector At = Pawn->GetActorLocation();
        for (TActorIterator<ASFCombatNpc> It(World); It; ++It)
        { if (FVector::Dist2D(It->GetActorLocation(), At) < 1500.0) { Defeat.Cause = TEXT("Wild Boar"); break; } }
        const FVitalsZone& Zone = VitalsZone();
        double Best = TNumericLimits<double>::Max();
        for (const FVitalsRespawnPoint& Point : Zone.Points)
        {
            const double Distance = FVector::DistSquared2D(At, Point.Location);
            if (Distance < Best) { Best = Distance; Defeat.RespawnPoint = FSFE1VitalsModel::RespawnTitle(Zone.Name, Point.Id); }
        }
    }
    // Desaturate the scene while defeated with a local, unbound post-process volume.
    if (Model.IsDefeated() && !DefeatGrade)
    {
        FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
        DefeatGrade = World->SpawnActor<APostProcessVolume>(Params);
        if (DefeatGrade)
        {
            DefeatGrade->bUnbound = true; DefeatGrade->Priority = 100.f;
            DefeatGrade->Settings.bOverride_ColorSaturation = true; DefeatGrade->Settings.ColorSaturation = FVector4(0.3, 0.3, 0.3, 1.0);
        }
    }
    else if (!Model.IsDefeated() && DefeatGrade) { DefeatGrade->Destroy(); DefeatGrade = nullptr; }
    FSFE1VitalsModel Shown = Model;
    const ASFE1Character* Character = Cast<ASFE1Character>(PC->GetPawn());
    const int32 MaximumHealth = Character ? Character->CombatMaximumHealth() : Settings->PlayerHealth;
    if (!Model.bActive) { Shown.Health = MaximumHealth; }
    Hud->Present(Shown, FSFE1VitalsModel::RemainingSeconds(Model.DefeatedAt, Now, Settings->PlayerRespawnDelaySeconds), Defeat);
    // The bars live in WBP_Hud; stamina is drawn full (no stamina stat yet).
    if (const ASFPlayerController* SFPlayer = Cast<ASFPlayerController>(PC))
    { if (USFHudWidget* HudWidget = SFPlayer->GetHudWidget()) { HudWidget->PresentVitals(Shown.Health, MaximumHealth, Settings->PlayerStamina, Settings->PlayerStamina); } }
}

void USFE1VitalsHudSubsystem::Deinitialize()
{
    if (DefeatGrade) { DefeatGrade->Destroy(); DefeatGrade = nullptr; }
    if (Hud) { Hud->RemoveFromParent(); Hud = nullptr; }
    Super::Deinitialize();
}
