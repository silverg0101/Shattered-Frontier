#include "Audio/SFSettlementMusic.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Sound/SoundWave.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSFSettlementMusic, Log, All);

bool USFSettlementMusicSettings::Validate(FString& Error) const
{
    auto In = [](float V, float Lo, float Hi) { return FMath::IsFinite(V) && V >= Lo && V <= Hi; };
    if (!In(Volume, 0, 1) || !In(FadeInSeconds, 0, 20) || !In(FadeOutSeconds, 0, 20) ||
        !In(MinSilenceSeconds, 0, 120) || !In(MaxSilenceSeconds, MinSilenceSeconds, 120))
    { Error = TEXT("Music volume, fades or loop gap exceeds the shipped bounds."); return false; }
    TSet<FString> Seen;
    for (const auto& Track : Tracks)
    {
        if (!Track.ZoneId.StartsWith(TEXT("zone/")) || Track.Sound.IsNull() || Seen.Contains(Track.ZoneId))
        { Error = TEXT("Music track mapping has an invalid or duplicate zone ID or missing SoundWave."); return false; }
        Seen.Add(Track.ZoneId);
    }
    for (const auto& Area : Areas)
    {
        if (Area.MapName.IsEmpty() || !Seen.Contains(Area.ZoneId) || !In(Area.RadiusMetres, 1, 2000) ||
            !FMath::IsFinite(Area.CentreMetres.X) || !FMath::IsFinite(Area.CentreMetres.Y))
        { Error = TEXT("Music area has an invalid map, zone, centre or radius."); return false; }
    }
    Error.Reset(); return true;
}

FString FSFSettlementMusicSelection::At(const TArray<FSFSettlementMusicArea>& Areas, const FString& Map, const FVector2D& Metres)
{
    for (const auto& Area : Areas)
    {
        if (Map.EndsWith(Area.MapName) && FVector2D::Distance(Metres, Area.CentreMetres) <= Area.RadiusMetres)
        { return Area.ZoneId; }
    }
    return FString();
}

FSFSettlementMusicSelection::FTransition FSFSettlementMusicSelection::Select(const FString& DesiredZoneId, bool bHasTrack)
{
    const FString Next = bHasTrack ? DesiredZoneId : FString();
    if (Next == ZoneId) { return {}; }
    FTransition Result{true, !ZoneId.IsEmpty(), !Next.IsEmpty()};
    ZoneId = Next;
    return Result;
}

void USFSettlementMusicSubsystem::Tick(float)
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC || !PC->IsLocalController() || !PC->GetPawn() || World->GetNetMode() == NM_DedicatedServer) { return; }
    const auto* Settings = GetDefault<USFSettlementMusicSettings>();
    FString Error;
    if (!Settings->Validate(Error))
    {
        if (!bInvalidLogged) { UE_LOG(LogSFSettlementMusic, Error, TEXT("SETTLEMENT_MUSIC_INVALID_CONFIG %s"), *Error); bInvalidLogged = true; }
        return;
    }
    const FVector Position = PC->GetPawn()->GetActorLocation() / 100.f;
    const FString Zone = FSFSettlementMusicSelection::At(Settings->Areas, World->GetMapName(), FVector2D(Position.X, Position.Y));
    const FSFSettlementMusicTrack* Track = Settings->Tracks.FindByPredicate([&Zone](const auto& Item) { return Item.ZoneId == Zone; });
    const auto Transition = Selection.Select(Zone, Track != nullptr);
    if (!Transition.bChanged) { return; }
    World->GetTimerManager().ClearTimer(LoopTimer);
    if (Transition.bFadeOut && Active)
    {
        Active->OnAudioFinished.RemoveDynamic(this, &USFSettlementMusicSubsystem::OnTrackFinished);
        Active->bAutoDestroy = true;
        Active->FadeOut(Settings->FadeOutSeconds, 0.f);
        Active = nullptr; ActiveWave = nullptr;
    }
    if (Transition.bFadeIn && Track) { StartTrack(*Track); }
}

void USFSettlementMusicSubsystem::StartTrack(const FSFSettlementMusicTrack& Track)
{
    UWorld* World = GetWorld();
    ActiveWave = Cast<USoundWave>(Track.Sound.TryLoad());
    if (!World || !ActiveWave) { UE_LOG(LogSFSettlementMusic, Warning, TEXT("SETTLEMENT_MUSIC_MISSING %s"), *Track.ZoneId); return; }
    Active = NewObject<UAudioComponent>(World);
    Active->bAutoDestroy = false;
    Active->bIsUISound = true;
    Active->SetSound(ActiveWave);
    Active->RegisterComponentWithWorld(World);
    Active->OnAudioFinished.AddDynamic(this, &USFSettlementMusicSubsystem::OnTrackFinished);
    const auto* Settings = GetDefault<USFSettlementMusicSettings>();
    Active->FadeIn(Settings->FadeInSeconds, Settings->Volume);
    UE_LOG(LogSFSettlementMusic, Display, TEXT("SETTLEMENT_MUSIC_START Zone=%s Track=%s"), *Track.ZoneId, *ActiveWave->GetName());
}

void USFSettlementMusicSubsystem::OnTrackFinished()
{
    if (!Active || Selection.ZoneId.IsEmpty()) { return; }
    Active->OnAudioFinished.RemoveDynamic(this, &USFSettlementMusicSubsystem::OnTrackFinished);
    Active->DestroyComponent(); Active = nullptr; ActiveWave = nullptr;
    const auto* Settings = GetDefault<USFSettlementMusicSettings>();
    const float Gap = Settings->bSilenceBetweenLoops ? FMath::FRandRange(Settings->MinSilenceSeconds, Settings->MaxSilenceSeconds) : 0.f;
    GetWorld()->GetTimerManager().SetTimer(LoopTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        const auto* Config = GetDefault<USFSettlementMusicSettings>();
        const auto* Track = Config->Tracks.FindByPredicate([this](const auto& Item) { return Item.ZoneId == Selection.ZoneId; });
        if (Track) { StartTrack(*Track); }
    }), FMath::Max(Gap, KINDA_SMALL_NUMBER), false);
}

void USFSettlementMusicSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(LoopTimer); }
    if (Active) { Active->Stop(); Active->DestroyComponent(); Active = nullptr; }
    ActiveWave = nullptr;
    Super::Deinitialize();
}
