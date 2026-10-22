#pragma once

#include "Blueprint/UserWidget.h"
#include "Framework/SFHudStyle.h"
#include "SFUiWidgets.generated.h"

class UBorder;
class UButton;
class UImage;
class UNamedSlot;
class UTextBlock;
class UTexture2D;

// Shared UMG parts (docs/blueprints/menus.md "C++ / Widget Blueprint split"). C++ owns data, state, events and the style
// tokens (SFHudStyle: kit pieces, colours, fonts, sizes); the Widget Blueprint (WBP_*) owns layout and look.
// A widget binds the parts it drives with meta=(BindWidget) / BindWidgetOptional: do not rename or delete those in a WBP.
UENUM(BlueprintType)
enum class ESFUiState : uint8 { Normal, Hover, Pressed, Selected, Disabled };

UENUM(BlueprintType)
enum class ESFUiButtonKind : uint8 { Secondary, Primary, Destructive };

// One text role: defaults come from SFHudStyle (ivory EB Garamond); the font itself is resolved at runtime (it is a composite font
// built from the imported OFL faces, never saved into a WBP).
USTRUCT(BlueprintType)
struct SHATTEREDFRONTIER_API FSFUiTextStyle
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") int32 SizePx = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") bool bBold = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") bool bShadow = false;  // 1 px dark shadow and outline for text over art
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") FLinearColor Color = SFHudStyle::Ivory();

    FSFUiTextStyle() {}
    FSFUiTextStyle(int32 InSizePx, const FLinearColor& InColor, bool bInBold = false, bool bInShadow = false)
        : SizePx(InSizePx), bBold(bInBold), bShadow(bInShadow), Color(InColor) {}
    void Apply(UTextBlock* Block) const;
};

// Base of every shared part: applies the kit art, fonts and colours from SFHudStyle on construct (and in the Designer preview).
// Untick bApplyKitStyle on one WBP to style it by hand instead.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFUiWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF Style") bool bApplyKitStyle = true;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativePreConstruct() override;
    virtual void ApplyKitStyle() {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFUiClicked, class USFButtonWidget*, Source);

// Titled frame with a Content slot. Binds: Frame (Border), Title (TextBlock), Content (NamedSlot), all optional.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFPanelFrameWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetTitle(const FText& InTitle);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) FText TitleText;
    UPROPERTY(EditAnywhere, Category = "SF Style") FName FramePiece = TEXT("panel_frame");  // kit element name in DA_HudKit
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle TitleStyle = FSFUiTextStyle(36, SFHudStyle::Ivory());

protected:
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Frame;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Title;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UNamedSlot> Content;
};

// Text button. Binds: Button (required), Label (optional). Hover, press and disabled brushes come from SFHudStyle::ButtonBrush;
// OnStateChanged is the hook for extra visuals (glow, sound, animation) in the WBP.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFButtonWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category = "SF") FSFUiClicked OnClicked;

    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetLabel(const FText& InLabel);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetButtonEnabled(bool bEnabled);
    UFUNCTION(BlueprintPure, Category = "SF UI") ESFUiState GetState() const { return State; }
    // Visual hook: Normal, Hover, Pressed or Disabled just became current.
    UFUNCTION(BlueprintImplementableEvent, Category = "SF UI") void OnStateChanged(ESFUiState NewState);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SF", meta = (ExposeOnSpawn)) FText LabelText;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SF Style") ESFUiButtonKind Kind = ESFUiButtonKind::Secondary;
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle LabelStyle = FSFUiTextStyle(24, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle PrimaryLabelStyle = FSFUiTextStyle(26, SFHudStyle::Hex(TEXT("2A1E10")));  // dark ink on brass

protected:
    virtual void NativeOnInitialized() override;
    virtual void ApplyKitStyle() override;
    void SetState(ESFUiState NewState);

    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidget)) TObjectPtr<UButton> Button;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Label;
    ESFUiState State = ESFUiState::Normal;

private:
    UFUNCTION() void HandleClicked();
    UFUNCTION() void HandleHovered();
    UFUNCTION() void HandleUnhovered();
    UFUNCTION() void HandlePressed();
    UFUNCTION() void HandleReleased();
};

// Tab or rail row: a button that can be Selected (tab_selected moss plate). Same binds as USFButtonWidget.
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFTabWidget : public USFButtonWidget
{
    GENERATED_BODY()
public:
    USFTabWidget() { LabelStyle = FSFUiTextStyle(20, SFHudStyle::Ivory()); }
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetSelected(bool bInSelected);
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsSelected() const { return bSelected; }

protected:
    virtual void ApplyKitStyle() override;
    bool bSelected = false;
};

// Slot or tile (bag, equipment, hotbar, catalogue). Binds, all optional: Frame (Border), Icon (Image), KeyLabel, CountLabel (TextBlocks).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFSlotWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetIcon(UTexture2D* Texture);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetKeyText(const FText& Key);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetCount(int32 Count);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetSelected(bool bInSelected);
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsSelected() const { return bSelected; }
    UFUNCTION(BlueprintPure, Category = "SF UI") FText GetCountText() const;
    // Count rule: shown from 2 up (tools are single items), empty otherwise.
    static FText CountText(int32 Count);

    // Selection hook. The native body swaps the frame to SFHudStyle::SlotBrush; override it in the WBP for a different look.
    UFUNCTION(BlueprintNativeEvent, Category = "SF UI") void OnSelectedChanged(bool bNowSelected);

    UPROPERTY(EditAnywhere, Category = "SF Style") FName NormalPiece = TEXT("slot_hotbar_normal");  // bag 64 / equipment 72 / hotbar 88 / catalogue 100
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle KeyStyle = FSFUiTextStyle(32, SFHudStyle::Ivory(), false, true);
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle CountStyle = FSFUiTextStyle(24, SFHudStyle::Ivory(), false, true);

protected:
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Frame;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Icon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> KeyLabel;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CountLabel;
    bool bSelected = false;
};

// List row (quest objective, toast card, recipe or market row). Binds, all optional: Frame (Border), Icon (Image), Caption, Label, Value
// (TextBlocks), Marker (checkbox group), DoneMark (tick shown when done), Rule (hairline shown for headings).
UCLASS(Abstract)
class SHATTEREDFRONTIER_API USFListRowWidget : public USFUiWidget
{
    GENERATED_BODY()
public:
    // bHeading: bold title style, no checkbox, hairline Rule. bDone shows the DoneMark.
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetEntry(const FText& InLabel, const FText& InValue, bool bDone = false, bool bHeading = false);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetCaption(const FText& InCaption);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetRowIcon(const FSlateBrush& Brush);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetTint(const FLinearColor& LabelColor, const FLinearColor& ValueColor);
    UFUNCTION(BlueprintCallable, Category = "SF UI") void SetSelected(bool bInSelected);
    UFUNCTION(BlueprintPure, Category = "SF UI") bool IsSelected() const { return bSelected; }

    // Selection hook. The native body swaps the frame to SFHudStyle::RowBrush (or FramePiece); override it in the WBP.
    UFUNCTION(BlueprintNativeEvent, Category = "SF UI") void OnSelectedChanged(bool bNowSelected);

    UPROPERTY(EditAnywhere, Category = "SF Style") FName FramePiece;  // kit element for the frame; none = SFHudStyle::RowBrush
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle LabelStyle = FSFUiTextStyle(20, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle ValueStyle = FSFUiTextStyle(20, SFHudStyle::Ivory());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle CaptionStyle = FSFUiTextStyle(18, SFHudStyle::Brass());
    UPROPERTY(EditAnywhere, Category = "SF Style") FSFUiTextStyle HeadingStyle = FSFUiTextStyle(24, SFHudStyle::Ivory(), true);

protected:
    virtual void ApplyKitStyle() override;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UBorder> Frame;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UImage> Icon;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Caption;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Label;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Value;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Marker;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> DoneMark;
    UPROPERTY(BlueprintReadOnly, Category = "SF", meta = (BindWidgetOptional)) TObjectPtr<UWidget> Rule;
    bool bSelected = false;
    bool bHeadingShown = false;
};
