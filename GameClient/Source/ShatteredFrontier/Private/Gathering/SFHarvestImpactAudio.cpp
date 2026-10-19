#include "Gathering/SFHarvestImpactAudio.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY_STATIC(LogSFHarvestImpactAudio, Log, All);

namespace
{
    constexpr TCHAR WoodHeavyPath[] = TEXT("/Game/Frontier/Gameplay/Harvest/AudioV1/TreeStrike_WoodHeavy_01.TreeStrike_WoodHeavy_01");
    constexpr TCHAR WoodMediumPath[] = TEXT("/Game/Frontier/Gameplay/Harvest/AudioV1/TreeStrike_WoodMedium_01.TreeStrike_WoodMedium_01");
    constexpr TCHAR Mining02Path[] = TEXT("/Game/Frontier/Gameplay/Harvest/AudioV1/RockStrike_Mining_02.RockStrike_Mining_02");
    constexpr TCHAR Mining03Path[] = TEXT("/Game/Frontier/Gameplay/Harvest/AudioV1/RockStrike_Mining_03.RockStrike_Mining_03");
    constexpr float CentimetresPerMetre = 100.0f;

    bool ValidateImpactSettingsOnce(const USFHarvestImpactAudioSettings* Settings)
    {
        FString Error;
        if (Settings && Settings->Validate(Error))
        {
            return true;
        }

        static bool bLoggedInvalidSettings = false;
        if (!bLoggedInvalidSettings)
        {
            UE_LOG(LogSFHarvestImpactAudio, Error, TEXT("HARVEST_IMPACT_AUDIO_INVALID_SETTINGS: %s"), *Error);
            bLoggedInvalidSettings = true;
        }
        return false;
    }
}

bool USFHarvestImpactAudioSettings::Validate(FString& Error) const
{
    const auto IsWithin = [](float Value, float Minimum, float Maximum)
    {
        return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
    };

    if (!IsWithin(Volume, 0.0f, 1.0f))
    {
        Error = TEXT("Volume must be in [0, 1].");
        return false;
    }
    if (!IsWithin(PitchVariationFraction, 0.0f, 0.2f))
    {
        Error = TEXT("Pitch variation must be in [0, 0.2].");
        return false;
    }
    if (!IsWithin(InnerRadiusMetres, 0.0f, 10.0f))
    {
        Error = TEXT("Full-volume radius must be in [0, 10] metres.");
        return false;
    }
    if (!IsWithin(MaxDistanceMetres, 2.0f, 60.0f) || MaxDistanceMetres <= InnerRadiusMetres)
    {
        Error = TEXT("Maximum distance must be in [2, 60] metres and exceed the full-volume radius.");
        return false;
    }
    if (MaxConcurrentImpactsPerResource < 1 || MaxConcurrentImpactsPerResource > 8)
    {
        Error = TEXT("Concurrent impacts per resource must be in [1, 8].");
        return false;
    }

    Error.Reset();
    return true;
}

USFHarvestImpactAudio::USFHarvestImpactAudio()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void USFHarvestImpactAudio::BeginPlay()
{
    Super::BeginPlay();

    CapturedSettings = DuplicateObject<USFHarvestImpactAudioSettings>(GetDefault<USFHarvestImpactAudioSettings>(), this);
    bSettingsValid = ValidateImpactSettingsOnce(CapturedSettings);
    if (!bSettingsValid)
    {
        return;
    }

    // Optional art can be absent while the mechanical gathering path remains usable.
    WoodHeavyImpact = LoadObject<USoundWave>(nullptr, WoodHeavyPath);
    WoodMediumImpact = LoadObject<USoundWave>(nullptr, WoodMediumPath);
    MiningImpact02 = LoadObject<USoundWave>(nullptr, Mining02Path);
    MiningImpact03 = LoadObject<USoundWave>(nullptr, Mining03Path);

    ImpactAttenuation = NewObject<USoundAttenuation>(this);
    if (ImpactAttenuation)
    {
        FSoundAttenuationSettings& Attenuation = ImpactAttenuation->Attenuation;
        Attenuation.bAttenuate = true;
        Attenuation.bSpatialize = true;
        Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
        Attenuation.AttenuationShape = EAttenuationShape::Sphere;
        Attenuation.AttenuationShapeExtents = FVector(CapturedSettings->InnerRadiusMetres * CentimetresPerMetre, 0.0, 0.0);
        Attenuation.FalloffDistance = (CapturedSettings->MaxDistanceMetres - CapturedSettings->InnerRadiusMetres) * CentimetresPerMetre;
    }

    ImpactConcurrency = NewObject<USoundConcurrency>(this);
    if (ImpactConcurrency)
    {
        FSoundConcurrencySettings& Concurrency = ImpactConcurrency->Concurrency;
        Concurrency.MaxCount = CapturedSettings->MaxConcurrentImpactsPerResource;
        Concurrency.bLimitToOwner = true;
        Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
    }
}

void USFHarvestImpactAudio::PlayImpact(const FVector& Location, bool bWood, uint32 ApprovedSeed)
{
    if (!bSettingsValid || !CapturedSettings || !ImpactAttenuation || !ImpactConcurrency || !GetOwner())
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    TArray<USoundWave*> AvailableVariants;
    if (bWood)
    {
        if (WoodHeavyImpact) { AvailableVariants.Add(WoodHeavyImpact); }
        if (WoodMediumImpact) { AvailableVariants.Add(WoodMediumImpact); }
    }
    else
    {
        if (MiningImpact02) { AvailableVariants.Add(MiningImpact02); }
        if (MiningImpact03) { AvailableVariants.Add(MiningImpact03); }
    }
    if (AvailableVariants.IsEmpty())
    {
        return;
    }

    FRandomStream Random(static_cast<int32>(ApprovedSeed));
    USoundWave* Sound = AvailableVariants[Random.RandRange(0, AvailableVariants.Num() - 1)];
    const float PitchMultiplier = 1.0f + Random.FRandRange(-CapturedSettings->PitchVariationFraction, CapturedSettings->PitchVariationFraction);
    UGameplayStatics::PlaySoundAtLocation(
        this,
        Sound,
        Location,
        FRotator::ZeroRotator,
        CapturedSettings->Volume,
        PitchMultiplier,
        0.0f,
        ImpactAttenuation,
        ImpactConcurrency,
        GetOwner());
}
