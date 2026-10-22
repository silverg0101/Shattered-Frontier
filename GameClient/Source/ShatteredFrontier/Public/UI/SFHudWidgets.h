#pragma once

#include "Quest/SFQuestPilot.h"
#include "UI/SFUiWidgets.h"
#include "SFHudWidgets.generated.h"

class UHorizontalBox;
class UProgressBar;
class UVerticalBox;
class USFGatheringPilotComponent;

// In-game HUD as Widget Blueprints on C++ bases (docs/blueprints/menus.md). C++ owns the data and rules here; WBP_Hud and its child
// WBPs under /Game/Frontier/UI/Widgets/Hud own layout and look. Every BindWidget below must keep its name and type in the WBP.

// One hotbar cell: the key that selects it, the icon stem (/Game/Frontier/UI/Icons/T_Icon_<IconStem>) and the inventory name it counts.
struct FSFHotbarSlot { int32 Key = 0; FString IconStem; FString Label; };

// Short-lived skill-gain toasts, newest last; MaxVisible caps what is drawn.
struct FSFToastQueue
{
    static constexpr int32 MaxVisible = 3;
    struct FToast { FString Text; double ExpiresAt = 0; };
    TArray<FToast> Items;

    void Push(const FString& Text, double Now, double LifetimeSeconds = 3.0);
    // Drops expired toasts and returns the newest MaxVisible texts, oldest first.
    TArray<FString> Visible(double Now);
};

// Health and stamina bars with their numerals and the level badge. Binds: HealthBar (required), StaminaBar, HealthText,
// StaminaText, LevelText (optional). The bar fill art is set here from the kit; a frame image over the bar is the WBP's.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudVitalsWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetVitals(int32 Health, int32 MaxHealth, int32 Stamina, int32 MaxStamina);
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetLevel(int32 Level);
    // "40 / 80"; the current value never shows below zero.
    static FText ValueText(int32 Current, int32 Max);

    UPROPERTY(EditAnywhere, Category = "SF Style") FName HealthFillPiece = TEXT("bar_hp_fill");
    UPROPERTY(EditAnywhere, Category = "SF Style") FName StaminaFillPiece = TEXT("bar_stamina_fill");
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle ValueStyle = FSFUiTextStyle(20, SFHudStyle::Ivory(), false, true);
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle LevelStyle = FSFUiTextStyle(28, SFHudStyle::Ivory(), false, true);

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> StaminaBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HealthText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StaminaText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LevelText;

protected:
    virtual void ApplyKitStyle() override;
};

// Six-slot hotbar. Binds: SlotRow (HorizontalBox whose children are the WBP_Slot cells, in key order; required).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudHotbarWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 SlotCount = 6;
    // Keys 1-6 as bound today: 1 Wall, 2 Foundation, 3 Roof, 4 Door (construction shapes) and 5 axe, 6 pickaxe (gathering tools).
    static TArray<FSFHotbarSlot> Slots();
    // The equipped tool's key (5 axe, 6 pickaxe) from the pilot's item list, or 0 when none is equipped.
    static int32 ActiveToolKey(const TArray<TPair<FText, int32>>& InventoryItems);
    // Stack count of one slot: the pilot items whose name contains the slot label, summed.
    static int32 StackCount(const TArray<TPair<FText, int32>>& InventoryItems, const FString& Label);

    // Selects the equipped tool's cell and sets every cell's stack count.
    void Present(const TArray<TPair<FText, int32>>& InventoryItems);
    UFUNCTION(BlueprintPure, Category = "SF HUD") USFSlotWidget* GetCell(int32 Index) const { return Cells.IsValidIndex(Index) ? Cells[Index].Get() : nullptr; }
    int32 GetActiveKey() const { return ActiveKey; }

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UHorizontalBox> SlotRow;

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> Cells;
    int32 ActiveKey = -1;
};

// Minimap frame footer ("Silverbrook · 120 m"). Binds: ZoneText (optional). The disc, ring and arrow art are the WBP's.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudMinimapWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    // "Greenlands_Silverbrook" -> "Silverbrook"
    static FString ZoneFromMapName(const FString& MapName);
    // "<zone> · <n> m", or just the zone for a negative distance.
    static FString FooterText(const FString& Zone, int32 Metres);
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetFooter(const FString& Text);

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle ZoneStyle = FSFUiTextStyle(18, SFHudStyle::Ivory());
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ZoneText;

protected:
    virtual void ApplyKitStyle() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    FString ZoneName;
    double NextFooterSeconds = 0.0;
};

// Tracked quests. Binds: Rows (VerticalBox; required). One USFListRowWidget (RowClass) per heading and objective line.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudQuestTrackerWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    // Rebuilds the rows when the shown quests changed; collapses itself when none are shown.
    void Present(const TArray<FSFQuestView>& Shown);
    int32 RowCount() const;

    // The row WBP; empty = /Game/Frontier/UI/Widgets/Hud/WBP_HudQuestRow.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFListRowWidget> RowClass;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UVerticalBox> Rows;

protected:
    virtual void NativeOnInitialized() override;

private:
    FString LastKey;
};

// Skill-gain and item toasts. Binds: CardRows (VerticalBox whose children are the WBP_HudToastCard cards, newest on top; required).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudToastsWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    // "<Name> +1": the text before the last space is the name, the rest the delta.
    void PushToast(const FString& Text);
    void Refresh(double Now);
    int32 VisibleCards() const;
    static const TCHAR* IconStem(const FString& ToastText);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") FText CaptionText = NSLOCTEXT("SFHud", "SkillGain", "Skill gain");
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UVerticalBox> CardRows;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TArray<TObjectPtr<USFListRowWidget>> Cards;
    FSFToastQueue Queue;
};

// The HUD root. Binds (all required): Vitals, Hotbar, Minimap, QuestTracker, Toasts. ASFPlayerController creates WBP_Hud and feeds it.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot) { Pilot = InPilot; }
    void PresentVitals(int32 Health, int32 MaxHealth, int32 Stamina, int32 MaxStamina);
    void PresentQuests(const TArray<FSFQuestView>& Shown);
    void PushToast(const FString& Text);

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudVitalsWidget> Vitals;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudHotbarWidget> Hotbar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudMinimapWidget> Minimap;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudQuestTrackerWidget> QuestTracker;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudToastsWidget> Toasts;

    /** Where the WBP lives. */
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_Hud.WBP_Hud_C");

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
};
