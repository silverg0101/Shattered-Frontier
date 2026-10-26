#include "Characters/SFE1VitalsHud.h"
#include "Components/Button.h"
#include "GameFramework/GameStateBase.h"

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

FString FSFE1VitalsModel::RespawnStatus(float RemainingSeconds, bool bDeadlineKnown, bool bRequestAccepted)
{
    if (bRequestAccepted) return TEXT("Respawning\u2026");
    if (!bDeadlineKnown) return TEXT("Waiting for respawn information\u2026");
    if (RemainingSeconds > 0) return FString::Printf(TEXT("Respawn available in %ds"), FMath::CeilToInt(RemainingSeconds));
    return TEXT("Ready to respawn.");
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
    if (RespawnButton)
    {
        RespawnButton->SetLabel(FText::FromString(TEXT("Respawn")));
        RespawnButton->OnClicked.AddDynamic(this, &USFE1VitalsHud::RequestRespawn);
    }
    if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &USFE1VitalsHud::CloseNotice);
    DefeatedLayer->SetVisibility(ESlateVisibility::Collapsed);
}

void USFE1VitalsHud::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFE1VitalsHud::Present(const FSFE1VitalsModel& Model, float RemainingSeconds, const FSFE1DefeatView& Defeat, bool bShowNotice)
{
    bDefeatShown = Model.IsDefeated() && bShowNotice;
    DefeatedLayer->SetVisibility(bDefeatShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!bDefeatShown) { return; }
    if (CountdownText) { CountdownText->SetText(FText::FromString(FSFE1VitalsModel::RespawnStatus(RemainingSeconds, Defeat.bDeadlineKnown, Defeat.bRequestAccepted))); }
    if (CauseText) { CauseText->SetText(FText::FromString(Defeat.Cause.IsEmpty() ? FString(TEXT("You have fallen")) : TEXT("Defeated by ") + Defeat.Cause)); }
    if (PointText && !Defeat.RespawnPoint.IsEmpty()) { PointText->SetText(FText::FromString(Defeat.RespawnPoint)); }
    RespawnProgress = Defeat.DelaySeconds > 0.f ? FMath::Clamp(1.f - RemainingSeconds / Defeat.DelaySeconds, 0.f, 1.f) : 1.f;
    if (RespawnButton)
    {
        RespawnButton->SetProgress(RespawnProgress);
        const ASFPlayerController* PC = Cast<ASFPlayerController>(GetOwningPlayer());
        RespawnButton->SetButtonEnabled(Defeat.bDeadlineKnown && RemainingSeconds <= 0 && !Defeat.bRequestAccepted &&
            (!PC || !PC->IsRespawnSubmissionPending()));
    }
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
    else
    {
        if (!Model.bActive && Pawn->CombatHealth() <= 0 && (Pawn->GetRespawnReadyAt() >= 0 || !Pawn->GetDefeatCause().IsEmpty())) { Model.bActive = true; Model.Health = 1; }
        Model.Observe(Pawn->CombatHealth(), Now);
    }
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
        Hud->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    // Client reads the shipped release defaults; the server applies the same captured values.
    const USFCombatPilotSettings* Settings = GetDefault<USFCombatPilotSettings>();
    if (Model.IsDefeated() && DefeatSeenAt != Model.DefeatedAt)
    {
        // Cause and deadline come from authority; location naming is read-only.
        DefeatSeenAt = Model.DefeatedAt;
        Defeat = FSFE1DefeatView();
        Defeat.DelaySeconds = Pawn->GetRespawnDelay();
        const FVector At = Pawn->GetActorLocation();
        Defeat.Cause = Pawn->GetDefeatCause();
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
    Defeat.Cause = bStagedDefeat ? TEXT("Wild Boar") : Pawn->GetDefeatCause();
    Defeat.DelaySeconds = bStagedDefeat ? Settings->ResAcceptSeconds : Pawn->GetRespawnDelay();
    Defeat.bDeadlineKnown = (bStagedDefeat ? StagedReadyAt : Pawn->GetRespawnReadyAt()) >= 0;
    const AGameStateBase* State = World->GetGameState();
    const double ServerNow = State ? State->GetServerWorldTimeSeconds() : Now;
    const float Remaining = Defeat.bDeadlineKnown ? FMath::Max(0.f, float((bStagedDefeat ? StagedReadyAt : Pawn->GetRespawnReadyAt()) - ServerNow)) : 0.f;
    Defeat.bRequestAccepted = bStagedDefeat ? bStagedAccepted : Pawn->IsRespawnRequestAccepted();
    Notice.Observe(Model.IsDefeated());
    Hud->SetVisibility(Notice.bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    Hud->Present(Shown, Remaining, Defeat, Notice.bOpen);
    // The bars live in WBP_Hud; stamina is drawn full (no stamina stat yet).
    if (const ASFPlayerController* SFPlayer = Cast<ASFPlayerController>(PC))
    { if (USFHudWidget* HudWidget = SFPlayer->GetHudWidget()) { HudWidget->PresentVitals(Shown.Health, MaximumHealth, Settings->PlayerStamina, Settings->PlayerStamina); HudWidget->PresentDefeated(Model.IsDefeated()); } }
}

void USFE1VitalsHudSubsystem::Deinitialize()
{
    if (DefeatGrade) { DefeatGrade->Destroy(); DefeatGrade = nullptr; }
    if (Hud) { Hud->RemoveFromParent(); Hud = nullptr; }
    Super::Deinitialize();
}

void USFE1VitalsHud::RequestRespawn(USFButtonWidget* Source)
{
    if (ASFPlayerController* PC = Cast<ASFPlayerController>(GetOwningPlayer())) PC->RequestNormalRespawn();
}
void USFE1VitalsHudSubsystem::StageDefeat(float RemainingSeconds, bool bAccepted)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bStagedDefeat = true;
    StagedReadyAt = RemainingSeconds >= 0 && GetWorld() ? GetWorld()->GetTimeSeconds() + RemainingSeconds : -1;
    bStagedAccepted = bAccepted;
#endif
}
void USFE1VitalsHudSubsystem::ClearStagedDefeat()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    bStagedDefeat = false;
    StagedReadyAt = -1; bStagedAccepted = false;
    Model = FSFE1VitalsModel();
    Notice.Observe(false);
#endif
}
void USFE1VitalsHud::CloseNotice(USFButtonWidget* Source)
{
    if (GetWorld()) if (USFE1VitalsHudSubsystem* Vitals = GetWorld()->GetSubsystem<USFE1VitalsHudSubsystem>()) Vitals->CloseNotice();
}
bool USFE1VitalsHudSubsystem::CloseNotice()
{
    if (!Notice.bOpen) return false;
    Notice.Close();
    if (Hud) Hud->SetVisibility(ESlateVisibility::Collapsed);
    return true;
}
void USFE1VitalsHudSubsystem::ReopenNotice()
{
    Notice.Reopen();
    if (Hud && Notice.bOpen) Hud->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
void USFPlayerStatusStrip::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (SkullButton) SkullButton->OnClicked.AddDynamic(this, &USFPlayerStatusStrip::ReopenNotice);
}
void USFPlayerStatusStrip::PresentDefeated(bool bDead)
{
    SetVisibility(bDead ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
void USFPlayerStatusStrip::ReopenNotice()
{
    if (GetWorld()) if (USFE1VitalsHudSubsystem* Vitals = GetWorld()->GetSubsystem<USFE1VitalsHudSubsystem>()) Vitals->ReopenNotice();
}
