#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "SFConstructionSandboxPalette.generated.h"

class UBorder;
class UTextBlock;

// UButton's focus initializer is protected and intended for constructors.
UCLASS()
class SHATTEREDFRONTIER_API USFConstructionPaletteButton : public UButton
{
    GENERATED_BODY()
public:
    USFConstructionPaletteButton(const FObjectInitializer& ObjectInitializer);
};

UENUM()
enum class ESFConstructionPaletteAction : uint8
{
    Cube,
    Slab,
    Slope,
    Door,
    Single,
    Line,
    Demolish,
    Rotate,
    Surface,
    Layer,
    LayerDown,
    LayerUp,
    Confirm,
    Cancel,
    Save,
    Load
};

DECLARE_DELEGATE_OneParam(FSFConstructionPaletteActionDelegate, ESFConstructionPaletteAction);

UCLASS()
class SHATTEREDFRONTIER_API USFConstructionSandboxPalette : public UUserWidget
{
    GENERATED_BODY()

public:
    FSFConstructionPaletteActionDelegate OnAction;

    void ConfigureNetworkPresentation();
    void SetStatusText(const FText& InStatusText);
    bool IsPointerOverPalette() const;

protected:
    virtual void NativeOnInitialized() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UFUNCTION() void HandleCube();
    UFUNCTION() void HandleSlab();
    UFUNCTION() void HandleSlope();
    UFUNCTION() void HandleDoor();
    UFUNCTION() void HandleSingle();
    UFUNCTION() void HandleLine();
    UFUNCTION() void HandleDemolish();
    UFUNCTION() void HandleRotate();
    UFUNCTION() void HandleSurface();
    UFUNCTION() void HandleLayer();
    UFUNCTION() void HandleLayerDown();
    UFUNCTION() void HandleLayerUp();
    UFUNCTION() void HandleConfirm();
    UFUNCTION() void HandleCancel();
    UFUNCTION() void HandleSave();
    UFUNCTION() void HandleLoad();

    void Dispatch(ESFConstructionPaletteAction Action);

    UPROPERTY(Transient)
    TObjectPtr<UBorder> PalettePanel;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> StatusTextBlock;

    UPROPERTY(Transient) TObjectPtr<UTextBlock> HintsTextBlock;
    FText CachedStatusText;
};
