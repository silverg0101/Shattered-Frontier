#pragma once

#include "Quest/SFQuestPilot.h"
#include "GameFramework/SaveGame.h"
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
struct FSFCastView;

// In-game HUD as Widget Blueprints on C++ bases (docs/blueprints/menus.md). C++ owns the data and rules here; WBP_Hud and its child
// WBPs under /Game/Frontier/UI/Widgets/Hud own layout and look. Every BindWidget below must keep its name and type in the WBP.

// What the hotbar draws from: item counts by definition id (bag + worn), the worn ids, and the build piece selected in build mode
// (-1 outside it; 0 wall .. 3 door, USFHudHotbarWidget::BuildPieces).
struct FSFHotbarStock { TMap<FString, int32> Counts; TSet<FString> Worn; int32 BuildPiece = -1; };

// A character's hotbar layout, saved locally (slot "Hotbar_<character>"): one assignment per slot, "" for empty.
UCLASS()
class SHATTEREDFRONTIER_API USFHotbarSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() TArray<FString> Layout;
};

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

// Hotbar of up to SlotCount cells; Settings > Gameplay picks how many show (USFGameUserSettings::HotbarSlots, 2-10). Binds: SlotRow
// (HorizontalBox whose children are the WBP_Slot cells, in key order; required).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudHotbarWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr int32 SlotCount = 10;  // cells authored; keys 1-9, 0
    // A slot holds a build piece (these ids; index = piece) or an item definition id ("item/iron_axe"); "" is empty.
    static const TArray<FString>& BuildPieces();
    // A new character's bar: 1-4 the build pieces, 5 axe, 6 pickaxe.
    static TArray<FString> DefaultLayout();
    // The number under a slot: the item's count, or a build piece's material bundles.
    static int32 CountFor(const FString& Assignment, const FSFHotbarStock& Stock);
    // The owning controller's pilot inventory (bag + worn) and build selection.
    static FSFHotbarStock StockOf(const APlayerController* Controller);
    // Layout edits (drag and drop); each saves the character's layout.
    void Assign(int32 Index, const FString& Id);
    void SwapSlots(int32 A, int32 B);
    // Loads Character's saved layout, else the default. Present loads the owning player's on first use.
    void LoadLayout(const FString& Character);
    const TArray<FString>& GetLayout() const { return Layout; }
    bool IsSlotShown(int32 Index) const { return Cells.IsValidIndex(Index) && Index < ShownCount; }

    // Icons and counts from the layout, the worn items and the build piece lit, and how many cells show (Settings > Gameplay).
    void Present(const FSFHotbarStock& Stock);
    UFUNCTION(BlueprintPure, Category = "SF HUD") USFSlotWidget* GetCell(int32 Index) const { return Cells.IsValidIndex(Index) ? Cells[Index].Get() : nullptr; }
    int32 GetActiveKey() const { return ActiveKey; }  // the first lit slot's key (1-10), 0 when none

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UHorizontalBox> SlotRow;

protected:
    virtual void NativeOnInitialized() override;
    // An item dropped on a slot is put there; a slot dropped on a slot swaps them.
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    void SaveLayout() const;
    UPROPERTY(Transient) TArray<TObjectPtr<USFSlotWidget>> Cells;
    TArray<FString> Layout;
    TArray<FString> ShownIds;  // the assignment each cell's icon was drawn for
    FString CharacterKey;
    int32 ShownCount = 6;
    int32 ActiveKey = 0;
};

// Minimap: footer ("Silverbrook · 120 m") plus a baked top-down capture of the level scrolled under the player (tools/Bake-Minimap.py).
// Binds: ZoneText, MapImage, PlayerArrow (all optional). The disc, ring and arrow art are the WBP's; the bounds below are the bake's.
UCLASS(Abstract, Config = Game)
class SHATTEREDFRONTIER_API USFHudMinimapWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    // "Greenlands_Silverbrook" -> "Silverbrook"
    static FString ZoneFromMapName(const FString& MapName);
    // "<zone> · <n> m", or just the zone for a negative distance.
    static FString FooterText(const FString& Zone, int32 Metres);
    UFUNCTION(BlueprintCallable, Category = "SF HUD") void SetFooter(const FString& Text);
    // UV rectangle (u0,v0,u1,v1) of the baked map to show around a world position. Map image: up = +X, right = +Y.
    // Bounds = (MinX,MinY,MaxX,MaxY) in cm; the window is clamped inside the map so edges never sample outside it.
    static FBox2f MapWindowUV(const FVector4& BoundsCm, double WindowCm, double WorldX, double WorldY);

    // The bake's world rectangle in cm (Bake-Minimap.py prints it; DefaultGame.ini carries it) and the world span shown across the disc.
    UPROPERTY(Config, EditAnywhere, Category = "SF Minimap") FVector4 MapBoundsCm = FVector4(-150000.0, -150000.0, 150000.0, 150000.0);
    UPROPERTY(Config, EditAnywhere, Category = "SF Minimap", meta = (Units = "cm", ClampMin = "1000")) double WindowCm = 15000.0;

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle ZoneStyle = FSFUiTextStyle(18, SFHudStyle::Ivory());
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ZoneText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> MapImage;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> PlayerArrow;

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

// Cast/channel data from the owner-replicated casting component. Layout belongs to WBP_HudCastBar.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFHudCastBarWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    void Present(const FSFCastView& View, float Progress);
    UPROPERTY(BlueprintReadOnly, Category="SF", meta=(BindWidget)) TObjectPtr<UProgressBar> CastProgress;
    UPROPERTY(BlueprintReadOnly, Category="SF", meta=(BindWidget)) TObjectPtr<UTextBlock> AbilityName;
    UPROPERTY(BlueprintReadOnly, Category="SF", meta=(BindWidget)) TObjectPtr<UTextBlock> CastState;
private:
    int32 LastLoggedQuarter = -1;
    uint8 LastLoggedPhase = 0;
    bool bLastLoggedInterrupted = false;
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
    // The player badge: the character level. SyncLevel (each tick) reads it from the pilot component and pushes only when it changes.
    void PresentLevel(int32 Level);
    void SyncLevel();
    void SyncRestedMinutes();
    UFUNCTION(BlueprintPure, Category="SF|Progression") double GetRestedMinutes() const { return ShownRestedMinutes; }
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
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFHudCastBarWidget> CastBar;

    /** Where the WBP lives. */
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_Hud.WBP_Hud_C");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFUnitFrameWidget> PartyFrameClass;  // empty = WBP_PartyFrame
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF") TSubclassOf<USFUnitFrameWidget> RaidFrameClass;   // empty = WBP_RaidFrame

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    USFUnitFrameWidget* MakeFrame(TSubclassOf<USFUnitFrameWidget>& Cached, const TCHAR* Path);
    UPROPERTY(Transient) TObjectPtr<USFGatheringPilotComponent> Pilot;
    int32 ShownLevel = 0;
    double ShownRestedMinutes = 0;
};
