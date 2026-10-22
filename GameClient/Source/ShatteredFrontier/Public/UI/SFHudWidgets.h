#pragma once

#include "Quest/SFQuestPilot.h"
#include "UI/SFUiWidgets.h"
#include "SFHudWidgets.generated.h"

class UHorizontalBox;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProgressBar;
class UVerticalBox;
class UWidget;
class UWrapBox;
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

// Unit frame: the level badge, name, health and stamina bars of one unit (player, target, party or raid member). WBP_UnitFrame holds the
// owner's Designer layout; WBP_PlayerFrame, WBP_TargetFrame, WBP_PartyFrame and WBP_RaidFrame are its child WBPs and differ only in the four
// part toggles below, which hide the optional parts in the Designer preview and at runtime. C++ sets DATA only (text, percent, visibility):
// fonts, sizes, colours and layout are the Designer's. Binds: HealthBar (required, UImage drawing a masked-fill material, parameter
// "Percent" 0-1); optional LevelText, NameText, StaminaBar, HealthText, StaminaText and the containers BadgeRoot and StaminaRow.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFUnitFrameWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetLevel(int32 Level);
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetName(const FText& Name);
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetHealth(int32 Current, int32 Max);
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetStamina(int32 Current, int32 Max);
    void SetVitals(int32 Health, int32 MaxHealth, int32 Stamina, int32 MaxStamina) { SetHealth(Health, MaxHealth); SetStamina(Stamina, MaxStamina); }
    // "40 / 80"; the current value never shows below zero.
    static FText ValueText(int32 Current, int32 Max);

    // The bar's dynamic material instance (null until the bar image holds a material), for tools and tests.
    static UMaterialInstanceDynamic* FillMaterial(UImage* Bar);
    // Collapses the parts the toggles turn off (and restores the ones turned on).
    void ApplyParts();

    UPROPERTY(EditAnywhere, Category = "SF Style") TSoftObjectPtr<UMaterialInterface> HealthFillMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Frontier/UI/Kit/HudVitals/MI_UI_BarFill_Health.MI_UI_BarFill_Health")));
    UPROPERTY(EditAnywhere, Category = "SF Style") TSoftObjectPtr<UMaterialInterface> StaminaFillMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Frontier/UI/Kit/HudVitals/MI_UI_BarFill_Stamina.MI_UI_BarFill_Stamina")));

    // Per-class parts (set in the child WBP's Class Defaults).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF Parts") bool bShowBadge = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF Parts") bool bShowName = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF Parts") bool bShowStamina = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SF Parts") bool bShowValues = true;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UImage> HealthBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> StaminaBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> HealthText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StaminaText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> LevelText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> NameText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> BadgeRoot;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> StaminaRow;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativePreConstruct() override;
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

// The HUD root. Binds (required): Vitals (WBP_PlayerFrame), Target (WBP_TargetFrame; hidden without a target), Hotbar, Minimap, QuestTracker, Toasts;
// optional PartyList (VerticalBox) and RaidGrid (WrapBox), empty and collapsed until a party system fills them. ASFPlayerController creates WBP_Hud and feeds it.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    void Configure(USFGatheringPilotComponent* InPilot) { Pilot = InPilot; }
    void PresentVitals(int32 Health, int32 MaxHealth, int32 Stamina, int32 MaxStamina);
    void PresentQuests(const TArray<FSFQuestView>& Shown);
    void PushToast(const FString& Text);
    // Shows the target frame with this data, or hides it (bShow false).
    void PresentTarget(bool bShow, const FString& Name, int32 Level, int32 Health, int32 MaxHealth);
    // For the future party system: a WBP_PartyFrame row (bRaid false) in PartyList or a WBP_RaidFrame cell in RaidGrid.
    void AddPartyMember(const FString& Name, int32 Level, int32 Health, int32 MaxHealth, int32 Stamina, int32 MaxStamina, bool bRaid = false);
    void ClearParty();
    int32 PartyCount() const;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFUnitFrameWidget> Vitals;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFUnitFrameWidget> Target;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UVerticalBox> PartyList;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWrapBox> RaidGrid;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudHotbarWidget> Hotbar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudMinimapWidget> Minimap;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudQuestTrackerWidget> QuestTracker;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<USFHudToastsWidget> Toasts;

    /** Where the WBP lives. */
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_Hud.WBP_Hud_C");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFUnitFrameWidget> PartyFrameClass;  // empty = WBP_PartyFrame
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFUnitFrameWidget> RaidFrameClass;   // empty = WBP_RaidFrame

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    USFUnitFrameWidget* MakeFrame(TSubclassOf<USFUnitFrameWidget>& Cached, const TCHAR* Path);
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
};
