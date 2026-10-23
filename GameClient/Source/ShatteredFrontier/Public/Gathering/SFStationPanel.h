#pragma once

#include "UI/SFItemTooltip.h"
#include "UI/SFUiWidgets.h"
#include "SFStationPanel.generated.h"

class UImage;
class UPanelWidget;
class UTextBlock;
class USFGatheringPilotComponent;
class ASFGatheringPilotNode;
struct FSFStationRecipe;

// Crafting station window (WBP_StationPanel, docs/blueprints/menus.md section 0): a WBP_WindowFrame (emblem icon_window_recipes, title = the
// station) with a WBP_TabBar, the recipe list (WBP_KitRow rows in a WBP_ScrollList) beside the selected recipe's detail in the SidePanel:
// output, description, separator, ingredients have / need (green when met, red when short), the station line, the quantity stepper and the
// Craft button. Presented by USFGatheringHud while the player aims at a station (opened and closed through USFWindowStack; Esc dismisses it
// until the aim leaves the station). A click on a recipe selects it for the station (SelectStationRecipe); Craft starts the station's timed
// craft exactly as the interact key does (InteractFromRay), and its fill shows the running craft.
// Binds (required): RecipeList, Ingredients (VerticalBoxes; C++ adds the WBP_KitRow children).
// Optional: Window (the WBP_WindowFrame; its title is the station), TabBar (All / Materials / Parts), OutputIcon, OutputName, OutputText, CraftTime, StationText, StationHint, QuantityStepper
// (WBP_Stepper, 1 to 1: one craft per interact today), CraftButton (WBP_Button), EmptyText, Tooltip with TipIcon, TipTitle, TipRarity,
// TipType, TipQuantity, TipWeight, TipDescription.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFStationPanel : public USFUiWidget
{
    GENERATED_BODY()
public:
    static constexpr const TCHAR* AssetPath = TEXT("/Game/Frontier/UI/Widgets/Gathering/WBP_StationPanel.WBP_StationPanel_C");

    // Fills the panel for the aimed station; null leaves it as it was (the owner closes the window). Rebuilds the lists only when the
    // station or its selected recipe changes.
    void Present(USFGatheringPilotComponent* Pilot, const ASFGatheringPilotNode* Station);
    // item/rough_timber -> rough_timber (the T_Icon_ stem).
    static FString IconStem(const FString& DefinitionId);
    bool IsPointerOver() const;
    int32 RecipeCount() const { return RecipeRows.Num(); }
    USFKitRowWidget* GetRecipeRow(int32 Index) const { return RecipeRows.IsValidIndex(Index) ? RecipeRows[Index].Get() : nullptr; }
    USFButtonWidget* GetCraftButton() const { return CraftButton; }

    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TextStyle = FSFUiTextStyle(34, SFHudStyle::Ivory());

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void ApplyKitStyle() override;

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> RecipeList;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UPanelWidget> Ingredients;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFWindowFrameWidget> Window;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFTabBarWidget> TabBar;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> OutputIcon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OutputName;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> OutputText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CraftTime;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StationText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StationHint;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFStepperWidget> QuantityStepper;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<USFButtonWidget> CraftButton;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> EmptyText;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Tooltip;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> TipIcon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipTitle;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipRarity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipType;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipQuantity;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipWeight;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TipDescription;

private:
    UFUNCTION() void HandleRecipeClicked(USFSelectableWidget* Source);
    UFUNCTION() void HandleCraft(USFButtonWidget* Source);
    void Rebuild(const ASFGatheringPilotNode* Station, const FString& RecipeId);
    void UpdateTooltip();

    TSubclassOf<USFKitRowWidget> RowClass;
    TWeakObjectPtr<USFGatheringPilotComponent> PilotRef;
    TWeakObjectPtr<const ASFGatheringPilotNode> StationRef;
    UPROPERTY(Transient) TArray<TObjectPtr<USFKitRowWidget>> RecipeRows;
    UPROPERTY(Transient) TArray<TObjectPtr<USFKitRowWidget>> IngredientRows;
    TArray<FString> RowRecipeIds;  // the recipe each recipe row selects, by row index
    TArray<FString> RowOutputIds;  // the output item of each recipe row (tooltip)
    TArray<FString> NeedIds;
    TArray<FString> NeedNames;
    TArray<int32> NeedAmounts;
    TArray<int32> HaveAmounts;
    FString ShownKey;  // station kind + recipe id the lists were built for
    FString OutputId;
    int32 OutputQuantity = 0;
    uint32 CraftOperation = 0;
    double CraftTotalSeconds = 0;
};
