#pragma once

#include "UI/SFUiWidgets.h"
#include "Subsystems/WorldSubsystem.h"
#include "SFCombatFeedback.generated.h"

class UTextBlock;

/** Cosmetic, server-authored landed damage. Never used to apply gameplay damage. */
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFCombatDamageWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_CombatDamage.WBP_CombatDamage_C");
    void Present(int32 Damage, bool bCritical);
    FVector2D MeasureVisualSize();
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DamageText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> CriticalText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> CriticalNumber;
protected:
    virtual void ApplyKitStyle() override;
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
    friend class FSFCombatFeedbackBurstTest;
#endif
};

/** Slate-unit animation bounds used to reserve separated lanes for the entire pop/float path. */
struct SHATTEREDFRONTIER_API FSFCombatDamageLayout
{
    static constexpr float Lifetime = 1.1f;
    static constexpr int32 MaxEntries = 12;
    static float PopScale(float Age, bool bCritical);
    static FVector2D Position(FVector2D Anchor, float LaneOffset, float Age);
    static FBox2D Bounds(FVector2D Anchor, FVector2D VisualSize, float LaneOffset, float Age, bool bCritical);
    static FBox2D Envelope(FVector2D Anchor, FVector2D VisualSize, float LaneOffset, bool bCritical);
    static float FindLane(FVector2D Anchor, FVector2D VisualSize, float PreferredOffset, bool bCritical, const TArray<FBox2D>& Occupied);
};

USTRUCT()
struct FSFCombatDamageEntry
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<USFCombatDamageWidget> Widget;
    TWeakObjectPtr<AActor> Receiver;
    FVector Anchor = FVector::ZeroVector;
    float Age = 0;
    FVector2D VisualSize = FVector2D::ZeroVector;
    float LaneOffset = 0;
    bool bCritical = false;
};

/** Local screen-space projection and bounded animation. Designer owns fonts, colors and layout. */
UCLASS()
class SHATTEREDFRONTIER_API USFCombatFeedbackSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    static FVector HeadAnchor(const AActor* Receiver);
    void Receive(AActor* Receiver, FVector HitAnchor, int32 Damage, bool bCritical);
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USFCombatFeedbackSubsystem, STATGROUP_Tickables); }
    virtual void Deinitialize() override;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type == EWorldType::Game || Type == EWorldType::PIE; }
private:
    UPROPERTY(Transient) TArray<FSFCombatDamageEntry> Entries;
};
