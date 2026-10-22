#include "UI/SFWidgetAuthoring.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "BaseWidgetBlueprint.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/ContentWidget.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Framework/SFHudStyle.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UI/SFHudWidgets.h"
#include "UI/SFInventoryWidget.h"
#include "UI/SFUiWidgets.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

using namespace SFHudStyle;

namespace
{
constexpr const TCHAR* CommonDir = TEXT("/Game/Frontier/UI/Widgets/Common");
constexpr const TCHAR* HudDir = TEXT("/Game/Frontier/UI/Widgets/Hud");
constexpr const TCHAR* UnitFramesDir = TEXT("/Game/Frontier/UI/Widgets/Unit");
constexpr const TCHAR* InventoryDir = TEXT("/Game/Frontier/UI/Widgets/Inventory");

// ---- tree helpers. Named widgets are Designer variables so the C++ BindWidget lookups find them. Fonts are never baked (the family is a
// runtime composite font); the C++ base applies them, so seeds only set text.
template <class T> T* N(UWidgetTree* Tree, const TCHAR* Name)
{
    T* Widget = Tree->ConstructWidget<T>(T::StaticClass(), FName(Name));
    Widget->bIsVariable = true;
    return Widget;
}
template <class T> T* A(UWidgetTree* Tree) { return Tree->ConstructWidget<T>(T::StaticClass()); }

UTextBlock* Txt(UWidgetTree* Tree, const TCHAR* Name, const TCHAR* Default)
{
    UTextBlock* Block = Name ? N<UTextBlock>(Tree, Name) : A<UTextBlock>(Tree);
    Block->SetText(FText::FromString(Default));
    Block->SetColorAndOpacity(FSlateColor(Ivory()));
    return Block;
}

// A kit piece drawn at its design size (or W x H); collapses when the kit is missing.
USizeBox* Piece(UWidgetTree* Tree, const TCHAR* Name, const TCHAR* KitName, float W = 0.f, float H = 0.f)
{
    USizeBox* Box = Name ? N<USizeBox>(Tree, Name) : A<USizeBox>(Tree);
    UImage* Img = A<UImage>(Tree);
    if (const FSFHudKitElement* Element = Kit(KitName))
    {
        Img->SetBrush(KitBrush(*Element));
        const FVector2D Design = KitDesignSize(*Element);
        Box->SetWidthOverride(W > 0.f ? W : static_cast<float>(Design.X));
        Box->SetHeightOverride(H > 0.f ? H : (W > 0.f ? W : static_cast<float>(Design.Y)));
    }
    else { Img->SetVisibility(ESlateVisibility::Collapsed); }
    Box->SetContent(Img);
    return Box;
}

USizeBox* Sized(UWidgetTree* Tree, const TCHAR* Name, UWidget* Child, float W, float H, float MinW = 0.f)
{
    USizeBox* Box = Name ? N<USizeBox>(Tree, Name) : A<USizeBox>(Tree);
    if (W > 0.f) { Box->SetWidthOverride(W); }
    if (H > 0.f) { Box->SetHeightOverride(H); }
    if (MinW > 0.f) { Box->SetMinDesiredWidth(MinW); }
    Box->SetContent(Child);
    return Box;
}

UBorder* Plain(UWidgetTree* Tree, const TCHAR* Name, const FMargin& Padding)
{
    UBorder* Border = Name ? N<UBorder>(Tree, Name) : A<UBorder>(Tree);
    Border->SetBrush(FSlateNoResource());
    Border->SetPadding(Padding);
    return Border;
}

UOverlaySlot* Put(UOverlay* Overlay, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Pad = FMargin(0.f))
{
    UOverlaySlot* Slot = Overlay->AddChildToOverlay(Child);
    Slot->SetHorizontalAlignment(H); Slot->SetVerticalAlignment(V); Slot->SetPadding(Pad);
    return Slot;
}

UCanvasPanelSlot* Anchor(UCanvasPanel* Root, UWidget* Child, float X, float Y, float AlignX, float AlignY)
{
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Child);
    Slot->SetAnchors(FAnchors(X, Y)); Slot->SetAlignment(FVector2D(AlignX, AlignY)); Slot->SetAutoSize(true); Slot->SetOffsets(FMargin(0.f));
    return Slot;
}

UClass* SharedClass(const TCHAR* Dir, const TCHAR* Name)
{
    return LoadClass<UUserWidget>(nullptr, *FString::Printf(TEXT("%s/%s.%s_C"), Dir, Name, Name));
}

// ---- layouts (first drafts; the owner rearranges them in the Designer)
void PanelFrame(UWidgetTree* T)
{
    UBorder* Plate = N<UBorder>(T, TEXT("Frame"));
    Plate->SetBrush(KitOrFrame(TEXT("panel_frame"), Slate(0.9f), Brass(), 3.f));
    Plate->SetPadding(FMargin(24.f, 20.f));
    UVerticalBox* Stack = A<UVerticalBox>(T);
    Stack->AddChildToVerticalBox(Txt(T, TEXT("Title"), TEXT("Title")))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Stack->AddChildToVerticalBox(N<UNamedSlot>(T, TEXT("Content")));
    Plate->SetContent(Stack);
    T->RootWidget = Plate;
}

void ButtonLayout(UWidgetTree* T, float Height, float MinWidth, const TCHAR* Default)
{
    UButton* Control = N<UButton>(T, TEXT("Button"));
    UBorder* Pad = Plain(T, nullptr, FMargin(24.f, 0.f));
    Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Center);
    UTextBlock* Label = Txt(T, TEXT("Label"), Default);
    Label->SetJustification(ETextJustify::Center);
    Pad->SetContent(Label);
    Control->SetContent(Pad);
    T->RootWidget = Sized(T, TEXT("RootSize"), Control, 0.f, Height, MinWidth);
}

void SlotLayout(UWidgetTree* T)
{
    UBorder* Plate = N<UBorder>(T, TEXT("Frame"));
    Plate->SetBrush(KitOrFrame(TEXT("slot_hotbar_normal"), WellFill(), Hairline(), 3.f));
    UOverlay* Stack = A<UOverlay>(T);
    Put(Stack, Sized(T, nullptr, N<UImage>(T, TEXT("Icon")), 56.f, 56.f), HAlign_Center, VAlign_Center);
    Put(Stack, Txt(T, TEXT("KeyLabel"), TEXT("")), HAlign_Left, VAlign_Top, FMargin(10.f, 6.f, 0.f, 0.f));
    Put(Stack, Txt(T, TEXT("CountLabel"), TEXT("")), HAlign_Right, VAlign_Bottom, FMargin(0.f, 0.f, 10.f, 6.f));
    Plate->SetContent(Stack);
    T->RootWidget = Sized(T, TEXT("RootSize"), Plate, 88.f, 88.f);
}

// Checkbox + label + count, with a hairline rule under headings.
void ListRowLayout(UWidgetTree* T)
{
    UVerticalBox* Column = A<UVerticalBox>(T);
    UHorizontalBox* Line = A<UHorizontalBox>(T);
    UOverlay* Marker = N<UOverlay>(T, TEXT("Marker"));
    Marker->AddChildToOverlay(Piece(T, nullptr, TEXT("checkbox"), 22.f, 22.f));
    Marker->AddChildToOverlay(Piece(T, TEXT("DoneMark"), TEXT("status_tick"), 22.f, 22.f));
    UHorizontalBoxSlot* MarkSlot = Line->AddChildToHorizontalBox(Marker);
    MarkSlot->SetVerticalAlignment(VAlign_Center); MarkSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
    UTextBlock* Label = Txt(T, TEXT("Label"), TEXT("Objective"));
    Label->SetAutoWrapText(true);
    UHorizontalBoxSlot* LabelSlot = Line->AddChildToHorizontalBox(Label);
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(Txt(T, TEXT("Value"), TEXT("0 / 1")));
    ValueSlot->SetVerticalAlignment(VAlign_Center); ValueSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    Column->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 3.f));
    Column->AddChildToVerticalBox(Piece(T, TEXT("Rule"), TEXT("divider"), 244.f, 4.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    T->RootWidget = Column;
}

// 280 x 66 kit toast card: icon in its well, "Skill gain" over "<Skill> +1".
void ToastCardLayout(UWidgetTree* T)
{
    UBorder* Card = KitPanel(T, TEXT("toast_frame"), 1.f, Slate(0.92f), Brass(), FMargin(12.f, 4.f, 12.f, 4.f), 0.f);
    Card->Rename(TEXT("Frame")); Card->bIsVariable = true;
    UHorizontalBox* Line = A<UHorizontalBox>(T);
    Line->AddChildToHorizontalBox(Sized(T, nullptr, N<UImage>(T, TEXT("Icon")), 48.f, 48.f))->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* Lines = A<UVerticalBox>(T);
    Lines->AddChildToVerticalBox(Txt(T, TEXT("Caption"), TEXT("Skill gain")));
    UHorizontalBox* Gain = A<UHorizontalBox>(T);
    Gain->AddChildToHorizontalBox(Txt(T, TEXT("Label"), TEXT("Logging")));
    Gain->AddChildToHorizontalBox(Txt(T, TEXT("Value"), TEXT("+1")))->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));
    Lines->AddChildToVerticalBox(Gain);
    UHorizontalBoxSlot* LinesSlot = Line->AddChildToHorizontalBox(Lines);
    LinesSlot->SetVerticalAlignment(VAlign_Center); LinesSlot->SetPadding(FMargin(22.f, 0.f, 0.f, 0.f));
    Card->SetContent(Line);
    T->RootWidget = Sized(T, TEXT("RootSize"), Card, 280.f, 66.f);
}

// HudVitals-v6 parts (tools/ui/import_hud_vitals.py). The art is 2x its 1x display size.
constexpr const TCHAR* VitalsDir = TEXT("/Game/Frontier/UI/Kit/HudVitals");

UImage* VitalsImage(UWidgetTree* T, const TCHAR* Name, const TCHAR* Part, float W, float H)
{
    UImage* Img = Name ? N<UImage>(T, Name) : A<UImage>(T);
    FSlateBrush Brush;
    Brush.ImageSize = FVector2D(W, H);
    if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s/T_HudVitals_%s.T_HudVitals_%s"), VitalsDir, Part, Part))) { Brush.SetResourceObject(Tex); }
    Img->SetBrush(Brush);
    return Img;
}

// icon, then the bar: 9-slice frame, the masked fill (the bound image) over it (the frame's navy interior is opaque; the mask keeps the
// fill inside it and off the brass border), numeral on top. Sizes are 1x of the 600x44 art.
UWidget* VitalsRow(UWidgetTree* T, const TCHAR* IconPart, const TCHAR* MaterialName, const TCHAR* BarName, const TCHAR* TextName)
{
    constexpr float BarW = 300.f, BarH = 22.f;
    UHorizontalBox* Row = A<UHorizontalBox>(T);
    Row->AddChildToHorizontalBox(Sized(T, nullptr, VitalsImage(T, nullptr, IconPart, 22.f, 22.f), 22.f, 22.f))->SetVerticalAlignment(VAlign_Center);
    UOverlay* Bar = A<UOverlay>(T);
    UImage* FrameImg = VitalsImage(T, nullptr, TEXT("bar_frame"), BarW, BarH);
    FSlateBrush FrameBrush = FrameImg->GetBrush();
    FrameBrush.DrawAs = ESlateBrushDrawType::Box;
    FrameBrush.Margin = FMargin(16.f / 600.f, 8.f / 44.f, 34.f / 600.f, 8.f / 44.f);  // left 16, top 8, right 34, bottom 8 px of the 2x art
    FrameImg->SetBrush(FrameBrush);
    Put(Bar, FrameImg, HAlign_Fill, VAlign_Fill);
    UImage* Fill = N<UImage>(T, BarName);
    FSlateBrush FillBrush;
    FillBrush.ImageSize = FVector2D(BarW, BarH);
    if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), VitalsDir, MaterialName, MaterialName))) { FillBrush.SetResourceObject(Mat); }
    Fill->SetBrush(FillBrush);
    Put(Bar, Fill, HAlign_Fill, VAlign_Fill);
    Put(Bar, Txt(T, TextName, TEXT("")), HAlign_Center, VAlign_Center);
    Row->AddChildToHorizontalBox(Sized(T, nullptr, Bar, BarW, BarH));
    return Row;
}

void VitalsLayout(UWidgetTree* T)
{
    UHorizontalBox* Row = A<UHorizontalBox>(T);
    UOverlay* Badge = A<UOverlay>(T);
    Badge->AddChildToOverlay(VitalsImage(T, nullptr, TEXT("badge_frame"), 64.f, 84.f));
    Put(Badge, Sized(T, nullptr, VitalsImage(T, nullptr, TEXT("badge_stag"), 40.f, 44.f), 40.f, 44.f), HAlign_Center, VAlign_Top, FMargin(0.f, 10.f, 0.f, 0.f));
    Put(Badge, Txt(T, TEXT("LevelText"), TEXT("1")), HAlign_Center, VAlign_Bottom, FMargin(0.f, 0.f, 0.f, 5.f));
    Row->AddChildToHorizontalBox(Sized(T, nullptr, Badge, 64.f, 84.f))->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* Bars = A<UVerticalBox>(T);
    Bars->AddChildToVerticalBox(VitalsRow(T, TEXT("icon_heart"), TEXT("MI_UI_BarFill_Health"), TEXT("HealthBar"), TEXT("HealthText")));
    Bars->AddChildToVerticalBox(VitalsRow(T, TEXT("icon_stamina_bolt"), TEXT("MI_UI_BarFill_Stamina"), TEXT("StaminaBar"), TEXT("StaminaText")));
    UHorizontalBoxSlot* BarsSlot = Row->AddChildToHorizontalBox(Bars);
    BarsSlot->SetVerticalAlignment(VAlign_Center); BarsSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
    T->RootWidget = Row;
}

// The widget From's ancestor that sits directly under an ancestor of Anchor (the sibling branch holding From).
UWidget* BranchOf(UWidget* From, UWidget* Anchor)
{
    TSet<const UWidget*> Up;
    for (const UWidget* P = Anchor ? Anchor->GetParent() : nullptr; P; P = P->GetParent()) { Up.Add(P); }
    for (UWidget* W = From; W && W->GetParent(); W = W->GetParent()) { if (Up.Contains(W->GetParent())) { return W; } }
    return nullptr;
}

// Idempotent: names the badge SizeBox BadgeRoot and the stamina row StaminaRow (renaming the existing widgets, so the Designer layout is
// untouched) and adds NameText above the health row. Returns true when it changed the tree. Blueprint may be null (seed: the caller registers variables).
bool AddUnitParts(UWidgetTree* T, UWidgetBlueprint* Blueprint)
{
    bool bChanged = false;
    auto Mark = [&](UWidget* W, const TCHAR* Name)
    {
        if (!W || T->FindWidget(FName(Name))) { return; }
        W->Rename(Name, T); W->bIsVariable = true;
        if (Blueprint) { Blueprint->OnVariableAdded(W->GetFName()); }
        bChanged = true;
    };
    UWidget* Health = T->FindWidget(TEXT("HealthBar"));
    if (!Health) { return false; }
    Mark(BranchOf(T->FindWidget(TEXT("LevelText")), Health), TEXT("BadgeRoot"));
    Mark(BranchOf(T->FindWidget(TEXT("StaminaBar")), Health), TEXT("StaminaRow"));
    if (!T->FindWidget(TEXT("NameText")))
    {
        UWidget* HealthRow = nullptr;
        UVerticalBox* Column = nullptr;
        for (UWidget* W = Health; W && W->GetParent(); W = W->GetParent())
        {
            if (UVerticalBox* Box = Cast<UVerticalBox>(W->GetParent())) { Column = Box; HealthRow = W; break; }
        }
        if (Column)
        {
            UTextBlock* Name = N<UTextBlock>(T, TEXT("NameText"));
            Name->SetText(FText::FromString(TEXT("Name")));
            Name->SetColorAndOpacity(FSlateColor(Ivory()));
            Name->SetJustification(ETextJustify::Left);
            if (const UTextBlock* Value = Cast<UTextBlock>(T->FindWidget(TEXT("HealthText"))))
            {
                FSlateFontInfo Font = Value->GetFont();  // the value text family, a little larger
                Font.Size += 4;
                Name->SetFont(Font);
            }
            float IconWidth = 0.f;  // line the name up with the bar, not the icon in front of it
            if (const UPanelWidget* Row = Cast<UPanelWidget>(HealthRow); Row && Row->GetChildrenCount() > 0)
            { if (const USizeBox* Icon = Cast<USizeBox>(Row->GetChildAt(0))) { IconWidth = Icon->GetWidthOverride(); } }
            UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Column->InsertChildAt(0, Name));
            if (Slot) { Slot->SetHorizontalAlignment(HAlign_Left); Slot->SetPadding(FMargin(IconWidth, 0.f, 0.f, 2.f)); }
            if (Blueprint) { Blueprint->OnVariableAdded(Name->GetFName()); }
            bChanged = true;
        }
    }
    return bChanged;
}

void UnitFrameLayout(UWidgetTree* T)
{
    VitalsLayout(T);
    AddUnitParts(T, nullptr);
}

// Hud additions: Target (WBP_TargetFrame, top centre, hidden), PartyList (left, empty) and RaidGrid (empty). True when something was added.
bool AddHudParts(UWidgetTree* T, UCanvasPanel* Root, UWidgetBlueprint* Blueprint)
{
    bool bChanged = false;
    if (!T->FindWidget(TEXT("Target")))
    {
        if (UClass* Class = SharedClass(UnitFramesDir, TEXT("WBP_TargetFrame")))
        {
            UUserWidget* Frame = T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(Class), FName(TEXT("Target")));
            Frame->bIsVariable = true; Frame->SetVisibility(ESlateVisibility::Collapsed);
            Anchor(Root, Frame, 0.4956f, 0.0296f, 0.5f, 0.f);  // centre 951.5 of 1920, y 32 px of 1080: where the old Slate frame sat
            if (Blueprint) { Blueprint->OnVariableAdded(Frame->GetFName()); }
            bChanged = true;
        }
    }
    if (!T->FindWidget(TEXT("PartyList")))
    {
        UVerticalBox* List = N<UVerticalBox>(T, TEXT("PartyList"));
        List->SetVisibility(ESlateVisibility::Collapsed);
        Anchor(Root, List, 0.02f, 0.25f, 0.f, 0.f);
        if (Blueprint) { Blueprint->OnVariableAdded(List->GetFName()); }
        bChanged = true;
    }
    if (!T->FindWidget(TEXT("RaidGrid")))
    {
        UWrapBox* Grid = N<UWrapBox>(T, TEXT("RaidGrid"));
        Grid->SetExplicitWrapSize(true); Grid->SetWrapSize(480.f);
        Grid->SetVisibility(ESlateVisibility::Collapsed);
        Anchor(Root, Grid, 0.02f, 0.45f, 0.f, 0.f);
        if (Blueprint) { Blueprint->OnVariableAdded(Grid->GetFName()); }
        bChanged = true;
    }
    return bChanged;
}

bool CompileAndSave(UWidgetBlueprint* Blueprint)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->GetPackage()->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    const bool bSaved = UPackage::SavePackage(Blueprint->GetPackage(), Blueprint, *FPackageName::LongPackageNameToFilename(Blueprint->GetPackage()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
    return bSaved && Blueprint->Status != BS_Error && Blueprint->Status != BS_UpToDateWithWarnings;
}

// Owner-edited WBP_Hud: swap the Vitals slot to WBP_PlayerFrame (same canvas slot), then add the Target/party parts. Idempotent.
FString MigrateHud()
{
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *FString::Printf(TEXT("%s/WBP_Hud.WBP_Hud"), HudDir));
    UCanvasPanel* Root = Blueprint && Blueprint->WidgetTree ? Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget) : nullptr;
    UClass* PlayerFrame = SharedClass(UnitFramesDir, TEXT("WBP_PlayerFrame"));
    if (!Root || !PlayerFrame) { return TEXT("skip   WBP_Hud migration (Hud or WBP_PlayerFrame missing)\n"); }
    UWidgetTree* T = Blueprint->WidgetTree;
    bool bChanged = false;
    UWidget* Old = T->FindWidget(TEXT("Vitals"));
    if (Old && Old->GetClass() != PlayerFrame)
    {
        const UCanvasPanelSlot* OldSlot = Cast<UCanvasPanelSlot>(Old->Slot);
        const FAnchorData Layout = OldSlot ? OldSlot->GetLayout() : FAnchorData();
        const int32 ZOrder = OldSlot ? OldSlot->GetZOrder() : 0;
        const bool bAutoSize = OldSlot ? OldSlot->GetAutoSize() : true;
        Old->Rename(TEXT("Vitals_old"), T);
        T->RemoveWidget(Old);
        UUserWidget* Frame = T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(PlayerFrame), FName(TEXT("Vitals")));
        Frame->bIsVariable = true;
        UCanvasPanelSlot* NewSlot = Root->AddChildToCanvas(Frame);
        NewSlot->SetLayout(Layout); NewSlot->SetZOrder(ZOrder); NewSlot->SetAutoSize(bAutoSize);
        bChanged = true;
    }
    bChanged |= AddHudParts(T, Root, Blueprint);
    // A unit-frame instance carries its class's part toggles (a template built mid-run can hold the parent's): resync them.
    TArray<UWidget*> All;
    T->GetAllWidgets(All);
    for (UWidget* W : All)
    {
        USFUnitFrameWidget* Unit = Cast<USFUnitFrameWidget>(W);
        const USFUnitFrameWidget* Class = Unit ? Cast<USFUnitFrameWidget>(Unit->GetClass()->GetDefaultObject()) : nullptr;
        if (!Class || (Unit->bShowBadge == Class->bShowBadge && Unit->bShowName == Class->bShowName && Unit->bShowStamina == Class->bShowStamina && Unit->bShowValues == Class->bShowValues)) { continue; }
        Unit->bShowBadge = Class->bShowBadge; Unit->bShowName = Class->bShowName; Unit->bShowStamina = Class->bShowStamina; Unit->bShowValues = Class->bShowValues;
        bChanged = true;
    }
    if (!bChanged) { return TEXT("skip   WBP_Hud (already migrated)\n"); }
    return FString::Printf(TEXT("%s WBP_Hud migrated\n"), CompileAndSave(Blueprint) ? TEXT("rebuilt") : TEXT("FAIL   "));
}

void HotbarLayout(UWidgetTree* T)
{
    UHorizontalBox* Row = N<UHorizontalBox>(T, TEXT("SlotRow"));
    UClass* SlotClass = SharedClass(CommonDir, TEXT("WBP_Slot"));
    for (int32 Index = 1; Index <= USFHudHotbarWidget::SlotCount; ++Index)
    {
        UUserWidget* Cell = T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(SlotClass), FName(*FString::Printf(TEXT("Slot%d"), Index)));
        if (Cell) { Cell->bIsVariable = true; Row->AddChildToHorizontalBox(Cell)->SetPadding(FMargin(6.f, 0.f)); }
    }
    T->RootWidget = Row;
}

void MinimapLayout(UWidgetTree* T)
{
    UVerticalBox* Column = A<UVerticalBox>(T);
    UOverlay* Disc = A<UOverlay>(T);
    UBorder* Fill = A<UBorder>(T);
    Fill->SetBrush(Frame(Hex(TEXT("0E171C"), 0.92f), FLinearColor(0.f, 0.f, 0.f, 0.f), 125.f));
    Put(Disc, Fill, HAlign_Fill, VAlign_Fill);
    if (const FSFHudKitElement* MapArt = Kit(TEXT("greenlands_map_base")))
    {
        UImage* Map = N<UImage>(T, TEXT("MapImage"));
        FSlateBrush Brush; Brush.SetResourceObject(MapArt->Texture); Brush.ImageSize = FVector2D(250.f, 250.f);
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius; Brush.OutlineSettings.CornerRadii = FVector4(125.f, 125.f, 125.f, 125.f);
        Map->SetBrush(Brush);
        Put(Disc, Map, HAlign_Fill, VAlign_Fill, FMargin(10.f));
    }
    if (const FSFHudKitElement* RingArt = Kit(TEXT("minimap_ring")))
    {
        UImage* Ring = N<UImage>(T, TEXT("Ring"));
        Ring->SetBrush(KitBrush(*RingArt));
        Put(Disc, Ring, HAlign_Fill, VAlign_Fill);
    }
    Put(Disc, Piece(T, TEXT("PlayerArrow"), TEXT("player_arrow"), 24.f), HAlign_Center, VAlign_Center);
    Column->AddChildToVerticalBox(Sized(T, TEXT("DiscSize"), Disc, 250.f, 250.f));
    UBorder* Footer = KitPanel(T, TEXT("minimap_footer"), 1.f, Slate(0.85f), Brass(), FMargin(4.f, 0.f), 3.f);
    Footer->SetVerticalAlignment(VAlign_Center);
    UTextBlock* Zone = Txt(T, TEXT("ZoneText"), TEXT("Silverbrook"));
    Zone->SetJustification(ETextJustify::Center); Zone->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Footer->SetContent(Zone);
    UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Sized(T, nullptr, Footer, 180.f, 32.f));
    FooterSlot->SetHorizontalAlignment(HAlign_Center); FooterSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
    T->RootWidget = Column;
}

void QuestTrackerLayout(UWidgetTree* T)
{
    UBorder* Plate = KitPanel(T, TEXT("quest_tracker_frame"), 1.f, Slate(0.88f), Brass(), FMargin(10.f, 12.f, 10.f, 14.f), 0.f);
    Plate->Rename(TEXT("Frame")); Plate->bIsVariable = true;
    Plate->SetContent(N<UVerticalBox>(T, TEXT("Rows")));
    T->RootWidget = Sized(T, TEXT("RootSize"), Plate, 286.f, 0.f);
}

void ToastsLayout(UWidgetTree* T)
{
    UVerticalBox* Column = N<UVerticalBox>(T, TEXT("CardRows"));
    UClass* CardClass = SharedClass(HudDir, TEXT("WBP_HudToastCard"));
    for (int32 Index = 1; Index <= FSFToastQueue::MaxVisible; ++Index)
    {
        UUserWidget* Card = T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(CardClass), FName(*FString::Printf(TEXT("Card%d"), Index)));
        if (Card) { Card->bIsVariable = true; Column->AddChildToVerticalBox(Card)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f)); }
    }
    T->RootWidget = Column;
}

// Percent anchors from docs/blueprints/ui-hud.md: vitals x2% bottom y97.4%, hotbar bottom y95%, minimap right x99% top y2%, tracker x84% y31.3%, toasts x84% y47%.
void HudLayout(UWidgetTree* T)
{
    UCanvasPanel* Root = N<UCanvasPanel>(T, TEXT("Root"));
    auto Child = [&](const TCHAR* Name, const TCHAR* WbpName, float X, float Y, float AlignX, float AlignY, const TCHAR* Dir = HudDir)
    {
        UClass* Class = SharedClass(Dir, WbpName);
        UUserWidget* Widget = T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(Class), FName(Name));
        if (!Widget) { return; }
        Widget->bIsVariable = true;
        Anchor(Root, Widget, X, Y, AlignX, AlignY);
    };
    Child(TEXT("Vitals"), TEXT("WBP_PlayerFrame"), 0.02f, 0.974f, 0.f, 1.f, UnitFramesDir);
    Child(TEXT("Hotbar"), TEXT("WBP_HudHotbar"), 0.5f, 0.95f, 0.5f, 1.f);
    Child(TEXT("Minimap"), TEXT("WBP_HudMinimap"), 0.99f, 0.02f, 1.f, 0.f);
    Child(TEXT("QuestTracker"), TEXT("WBP_HudQuestTracker"), 0.84f, 0.313f, 0.f, 0.f);
    Child(TEXT("Toasts"), TEXT("WBP_HudToasts"), 0.84f, 0.47f, 0.f, 0.f);
    AddHudParts(T, Root, nullptr);
    T->RootWidget = Root;
}

// ---- inventory (WBP_Inventory, rough start from the old code-built panel and v5 inventory.png; the owner lays it out in the Designer)
UUserWidget* Inst(UWidgetTree* T, const TCHAR* Dir, const TCHAR* Wbp, const TCHAR* Name)
{
    UClass* Class = SharedClass(Dir, Wbp);
    UUserWidget* Widget = Class ? T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(Class), Name ? FName(Name) : NAME_None) : nullptr;
    if (Widget && Name) { Widget->bIsVariable = true; }
    return Widget;
}

void InventoryLayout(UWidgetTree* T)
{
    UCanvasPanel* Root = N<UCanvasPanel>(T, TEXT("Root"));
    // The panel frame (WBP_PanelFrame) at the concept rect (85,31) 575 x 887 of 1920 x 1080, the body over it.
    UUserWidget* Frame = Inst(T, CommonDir, TEXT("WBP_PanelFrame"), TEXT("Panel"));
    UCanvasPanel* Body = A<UCanvasPanel>(T);
    auto Place = [Body](UWidget* Child, float X, float Y)
    {
        if (!Child) { return; }
        UCanvasPanelSlot* Placed = Body->AddChildToCanvas(Child);
        Placed->SetAutoSize(true); Placed->SetPosition(FVector2D(X, Y));
    };
    // Tabs and close.
    UHorizontalBox* Tabs = A<UHorizontalBox>(T);
    for (const TCHAR* Name : {TEXT("TabAll"), TEXT("TabMaterials"), TEXT("TabTools"), TEXT("TabFood")})
    { if (UUserWidget* Tab = Inst(T, CommonDir, TEXT("WBP_Tab"), Name)) { Tabs->AddChildToHorizontalBox(Tab)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); } }
    Place(Tabs, 0.f, 0.f);
    Place(Inst(T, CommonDir, TEXT("WBP_Button"), TEXT("CloseButton")), 440.f, 0.f);
    // Equipment: five armour slots left, the paperdoll, the main hand (the equipped tool) and three more right. Only the main hand is live today.
    UVerticalBox* Left = A<UVerticalBox>(T);
    for (int32 Index = 0; Index < 5; ++Index) { if (UUserWidget* Cell = Inst(T, CommonDir, TEXT("WBP_Slot"), nullptr)) { Left->AddChildToVerticalBox(Cell)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f)); } }
    Place(Left, 0.f, 60.f);
    Place(Piece(T, nullptr, TEXT("paperdoll_adventurer"), 158.f, 390.f), 180.f, 70.f);
    UVerticalBox* Right = A<UVerticalBox>(T);
    for (int32 Index = 0; Index < 4; ++Index) { if (UUserWidget* Cell = Inst(T, CommonDir, TEXT("WBP_Slot"), Index == 0 ? TEXT("MainHandSlot") : nullptr)) { Right->AddChildToVerticalBox(Cell)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f)); } }
    Place(Right, 400.f, 60.f);
    // The bag: C++ fills SlotGrid with one WBP_Slot per backpack slot.
    UUniformGridPanel* Grid = N<UUniformGridPanel>(T, TEXT("SlotGrid"));
    Grid->SetSlotPadding(FMargin(2.f));
    UOverlay* GridStack = A<UOverlay>(T);
    GridStack->AddChildToOverlay(Grid);
    UTextBlock* Empty = Txt(T, TEXT("EmptyText"), TEXT("Your pack is empty."));
    Put(GridStack, Empty, HAlign_Center, VAlign_Center);
    Place(GridStack, 0.f, 540.f);
    // Footer.
    Place(Txt(T, TEXT("WeightText"), TEXT("Weight 0.0 kg")), 0.f, 760.f);
    UHorizontalBox* Gold = N<UHorizontalBox>(T, TEXT("GoldRow"));
    Gold->AddChildToHorizontalBox(Txt(T, TEXT("GoldText"), TEXT("Gold 0")))->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
    Gold->AddChildToHorizontalBox(Piece(T, nullptr, Kit(TEXT("icon_coin")) ? TEXT("icon_coin") : TEXT("icon_coin_stack"), 20.f))->SetVerticalAlignment(VAlign_Center);
    Gold->SetVisibility(ESlateVisibility::Collapsed);
    Place(Gold, 380.f, 760.f);
    if (Frame)
    {
        // A nested WBP cannot hold tree widgets in its NamedSlot from here, so the shared frame is the backdrop and the body sits over it.
        if (USFPanelFrameWidget* Titled = Cast<USFPanelFrameWidget>(Frame)) { Titled->TitleText = FText::FromString(TEXT("Inventory")); }
        UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Sized(T, nullptr, Frame, 575.f, 887.f));
        FrameSlot->SetAnchors(FAnchors(85.f / 1920.f, 31.f / 1080.f)); FrameSlot->SetOffsets(FMargin(0.f)); FrameSlot->SetAutoSize(true);
    }
    UCanvasPanelSlot* BodySlot = Root->AddChildToCanvas(Body);
    BodySlot->SetAnchors(FAnchors(85.f / 1920.f, 31.f / 1080.f)); BodySlot->SetOffsets(FMargin(24.f, 70.f, 0.f, 0.f)); BodySlot->SetAutoSize(true);
    // The tooltip sits beside the panel (587,120): art, name, type line, quantity and weight rows, description.
    UBorder* Tip = KitPanel(T, TEXT("tooltip_frame"), 1.f, Slate(0.96f), Brass(), FMargin(16.f, 12.f), 3.f);
    Tip->Rename(TEXT("Tooltip")); Tip->bIsVariable = true;
    UHorizontalBox* TipLayout = A<UHorizontalBox>(T);
    TipLayout->AddChildToHorizontalBox(Sized(T, nullptr, N<UImage>(T, TEXT("TipIcon")), 120.f, 178.f))->SetVerticalAlignment(VAlign_Top);
    UVerticalBox* TipText = A<UVerticalBox>(T);
    TipText->AddChildToVerticalBox(Txt(T, TEXT("TipTitle"), TEXT("Item")));
    TipText->AddChildToVerticalBox(Txt(T, TEXT("TipType"), TEXT("Type")));
    TipText->AddChildToVerticalBox(Txt(T, TEXT("TipQuantity"), TEXT("0")));
    TipText->AddChildToVerticalBox(Txt(T, TEXT("TipWeight"), TEXT("0.0 kg")));
    UTextBlock* Description = Txt(T, TEXT("TipDescription"), TEXT(""));
    Description->SetAutoWrapText(true);
    TipText->AddChildToVerticalBox(Sized(T, nullptr, Description, 300.f, 0.f));
    TipLayout->AddChildToHorizontalBox(TipText)->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    Tip->SetContent(TipLayout);
    Tip->SetVisibility(ESlateVisibility::Collapsed);
    UCanvasPanelSlot* TipSlot = Root->AddChildToCanvas(Tip);
    TipSlot->SetAnchors(FAnchors(587.f / 1920.f, 120.f / 1080.f)); TipSlot->SetAutoSize(true); TipSlot->SetOffsets(FMargin(0.f));
    T->RootWidget = Root;
}

struct FSpec
{
    const TCHAR* Dir;
    const TCHAR* Name;
    UClass* Parent;
    TFunction<void(UWidgetTree*)> Layout;
    TFunction<void(UObject*)> Defaults;
    const TCHAR* ParentWbp = nullptr;  // parent is that Widget Blueprint (resolved at build time) instead of Parent
};

// Dependency order: a WBP that nests another is listed after it.
TArray<FSpec> Specs()
{
    auto Row = [](UObject* Cdo) { return Cast<USFListRowWidget>(Cdo); };
    // Badge, name, stamina, values.
    auto Parts = [](bool bBadge, bool bName, bool bStamina, bool bValues)
    {
        return TFunction<void(UObject*)>([=](UObject* Cdo)
        {
            USFUnitFrameWidget* Frame = Cast<USFUnitFrameWidget>(Cdo);
            Frame->bShowBadge = bBadge; Frame->bShowName = bName; Frame->bShowStamina = bStamina; Frame->bShowValues = bValues;
        });
    };
    return {
        {CommonDir, TEXT("WBP_PanelFrame"), USFPanelFrameWidget::StaticClass(), PanelFrame, nullptr},
        {CommonDir, TEXT("WBP_Button"), USFButtonWidget::StaticClass(), [](UWidgetTree* T) { ButtonLayout(T, 56.f, 160.f, TEXT("Button")); }, nullptr},
        {CommonDir, TEXT("WBP_ButtonPrimary"), USFButtonWidget::StaticClass(), [](UWidgetTree* T) { ButtonLayout(T, 72.f, 160.f, TEXT("Primary")); },
            [](UObject* Cdo) { Cast<USFButtonWidget>(Cdo)->Kind = ESFUiButtonKind::Primary; }},
        {CommonDir, TEXT("WBP_Tab"), USFTabWidget::StaticClass(), [](UWidgetTree* T) { ButtonLayout(T, 44.f, 100.f, TEXT("Tab")); }, nullptr},
        {CommonDir, TEXT("WBP_Slot"), USFSlotWidget::StaticClass(), SlotLayout, nullptr},
        {CommonDir, TEXT("WBP_ListRow"), USFListRowWidget::StaticClass(), ListRowLayout, nullptr},
        {HudDir, TEXT("WBP_HudQuestRow"), USFListRowWidget::StaticClass(), ListRowLayout, nullptr},
        {HudDir, TEXT("WBP_HudToastCard"), USFListRowWidget::StaticClass(), ToastCardLayout,
            [Row](UObject* Cdo)
            {
                USFListRowWidget* Card = Row(Cdo);
                Card->FramePiece = TEXT("toast_frame");
                Card->LabelStyle = FSFUiTextStyle(22, Ivory());
                Card->ValueStyle = FSFUiTextStyle(22, Moss());
            }},
        {UnitFramesDir, TEXT("WBP_UnitFrame"), USFUnitFrameWidget::StaticClass(), UnitFrameLayout, nullptr},
        {UnitFramesDir, TEXT("WBP_PlayerFrame"), nullptr, nullptr, Parts(true, false, true, true), TEXT("WBP_UnitFrame")},
        {UnitFramesDir, TEXT("WBP_TargetFrame"), nullptr, nullptr, Parts(true, true, false, true), TEXT("WBP_UnitFrame")},
        {UnitFramesDir, TEXT("WBP_PartyFrame"), nullptr, nullptr, Parts(true, true, true, false), TEXT("WBP_UnitFrame")},
        {UnitFramesDir, TEXT("WBP_RaidFrame"), nullptr, nullptr, Parts(false, true, false, false), TEXT("WBP_UnitFrame")},
        {HudDir, TEXT("WBP_HudHotbar"), USFHudHotbarWidget::StaticClass(), HotbarLayout, nullptr},
        {HudDir, TEXT("WBP_HudMinimap"), USFHudMinimapWidget::StaticClass(), MinimapLayout, nullptr},
        {HudDir, TEXT("WBP_HudQuestTracker"), USFHudQuestTrackerWidget::StaticClass(), QuestTrackerLayout, nullptr},
        {HudDir, TEXT("WBP_HudToasts"), USFHudToastsWidget::StaticClass(), ToastsLayout, nullptr},
        {HudDir, TEXT("WBP_Hud"), USFHudWidget::StaticClass(), HudLayout, nullptr},
        {InventoryDir, TEXT("WBP_Inventory"), USFInventoryWidget::StaticClass(), InventoryLayout, nullptr},
    };
}
}

FString USFWidgetAuthoring::DumpTree(const FString& AssetPath)
{
    UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *AssetPath);
    if (!Blueprint || !Blueprint->WidgetTree) { return TEXT("cannot load ") + AssetPath; }
    FString Out = FString::Printf(TEXT("parent=%s\n"), Blueprint->ParentClass ? *Blueprint->ParentClass->GetPathName() : TEXT("none"));
    TFunction<void(UWidget*, int32)> Walk = [&](UWidget* W, int32 Depth)
    {
        Out += FString::Printf(TEXT("%s%s [%s]%s vis=%d\n"), *FString::ChrN(Depth * 2, ' '), *W->GetName(), *W->GetClass()->GetName(), W->bIsVariable ? TEXT(" VAR") : TEXT(""), static_cast<int32>(W->GetVisibility()));
        if (const USFUnitFrameWidget* Unit = Cast<USFUnitFrameWidget>(W))
        {
            Out += FString::Printf(TEXT("%s    parts badge=%d name=%d stamina=%d values=%d kitstyle=%d\n"), *FString::ChrN(Depth * 2, ' '), Unit->bShowBadge, Unit->bShowName, Unit->bShowStamina, Unit->bShowValues, Unit->bApplyKitStyle);
        }
        if (const UTextBlock* Text = Cast<UTextBlock>(W))
        {
            const FSlateFontInfo& F = Text->GetFont();
            Out += FString::Printf(TEXT("%s    font=%s size=%.0f face=%s color=%s\n"), *FString::ChrN(Depth * 2, ' '), F.FontObject ? *F.FontObject->GetPathName() : TEXT("none"), F.Size, *F.TypefaceFontName.ToString(), *Text->GetColorAndOpacity().GetSpecifiedColor().ToString());
        }
        if (const UPanelWidget* Panel = Cast<UPanelWidget>(W)) { for (UWidget* Child : Panel->GetAllChildren()) { Walk(Child, Depth + 1); } }
        else if (const UContentWidget* Holder = Cast<UContentWidget>(W)) { if (Holder->GetContent()) { Walk(Holder->GetContent(), Depth + 1); } }
    };
    if (Blueprint->WidgetTree->RootWidget) { Walk(Blueprint->WidgetTree->RootWidget, 0); }
    return Out;
}

FString USFWidgetAuthoring::BuildAll(bool bOverwrite, const FString& Only)
{
    FString Report;
    if (Only.IsEmpty() || Only == TEXT("WBP_UnitFrame"))  // the owner hand-laid WBP_UnitFrame (duplicated from WBP_HudVitals): add the new named parts
    {
        UWidgetBlueprint* Frame = bOverwrite ? nullptr : LoadObject<UWidgetBlueprint>(nullptr, *FString::Printf(TEXT("%s/WBP_UnitFrame.WBP_UnitFrame"), UnitFramesDir));
        // Kit style is fill-only now (project font family where the Designer left the default; never sizes, colours or layout), so it is on.
        USFUnitFrameWidget* Defaults = Frame && Frame->GeneratedClass ? Cast<USFUnitFrameWidget>(Frame->GeneratedClass->GetDefaultObject()) : nullptr;
        const bool bTurnStyleOn = Defaults && !Defaults->bApplyKitStyle;
        if (bTurnStyleOn) { Defaults->bApplyKitStyle = true; }
        if (Frame && Frame->WidgetTree && (AddUnitParts(Frame->WidgetTree, Frame) || bTurnStyleOn)) { Report += FString::Printf(TEXT("%s WBP_UnitFrame parts added\n"), CompileAndSave(Frame) ? TEXT("rebuilt") : TEXT("FAIL   ")); }
    }
    for (const FSpec& Spec : Specs())
    {
        if (!Only.IsEmpty() && Only != Spec.Name) { continue; }
        const FString PackageName = FString::Printf(TEXT("%s/%s"), Spec.Dir, Spec.Name);
        const bool bExists = FPackageName::DoesPackageExist(PackageName);
        if (bExists && !bOverwrite) { Report += FString::Printf(TEXT("skip   %s (exists; --force rebuilds it and discards layout edits)\n"), *PackageName); continue; }

        UWidgetBlueprint* Blueprint = nullptr;
        UPackage* Package = nullptr;
        if (bExists)
        {
            Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, Spec.Name));
            if (!Blueprint) { Report += FString::Printf(TEXT("FAIL   %s (cannot load)\n"), *PackageName); continue; }
            Package = Blueprint->GetPackage();
            Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, NAME_None, RF_Transactional);  // the old tree is garbage collected
        }
        else
        {
            Package = CreatePackage(*PackageName);
            UClass* Parent = Spec.ParentWbp ? SharedClass(UnitFramesDir, Spec.ParentWbp) : Spec.Parent;
            if (!Parent) { Report += FString::Printf(TEXT("FAIL   %s (parent class missing)\n"), *PackageName); continue; }
            Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, FName(Spec.Name), BPTYPE_Normal, TSubclassOf<UUserWidget>(Parent), nullptr, NAME_None, false);
            if (!Blueprint) { Report += FString::Printf(TEXT("FAIL   %s (create)\n"), *PackageName); continue; }
            FAssetRegistryModule::AssetCreated(Blueprint);
        }

        if (Spec.Layout) { Spec.Layout(Blueprint->WidgetTree); }
        // Register every named widget as a Designer variable (GUID map), or the compiler finds no BindWidget targets.
        TArray<UWidget*> Widgets;
        Blueprint->WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets) { if (Widget->bIsVariable) { Blueprint->OnVariableAdded(Widget->GetFName()); } }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        if (Spec.Defaults && Blueprint->GeneratedClass) { Spec.Defaults(Blueprint->GeneratedClass->GetDefaultObject()); }
        Package->MarkPackageDirty();

        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        const bool bSaved = UPackage::SavePackage(Package, Blueprint, *FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension()), Args);
        const bool bOk = bSaved && Blueprint->Status != BS_Error && Blueprint->Status != BS_UpToDateWithWarnings;  // a missing BindWidget is a compile warning
        Report += FString::Printf(TEXT("%s %s%s\n"), bOk ? (bExists ? TEXT("rebuilt") : TEXT("created")) : TEXT("FAIL   "), *PackageName,
            Blueprint->Status == BS_Error || Blueprint->Status == BS_UpToDateWithWarnings ? TEXT(" (compile errors or warnings: open the asset)") : bSaved ? TEXT("") : TEXT(" (save failed)"));
    }
    if (Only.IsEmpty() || Only == TEXT("WBP_Hud")) { Report += MigrateHud(); }
    return Report;
}
#endif
