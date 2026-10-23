#include "UI/SFWidgetAuthoring.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "BaseWidgetBlueprint.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/ComboBoxString.h"
#include "Components/ContentWidget.h"
#include "Components/EditableText.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Spacer.h"
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
#include "Exchange/SFExchangeWidgets.h"
#include "Gathering/SFStationPanel.h"
#include "Construction/SFConstructionSandboxPalette.h"
#include "Characters/SFE1VitalsHud.h"
#include "Dialogue/SFDialogue.h"
#include "Framework/SFGraphicsMenu.h"
#include "Gathering/SFGatheringSkillsPanel.h"
#include "Quest/SFQuestBoard.h"
#include "UI/SFGameMenu.h"
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
constexpr const TCHAR* ExchangeDir = TEXT("/Game/Frontier/UI/Widgets/Exchange");
constexpr const TCHAR* GatheringDir = TEXT("/Game/Frontier/UI/Widgets/Gathering");
constexpr const TCHAR* BuildDir = TEXT("/Game/Frontier/UI/Widgets/Build");
constexpr const TCHAR* QuestDir = TEXT("/Game/Frontier/UI/Widgets/Quest");
constexpr const TCHAR* DialogueDir = TEXT("/Game/Frontier/UI/Widgets/Dialogue");
constexpr const TCHAR* SettingsDir = TEXT("/Game/Frontier/UI/Widgets/Settings");
constexpr const TCHAR* MenuDir = TEXT("/Game/Frontier/UI/Widgets/Menu");

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

// ---- shared menu kit v7 (docs/dependency-map/ui.md "Shared menu kit v7"): art from tools/ui/import_kit_v7.py.
// Scale convention: the kit art is 2x. Slate draws Box 9-slice margins in texture pixels (never in brush ImageSize), so every kit widget
// is authored in ART PIXELS and WBP_WindowFrame draws its whole content at 0.5 (a ScaleBox at the window root). A kit widget used outside a
// window sits in its own 0.5 ScaleBox. Margins below are the README table (source pixels, L,T,R,B).
UUserWidget* Inst(UWidgetTree* T, const TCHAR* Dir, const TCHAR* Wbp, const TCHAR* Name)
{
    UClass* Class = SharedClass(Dir, Wbp);
    UUserWidget* Widget = Class ? T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(Class), Name ? FName(Name) : NAME_None) : nullptr;
    if (Widget && Name) { Widget->bIsVariable = true; }
    return Widget;
}

constexpr const TCHAR* KitV7Dir = TEXT("/Game/Frontier/UI/Kit/v7");
constexpr const TCHAR* InventoryKitDir = TEXT("/Game/Frontier/UI/Kit/Inventory");  // v6: tooltip frame, weight bar (not in kit v7)
constexpr float KitScale = 0.5f;

UTexture2D* KitTex(const TCHAR* Part) { return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s/T_KitV7_%s.T_KitV7_%s"), KitV7Dir, Part, Part)); }

// A kit part as a plain image at its art size (or W x H).
FSlateBrush KImg(const TCHAR* Part, float W = 0.f, float H = 0.f)
{
    FSlateBrush Brush;
    if (UTexture2D* Tex = KitTex(Part))
    {
        Brush.SetResourceObject(Tex);
        Brush.ImageSize = FVector2D(W > 0.f ? W : Tex->Source.GetSizeX(), H > 0.f ? H : Tex->Source.GetSizeY());
    }
    return Brush;
}

// A 9-slice part: margins L,T,R,B in source pixels; ImageSize is the art size unless W x H is given.
FSlateBrush KBox(const TCHAR* Part, float L, float Top, float R, float B, float W = 0.f, float H = 0.f)
{
    FSlateBrush Brush = KImg(Part, W, H);
    if (const UTexture2D* Tex = Cast<UTexture2D>(Brush.GetResourceObject()))
    {
        Brush.DrawAs = ESlateBrushDrawType::Box;
        Brush.Margin = FMargin(L / Tex->Source.GetSizeX(), Top / Tex->Source.GetSizeY(), R / Tex->Source.GetSizeX(), B / Tex->Source.GetSizeY());
    }
    return Brush;
}

FSlateBrush InvBrush(const TCHAR* Part, float W, float H, float L = 0.f, float Top = 0.f, float R = 0.f, float B = 0.f, float SrcW = 0.f, float SrcH = 0.f)
{
    FSlateBrush Brush;
    Brush.ImageSize = FVector2D(W, H);
    if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s/T_Inventory_%s.T_Inventory_%s"), InventoryKitDir, Part, Part))) { Brush.SetResourceObject(Tex); }
    if (SrcW > 0.f)
    {
        Brush.DrawAs = ESlateBrushDrawType::Box;
        Brush.Margin = FMargin(L / SrcW, Top / SrcH, R / SrcW, B / SrcH);
    }
    return Brush;
}

UImage* InvImage(UWidgetTree* T, const TCHAR* Name, const FSlateBrush& Brush, bool bHitInvisible = false)
{
    UImage* Img = Name ? N<UImage>(T, Name) : A<UImage>(T);
    Img->SetBrush(Brush);
    if (bHitInvisible) { Img->SetVisibility(ESlateVisibility::HitTestInvisible); }
    return Img;
}

UTextBlock* SizedText(UWidgetTree* T, const TCHAR* Name, const TCHAR* Default, float Size)
{
    UTextBlock* Block = Txt(T, Name, Default);
    FSlateFontInfo Font = Block->GetFont();
    Font.Size = Size;  // size only; the project family is filled in by the C++ base
    Block->SetFont(Font);
    return Block;
}

UScaleBox* Half(UWidgetTree* T, UWidget* Child)
{
    UScaleBox* Box = A<UScaleBox>(T);
    Box->SetStretch(EStretch::UserSpecified);
    Box->SetUserSpecifiedScale(KitScale);
    Box->SetContent(Child);
    return Box;
}

FSlateBrush InvRule(const TCHAR* Part, float W, float H, float Cap) { return InvBrush(Part, W, H, Cap, 0.f, Cap, 0.f, W, H); }

void StyleFace(UButton* Control, const FSlateBrush& Normal, const FSlateBrush& Hover, const FSlateBrush& Pressed, const FSlateBrush& Disabled)
{
    FButtonStyle Style = Control->GetStyle();
    Style.Normal = Normal; Style.Hovered = Hover; Style.Pressed = Pressed; Style.Disabled = Disabled;
    Style.NormalPadding = FMargin(0.f); Style.PressedPadding = FMargin(0.f);
    Control->SetStyle(Style);
}

UProgressBar* FillBar(UWidgetTree* T, const TCHAR* Name, const FSlateBrush& Fill)
{
    UProgressBar* Bar = N<UProgressBar>(T, Name);
    FProgressBarStyle Style = Bar->GetWidgetStyle();
    Style.SetBackgroundImage(FSlateNoResource());
    Style.SetFillImage(Fill);
    Bar->SetWidgetStyle(Style);
    Bar->SetPercent(0.f);
    return Bar;
}

// WBP_Separator: a thin brass rule; the horizontal brush is the seed, the vertical one is a class default picked by bVertical.
void SeparatorLayout(UWidgetTree* T) { T->RootWidget = InvImage(T, TEXT("Line"), KBox(TEXT("separator"), 40.f, 0.f, 40.f, 0.f, 1.f, 24.f)); }
void SeparatorDefaults(UObject* Cdo)
{
    USFSeparatorWidget* Rule = Cast<USFSeparatorWidget>(Cdo);
    Rule->HorizontalBrush = KBox(TEXT("separator"), 40.f, 0.f, 40.f, 0.f, 1.f, 24.f);
    Rule->VerticalBrush = KBox(TEXT("separator_vertical"), 0.f, 40.f, 0.f, 40.f, 24.f, 1.f);
}

// WBP_Button: the face over the hold fill. The face is semi-transparent; button_hold_fill (HoldFill) sits UNDER it.
void KitButtonLayout(UWidgetTree* T)
{
    UOverlay* Stack = A<UOverlay>(T);
    Put(Stack, FillBar(T, TEXT("HoldFill"), KBox(TEXT("button_hold_fill"), 18.f, 18.f, 18.f, 18.f)), HAlign_Fill, VAlign_Fill);
    UButton* Control = N<UButton>(T, TEXT("Button"));
    StyleFace(Control, KBox(TEXT("button_normal"), 20.f, 18.f, 20.f, 18.f), KBox(TEXT("button_hover"), 20.f, 18.f, 20.f, 18.f),
        KBox(TEXT("button_pressed"), 20.f, 18.f, 20.f, 18.f), KBox(TEXT("button_disabled"), 20.f, 18.f, 20.f, 18.f));
    UBorder* Pad = Plain(T, nullptr, FMargin(40.f, 0.f));
    Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Center);
    UTextBlock* Label = SizedText(T, TEXT("Label"), TEXT("Button"), 38.f);
    Label->SetJustification(ETextJustify::Center);
    Pad->SetContent(Label);
    Control->SetContent(Pad);
    Put(Stack, Control, HAlign_Fill, VAlign_Fill);
    T->RootWidget = Sized(T, TEXT("RootSize"), Stack, 0.f, 96.f, 240.f);
}

// WBP_Tab: tab_normal / tab_hover / tab_selected (class defaults below), icon above the label (the WBP can move it left).
void TabDefaults(UObject* Cdo)
{
    USFTabWidget* Tab = Cast<USFTabWidget>(Cdo);
    Tab->NormalBrush = KBox(TEXT("tab_normal"), 18.f, 16.f, 18.f, 16.f);
    Tab->HoverBrush = KBox(TEXT("tab_hover"), 18.f, 16.f, 18.f, 16.f);
    Tab->SelectedBrush = KBox(TEXT("tab_selected"), 18.f, 16.f, 18.f, 16.f);
    Tab->LabelStyle = FSFUiTextStyle(34, Ivory());
}

void KitTabLayout(UWidgetTree* T)
{
    UButton* Control = N<UButton>(T, TEXT("Button"));
    StyleFace(Control, KBox(TEXT("tab_normal"), 18.f, 16.f, 18.f, 16.f), KBox(TEXT("tab_hover"), 18.f, 16.f, 18.f, 16.f),
        KBox(TEXT("tab_selected"), 18.f, 16.f, 18.f, 16.f), KBox(TEXT("tab_normal"), 18.f, 16.f, 18.f, 16.f));
    UVerticalBox* Stack = A<UVerticalBox>(T);
    Stack->AddChildToVerticalBox(Sized(T, nullptr, N<UImage>(T, TEXT("TabIcon")), 44.f, 44.f))->SetHorizontalAlignment(HAlign_Center);
    UTextBlock* Label = SizedText(T, TEXT("Label"), TEXT("Tab"), 34.f);
    Label->SetJustification(ETextJustify::Center);
    // A label longer than its tab shrinks to fit instead of spilling over the border (never grows past its set size).
    UScaleBox* Fit = A<UScaleBox>(T);
    Fit->SetStretch(EStretch::ScaleToFit);
    Fit->SetStretchDirection(EStretchDirection::DownOnly);
    Fit->SetContent(Label);
    Stack->AddChildToVerticalBox(Fit)->SetHorizontalAlignment(HAlign_Fill);
    UBorder* Pad = Plain(T, nullptr, FMargin(12.f, 8.f));
    Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Center);
    Pad->SetContent(Stack);
    Control->SetContent(Pad);
    USizeBox* Root = Sized(T, TEXT("RootSize"), Control, 0.f, 0.f);
    Root->SetMinDesiredHeight(84.f);
    T->RootWidget = Root;
}

void TabBarLayout(UWidgetTree* T) { T->RootWidget = N<UHorizontalBox>(T, TEXT("TabRow")); }

// WBP_ItemSlot (208 x 208): slot_back, icon, hover and select frames (toggled by USFSelectableWidget), name fallback, stack count.
void ItemSlotDefaults(UObject* Cdo)
{
    USFSlotWidget* Slot = Cast<USFSlotWidget>(Cdo);
    Slot->bKitFrameSwap = false;
    Slot->bShowSingleCount = true;
    Slot->CountStyle = FSFUiTextStyle(36, Ivory(), false, true);
    Slot->KeyStyle = FSFUiTextStyle(26, Ivory());
}

void ItemSlotLayout(UWidgetTree* T)
{
    UOverlay* Stack = A<UOverlay>(T);
    UBorder* Plate = N<UBorder>(T, TEXT("Frame"));
    Plate->SetBrush(KBox(TEXT("slot_back"), 18.f, 18.f, 18.f, 18.f));
    Plate->SetPadding(FMargin(0.f));
    Put(Stack, Plate, HAlign_Fill, VAlign_Fill);
    Put(Stack, Sized(T, nullptr, N<UImage>(T, TEXT("Icon")), 160.f, 160.f), HAlign_Center, VAlign_Center);
    UTextBlock* Name = SizedText(T, TEXT("KeyLabel"), TEXT(""), 26.f);
    Name->SetJustification(ETextJustify::Center); Name->SetAutoWrapText(true);
    Put(Stack, Name, HAlign_Fill, VAlign_Center, FMargin(16.f, 0.f));
    Put(Stack, SizedText(T, TEXT("CountLabel"), TEXT(""), 36.f), HAlign_Right, VAlign_Bottom, FMargin(0.f, 0.f, 24.f, 16.f));
    UTextBlock* TileCaption = SizedText(T, TEXT("CaptionLabel"), TEXT(""), 26.f);  // tile caption (build catalogue); collapsed while empty
    TileCaption->SetJustification(ETextJustify::Center); TileCaption->SetVisibility(ESlateVisibility::Collapsed);
    Put(Stack, TileCaption, HAlign_Fill, VAlign_Bottom, FMargin(10.f, 0.f, 10.f, 10.f));
    Put(Stack, InvImage(T, TEXT("HoverFrame"), KBox(TEXT("hover_frame"), 14.f, 14.f, 14.f, 14.f, 84.f, 84.f), true), HAlign_Fill, VAlign_Fill);
    Put(Stack, InvImage(T, TEXT("SelectFrame"), KBox(TEXT("select_frame"), 14.f, 14.f, 14.f, 14.f, 84.f, 84.f), true), HAlign_Fill, VAlign_Fill);
    T->RootWidget = Sized(T, TEXT("RootSize"), Stack, 208.f, 208.f);
}

// WBP_ItemTile (art width follows its grid cell, 250 tall): the item slot as a catalogue tile. slot_back, the icon above and the caption in its own row
// below it (never over the icon), hover and select frames. Same USFSlotWidget API as WBP_ItemSlot.
void ItemTileLayout(UWidgetTree* T)
{
    UOverlay* Stack = A<UOverlay>(T);
    UBorder* Plate = N<UBorder>(T, TEXT("Frame"));
    Plate->SetBrush(KBox(TEXT("slot_back"), 18.f, 18.f, 18.f, 18.f));
    Plate->SetPadding(FMargin(0.f));
    Put(Stack, Plate, HAlign_Fill, VAlign_Fill);
    UVerticalBox* Column = A<UVerticalBox>(T);
    UVerticalBoxSlot* IconSlot = Column->AddChildToVerticalBox(Sized(T, nullptr, N<UImage>(T, TEXT("Icon")), 150.f, 150.f));
    IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 6.f));
    UTextBlock* TileCaption = SizedText(T, TEXT("CaptionLabel"), TEXT(""), 30.f);
    TileCaption->SetJustification(ETextJustify::Center);
    UVerticalBoxSlot* CaptionSlot = Column->AddChildToVerticalBox(TileCaption);
    CaptionSlot->SetHorizontalAlignment(HAlign_Center); CaptionSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 14.f));
    Put(Stack, Column, HAlign_Fill, VAlign_Fill);
    Put(Stack, InvImage(T, TEXT("HoverFrame"), KBox(TEXT("hover_frame"), 14.f, 14.f, 14.f, 14.f), true), HAlign_Fill, VAlign_Fill);
    Put(Stack, InvImage(T, TEXT("SelectFrame"), KBox(TEXT("select_frame"), 14.f, 14.f, 14.f, 14.f), true), HAlign_Fill, VAlign_Fill);
    T->RootWidget = Sized(T, TEXT("RootSize"), Stack, 0.f, 250.f);
}

// WBP_ScrollList: the one scrollbar style (scrollbar_track / scrollbar_thumb) around a Content slot.
void ScrollListLayout(UWidgetTree* T)
{
    UScrollBox* Scroll = N<UScrollBox>(T, TEXT("Scroll"));
    Scroll->SetScrollbarThickness(FVector2D(36.f, 36.f));
    Scroll->SetScrollbarPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    Scroll->SetAlwaysShowScrollbar(true);
    Scroll->SetAlwaysShowScrollbarTrack(true);
    FScrollBarStyle Bar = Scroll->GetWidgetBarStyle();
    Bar.SetVerticalBackgroundImage(KBox(TEXT("scrollbar_track"), 10.f, 20.f, 10.f, 20.f, 36.f, 600.f));
    const FSlateBrush Thumb = KBox(TEXT("scrollbar_thumb"), 10.f, 20.f, 10.f, 20.f, 32.f, 160.f);
    Bar.SetNormalThumbImage(Thumb).SetHoveredThumbImage(Thumb).SetDraggedThumbImage(Thumb).SetThickness(36.f);
    Scroll->SetWidgetBarStyle(Bar);
    UBorder* Centre = Plain(T, nullptr, FMargin(0.f));  // the content sits centred under the top edge
    Centre->SetHorizontalAlignment(HAlign_Center); Centre->SetVerticalAlignment(VAlign_Top);
    Centre->SetContent(N<UNamedSlot>(T, TEXT("Content")));
    Scroll->AddChild(Centre);
    T->RootWidget = Scroll;
}

void GoldDisplayLayout(UWidgetTree* T)
{
    UHorizontalBox* Row = A<UHorizontalBox>(T);
    Row->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, nullptr, KImg(TEXT("icon_gold_coin"), 56.f, 56.f)), 56.f, 56.f))->SetVerticalAlignment(VAlign_Center);
    UTextBlock* Amount = SizedText(T, TEXT("AmountText"), TEXT("0"), 36.f);
    Amount->SetColorAndOpacity(FSlateColor(Amber()));
    UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Amount);
    Slot->SetVerticalAlignment(VAlign_Center); Slot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    T->RootWidget = Row;
}

// WBP_SearchField / WBP_InputField: the field art with an EditableText over it (and the magnifier for search).
void FieldLayout(UWidgetTree* T, const TCHAR* Part, float Width, bool bSearchIcon)
{
    UOverlay* Stack = A<UOverlay>(T);
    Put(Stack, InvImage(T, nullptr, KBox(Part, 14.f, 14.f, 14.f, 14.f)), HAlign_Fill, VAlign_Fill);
    if (bSearchIcon) { Put(Stack, Sized(T, nullptr, InvImage(T, nullptr, KImg(TEXT("icon_search"), 44.f, 44.f), true), 44.f, 44.f), HAlign_Left, VAlign_Center, FMargin(28.f, 0.f, 0.f, 0.f)); }
    UEditableText* Input = N<UEditableText>(T, TEXT("Input"));
    FSlateFontInfo Font = Input->GetFont(); Font.Size = 34.f; Input->SetFont(Font);
    FEditableTextStyle Style = Input->WidgetStyle; Style.ColorAndOpacity = FSlateColor(Ivory()); Input->SetWidgetStyle(Style);
    Put(Stack, Input, HAlign_Fill, VAlign_Center, FMargin(bSearchIcon ? 96.f : 28.f, 0.f, 28.f, 0.f));
    T->RootWidget = Sized(T, TEXT("RootSize"), Stack, 0.f, 84.f, Width);
}

void SearchDefaults(UObject* Cdo) { Cast<USFTextFieldWidget>(Cdo)->HintText = FText::FromString(TEXT("Search")); }

// WBP_Dropdown: dropdown_field with the ComboBox transparent over it and the chevron on top.
void DropdownLayout(UWidgetTree* T)
{
    UOverlay* Stack = A<UOverlay>(T);
    Put(Stack, InvImage(T, nullptr, KBox(TEXT("dropdown_field"), 14.f, 14.f, 14.f, 14.f)), HAlign_Fill, VAlign_Fill);
    UComboBoxString* Combo = N<UComboBoxString>(T, TEXT("Combo"));
    FComboBoxStyle Style = Combo->GetWidgetStyle();
    FButtonStyle Face = Style.ComboButtonStyle.ButtonStyle;
    Face.Normal = FSlateNoResource(); Face.Hovered = FSlateNoResource(); Face.Pressed = FSlateNoResource(); Face.Disabled = FSlateNoResource();
    Style.ComboButtonStyle.ButtonStyle = Face;
    Style.ComboButtonStyle.DownArrowImage = FSlateNoResource();
    Combo->SetWidgetStyle(Style);
    // Open list: slate rows, the kit hover/select frames, ivory text (the font family is filled at runtime by USFDropdownWidget::ApplyKitStyle).
    FTableRowStyle Item = Combo->GetItemStyle();
    const FSlateBrush Frame = KBox(TEXT("hover_frame"), 14.f, 14.f, 14.f, 14.f);
    const FSlateBrush Gold = KBox(TEXT("select_frame"), 14.f, 14.f, 14.f, 14.f);
    const FSlateColorBrush Dark(Slate(1.f));
    Item.SetEvenRowBackgroundBrush(Dark).SetOddRowBackgroundBrush(Dark).SetEvenRowBackgroundHoveredBrush(Frame).SetOddRowBackgroundHoveredBrush(Frame);
    Item.SetActiveBrush(Gold).SetActiveHoveredBrush(Gold).SetInactiveBrush(Dark).SetInactiveHoveredBrush(Frame).SetSelectorFocusedBrush(Frame);
    Item.SetTextColor(FSlateColor(Ivory())).SetSelectedTextColor(FSlateColor(Ivory()));
    Combo->SetItemStyle(Item);
    Combo->SetContentPadding(FMargin(28.f, 0.f, 96.f, 0.f));
    PRAGMA_DISABLE_DEPRECATION_WARNINGS
    Combo->Font.Size = 34.f;
    Combo->ForegroundColor = FSlateColor(Ivory());
    PRAGMA_ENABLE_DEPRECATION_WARNINGS
    Put(Stack, Combo, HAlign_Fill, VAlign_Fill);
    Put(Stack, Sized(T, nullptr, InvImage(T, nullptr, KImg(TEXT("icon_chevron_down"), 40.f, 40.f), true), 40.f, 40.f), HAlign_Right, VAlign_Center, FMargin(0.f, 0.f, 28.f, 0.f));
    T->RootWidget = Sized(T, TEXT("RootSize"), Stack, 0.f, 84.f, 360.f);
}

// WBP_Stepper: [-] number [+]. The click fill (stepper_fill) sits over each button face; + fills left to right, - right to left.
UWidget* StepperButton(UWidgetTree* T, const TCHAR* ButtonName, const TCHAR* FillName, const TCHAR* Glyph, bool bRightToLeft)
{
    UOverlay* Stack = A<UOverlay>(T);
    UButton* Control = N<UButton>(T, ButtonName);
    const FSlateBrush Face = KBox(TEXT("stepper_button_normal"), 12.f, 12.f, 12.f, 12.f);
    const FSlateBrush Hover = KBox(TEXT("stepper_button_hover"), 12.f, 12.f, 12.f, 12.f);
    StyleFace(Control, Face, Hover, Hover, Face);
    Put(Stack, Control, HAlign_Fill, VAlign_Fill);
    UProgressBar* Fill = FillBar(T, FillName, KBox(TEXT("stepper_fill"), 10.f, 10.f, 10.f, 10.f));
    Fill->SetBarFillType(bRightToLeft ? EProgressBarFillType::RightToLeft : EProgressBarFillType::LeftToRight);
    Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
    Put(Stack, Fill, HAlign_Fill, VAlign_Fill);
    Put(Stack, Sized(T, nullptr, InvImage(T, nullptr, KImg(Glyph, 44.f, 44.f), true), 44.f, 44.f), HAlign_Center, VAlign_Center);
    return Sized(T, nullptr, Stack, 84.f, 84.f);
}

void StepperLayout(UWidgetTree* T)
{
    UHorizontalBox* Row = A<UHorizontalBox>(T);
    Row->AddChildToHorizontalBox(StepperButton(T, TEXT("MinusButton"), TEXT("MinusFill"), TEXT("glyph_minus"), true));
    UOverlay* Field = A<UOverlay>(T);
    Put(Field, InvImage(T, nullptr, KBox(TEXT("number_field"), 12.f, 12.f, 12.f, 12.f)), HAlign_Fill, VAlign_Fill);
    UTextBlock* Number = SizedText(T, TEXT("ValueText"), TEXT("1"), 40.f);
    Number->SetJustification(ETextJustify::Center);
    Put(Field, Number, HAlign_Center, VAlign_Center);
    Row->AddChildToHorizontalBox(Sized(T, nullptr, Field, 180.f, 84.f))->SetPadding(FMargin(8.f, 0.f));
    Row->AddChildToHorizontalBox(StepperButton(T, TEXT("PlusButton"), TEXT("PlusFill"), TEXT("glyph_plus"), false));
    T->RootWidget = Row;
}

// WBP_WindowFrame: tiled slate fill, four rails (136 px in from each corner, so 24 px under the 160 px corners), the body, then the four
// corners last. Art pixels throughout; the root ScaleBox draws it at half. Header: banner shield + emblem + title, then the rule.
// NamedSlots: Tabs, Content, SidePanel, Footer (a rule above the Footer).
void WindowFrameLayout(UWidgetTree* T)
{
    UOverlay* Chrome = A<UOverlay>(T);
    FSlateBrush Fill = KImg(TEXT("frame_fill"));
    Fill.Tiling = ESlateBrushTileType::Both;
    Put(Chrome, InvImage(T, nullptr, Fill, true), HAlign_Fill, VAlign_Fill);
    auto Rail = [&](const TCHAR* Part, ESlateBrushTileType::Type Tile, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Pad)
    {
        FSlateBrush Brush = KImg(Part);
        Brush.Tiling = Tile;
        Put(Chrome, InvImage(T, nullptr, Brush, true), H, V, Pad);
    };
    Rail(TEXT("frame_edge_top"), ESlateBrushTileType::Horizontal, HAlign_Fill, VAlign_Top, FMargin(136.f, 0.f));
    Rail(TEXT("frame_edge_bottom"), ESlateBrushTileType::Horizontal, HAlign_Fill, VAlign_Bottom, FMargin(136.f, 0.f));
    Rail(TEXT("frame_edge_left"), ESlateBrushTileType::Vertical, HAlign_Left, VAlign_Fill, FMargin(0.f, 136.f));
    Rail(TEXT("frame_edge_right"), ESlateBrushTileType::Vertical, HAlign_Right, VAlign_Fill, FMargin(0.f, 136.f));

    UVerticalBox* Body = A<UVerticalBox>(T);
    UHorizontalBox* Head = A<UHorizontalBox>(T);
    UOverlay* Banner = A<UOverlay>(T);
    Banner->AddChildToOverlay(InvImage(T, nullptr, KImg(TEXT("header_banner_shield"), 160.f, 192.f), true));
    Put(Banner, Sized(T, nullptr, InvImage(T, TEXT("Emblem"), FSlateBrush(), true), 96.f, 96.f), HAlign_Center, VAlign_Top, FMargin(0.f, 30.f, 0.f, 0.f));
    Head->AddChildToHorizontalBox(Sized(T, nullptr, Banner, 160.f, 192.f))->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* TitleSlot = Head->AddChildToHorizontalBox(SizedText(T, TEXT("Title"), TEXT("Window"), 60.f));
    TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); TitleSlot->SetVerticalAlignment(VAlign_Center); TitleSlot->SetPadding(FMargin(28.f, 0.f, 0.f, 0.f));
    Body->AddChildToVerticalBox(Head);
    Body->AddChildToVerticalBox(Inst(T, CommonDir, TEXT("WBP_Separator"), TEXT("HeaderRule")))->SetPadding(FMargin(0.f, 10.f, 0.f, 14.f));
    Body->AddChildToVerticalBox(N<UNamedSlot>(T, TEXT("Tabs")))->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    UHorizontalBox* Middle = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* ContentSlot = Middle->AddChildToHorizontalBox(N<UNamedSlot>(T, TEXT("Content")));
    ContentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ContentSlot->SetVerticalAlignment(VAlign_Fill);
    Middle->AddChildToHorizontalBox(N<UNamedSlot>(T, TEXT("SidePanel")))->SetVerticalAlignment(VAlign_Top);
    UVerticalBoxSlot* MiddleSlot = Body->AddChildToVerticalBox(Middle);
    MiddleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MiddleSlot->SetVerticalAlignment(VAlign_Fill);
    Body->AddChildToVerticalBox(Inst(T, CommonDir, TEXT("WBP_Separator"), TEXT("FooterRule")))->SetPadding(FMargin(0.f, 14.f, 0.f, 14.f));
    Body->AddChildToVerticalBox(N<UNamedSlot>(T, TEXT("Footer")));
    UBorder* BodyPad = Plain(T, nullptr, FMargin(72.f, 56.f, 72.f, 64.f));
    BodyPad->SetContent(Body);
    Put(Chrome, BodyPad, HAlign_Fill, VAlign_Fill);

    auto Corner = [&](const TCHAR* Part, EHorizontalAlignment H, EVerticalAlignment V) { Put(Chrome, InvImage(T, nullptr, KImg(Part, 160.f, 160.f), true), H, V); };
    Corner(TEXT("frame_corner_tl"), HAlign_Left, VAlign_Top);
    Corner(TEXT("frame_corner_tr"), HAlign_Right, VAlign_Top);
    Corner(TEXT("frame_corner_bl"), HAlign_Left, VAlign_Bottom);
    Corner(TEXT("frame_corner_br"), HAlign_Right, VAlign_Bottom);

    USizeBox* Size = N<USizeBox>(T, TEXT("WindowSize"));
    Size->SetMinDesiredWidth(1360.f);
    Size->SetContent(Chrome);
    T->RootWidget = Half(T, Size);
}

// Puts Content into a NamedSlot of a nested widget by writing its NamedSlotBindings (UUserWidget::SetContentForSlot drops the content while the
// template has a widget tree without that slot). GetAllWidgets, the compiler and the instance then all find it.
void Fill(UUserWidget* Host, const TCHAR* Slot, UWidget* Content)
{
    FArrayProperty* Bindings = CastField<FArrayProperty>(UUserWidget::StaticClass()->FindPropertyByName(TEXT("NamedSlotBindings")));
    FStructProperty* Item = Bindings ? CastField<FStructProperty>(Bindings->Inner) : nullptr;
    if (!Item) { return; }
    FScriptArrayHelper Array(Bindings, Bindings->ContainerPtrToValuePtr<void>(Host));
    const int32 Index = Array.AddValue();
    void* Element = Array.GetRawPtr(Index);
    if (FNameProperty* Name = CastField<FNameProperty>(Item->Struct->FindPropertyByName(TEXT("Name")))) { Name->SetPropertyValue_InContainer(Element, FName(Slot)); }
    if (FObjectProperty* Object = CastField<FObjectProperty>(Item->Struct->FindPropertyByName(TEXT("Content")))) { Object->SetObjectPropertyValue(Object->ContainerPtrToValuePtr<void>(Element), Content); }
}

// The item tooltip every menu window shares (Tooltip, TipIcon, TipTitle, ...): beside the window, not in it. Flowing content: art + name + description,
// rule, rarity . type, weight and owned count. Inventory v6 tooltip art. Pinned top-left in the window's root overlay; the C++ (SFItemTooltip) moves its
// holder to the cursor each tick, so the window never resizes or moves.
UScaleBox* ItemTooltipLayout(UWidgetTree* T)
{
    UBorder* TipHolder = N<UBorder>(T, TEXT("Tooltip"));
    TipHolder->SetBrush(FSlateNoResource());
    TipHolder->SetPadding(FMargin(32.f, 0.f, 0.f, 0.f));
    UOverlay* Tip = A<UOverlay>(T);
    Put(Tip, InvImage(T, nullptr, InvBrush(TEXT("tooltip_frame"), 760.f, 430.f, 34.f, 34.f, 34.f, 34.f, 760.f, 430.f)), HAlign_Fill, VAlign_Fill);
    UVerticalBox* TipBody = A<UVerticalBox>(T);
    UHorizontalBox* Head = A<UHorizontalBox>(T);
    Head->AddChildToHorizontalBox(Sized(T, nullptr, N<UImage>(T, TEXT("TipIcon")), 184.f, 184.f))->SetVerticalAlignment(VAlign_Top);
    UVerticalBox* Words = A<UVerticalBox>(T);
    Words->AddChildToVerticalBox(SizedText(T, TEXT("TipTitle"), TEXT("Item"), 48.f));
    UTextBlock* Description = SizedText(T, TEXT("TipDescription"), TEXT(""), 30.f);
    Description->SetWrapTextAt(440.f);
    Words->AddChildToVerticalBox(Description)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
    Head->AddChildToHorizontalBox(Words)->SetPadding(FMargin(32.f, 0.f, 0.f, 0.f));
    TipBody->AddChildToVerticalBox(Head);
    TipBody->AddChildToVerticalBox(Sized(T, nullptr, InvImage(T, nullptr, InvRule(TEXT("tooltip_divider"), 680.f, 12.f, 28.f)), 660.f, 12.f))->SetPadding(FMargin(0.f, 28.f, 0.f, 20.f));
    UHorizontalBox* Kind = A<UHorizontalBox>(T);
    UTextBlock* Rarity = SizedText(T, TEXT("TipRarity"), TEXT("Common"), 30.f);
    Rarity->SetColorAndOpacity(FSlateColor(Moss()));
    Kind->AddChildToHorizontalBox(Rarity);
    UBorder* Dot = Plain(T, nullptr, FMargin(0.f));
    Dot->SetBrush(FSlateColorBrush(Brass()));
    Kind->AddChildToHorizontalBox(Sized(T, nullptr, Dot, 6.f, 6.f))->SetPadding(FMargin(16.f, 0.f));
    Kind->AddChildToHorizontalBox(SizedText(T, TEXT("TipType"), TEXT("Material"), 30.f));
    TipBody->AddChildToVerticalBox(Kind);
    UHorizontalBox* Load = A<UHorizontalBox>(T);
    Load->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, nullptr, InvBrush(TEXT("icon_weight"), 36.f, 36.f)), 36.f, 36.f))->SetVerticalAlignment(VAlign_Center);
    Load->AddChildToHorizontalBox(SizedText(T, TEXT("TipWeight"), TEXT("Weight  0.0 kg (per 1)"), 30.f))->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
    UTextBlock* Owned = SizedText(T, TEXT("TipQuantity"), TEXT("Owned  0"), 28.f);
    Owned->SetJustification(ETextJustify::Right);
    UHorizontalBoxSlot* OwnedSlot = Load->AddChildToHorizontalBox(Owned);
    OwnedSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); OwnedSlot->SetHorizontalAlignment(HAlign_Right); OwnedSlot->SetVerticalAlignment(VAlign_Center);
    TipBody->AddChildToVerticalBox(Load)->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
    UBorder* TipPad = Plain(T, nullptr, FMargin(56.f, 52.f, 56.f, 48.f));
    TipPad->SetContent(TipBody);
    Put(Tip, TipPad, HAlign_Fill, VAlign_Fill);
    USizeBox* TipSize = Sized(T, nullptr, Tip, 780.f, 0.f);
    TipSize->SetMinDesiredHeight(400.f);
    TipHolder->SetContent(TipSize);
    TipHolder->SetVisibility(ESlateVisibility::Collapsed);
    return Half(T, TipHolder);
}

// WBP_Inventory: a WBP_WindowFrame (emblem, title, rules) holding a WBP_TabBar, the grid in a WBP_ScrollList, the item tooltip in the
// SidePanel and the weight / gold footer. C++ fills SlotGrid with WBP_ItemSlot cells, 5 wide.
void InventoryLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    if (USFWindowFrameWidget* Frame = Cast<USFWindowFrameWidget>(Window))
    {
        Frame->TitleText = FText::FromString(TEXT("Inventory"));
        Frame->WindowIcon = KImg(TEXT("icon_window_inventory"), 96.f, 96.f);
        Frame->WindowWidth = 1262.f;  // 72 + 1070 grid + 12 + 36 scrollbar + 72: the frame ends at the scrollbar
    }
    if (UUserWidget* Tabs = Inst(T, CommonDir, TEXT("WBP_TabBar"), TEXT("TabBar"))) { Fill(Window, TEXT("Tabs"), Tabs); }

    UUniformGridPanel* Grid = N<UUniformGridPanel>(T, TEXT("SlotGrid"));
    Grid->SetSlotPadding(FMargin(3.f));
    if (UUserWidget* Scroll = Inst(T, CommonDir, TEXT("WBP_ScrollList"), TEXT("SlotScroll")))
    {
        Fill(Scroll, TEXT("Content"), Sized(T, nullptr, Grid, 1070.f, 0.f));  // 5 cells of 208 + 2 x 3 padding
        UOverlay* Pack = A<UOverlay>(T);
        Put(Pack, Sized(T, nullptr, Scroll, 0.f, 1070.f), HAlign_Fill, VAlign_Fill);  // the grid scrolls inside 5 rows
        UTextBlock* Empty = SizedText(T, TEXT("EmptyText"), TEXT("Your pack is empty."), 40.f);
        Empty->SetVisibility(ESlateVisibility::Collapsed);
        Put(Pack, Empty, HAlign_Center, VAlign_Center);
        Fill(Window, TEXT("Content"), Pack);
    }

    // Footer: weight icon, text, bar, percent; gold at the right.
    UHorizontalBox* Foot = A<UHorizontalBox>(T);
    Foot->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, nullptr, InvBrush(TEXT("icon_weight"), 40.f, 40.f)), 40.f, 40.f))->SetVerticalAlignment(VAlign_Center);
    Foot->AddChildToHorizontalBox(SizedText(T, TEXT("WeightText"), TEXT("Weight 0.0 / 0 kg"), 34.f))->SetPadding(FMargin(18.f, 0.f, 24.f, 0.f));
    UOverlay* Meter = A<UOverlay>(T);
    Put(Meter, InvImage(T, nullptr, InvBrush(TEXT("progress_track"), 680.f, 26.f, 18.f, 8.f, 18.f, 8.f, 680.f, 26.f)), HAlign_Fill, VAlign_Fill);
    UProgressBar* Weight = N<UProgressBar>(T, TEXT("WeightBar"));
    FProgressBarStyle WeightStyle = Weight->GetWidgetStyle();
    WeightStyle.SetBackgroundImage(FSlateNoResource());
    WeightStyle.SetFillImage(InvBrush(TEXT("progress_fill_green"), 680.f, 22.f, 16.f, 7.f, 16.f, 7.f, 680.f, 22.f));
    Weight->SetWidgetStyle(WeightStyle);
    Weight->SetPercent(0.f);
    Put(Meter, Weight, HAlign_Fill, VAlign_Fill, FMargin(0.f, 2.f));
    Foot->AddChildToHorizontalBox(Sized(T, nullptr, Meter, 360.f, 26.f))->SetVerticalAlignment(VAlign_Center);
    UTextBlock* Percent = SizedText(T, TEXT("WeightPercentText"), TEXT("0%"), 34.f);
    Percent->SetJustification(ETextJustify::Right);
    Foot->AddChildToHorizontalBox(Sized(T, nullptr, Percent, 110.f, 0.f))->SetPadding(FMargin(18.f, 0.f, 0.f, 0.f));
    UHorizontalBoxSlot* GoldSlot = Foot->AddChildToHorizontalBox(Inst(T, CommonDir, TEXT("WBP_GoldDisplay"), TEXT("Gold")));
    GoldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); GoldSlot->SetHorizontalAlignment(HAlign_Right); GoldSlot->SetVerticalAlignment(VAlign_Center);
    Fill(Window, TEXT("Footer"), Foot);

    UScaleBox* TipScale = ItemTooltipLayout(T);
    UOverlay* Root = A<UOverlay>(T);
    Put(Root, Window, HAlign_Fill, VAlign_Fill);
    Put(Root, TipScale, HAlign_Left, VAlign_Top);
    T->RootWidget = Root;
}

// ---- menus on the shared kit (market board, trade, recipes, build catalogue). Every screen is a WBP_WindowFrame instance filled through its
// NamedSlots, built only from the kit parents (button, tab bar, stepper, separator, item slot, scroll list, fields, dropdown, gold) and the one
// shared row. C++ fills the lists; widths are art pixels (the window draws them at half).
FLinearColor DimInk() { return Hex(TEXT("A9A593")); }

USFWindowFrameWidget* WindowOf(UUserWidget* Window, const TCHAR* Title, const TCHAR* Icon, float Width)
{
    USFWindowFrameWidget* Frame = Cast<USFWindowFrameWidget>(Window);
    if (!Frame) { return nullptr; }
    Frame->TitleText = FText::FromString(Title);
    Frame->WindowIcon = KImg(Icon, 96.f, 96.f);
    Frame->WindowWidth = Width;
    return Frame;
}

UUserWidget* KitButton(UWidgetTree* T, const TCHAR* Name, const TCHAR* Label)
{
    UUserWidget* Instance = Inst(T, CommonDir, TEXT("WBP_Button"), Name);
    if (USFButtonWidget* Face = Cast<USFButtonWidget>(Instance)) { Face->LabelText = FText::FromString(Label); }
    return Instance;
}

UUserWidget* KitRule(UWidgetTree* T, bool bVertical = false)
{
    UUserWidget* Instance = Inst(T, CommonDir, TEXT("WBP_Separator"), nullptr);
    if (USFSeparatorWidget* Line = Cast<USFSeparatorWidget>(Instance)) { Line->bVertical = bVertical; }
    return Instance;
}

UWidget* KitField(UWidgetTree* T, const TCHAR* Wbp, const TCHAR* Name, const TCHAR* Hint, bool bDigits)
{
    UUserWidget* Instance = Inst(T, CommonDir, Wbp, Name);
    if (USFTextFieldWidget* Input = Cast<USFTextFieldWidget>(Instance)) { Input->HintText = FText::FromString(Hint); Input->bDigitsOnly = bDigits; }
    return Instance;
}

// A window root: the frame, with the shared item tooltip pinned outside it.
UOverlay* WindowRoot(UWidgetTree* T, UUserWidget* Window, bool bTooltip)
{
    UOverlay* Root = A<UOverlay>(T);
    Put(Root, Window, HAlign_Fill, VAlign_Fill);
    if (bTooltip) { Put(Root, ItemTooltipLayout(T), HAlign_Left, VAlign_Top); }
    T->RootWidget = Root;
    return Root;
}

UTextBlock* Caption(UWidgetTree* T, const TCHAR* Name, const TCHAR* Text, float Size, bool bDim = false)
{
    UTextBlock* Block = SizedText(T, Name, Text, Size);
    if (bDim) { Block->SetColorAndOpacity(FSlateColor(DimInk())); }
    return Block;
}

// WBP_KitRow: row_back with the item in its slot well, a name, up to two text cells, a price with the coin, an action button and a chevron; the gold
// select frame and the dim hover frame over it. Cells the screen does not use collapse (USFKitRowWidget).
void KitRowLayout(UWidgetTree* T)
{
    UOverlay* Stack = A<UOverlay>(T);
    UBorder* Plate = N<UBorder>(T, TEXT("Frame"));
    Plate->SetBrush(KBox(TEXT("row_back"), 16.f, 16.f, 16.f, 16.f));
    Plate->SetPadding(FMargin(0.f));
    Put(Stack, Plate, HAlign_Fill, VAlign_Fill);
    UHorizontalBox* Line = A<UHorizontalBox>(T);
    UOverlay* Well = A<UOverlay>(T);
    Put(Well, InvImage(T, nullptr, KBox(TEXT("slot_back"), 18.f, 18.f, 18.f, 18.f, 104.f, 104.f), true), HAlign_Fill, VAlign_Fill);
    Put(Well, Sized(T, nullptr, N<UImage>(T, TEXT("Icon")), 88.f, 88.f), HAlign_Center, VAlign_Center);
    Line->AddChildToHorizontalBox(Sized(T, TEXT("IconBox"), Well, 104.f, 104.f))->SetVerticalAlignment(VAlign_Center);
    UTextBlock* Name = SizedText(T, TEXT("Label"), TEXT("Item"), 38.f);
    Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name);
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center); NameSlot->SetPadding(FMargin(24.f, 0.f, 12.f, 0.f));
    UTextBlock* Cell1 = SizedText(T, TEXT("Cell1"), TEXT(""), 34.f);
    Cell1->SetJustification(ETextJustify::Center);
    Line->AddChildToHorizontalBox(Sized(T, TEXT("Cell1Box"), Cell1, 200.f, 0.f))->SetVerticalAlignment(VAlign_Center);
    UTextBlock* Cell2 = SizedText(T, TEXT("Cell2"), TEXT(""), 34.f);
    Cell2->SetJustification(ETextJustify::Center);
    Line->AddChildToHorizontalBox(Sized(T, TEXT("Cell2Box"), Cell2, 260.f, 0.f))->SetVerticalAlignment(VAlign_Center);
    UHorizontalBox* Price = A<UHorizontalBox>(T);
    Price->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, nullptr, KImg(TEXT("icon_gold_coin"), 48.f, 48.f), true), 48.f, 48.f))->SetVerticalAlignment(VAlign_Center);
    Price->AddChildToHorizontalBox(SizedText(T, TEXT("Value"), TEXT(""), 36.f))->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    Cast<UHorizontalBoxSlot>(Price->GetChildAt(1)->Slot)->SetVerticalAlignment(VAlign_Center);
    Line->AddChildToHorizontalBox(Sized(T, TEXT("PriceBox"), Price, 0.f, 0.f, 240.f))->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* ActionSlot = Line->AddChildToHorizontalBox(Sized(T, TEXT("ActionBox"), KitButton(T, TEXT("Action"), TEXT("Buy")), 240.f, 88.f));
    ActionSlot->SetVerticalAlignment(VAlign_Center); ActionSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    UImage* Arrow = InvImage(T, TEXT("Chevron"), KImg(TEXT("icon_chevron_down"), 48.f, 48.f), true);
    Arrow->SetRenderTransformAngle(-90.f);
    Line->AddChildToHorizontalBox(Sized(T, nullptr, Arrow, 56.f, 56.f))->SetVerticalAlignment(VAlign_Center);
    UBorder* Pad = Plain(T, nullptr, FMargin(20.f, 12.f, 20.f, 12.f));
    Pad->SetContent(Line);
    Put(Stack, Pad, HAlign_Fill, VAlign_Fill);
    Put(Stack, InvImage(T, TEXT("HoverFrame"), KBox(TEXT("hover_frame"), 14.f, 14.f, 14.f, 14.f), true), HAlign_Fill, VAlign_Fill);
    Put(Stack, InvImage(T, TEXT("SelectFrame"), KBox(TEXT("select_frame"), 14.f, 14.f, 14.f, 14.f), true), HAlign_Fill, VAlign_Fill);
    T->RootWidget = Sized(T, TEXT("RootSize"), Stack, 0.f, 128.f);
}

// A scroll list of the shared style holding Content of a fixed width, VisibleHeight tall.
UWidget* ListOf(UWidgetTree* T, const TCHAR* ScrollName, UWidget* Content, float Width, float VisibleHeight)
{
    UUserWidget* Scroll = Inst(T, CommonDir, TEXT("WBP_ScrollList"), ScrollName);
    if (!Scroll) { return nullptr; }
    Fill(Scroll, TEXT("Content"), Sized(T, nullptr, Content, Width, 0.f));
    return Sized(T, nullptr, Scroll, 0.f, VisibleHeight);
}

// WBP_MarketBoard (reference-market): tabs + search + sort on top, the listing table, "Sell an item" at the side, gold in the footer.
void MarketLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Market Board"), TEXT("icon_window_market"), 2400.f);

    UHorizontalBox* Top = A<UHorizontalBox>(T);
    Top->AddChildToHorizontalBox(Sized(T, nullptr, Inst(T, CommonDir, TEXT("WBP_TabBar"), TEXT("TabBar")), 1040.f, 0.f))->SetVerticalAlignment(VAlign_Center);
    Top->AddChildToHorizontalBox(A<USizeBox>(T))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Top->AddChildToHorizontalBox(Sized(T, nullptr, KitField(T, TEXT("WBP_SearchField"), TEXT("SearchField"), TEXT("Search items..."), false), 560.f, 84.f))->SetVerticalAlignment(VAlign_Center);
    Top->AddChildToHorizontalBox(Sized(T, nullptr, Inst(T, CommonDir, TEXT("WBP_Dropdown"), TEXT("SortDropdown")), 480.f, 84.f))->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
    Fill(Window, TEXT("Tabs"), Top);

    // Table: header over the rows. The header cells line up with the row cells (icon well 104 + pad 20 + 24 before the name).
    UVerticalBox* Table = A<UVerticalBox>(T);
    UHorizontalBox* Head = A<UHorizontalBox>(T);
    Head->AddChildToHorizontalBox(Sized(T, nullptr, A<USizeBox>(T), 148.f, 0.f));
    Head->AddChildToHorizontalBox(Caption(T, nullptr, TEXT("Item"), 32.f, true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const TCHAR* const Columns[3] = {TEXT("Quantity"), TEXT("Seller"), TEXT("Price")};
    const float Widths[3] = {200.f, 260.f, 240.f};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UTextBlock* Heading = Caption(T, nullptr, Columns[Index], 32.f, true);
        Heading->SetJustification(ETextJustify::Center);
        Head->AddChildToHorizontalBox(Sized(T, nullptr, Heading, Widths[Index], 0.f));
    }
    Head->AddChildToHorizontalBox(Sized(T, nullptr, A<USizeBox>(T), 272.f, 0.f));
    UVerticalBoxSlot* HeadSlot = Table->AddChildToVerticalBox(Sized(T, nullptr, Head, 1568.f, 0.f));  // left: the cells line up with the rows (a width override does not shrink a Fill slot)
    HeadSlot->SetHorizontalAlignment(HAlign_Left); HeadSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    Table->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    UOverlay* Pack = A<UOverlay>(T);
    Put(Pack, ListOf(T, TEXT("ListScroll"), N<UVerticalBox>(T, TEXT("ListingRows")), 1568.f, 896.f), HAlign_Fill, VAlign_Fill);
    UTextBlock* Empty = Caption(T, TEXT("EmptyText"), TEXT("No listings."), 40.f, true);
    Empty->SetVisibility(ESlateVisibility::Collapsed);
    Put(Pack, Empty, HAlign_Center, VAlign_Center);
    Table->AddChildToVerticalBox(Pack);
    Fill(Window, TEXT("Content"), Table);

    // Side panel: Sell an item.
    UVerticalBox* Sell = A<UVerticalBox>(T);
    Sell->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Sell an item"), 48.f));
    Sell->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 12.f, 0.f, 20.f));
    UOverlay* SlotStack = A<UOverlay>(T);
    Put(SlotStack, InvImage(T, TEXT("SellEmpty"), KBox(TEXT("plus_slot"), 18.f, 18.f, 18.f, 18.f), true), HAlign_Fill, VAlign_Fill);
    if (UUserWidget* Offer = Inst(T, CommonDir, TEXT("WBP_ItemSlot"), TEXT("SellSlot"))) { Put(SlotStack, Offer, HAlign_Fill, VAlign_Fill); }
    Sell->AddChildToVerticalBox(Sized(T, nullptr, SlotStack, 208.f, 208.f))->SetHorizontalAlignment(HAlign_Center);
    UTextBlock* Hint = Caption(T, TEXT("SellItemText"), TEXT("Select an item from your inventory"), 30.f, true);
    Hint->SetJustification(ETextJustify::Center); Hint->SetAutoWrapText(true);
    Sell->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.f, 14.f, 0.f, 24.f));
    Sell->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Quantity"), 34.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    Sell->AddChildToVerticalBox(KitField(T, TEXT("WBP_InputField"), TEXT("QuantityField"), TEXT("1"), true))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    Sell->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Price (per item)"), 34.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    Sell->AddChildToVerticalBox(KitField(T, TEXT("WBP_InputField"), TEXT("PriceField"), TEXT("0"), true))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    Sell->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    UHorizontalBox* Fee = A<UHorizontalBox>(T);
    Fee->AddChildToHorizontalBox(Caption(T, TEXT("FeeText"), TEXT("Fee 0 g"), 34.f, true))->SetVerticalAlignment(VAlign_Center);
    Fee->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, nullptr, KImg(TEXT("icon_gold_coin"), 40.f, 40.f), true), 40.f, 40.f))->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    Sell->AddChildToVerticalBox(Fee)->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    Sell->AddChildToVerticalBox(KitButton(T, TEXT("ListButton"), TEXT("List item")));
    UBorder* SellPad = Plain(T, nullptr, FMargin(48.f, 0.f, 0.f, 0.f));
    SellPad->SetContent(Sized(T, nullptr, Sell, 600.f, 0.f));
    Fill(Window, TEXT("SidePanel"), SellPad);

    UHorizontalBox* Foot = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* GoldSlot = Foot->AddChildToHorizontalBox(Inst(T, CommonDir, TEXT("WBP_GoldDisplay"), TEXT("Gold")));
    GoldSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); GoldSlot->SetHorizontalAlignment(HAlign_Right); GoldSlot->SetVerticalAlignment(VAlign_Center);
    Fill(Window, TEXT("Footer"), Foot);
    WindowRoot(T, Window, true);
}

// WBP_TradeWindow (hud-trade): your offer and theirs side by side, gold lines, lock state, Cancel, hold to accept.
UWidget* TradeColumn(UWidgetTree* T, bool bMine)
{
    UVerticalBox* Column = A<UVerticalBox>(T);
    Column->AddChildToVerticalBox(Caption(T, bMine ? TEXT("MyName") : TEXT("TheirName"), bMine ? TEXT("You") : TEXT("Partner"), 48.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Column->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    UUniformGridPanel* Grid = N<UUniformGridPanel>(T, bMine ? TEXT("MySlots") : TEXT("TheirSlots"));
    Grid->SetSlotPadding(FMargin(3.f));
    UVerticalBoxSlot* GridSlot = Column->AddChildToVerticalBox(Sized(T, nullptr, Grid, 856.f, 0.f));
    GridSlot->SetHorizontalAlignment(HAlign_Center); GridSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    UHorizontalBox* Gold = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* GoldLabel = Gold->AddChildToHorizontalBox(Caption(T, nullptr, TEXT("Gold Offered"), 36.f));
    GoldLabel->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); GoldLabel->SetVerticalAlignment(VAlign_Center);
    if (bMine) { Gold->AddChildToHorizontalBox(Sized(T, nullptr, KitField(T, TEXT("WBP_InputField"), TEXT("MyGold"), TEXT("0"), true), 360.f, 84.f)); }
    else { Gold->AddChildToHorizontalBox(Inst(T, CommonDir, TEXT("WBP_GoldDisplay"), TEXT("TheirGold")))->SetVerticalAlignment(VAlign_Center); }
    UVerticalBoxSlot* GoldRow = Column->AddChildToVerticalBox(Sized(T, nullptr, Gold, 856.f, 0.f));
    GoldRow->SetHorizontalAlignment(HAlign_Center); GoldRow->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    UHorizontalBox* State = A<UHorizontalBox>(T);
    if (bMine)
    {
        State->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, TEXT("LockButton"), TEXT("Lock Offer")), 480.f, 96.f));
        UTextBlock* Undo = Caption(T, TEXT("LockHint"), TEXT("Press again to unlock"), 30.f, true);
        Undo->SetVisibility(ESlateVisibility::Collapsed);
        UHorizontalBoxSlot* UndoSlot = State->AddChildToHorizontalBox(Undo);
        UndoSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f)); UndoSlot->SetVerticalAlignment(VAlign_Center);
    }
    else
    {
        UImage* Mark = InvImage(T, TEXT("TheirLockMark"), KImg(TEXT("icon_check_ok"), 64.f, 64.f), true);
        Mark->SetVisibility(ESlateVisibility::Collapsed);
        State->AddChildToHorizontalBox(Sized(T, nullptr, Mark, 64.f, 64.f))->SetVerticalAlignment(VAlign_Center);
        UHorizontalBoxSlot* StatusSlot = State->AddChildToHorizontalBox(Caption(T, TEXT("TheirStatus"), TEXT("Reviewing offer"), 36.f));
        StatusSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f)); StatusSlot->SetVerticalAlignment(VAlign_Center);
    }
    Column->AddChildToVerticalBox(Sized(T, nullptr, State, 856.f, 96.f))->SetHorizontalAlignment(HAlign_Center);
    return Column;
}

void TradeLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Direct Trade"), TEXT("icon_window_trade"), 1930.f);
    UHorizontalBox* Columns = A<UHorizontalBox>(T);
    Columns->AddChildToHorizontalBox(TradeColumn(T, true))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UHorizontalBoxSlot* Split = Columns->AddChildToHorizontalBox(KitRule(T, true));
    Split->SetPadding(FMargin(24.f, 0.f)); Split->SetVerticalAlignment(VAlign_Fill);
    Columns->AddChildToHorizontalBox(TradeColumn(T, false))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Fill(Window, TEXT("Content"), Columns);

    UHorizontalBox* Foot = A<UHorizontalBox>(T);
    Foot->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, TEXT("CancelButton"), TEXT("Cancel Trade")), 440.f, 96.f));
    UVerticalBox* Notes = A<UVerticalBox>(T);
    UTextBlock* Review = Caption(T, nullptr, TEXT("Review both offers before accepting."), 30.f);
    Review->SetColorAndOpacity(FSlateColor(Amber())); Review->SetJustification(ETextJustify::Center);
    Notes->AddChildToVerticalBox(Review);
    UTextBlock* Status = Caption(T, TEXT("StatusText"), TEXT(""), 30.f, true);
    Status->SetJustification(ETextJustify::Center);
    Notes->AddChildToVerticalBox(Status);
    UHorizontalBoxSlot* NotesSlot = Foot->AddChildToHorizontalBox(Notes);
    NotesSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NotesSlot->SetVerticalAlignment(VAlign_Center); NotesSlot->SetPadding(FMargin(24.f, 0.f));
    Foot->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, TEXT("AcceptButton"), TEXT("Hold to Accept Trade")), 600.f, 96.f));
    Fill(Window, TEXT("Footer"), Foot);
    WindowRoot(T, Window, true);
}

// WBP_StationPanel (reference-recipes): tabs, recipe list left, the selected recipe's detail right.
void StationLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Recipes"), TEXT("icon_window_recipes"), 2100.f);
    UHorizontalBox* Top = A<UHorizontalBox>(T);
    Top->AddChildToHorizontalBox(Sized(T, nullptr, Inst(T, CommonDir, TEXT("WBP_TabBar"), TEXT("TabBar")), 1100.f, 0.f));
    Fill(Window, TEXT("Tabs"), Top);

    UOverlay* Pack = A<UOverlay>(T);
    Put(Pack, ListOf(T, TEXT("RecipeScroll"), N<UVerticalBox>(T, TEXT("RecipeList")), 860.f, 768.f), HAlign_Fill, VAlign_Fill);
    UTextBlock* Empty = Caption(T, TEXT("EmptyText"), TEXT("No recipes."), 40.f, true);
    Empty->SetVisibility(ESlateVisibility::Collapsed);
    Put(Pack, Empty, HAlign_Center, VAlign_Center);
    Fill(Window, TEXT("Content"), Pack);

    UVerticalBox* Detail = A<UVerticalBox>(T);
    UHorizontalBox* Output = A<UHorizontalBox>(T);
    Output->AddChildToHorizontalBox(Sized(T, nullptr, N<UImage>(T, TEXT("OutputIcon")), 256.f, 256.f))->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* Words = A<UVerticalBox>(T);
    Words->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Output:"), 34.f, true));
    Words->AddChildToVerticalBox(Caption(T, TEXT("OutputName"), TEXT("Recipe"), 52.f));
    UTextBlock* Description = Caption(T, TEXT("OutputText"), TEXT(""), 32.f, true);
    Description->SetAutoWrapText(true);
    Words->AddChildToVerticalBox(Description)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    UHorizontalBoxSlot* WordsSlot = Output->AddChildToHorizontalBox(Words);
    WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); WordsSlot->SetVerticalAlignment(VAlign_Center); WordsSlot->SetPadding(FMargin(32.f, 0.f, 0.f, 0.f));
    Detail->AddChildToVerticalBox(Output)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    Detail->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    Detail->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Ingredients (Have / Need)"), 38.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Detail->AddChildToVerticalBox(N<UVerticalBox>(T, TEXT("Ingredients")))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    UHorizontalBox* Station = A<UHorizontalBox>(T);
    Station->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, nullptr, KImg(TEXT("icon_window_build"), 80.f, 80.f), true), 80.f, 80.f))->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* StationWords = A<UVerticalBox>(T);
    StationWords->AddChildToVerticalBox(Caption(T, TEXT("StationText"), TEXT("Station"), 38.f));
    StationWords->AddChildToVerticalBox(Caption(T, TEXT("StationHint"), TEXT(""), 30.f, true));
    Station->AddChildToHorizontalBox(StationWords)->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
    Detail->AddChildToVerticalBox(Station)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    Detail->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    UHorizontalBox* Quantity = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* QuantityLabel = Quantity->AddChildToHorizontalBox(Caption(T, nullptr, TEXT("Quantity"), 38.f));
    QuantityLabel->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); QuantityLabel->SetVerticalAlignment(VAlign_Center);
    Quantity->AddChildToHorizontalBox(Inst(T, CommonDir, TEXT("WBP_Stepper"), TEXT("QuantityStepper")));
    Detail->AddChildToVerticalBox(Quantity)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    UTextBlock* Time = Caption(T, TEXT("CraftTime"), TEXT(""), 30.f, true);
    Time->SetJustification(ETextJustify::Center);
    Detail->AddChildToVerticalBox(Time)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Detail->AddChildToVerticalBox(KitButton(T, TEXT("CraftButton"), TEXT("Craft")));
    UBorder* DetailPad = Plain(T, nullptr, FMargin(48.f, 0.f, 0.f, 0.f));
    DetailPad->SetContent(Sized(T, nullptr, Detail, 1000.f, 0.f));
    Fill(Window, TEXT("SidePanel"), DetailPad);
    WindowRoot(T, Window, true);
}

// WBP_BuildCatalogue (reference-build-catalogue): search, tabs, tile grid with the gold select frame, material, the selected object's detail.
void BuildLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Build Catalogue"), TEXT("icon_window_build"), 950.f);
    UVerticalBox* Body = A<UVerticalBox>(T);
    Body->AddChildToVerticalBox(KitField(T, TEXT("WBP_SearchField"), TEXT("SearchField"), TEXT("Search objects..."), false))->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    Body->AddChildToVerticalBox(Inst(T, CommonDir, TEXT("WBP_TabBar"), TEXT("TabBar")))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    UUniformGridPanel* Grid = N<UUniformGridPanel>(T, TEXT("TileGrid"));
    Grid->SetSlotPadding(FMargin(6.f));
    UOverlay* Pack = A<UOverlay>(T);
    Put(Pack, ListOf(T, TEXT("TileScroll"), Grid, 758.f, 540.f), HAlign_Fill, VAlign_Fill);  // 950 window - 2x72 frame - 48 scrollbar: the 3 tiles fill the row
    UTextBlock* Empty = Caption(T, TEXT("EmptyText"), TEXT("No matching objects."), 36.f, true);
    Empty->SetVisibility(ESlateVisibility::Collapsed);
    Put(Pack, Empty, HAlign_Center, VAlign_Center);
    Body->AddChildToVerticalBox(Pack)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    Body->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    UHorizontalBox* Material = A<UHorizontalBox>(T);
    Material->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, nullptr, KImg(TEXT("icon_material_log"), 72.f, 72.f), true), 72.f, 72.f))->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* MaterialLabel = Material->AddChildToHorizontalBox(Caption(T, nullptr, TEXT("Material"), 36.f));
    MaterialLabel->SetPadding(FMargin(16.f, 0.f, 24.f, 0.f)); MaterialLabel->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* Pick = Material->AddChildToHorizontalBox(Inst(T, CommonDir, TEXT("WBP_Dropdown"), TEXT("MaterialDropdown")));
    Pick->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Pick->SetVerticalAlignment(VAlign_Center);
    Body->AddChildToVerticalBox(Material)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    Body->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

    UHorizontalBox* Detail = A<UHorizontalBox>(T);
    if (UUserWidget* Preview = Inst(T, CommonDir, TEXT("WBP_ItemSlot"), TEXT("DetailSlot"))) { Detail->AddChildToHorizontalBox(Preview)->SetVerticalAlignment(VAlign_Top); }
    UVerticalBox* Words = A<UVerticalBox>(T);
    UHorizontalBox* Title = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* NameSlot = Title->AddChildToHorizontalBox(Caption(T, TEXT("DetailName"), TEXT("Wall"), 48.f));
    NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center);
    UButton* Star = N<UButton>(T, TEXT("FavouriteButton"));
    StyleFace(Star, FSlateNoResource(), FSlateNoResource(), FSlateNoResource(), FSlateNoResource());
    UOverlay* StarStack = A<UOverlay>(T);
    Put(StarStack, InvImage(T, TEXT("FavouriteOff"), KImg(TEXT("icon_favorite_star"), 72.f, 72.f), true), HAlign_Fill, VAlign_Fill);
    UImage* StarOn = InvImage(T, TEXT("FavouriteOn"), KImg(TEXT("icon_favorite_star_filled"), 72.f, 72.f), true);
    StarOn->SetVisibility(ESlateVisibility::Collapsed);
    Put(StarStack, StarOn, HAlign_Fill, VAlign_Fill);
    Star->SetContent(Sized(T, nullptr, StarStack, 72.f, 72.f));
    Title->AddChildToHorizontalBox(Star);
    Words->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    UHorizontalBox* Cost = A<UHorizontalBox>(T);
    Cost->AddChildToHorizontalBox(Caption(T, nullptr, TEXT("Cost"), 34.f, true))->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));
    Cost->AddChildToHorizontalBox(Caption(T, TEXT("CostText"), TEXT("1 Wall Material Bundle"), 34.f));
    Words->AddChildToVerticalBox(Cost)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    UHorizontalBox* Ready = A<UHorizontalBox>(T);
    Ready->AddChildToHorizontalBox(Sized(T, nullptr, InvImage(T, TEXT("StatusOk"), KImg(TEXT("icon_check_ok"), 56.f, 56.f), true), 56.f, 56.f))->SetVerticalAlignment(VAlign_Center);
    UTextBlock* Status = Caption(T, TEXT("StatusText"), TEXT("Valid placement"), 34.f);
    Status->SetColorAndOpacity(FSlateColor(Hex(TEXT("8FBF6A"))));
    UHorizontalBoxSlot* StatusSlot = Ready->AddChildToHorizontalBox(Status);
    StatusSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f)); StatusSlot->SetVerticalAlignment(VAlign_Center);
    Words->AddChildToVerticalBox(Ready);
    UHorizontalBoxSlot* WordsSlot = Detail->AddChildToHorizontalBox(Words);
    WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); WordsSlot->SetVerticalAlignment(VAlign_Top); WordsSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
    Body->AddChildToVerticalBox(Detail);
    Fill(Window, TEXT("Content"), Sized(T, nullptr, Body, 678.f, 0.f));
    UOverlay* Root = A<UOverlay>(T);
    Put(Root, Window, HAlign_Fill, VAlign_Fill);
    T->RootWidget = Root;
}

// ---- screens converted to the kit in the second pass: quests, dialogue, gathering skills, settings, game menu, defeated. Same rule as above: a
// WBP_WindowFrame instance filled through its NamedSlots, built only from the kit parents; C++ owns data and events.

// WBP_Toggle (84 x 84): the field back, the gold select frame while on and the bronze hover frame, with the check glyph while on.
void ToggleLayout(UWidgetTree* T)
{
    UOverlay* Stack = A<UOverlay>(T);
    Put(Stack, InvImage(T, nullptr, KBox(TEXT("number_field"), 12.f, 12.f, 12.f, 12.f), true), HAlign_Fill, VAlign_Fill);
    Put(Stack, Sized(T, nullptr, InvImage(T, TEXT("Check"), KImg(TEXT("icon_check_ok"), 56.f, 56.f), true), 56.f, 56.f), HAlign_Center, VAlign_Center);
    Put(Stack, InvImage(T, TEXT("HoverFrame"), KBox(TEXT("hover_frame"), 14.f, 14.f, 14.f, 14.f), true), HAlign_Fill, VAlign_Fill);
    Put(Stack, InvImage(T, TEXT("SelectFrame"), KBox(TEXT("select_frame"), 14.f, 14.f, 14.f, 14.f), true), HAlign_Fill, VAlign_Fill);
    T->RootWidget = Sized(T, TEXT("RootSize"), Stack, 84.f, 84.f);
}

// WBP_SettingRow: the label on the left, the control (a NamedSlot) on the right, a separator under the line. A heading row shows Heading instead.
void SettingRowLayout(UWidgetTree* T)
{
    UVerticalBox* Column = A<UVerticalBox>(T);
    UOverlay* Stack = A<UOverlay>(T);
    UHorizontalBox* Body = N<UHorizontalBox>(T, TEXT("RowBody"));
    UHorizontalBoxSlot* LabelSlot = Body->AddChildToHorizontalBox(SizedText(T, TEXT("Label"), TEXT("Setting"), 36.f));
    LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* ControlSlot = Body->AddChildToHorizontalBox(N<UNamedSlot>(T, TEXT("Control")));
    ControlSlot->SetVerticalAlignment(VAlign_Center); ControlSlot->SetHorizontalAlignment(HAlign_Right);
    Put(Stack, Body, HAlign_Fill, VAlign_Center);
    UBorder* HeadingBox = Plain(T, TEXT("HeadingBox"), FMargin(0.f, 24.f, 0.f, 0.f));
    HeadingBox->SetContent(SizedText(T, TEXT("Heading"), TEXT("Group"), 44.f));
    Put(Stack, HeadingBox, HAlign_Fill, VAlign_Center);
    Column->AddChildToVerticalBox(Sized(T, nullptr, Stack, 0.f, 0.f))->SetPadding(FMargin(0.f, 10.f, 0.f, 10.f));
    Column->AddChildToVerticalBox(KitRule(T));
    T->RootWidget = Column;
}

// Objective rows 0..2 (tick, label, count), named Objective<i>Row / Tick / Label / Count (USFQuestBoardPanel, USFDialoguePanel).
void ObjectiveRows(UWidgetTree* T, UVerticalBox* Into)
{
    for (int32 Index = 0; Index < 3; ++Index)
    {
        const FString Prefix = FString::Printf(TEXT("Objective%d"), Index);
        UHorizontalBox* Row = N<UHorizontalBox>(T, *(Prefix + TEXT("Row")));
        UImage* Tick = InvImage(T, *(Prefix + TEXT("Tick")), KImg(TEXT("icon_check_ok"), 56.f, 56.f), true);
        Tick->SetVisibility(ESlateVisibility::Hidden);
        Row->AddChildToHorizontalBox(Sized(T, nullptr, Tick, 56.f, 56.f))->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Label = SizedText(T, *(Prefix + TEXT("Label")), TEXT(""), 34.f);
        Label->SetAutoWrapText(true);
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetVerticalAlignment(VAlign_Center); LabelSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));
        UTextBlock* Count = SizedText(T, *(Prefix + TEXT("Count")), TEXT(""), 34.f);
        Count->SetJustification(ETextJustify::Right);
        Row->AddChildToHorizontalBox(Count)->SetVerticalAlignment(VAlign_Center);
        Into->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    }
}

// Reward item slots Reward0..3 (WBP_ItemSlot), hidden until a quest shows rewards.
UHorizontalBox* RewardSlotRow(UWidgetTree* T)
{
    UHorizontalBox* Row = A<UHorizontalBox>(T);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (UUserWidget* Slot = Inst(T, CommonDir, TEXT("WBP_ItemSlot"), *FString::Printf(TEXT("Reward%d"), Index)))
        {
            Slot->SetVisibility(ESlateVisibility::Collapsed);
            Row->AddChildToHorizontalBox(Slot)->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
        }
    }
    return Row;
}

// A button of the footer: a fixed-width WBP_Button, right of the line.
void FooterButton(UWidgetTree* T, UHorizontalBox* Line, const TCHAR* Name, const TCHAR* Label, float Width)
{
    Line->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, Name, Label), Width, 0.f))->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
}

// WBP_QuestBoard: quest list left, the selected quest right (description, objectives, rewards), Accept / Track / hold-to-Abandon in the footer.
void QuestBoardLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Quest Journal"), TEXT("icon_window_quests"), 2100.f);
    UOverlay* Pack = A<UOverlay>(T);
    Put(Pack, ListOf(T, TEXT("QuestScroll"), N<UVerticalBox>(T, TEXT("QuestList")), 860.f, 900.f), HAlign_Fill, VAlign_Fill);
    UTextBlock* Empty = Caption(T, TEXT("EmptyText"), TEXT("No quests yet."), 40.f, true);
    Empty->SetVisibility(ESlateVisibility::Collapsed);
    Put(Pack, Empty, HAlign_Center, VAlign_Center);
    Fill(Window, TEXT("Content"), Pack);

    UVerticalBox* Detail = N<UVerticalBox>(T, TEXT("Detail"));
    Detail->AddChildToVerticalBox(Caption(T, TEXT("QuestTitle"), TEXT("Quest"), 56.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    UTextBlock* Summary = Caption(T, TEXT("Summary"), TEXT(""), 32.f, true);
    Summary->SetAutoWrapText(true);
    Detail->AddChildToVerticalBox(Summary)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    UTextBlock* Reason = Caption(T, TEXT("Reason"), TEXT(""), 30.f);
    Reason->SetColorAndOpacity(FSlateColor(Amber()));
    Reason->SetVisibility(ESlateVisibility::Collapsed);
    Detail->AddChildToVerticalBox(Reason)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Detail->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 4.f, 0.f, 16.f));
    Detail->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Objectives"), 40.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    ObjectiveRows(T, Detail);
    Detail->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 8.f, 0.f, 16.f));
    Detail->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Rewards"), 40.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Detail->AddChildToVerticalBox(RewardSlotRow(T));
    UHorizontalBox* Pay = A<UHorizontalBox>(T);
    Pay->AddChildToHorizontalBox(Inst(T, CommonDir, TEXT("WBP_GoldDisplay"), TEXT("RewardGold")))->SetVerticalAlignment(VAlign_Center);
    Pay->AddChildToHorizontalBox(Caption(T, TEXT("NoReward"), TEXT("No item reward"), 34.f, true))->SetVerticalAlignment(VAlign_Center);
    Detail->AddChildToVerticalBox(Pay)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    UBorder* DetailPad = Plain(T, nullptr, FMargin(48.f, 0.f, 0.f, 0.f));
    DetailPad->SetContent(Sized(T, nullptr, Detail, 1000.f, 0.f));
    Fill(Window, TEXT("SidePanel"), DetailPad);

    UHorizontalBox* Foot = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* Spacer = Foot->AddChildToHorizontalBox(A<USpacer>(T));
    Spacer->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    FooterButton(T, Foot, TEXT("AbandonButton"), TEXT("Hold to Abandon"), 520.f);
    FooterButton(T, Foot, TEXT("TrackButton"), TEXT("Track"), 420.f);
    FooterButton(T, Foot, TEXT("AcceptButton"), TEXT("Accept"), 420.f);
    Fill(Window, TEXT("Footer"), Foot);
    WindowRoot(T, Window, true);
}

// WBP_Dialogue: the speaker's portrait and line, the responses as kit rows; the offered quest beside it (art, summary, objectives, rewards, buttons).
void DialogueLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Speaker"), TEXT("icon_window_quests"), 2152.f);  // the quests emblem is the closest existing one
    UVerticalBox* Body = A<UVerticalBox>(T);
    UHorizontalBox* Speech = A<UHorizontalBox>(T);
    UOverlay* Well = A<UOverlay>(T);
    Put(Well, InvImage(T, nullptr, KBox(TEXT("slot_back"), 18.f, 18.f, 18.f, 18.f), true), HAlign_Fill, VAlign_Fill);
    Put(Well, Sized(T, nullptr, N<UImage>(T, TEXT("Portrait")), 224.f, 224.f), HAlign_Center, VAlign_Center);
    Speech->AddChildToHorizontalBox(Sized(T, nullptr, Well, 256.f, 256.f))->SetVerticalAlignment(VAlign_Top);
    UTextBlock* Line = Caption(T, TEXT("LineText"), TEXT(""), 42.f);
    Line->SetAutoWrapText(true);
    UHorizontalBoxSlot* LineSlot = Speech->AddChildToHorizontalBox(Line);
    LineSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LineSlot->SetVerticalAlignment(VAlign_Center); LineSlot->SetPadding(FMargin(40.f, 0.f, 0.f, 0.f));
    Body->AddChildToVerticalBox(Speech)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
    Body->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
    Body->AddChildToVerticalBox(N<UVerticalBox>(T, TEXT("Choices")));
    Fill(Window, TEXT("Content"), Sized(T, nullptr, Body, 1060.f, 0.f));

    UVerticalBox* Offer = N<UVerticalBox>(T, TEXT("OfferColumn"));
    Offer->AddChildToVerticalBox(Sized(T, nullptr, InvImage(T, TEXT("OfferArt"), FSlateBrush(), true), 836.f, 400.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));
    Offer->AddChildToVerticalBox(Caption(T, TEXT("OfferTitle"), TEXT("Quest"), 52.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    UTextBlock* Summary = Caption(T, TEXT("OfferSummary"), TEXT(""), 32.f, true);
    Summary->SetAutoWrapText(true);
    Offer->AddChildToVerticalBox(Summary)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Offer->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Offer->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Objectives"), 38.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    ObjectiveRows(T, Offer);
    Offer->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f));
    Offer->AddChildToVerticalBox(Caption(T, nullptr, TEXT("Rewards"), 38.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    Offer->AddChildToVerticalBox(RewardSlotRow(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    UHorizontalBox* Buttons = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* Spacer = Buttons->AddChildToHorizontalBox(A<USpacer>(T));
    Spacer->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    FooterButton(T, Buttons, TEXT("DeclineButton"), TEXT("Decline"), 380.f);
    FooterButton(T, Buttons, TEXT("AcceptButton"), TEXT("Accept"), 380.f);
    Offer->AddChildToVerticalBox(Buttons);
    UBorder* OfferPad = Plain(T, nullptr, FMargin(48.f, 0.f, 0.f, 0.f));
    OfferPad->SetContent(Sized(T, nullptr, Offer, 900.f, 0.f));
    Fill(Window, TEXT("SidePanel"), OfferPad);
    WindowRoot(T, Window, true);
}

// WBP_GatheringSkills: four skill rows (icon over the whole-point numeral, name, raise / hold / lower button, the points bar in the inventory weight
// bar style, the next material tier).
void GatheringSkillsLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Gathering Skills"), TEXT("icon_window_recipes"), 1294.f);  // the recipes emblem is the closest existing one
    UVerticalBox* Column = A<UVerticalBox>(T);
    const TCHAR* const Names[4] = {TEXT("Logging"), TEXT("Mining"), TEXT("Herbalism"), TEXT("Fishing")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        const auto Part = [Index](const TCHAR* Suffix) { return FString::Printf(TEXT("Skill%d%s"), Index, Suffix); };
        UOverlay* Row = A<UOverlay>(T);
        UHorizontalBox* Line = A<UHorizontalBox>(T);
        UOverlay* Badge = A<UOverlay>(T);
        Put(Badge, InvImage(T, nullptr, KBox(TEXT("slot_back"), 18.f, 18.f, 18.f, 18.f), true), HAlign_Fill, VAlign_Fill);
        UVerticalBox* BadgeBody = A<UVerticalBox>(T);
        BadgeBody->AddChildToVerticalBox(Sized(T, nullptr, N<UImage>(T, *Part(TEXT("Icon"))), 112.f, 112.f))->SetHorizontalAlignment(HAlign_Center);
        UTextBlock* Level = SizedText(T, *Part(TEXT("Level")), TEXT("0"), 52.f);
        Level->SetJustification(ETextJustify::Center);
        BadgeBody->AddChildToVerticalBox(Level)->SetHorizontalAlignment(HAlign_Center);
        Put(Badge, BadgeBody, HAlign_Center, VAlign_Center, FMargin(0.f, 8.f));
        Line->AddChildToHorizontalBox(Sized(T, nullptr, Badge, 168.f, 208.f))->SetVerticalAlignment(VAlign_Center);

        UVerticalBox* Words = A<UVerticalBox>(T);
        UHorizontalBox* NameLine = A<UHorizontalBox>(T);
        UHorizontalBoxSlot* NameSlot = NameLine->AddChildToHorizontalBox(SizedText(T, *Part(TEXT("Name")), Names[Index], 44.f));
        NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center);
        NameLine->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, *Part(TEXT("Lock")), TEXT("Raising")), 400.f, 0.f))->SetVerticalAlignment(VAlign_Center);
        Words->AddChildToVerticalBox(NameLine)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
        UHorizontalBox* BarLine = A<UHorizontalBox>(T);
        UOverlay* Meter = A<UOverlay>(T);
        Put(Meter, InvImage(T, nullptr, InvBrush(TEXT("progress_track"), 680.f, 26.f, 18.f, 8.f, 18.f, 8.f, 680.f, 26.f)), HAlign_Fill, VAlign_Fill);
        UProgressBar* Bar = N<UProgressBar>(T, *Part(TEXT("Bar")));
        FProgressBarStyle BarStyle = Bar->GetWidgetStyle();
        BarStyle.SetBackgroundImage(FSlateNoResource());
        BarStyle.SetFillImage(InvBrush(TEXT("progress_fill_green"), 680.f, 22.f, 16.f, 7.f, 16.f, 7.f, 680.f, 22.f));
        Bar->SetWidgetStyle(BarStyle);
        Bar->SetPercent(0.f);
        Put(Meter, Bar, HAlign_Fill, VAlign_Fill, FMargin(0.f, 2.f));
        UHorizontalBoxSlot* MeterSlot = BarLine->AddChildToHorizontalBox(Sized(T, nullptr, Meter, 0.f, 26.f));
        MeterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); MeterSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Points = SizedText(T, *Part(TEXT("Points")), TEXT(""), 32.f);
        Points->SetJustification(ETextJustify::Right);
        BarLine->AddChildToHorizontalBox(Sized(T, nullptr, Points, 260.f, 0.f))->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));
        Words->AddChildToVerticalBox(BarLine)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
        UHorizontalBox* NextLine = A<UHorizontalBox>(T);
        NextLine->AddChildToHorizontalBox(Caption(T, nullptr, TEXT("Next:"), 32.f, true))->SetVerticalAlignment(VAlign_Center);
        NextLine->AddChildToHorizontalBox(Sized(T, nullptr, N<UImage>(T, *Part(TEXT("NextIcon"))), 56.f, 56.f))->SetPadding(FMargin(16.f, 0.f, 16.f, 0.f));
        UTextBlock* Next = SizedText(T, *Part(TEXT("Next")), TEXT(""), 32.f);
        Next->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        UHorizontalBoxSlot* NextSlot = NextLine->AddChildToHorizontalBox(Next);
        NextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NextSlot->SetVerticalAlignment(VAlign_Center);
        Words->AddChildToVerticalBox(NextLine);
        UHorizontalBoxSlot* WordsSlot = Line->AddChildToHorizontalBox(Words);
        WordsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); WordsSlot->SetVerticalAlignment(VAlign_Center); WordsSlot->SetPadding(FMargin(32.f, 0.f, 0.f, 0.f));
        UBorder* RowPad = Plain(T, nullptr, FMargin(24.f, 20.f, 32.f, 32.f));
        RowPad->SetContent(Line);
        Put(Row, RowPad, HAlign_Fill, VAlign_Fill);
        Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
        if (Index < 3) { Column->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f)); }
    }
    Fill(Window, TEXT("Content"), Sized(T, nullptr, Column, 1150.f, 0.f));
    WindowRoot(T, Window, false);
}

// A weight-bar-styled progress meter named Name (the keep countdown).
UWidget* MeterBar(UWidgetTree* T, const TCHAR* Name, float Width)
{
    UOverlay* Meter = A<UOverlay>(T);
    Put(Meter, InvImage(T, nullptr, InvBrush(TEXT("progress_track"), 680.f, 26.f, 18.f, 8.f, 18.f, 8.f, 680.f, 26.f)), HAlign_Fill, VAlign_Fill);
    UProgressBar* Bar = N<UProgressBar>(T, Name);
    FProgressBarStyle Style = Bar->GetWidgetStyle();
    Style.SetBackgroundImage(FSlateNoResource());
    Style.SetFillImage(InvBrush(TEXT("progress_fill_green"), 680.f, 22.f, 16.f, 7.f, 16.f, 7.f, 680.f, 22.f));
    Bar->SetWidgetStyle(Style);
    Bar->SetPercent(1.f);
    Put(Meter, Bar, HAlign_Fill, VAlign_Fill, FMargin(0.f, 2.f));
    return Sized(T, nullptr, Meter, Width, 26.f);
}

// WBP_Settings: tabs, the preset dropdown with Auto-Detect, the setting rows (C++ adds WBP_SettingRow rows into Rows), Apply / Reset in the footer,
// and the keep-or-revert prompt as a framed card over a dim.
void SettingsLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Settings"), TEXT("icon_window_settings"), 1352.f);
    if (UUserWidget* Tabs = Inst(T, CommonDir, TEXT("WBP_TabBar"), TEXT("TabBar"))) { Fill(Window, TEXT("Tabs"), Tabs); }

    UOverlay* Pack = A<UOverlay>(T);
    UVerticalBox* Page = N<UVerticalBox>(T, TEXT("GraphicsPage"));
    UHorizontalBox* Preset = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* PresetLabel = Preset->AddChildToHorizontalBox(Caption(T, nullptr, TEXT("Graphics Preset"), 44.f));
    PresetLabel->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); PresetLabel->SetVerticalAlignment(VAlign_Center);
    Preset->AddChildToHorizontalBox(Inst(T, CommonDir, TEXT("WBP_Dropdown"), TEXT("PresetDropdown")))->SetVerticalAlignment(VAlign_Center);
    Preset->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, TEXT("AutoDetectButton"), TEXT("Auto-Detect")), 380.f, 0.f))->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
    Page->AddChildToVerticalBox(Preset)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    Page->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    Page->AddChildToVerticalBox(ListOf(T, TEXT("SettingsScroll"), N<UVerticalBox>(T, TEXT("Rows")), 1160.f, 760.f));
    Put(Pack, Page, HAlign_Fill, VAlign_Fill);
    UTextBlock* Soon = Caption(T, TEXT("ComingSoon"), TEXT("Coming soon."), 48.f, true);
    Soon->SetVisibility(ESlateVisibility::Collapsed);
    Put(Pack, Soon, HAlign_Center, VAlign_Center);
    Fill(Window, TEXT("Content"), Sized(T, nullptr, Pack, 1208.f, 0.f));

    UHorizontalBox* Foot = A<UHorizontalBox>(T);
    UHorizontalBoxSlot* Spacer = Foot->AddChildToHorizontalBox(A<USpacer>(T));
    Spacer->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    FooterButton(T, Foot, TEXT("ResetButton"), TEXT("Reset"), 420.f);
    FooterButton(T, Foot, TEXT("ApplyButton"), TEXT("Apply"), 420.f);
    Fill(Window, TEXT("Footer"), Foot);

    UOverlay* Root = A<UOverlay>(T);
    Put(Root, Window, HAlign_Fill, VAlign_Fill);
    // The keep prompt: dims the settings window, a small framed card in the middle.
    UOverlay* Keep = N<UOverlay>(T, TEXT("KeepLayer"));
    UBorder* Dim = Plain(T, nullptr, FMargin(0.f));
    Dim->SetBrush(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f)));
    Put(Keep, Dim, HAlign_Fill, VAlign_Fill);
    if (UUserWidget* Card = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("KeepWindow")))
    {
        WindowOf(Card, TEXT("Keep these settings?"), TEXT("icon_window_settings"), 1000.f);
        UVerticalBox* Ask = A<UVerticalBox>(T);
        UTextBlock* Detail = Caption(T, TEXT("KeepDetailText"), TEXT(""), 40.f);
        Detail->SetJustification(ETextJustify::Center);
        Ask->AddChildToVerticalBox(Detail)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
        UTextBlock* Countdown = Caption(T, TEXT("KeepCountdown"), TEXT(""), 34.f, true);
        Countdown->SetJustification(ETextJustify::Center);
        Ask->AddChildToVerticalBox(Countdown)->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
        Ask->AddChildToVerticalBox(MeterBar(T, TEXT("KeepBar"), 0.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
        UHorizontalBox* Choice = A<UHorizontalBox>(T);
        Choice->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, TEXT("KeepButton"), TEXT("Keep Changes")), 400.f, 0.f))->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
        Choice->AddChildToHorizontalBox(Sized(T, nullptr, KitButton(T, TEXT("UndoButton"), TEXT("Revert")), 400.f, 0.f));
        Ask->AddChildToVerticalBox(Choice)->SetHorizontalAlignment(HAlign_Center);
        Fill(Card, TEXT("Content"), Sized(T, nullptr, Ask, 840.f, 0.f));
        Put(Keep, Card, HAlign_Center, VAlign_Center);
    }
    Keep->SetVisibility(ESlateVisibility::Collapsed);
    Put(Root, Keep, HAlign_Fill, VAlign_Fill);
    T->RootWidget = Root;
}

// WBP_GameMenu: the small Esc menu, four buttons under a frame header.
void GameMenuLayout(UWidgetTree* T)
{
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (!Window) { return; }
    WindowOf(Window, TEXT("Menu"), TEXT("icon_window_settings"), 784.f);
    UVerticalBox* Column = A<UVerticalBox>(T);
    const TCHAR* const Buttons[4][2] = {{TEXT("ResumeButton"), TEXT("Resume")}, {TEXT("SettingsButton"), TEXT("Settings")}, {TEXT("LogOutButton"), TEXT("Log Out")}, {TEXT("QuitButton"), TEXT("Quit")}};
    for (const TCHAR* const* Entry : Buttons) { Column->AddChildToVerticalBox(KitButton(T, Entry[0], Entry[1]))->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f)); }
    Fill(Window, TEXT("Content"), Sized(T, nullptr, Column, 640.f, 0.f));
    UOverlay* Root = A<UOverlay>(T);
    Put(Root, Window, HAlign_Fill, VAlign_Fill);
    T->RootWidget = Root;
}

// WBP_Defeated: the whole-screen layer (a dim and a centred frame): cause, respawn point, countdown and the Respawn button.
void DefeatedLayout(UWidgetTree* T)
{
    UOverlay* Layer = N<UOverlay>(T, TEXT("DefeatedLayer"));
    UBorder* Dim = Plain(T, nullptr, FMargin(0.f));
    Dim->SetBrush(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.4f)));
    Put(Layer, Dim, HAlign_Fill, VAlign_Fill);
    UUserWidget* Window = Inst(T, CommonDir, TEXT("WBP_WindowFrame"), TEXT("Window"));
    if (Window)
    {
        WindowOf(Window, TEXT("Defeated"), TEXT("icon_window_quests"), 1444.f);  // no defeat emblem in the kit: the quests scroll is the closest existing one
        UVerticalBox* Column = A<UVerticalBox>(T);
        UTextBlock* Cause = Caption(T, TEXT("CauseText"), TEXT("You have fallen"), 56.f);
        Cause->SetJustification(ETextJustify::Center);
        Column->AddChildToVerticalBox(Cause)->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
        Column->AddChildToVerticalBox(KitRule(T))->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
        UTextBlock* Label = Caption(T, nullptr, TEXT("Respawn point"), 34.f, true);
        Label->SetJustification(ETextJustify::Center);
        Column->AddChildToVerticalBox(Label);
        UTextBlock* Point = Caption(T, TEXT("PointText"), TEXT("Nearest respawn"), 48.f);
        Point->SetJustification(ETextJustify::Center);
        Column->AddChildToVerticalBox(Point)->SetPadding(FMargin(0.f, 0.f, 0.f, 36.f));
        Column->AddChildToVerticalBox(Sized(T, nullptr, KitButton(T, TEXT("RespawnButton"), TEXT("Respawn")), 640.f, 0.f))->SetHorizontalAlignment(HAlign_Center);
        UTextBlock* Countdown = Caption(T, TEXT("CountdownText"), TEXT(""), 30.f, true);
        Countdown->SetJustification(ETextJustify::Center);
        Column->AddChildToVerticalBox(Countdown)->SetPadding(FMargin(0.f, 20.f, 0.f, 0.f));
        Fill(Window, TEXT("Content"), Sized(T, nullptr, Column, 1300.f, 0.f));
        Put(Layer, Window, HAlign_Center, VAlign_Center);
    }
    T->RootWidget = Layer;
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
        {CommonDir, TEXT("WBP_Button"), USFButtonWidget::StaticClass(), KitButtonLayout, nullptr},
        {CommonDir, TEXT("WBP_ButtonPrimary"), USFButtonWidget::StaticClass(), [](UWidgetTree* T) { ButtonLayout(T, 72.f, 160.f, TEXT("Primary")); },
            [](UObject* Cdo) { Cast<USFButtonWidget>(Cdo)->Kind = ESFUiButtonKind::Primary; }},
        {CommonDir, TEXT("WBP_Tab"), USFTabWidget::StaticClass(), KitTabLayout, TabDefaults},
        {CommonDir, TEXT("WBP_TabBar"), USFTabBarWidget::StaticClass(), TabBarLayout, nullptr},
        {CommonDir, TEXT("WBP_Slot"), USFSlotWidget::StaticClass(), SlotLayout, nullptr},
        {CommonDir, TEXT("WBP_ItemSlot"), USFSlotWidget::StaticClass(), ItemSlotLayout, ItemSlotDefaults},
        {CommonDir, TEXT("WBP_ItemTile"), USFSlotWidget::StaticClass(), ItemTileLayout, ItemSlotDefaults},
        {CommonDir, TEXT("WBP_ListRow"), USFListRowWidget::StaticClass(), ListRowLayout, nullptr},
        {CommonDir, TEXT("WBP_Separator"), USFSeparatorWidget::StaticClass(), SeparatorLayout, SeparatorDefaults},
        {CommonDir, TEXT("WBP_ScrollList"), USFScrollListWidget::StaticClass(), ScrollListLayout, nullptr},
        {CommonDir, TEXT("WBP_GoldDisplay"), USFGoldDisplayWidget::StaticClass(), GoldDisplayLayout, nullptr},
        {CommonDir, TEXT("WBP_SearchField"), USFTextFieldWidget::StaticClass(), [](UWidgetTree* T) { FieldLayout(T, TEXT("search_field"), 400.f, true); }, SearchDefaults},
        {CommonDir, TEXT("WBP_InputField"), USFTextFieldWidget::StaticClass(), [](UWidgetTree* T) { FieldLayout(T, TEXT("input_field"), 300.f, false); }, nullptr},
        {CommonDir, TEXT("WBP_Dropdown"), USFDropdownWidget::StaticClass(), DropdownLayout, nullptr},
        {CommonDir, TEXT("WBP_Stepper"), USFStepperWidget::StaticClass(), StepperLayout, nullptr},
        {CommonDir, TEXT("WBP_WindowFrame"), USFWindowFrameWidget::StaticClass(), WindowFrameLayout, nullptr},
        {CommonDir, TEXT("WBP_KitRow"), USFKitRowWidget::StaticClass(), KitRowLayout, nullptr},
        {CommonDir, TEXT("WBP_Toggle"), USFToggleWidget::StaticClass(), ToggleLayout, nullptr},
        {CommonDir, TEXT("WBP_SettingRow"), USFSettingRowWidget::StaticClass(), SettingRowLayout, nullptr},
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
        {ExchangeDir, TEXT("WBP_MarketBoard"), USFMarketBoard::StaticClass(), MarketLayout, nullptr},
        {ExchangeDir, TEXT("WBP_TradeWindow"), USFTradeWindow::StaticClass(), TradeLayout, nullptr},
        {GatheringDir, TEXT("WBP_StationPanel"), USFStationPanel::StaticClass(), StationLayout, nullptr},
        {BuildDir, TEXT("WBP_BuildCatalogue"), USFConstructionSandboxPalette::StaticClass(), BuildLayout,
            [](UObject* Cdo) { Cast<USFConstructionSandboxPalette>(Cdo)->TabIcons = {KImg(TEXT("icon_tab_voxel"), 44.f, 44.f), KImg(TEXT("icon_tab_blueprints"), 44.f, 44.f)}; }},
        {QuestDir, TEXT("WBP_QuestBoard"), USFQuestBoardPanel::StaticClass(), QuestBoardLayout, nullptr},
        {DialogueDir, TEXT("WBP_Dialogue"), USFDialoguePanel::StaticClass(), DialogueLayout, nullptr},
        {GatheringDir, TEXT("WBP_GatheringSkills"), USFGatheringSkillsPanel::StaticClass(), GatheringSkillsLayout, nullptr},
        {SettingsDir, TEXT("WBP_Settings"), USFGraphicsMenu::StaticClass(), SettingsLayout, nullptr},
        {MenuDir, TEXT("WBP_GameMenu"), USFGameMenu::StaticClass(), GameMenuLayout, nullptr},
        {HudDir, TEXT("WBP_Defeated"), USFE1VitalsHud::StaticClass(), DefeatedLayout, nullptr},
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
    TArray<FString> OnlyList;  // Only may be one name or a comma-separated list
    Only.ParseIntoArray(OnlyList, TEXT(","));
    auto Wanted = [&OnlyList](const FString& Name) { return OnlyList.IsEmpty() || OnlyList.Contains(Name); };
    if (Wanted(TEXT("WBP_UnitFrame")))  // the owner hand-laid WBP_UnitFrame (duplicated from WBP_HudVitals): add the new named parts
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
        if (!Wanted(Spec.Name)) { continue; }
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
            Blueprint->WidgetVariableNameToGuidMap.Empty();  // the new tree registers its own variables
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
        for (UWidget* Widget : Widgets) { Blueprint->OnVariableAdded(Widget->GetFName()); }  // every widget, not only the variables: the compiler wants a GUID for each
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
    if (Wanted(TEXT("WBP_Hud"))) { Report += MigrateHud(); }
    return Report;
}
#endif
