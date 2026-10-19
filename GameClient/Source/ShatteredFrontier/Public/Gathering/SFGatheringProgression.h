#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SFGatheringProgression.generated.h"

// Canonical gathering track only. No character-level gate or client mutation RPC.
UENUM(BlueprintType)
enum class ESFGatheringSkill : uint8 { Mining, Logging, Herbalism, Farming, Fishing, Hunting };
UENUM()
enum class ESFGatheringMaterialTier : uint8 { Common, Uncommon, Rare, Epic, Legendary };
UENUM()
enum class ESFGatheringSkillLock : uint8 { Up, Down, Locked };

// Versioned content identity. Current released profiles are beginner/common only.
struct FSFGatheringResourceProfile
{
    FName ResourceId;
    ESFGatheringSkill Skill = ESFGatheringSkill::Mining;
    ESFGatheringMaterialTier Tier = ESFGatheringMaterialTier::Common;
    int32 ToolTier = 1;
};

// Release authors; captured per admitted controller, effective next world/session.
// Production progression-service, account bonuses and security projection are absent.
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Gathering Progression Pilot (Release Authors)"))
class SHATTEREDFRONTIER_API USFGatheringProgressionSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, Category="Gain", meta=(ClampMin="0.001", ClampMax="0.9", DisplayName="Base chance to improve", ToolTip="Success probability before action duration, skill and practice modifiers; 0.15 means 15 percent. Release authors; next session.")) double BaseGainRate = 0.15;
    UPROPERTY(Config, EditAnywhere, Category="Gain", meta=(ClampMin="0.01", ClampMax="0.9", DisplayName="Maximum improvement chance", ToolTip="Upper probability before fatigue and repetition reductions. Release authors; next session.")) float MaximumGainChance = 0.9f;
    UPROPERTY(Config, EditAnywhere, Category="Gain", meta=(ClampMin="1", ClampMax="30", Units="s", DisplayName="Standard practice duration", ToolTip="Authored action seconds equal to one weighted attempt. Release authors; next session.")) float AttemptWeightSeconds = 6;
    UPROPERTY(Config, EditAnywhere, Category="Difficulty", meta=(ClampMin="-100", ClampMax="100", DisplayName="Best challenge above current skill", ToolTip="Difficulty minus skill points that gives the strongest practice. Release authors; next session.")) float DifficultyOptimum = 25;
    UPROPERTY(Config, EditAnywhere, Category="Difficulty", meta=(ClampMin="1", ClampMax="300", DisplayName="Useful challenge width", ToolTip="Skill points controlling how quickly practice chance falls away from the best challenge. Release authors; next session.")) float DifficultyWidth = 100;
    UPROPERTY(Config, EditAnywhere, Category="Bands", meta=(ClampMin="0", ClampMax="1", DisplayName="Expert practice multiplier", ToolTip="Improvement multiplier at skill500 through699. Release authors; next session.")) double ExpertGainFactor = 0.9;
    UPROPERTY(Config, EditAnywhere, Category="Bands", meta=(ClampMin="0", ClampMax="1", DisplayName="Master practice multiplier", ToolTip="Improvement multiplier at skill700 through899. Release authors; next session.")) double MasterGainFactor = 0.8;
    UPROPERTY(Config, EditAnywhere, Category="Bands", meta=(ClampMin="0", ClampMax="1", DisplayName="Legendary practice multiplier", ToolTip="Formula coefficient at skill900 and above; specialization is unavailable in this pilot. Release authors; next session.")) double LegendaryGainFactor = 0.5;
    UPROPERTY(Config, EditAnywhere, Category="Fatigue", meta=(ClampMin="120", ClampMax="480", Units="min", DisplayName="Recent practice window", ToolTip="Rolling window of active minutes for one skill. Release authors; next session.")) int32 FatigueWindowMinutes = 240;
    UPROPERTY(Config, EditAnywhere, Category="Fatigue", meta=(ClampMin="0", ClampMax="120", Units="min", DisplayName="Active practice before fatigue", ToolTip="Active minutes within the window before improvement starts slowing. Must be below the window. Release authors; next session.")) int32 FatigueFreeMinutes = 90;
    UPROPERTY(Config, EditAnywhere, Category="Fatigue", meta=(ClampMin="0.1", ClampMax="1", DisplayName="Minimum fatigued practice multiplier", ToolTip="Lowest multiplier due to repeated active minutes. Release authors; next session.")) float FatigueFloor = 0.25f;
    UPROPERTY(Config, EditAnywhere, Category="Repetition", meta=(ClampMin="1", ClampMax="1000", DisplayName="Repeated harvests before reduced practice", ToolTip="Successful harvest records for the same skill and resource position within an hour. Release authors; next session.")) int32 RepetitionThreshold = 80;
    UPROPERTY(Config, EditAnywhere, Category="Input Timing", meta=(ClampMin="0", ClampMax="0.2", DisplayName="Minimum input timing variation", ToolTip="Standard deviation divided by mean input interval over200 practice inputs. Release authors; next session.")) float TimingCoefficientMinimum = 0.05f;
    UPROPERTY(Config, EditAnywhere, Category="Input Timing", meta=(ClampMin="0", ClampMax="1", DisplayName="Uniform input timing practice multiplier", ToolTip="Improvement multiplier when the latest200 practice inputs are unusually regular. Release authors; next session.")) float RegularTimingGainFactor = 0.25f;
    bool Validate() const;
    UPROPERTY(Config, EditAnywhere, Category="Skill Choices", meta=(ClampMin="5", ClampMax="60", Units="s", DisplayName="Time between training preference changes", ToolTip="Per-skill minimum delay between accepted player training preference changes. Release authors; captured next session. Cannot shorten the canonical five-second limit.")) float SkillLockChangeCooldownSeconds = 5;
};

USTRUCT(BlueprintType)
struct SHATTEREDFRONTIER_API FSFGatheringSkillSnapshot
{
    GENERATED_BODY()
    // Enum-indexed values; only gathering exists in this session pilot.
    UPROPERTY(BlueprintReadOnly) TArray<int32> Values = {0,0,0,0,0,0};
    UPROPERTY() TArray<ESFGatheringSkillLock> Locks = {ESFGatheringSkillLock::Up,ESFGatheringSkillLock::Up,ESFGatheringSkillLock::Up,ESFGatheringSkillLock::Up,ESFGatheringSkillLock::Up,ESFGatheringSkillLock::Up};
    UPROPERTY(BlueprintReadOnly) int64 PracticeCount = 0;
    UPROPERTY(BlueprintReadOnly) int64 GainCount = 0;
    UPROPERTY(BlueprintReadOnly) ESFGatheringSkill LastGainedSkill = ESFGatheringSkill::Mining;
    int32 Get(ESFGatheringSkill Skill) const { return Values.IsValidIndex(int32(Skill)) ? Values[int32(Skill)] : 0; }
};

// Session authority adapter, not durable production progression. Bonus/risk projections
// are unavailable: M_bonus=1, AM_risk=1; no XP, attributes, specialization or other tracks.
class SHATTEREDFRONTIER_API FSFGatheringProgressionSession
{
public:
    static const TCHAR* SkillId(ESFGatheringSkill Skill);
    static const TCHAR* SkillName(ESFGatheringSkill Skill);
    static int32 Difficulty(ESFGatheringMaterialTier Tier);
    static int32 RequiredSkill(ESFGatheringMaterialTier Tier);
    static double GainChance(int32 Skill,int32 Difficulty,double Duration,double Fatigue,double AntiMacro,const USFGatheringProgressionSettings& Settings);
    // Called only once by the server after the inventory transaction succeeds.
    void Practice(FSFGatheringSkillSnapshot& Snapshot,ESFGatheringSkill Skill,int32 Difficulty,double Duration,uint64 Context,double InputTime,double Now,const USFGatheringProgressionSettings& Settings);
private:
    friend class FSFGatheringProgressionFormulaTest;
    friend class FSFGatheringPilotProgressionTest;
    struct FRecord { double At; uint64 Context; bool bGain; };
    TArray<FRecord> Records[6];
    TArray<int64> ActiveMinutes[6];
    TArray<double> InputTimes;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    TOptional<double> TestRoll;
#endif
};
