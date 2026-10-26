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
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DamageText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> CriticalText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> CriticalNumber;
protected:
    virtual void ApplyKitStyle() override;
};

USTRUCT()
struct FSFCombatDamageEntry
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<USFCombatDamageWidget> Widget;
    TWeakObjectPtr<AActor> Receiver;
    FVector Anchor = FVector::ZeroVector;
    float Age = 0;
    int32 Lane = 0;
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
    uint32 Sequence = 0;
};
