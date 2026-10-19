#pragma once

#include "Components/ActorComponent.h"
#include "Engine/DeveloperSettings.h"
#include "SFHarvestImpactAudio.generated.h"

class USoundAttenuation;
class USoundConcurrency;
class USoundWave;

// Release-author controls. Values are captured when each resource component begins play;
// edits belong in the reviewed game configuration for the next world launch.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Harvest Impact Audio (Release Authors)"))
class SHATTEREDFRONTIER_API USFHarvestImpactAudioSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Harvest impact audio", meta=(DisplayName="Volume", ClampMin="0.0", ClampMax="1.0", ToolTip="Linear impact volume multiplier. Default 0.65; release authors only; captured when a resource begins play for the next world."))
    float Volume = 0.65f;

    UPROPERTY(Config, EditAnywhere, Category="Harvest impact audio", meta=(DisplayName="Pitch variation", ClampMin="0.0", ClampMax="0.2", ToolTip="Maximum seeded pitch offset as a unitless fraction of nominal pitch. Default 0.06; release authors only; captured when a resource begins play for the next world."))
    float PitchVariationFraction = 0.06f;

    UPROPERTY(Config, EditAnywhere, Category="Distance", meta=(DisplayName="Full-volume radius", ClampMin="0.0", ClampMax="10.0", Units="m", ToolTip="Spherical radius that retains full volume before distance falloff begins. Default 2 metres; release authors only; captured when a resource begins play for the next world."))
    float InnerRadiusMetres = 2.0f;

    UPROPERTY(Config, EditAnywhere, Category="Distance", meta=(DisplayName="Maximum distance", ClampMin="2.0", ClampMax="60.0", Units="m", ToolTip="Spherical distance where the impact fades to silence. Must exceed the full-volume radius. Default 20 metres; release authors only; captured when a resource begins play for the next world."))
    float MaxDistanceMetres = 20.0f;

    UPROPERTY(Config, EditAnywhere, Category="Voice limit", meta=(DisplayName="Concurrent impacts per resource", ClampMin="1", ClampMax="8", ToolTip="Maximum overlapping harvest impacts owned by one resource actor. This does not set a global level-wide voice cap. Default 4; release authors only; captured when a resource begins play for the next world."))
    int32 MaxConcurrentImpactsPerResource = 4;

    bool Validate(FString& Error) const;
};

UCLASS(ClassGroup=(Gathering), meta=(BlueprintSpawnableComponent))
class SHATTEREDFRONTIER_API USFHarvestImpactAudio : public UActorComponent
{
    GENERATED_BODY()

public:
    USFHarvestImpactAudio();
    virtual void BeginPlay() override;

    // Call only for a server-approved, newly committed extraction contact. The caller owns
    // approval, duplicate/replay suppression, and late-join behavior; this helper is cosmetic.
    void PlayImpact(const FVector& Location, bool bWood, uint32 ApprovedSeed);

private:
    UPROPERTY(Transient)
    TObjectPtr<USFHarvestImpactAudioSettings> CapturedSettings;

    UPROPERTY(Transient)
    TObjectPtr<USoundWave> WoodHeavyImpact;

    UPROPERTY(Transient)
    TObjectPtr<USoundWave> WoodMediumImpact;

    UPROPERTY(Transient)
    TObjectPtr<USoundWave> MiningImpact02;

    UPROPERTY(Transient)
    TObjectPtr<USoundWave> MiningImpact03;

    UPROPERTY(Transient)
    TObjectPtr<USoundAttenuation> ImpactAttenuation;

    UPROPERTY(Transient)
    TObjectPtr<USoundConcurrency> ImpactConcurrency;

    bool bSettingsValid = false;
};
