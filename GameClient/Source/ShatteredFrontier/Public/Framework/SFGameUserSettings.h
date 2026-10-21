#pragma once

#include "GameFramework/GameUserSettings.h"
#include "SFGameUserSettings.generated.h"

UENUM(BlueprintType)
enum class ESFCombatShoulderSide : uint8 { Right, Left };

// Local player preferences. SaveSettings persists changes in GameUserSettings.ini; effective next frame.
UCLASS(Config=GameUserSettings)
class SHATTEREDFRONTIER_API USFGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Camera|Combat Framing", meta=(DisplayName="Combat framing", ToolTip="Frame your selected combat target or dialogue speaker when zoomed in."))
    bool bCombatFraming = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Camera|Combat Framing", meta=(DisplayName="Combat depth of field", ToolTip="Keep the target sharp and soften the nearby player during framing."))
    bool bCombatDepthOfField = true;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Camera|Combat Framing", meta=(DisplayName="Camera shoulder", ToolTip="Choose a fixed right or left shoulder; changing it takes effect next frame."))
    ESFCombatShoulderSide CombatShoulderSide = ESFCombatShoulderSide::Right;
};
