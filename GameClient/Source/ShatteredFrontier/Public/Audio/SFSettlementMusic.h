#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFSettlementMusic.generated.h"

class UAudioComponent;
class USoundWave;

USTRUCT()
struct FSFSettlementMusicTrack
{
    GENERATED_BODY()
    UPROPERTY(Config, EditAnywhere) FString ZoneId;
    UPROPERTY(Config, EditAnywhere) FSoftObjectPath Sound;
};

USTRUCT()
struct FSFSettlementMusicArea
{
    GENERATED_BODY()
    UPROPERTY(Config, EditAnywhere) FString MapName;
    UPROPERTY(Config, EditAnywhere) FString ZoneId;
    UPROPERTY(Config, EditAnywhere) FVector2D CentreMetres = FVector2D::ZeroVector;
    UPROPERTY(Config, EditAnywhere, meta=(ClampMin="1", ClampMax="2000", Units="m")) float RadiusMetres = 400.f;
};

// Shipped release-author configuration; read on world launch. No remote or console override.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Settlement Music (Release Authors)"))
class SHATTEREDFRONTIER_API USFSettlementMusicSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Tracks", meta=(ToolTip="Stable zone IDs from the world zone definition and their imported SoundWaves. Next world launch; release authors only."))
    TArray<FSFSettlementMusicTrack> Tracks;
    UPROPERTY(Config, EditAnywhere, Category="Areas", meta=(ToolTip="Built settlement footprints in map-local metres. Next world launch; release authors only."))
    TArray<FSFSettlementMusicArea> Areas;
    UPROPERTY(Config, EditAnywhere, Category="Playback", meta=(ClampMin="0", ClampMax="1", ToolTip="Linear music volume, default 0.6. Next world launch; release authors only."))
    float Volume = 0.6f;
    UPROPERTY(Config, EditAnywhere, Category="Playback", meta=(ClampMin="0", ClampMax="20", Units="s", ToolTip="Fade in time, default 4 seconds. Next world launch; release authors only."))
    float FadeInSeconds = 4.f;
    UPROPERTY(Config, EditAnywhere, Category="Playback", meta=(ClampMin="0", ClampMax="20", Units="s", ToolTip="Fade out time, default 6 seconds. Next world launch; release authors only."))
    float FadeOutSeconds = 6.f;
    UPROPERTY(Config, EditAnywhere, Category="Playback", meta=(ToolTip="Insert a random quiet gap between loops. Default enabled; next world launch; release authors only."))
    bool bSilenceBetweenLoops = true;
    UPROPERTY(Config, EditAnywhere, Category="Playback", meta=(ClampMin="0", ClampMax="120", Units="s", ToolTip="Shortest loop gap, default 20 seconds. Next world launch; release authors only."))
    float MinSilenceSeconds = 20.f;
    UPROPERTY(Config, EditAnywhere, Category="Playback", meta=(ClampMin="0", ClampMax="120", Units="s", ToolTip="Longest loop gap, default 60 seconds. Next world launch; release authors only."))
    float MaxSilenceSeconds = 60.f;
    bool Validate(FString& Error) const;
};

// Device-free transition decision, shared by runtime and automation test.
struct SHATTEREDFRONTIER_API FSFSettlementMusicSelection
{
    FString ZoneId;
    struct FTransition { bool bChanged = false; bool bFadeOut = false; bool bFadeIn = false; };
    FTransition Select(const FString& DesiredZoneId, bool bHasTrack);
    static FString At(const TArray<FSFSettlementMusicArea>& Areas, const FString& Map, const FVector2D& Metres);
};

UCLASS()
class SHATTEREDFRONTIER_API USFSettlementMusicSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USFSettlementMusicSubsystem, STATGROUP_Tickables); }
    virtual void Deinitialize() override;
protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type Type) const override { return Type == EWorldType::Game || Type == EWorldType::PIE; }
private:
    UFUNCTION() void OnTrackFinished();
    void StartTrack(const FSFSettlementMusicTrack& Track);
    FSFSettlementMusicSelection Selection;
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Active;
    UPROPERTY(Transient) TObjectPtr<USoundWave> ActiveWave;
    FTimerHandle LoopTimer;
    bool bInvalidLogged = false;
};
