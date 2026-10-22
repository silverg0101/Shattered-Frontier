#include "UI/SFWidgetAuthoring.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "BaseWidgetBlueprint.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Framework/SFHudStyle.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UI/SFHudWidgets.h"
#include "UI/SFUiWidgets.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintOperationUtils.h"

using namespace SFHudStyle;

namespace
{
constexpr const TCHAR* CommonDir = TEXT("/Game/Frontier/UI/Widgets/Common");
constexpr const TCHAR* HudDir = TEXT("/Game/Frontier/UI/Widgets/Hud");

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

// One framed bar: back art, a progress bar over the fill region, frame art on top, numeral centred (Kit-v5 bar geometry).
UWidget* BarStack(UWidgetTree* T, const TCHAR* Base, const TCHAR* BarName, const TCHAR* TextName)
{
    const FSFHudKitElement* FrameArt = Kit(*FString::Printf(TEXT("%s_frame"), Base));
    constexpr float InsetLeft = 21.f, InsetTop = 6.5f, InsetRight = 359.f, InsetBottom = 25.5f;  // 1x px of the 760x64 source
    const float Scale = FrameArt ? 380.f / static_cast<float>(KitDesignSize(*FrameArt).X) : 1.f;
    const float FrameHeight = FrameArt ? static_cast<float>(KitDesignSize(*FrameArt).Y) * Scale : 32.f;
    UOverlay* Stack = A<UOverlay>(T);
    Put(Stack, Piece(T, nullptr, *FString::Printf(TEXT("%s_back"), Base), 380.f, FrameHeight), HAlign_Left, VAlign_Top);
    UProgressBar* Bar = N<UProgressBar>(T, BarName);
    Bar->SetPercent(1.f);
    Put(Stack, Sized(T, nullptr, Bar, (InsetRight - InsetLeft) * Scale - 6.f, (InsetBottom - InsetTop) * Scale), HAlign_Left, VAlign_Center, FMargin(InsetLeft * Scale, 0.f, 0.f, 0.f));
    Put(Stack, Piece(T, nullptr, *FString::Printf(TEXT("%s_frame"), Base), 380.f, FrameHeight), HAlign_Left, VAlign_Top);
    Put(Stack, Txt(T, TextName, TEXT("")), HAlign_Center, VAlign_Center);
    return Sized(T, nullptr, Stack, 380.f, FrameHeight);
}

void VitalsLayout(UWidgetTree* T)
{
    UHorizontalBox* Row = A<UHorizontalBox>(T);
    UOverlay* Badge = A<UOverlay>(T);
    Badge->AddChildToOverlay(Piece(T, nullptr, TEXT("hex_level_badge"), 115.f, 158.f));
    Put(Badge, Txt(T, TEXT("LevelText"), TEXT("1")), HAlign_Center, VAlign_Bottom, FMargin(0.f, 0.f, 0.f, 18.f));
    Row->AddChildToHorizontalBox(Badge)->SetVerticalAlignment(VAlign_Center);
    UVerticalBox* Bars = A<UVerticalBox>(T);
    Bars->AddChildToVerticalBox(BarStack(T, TEXT("bar_hp"), TEXT("HealthBar"), TEXT("HealthText")));
    Bars->AddChildToVerticalBox(BarStack(T, TEXT("bar_stamina"), TEXT("StaminaBar"), TEXT("StaminaText")))->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    UHorizontalBoxSlot* BarsSlot = Row->AddChildToHorizontalBox(Bars);
    BarsSlot->SetVerticalAlignment(VAlign_Center); BarsSlot->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
    T->RootWidget = Row;
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
    Zone->SetJustification(ETextJustify::Center);
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
    auto Child = [&](const TCHAR* Name, const TCHAR* WbpName, float X, float Y, float AlignX, float AlignY)
    {
        UClass* Class = SharedClass(HudDir, WbpName);
        UUserWidget* Widget = T->ConstructWidget<UUserWidget>(TSubclassOf<UUserWidget>(Class), FName(Name));
        if (!Widget) { return; }
        Widget->bIsVariable = true;
        Anchor(Root, Widget, X, Y, AlignX, AlignY);
    };
    Child(TEXT("Vitals"), TEXT("WBP_HudVitals"), 0.02f, 0.974f, 0.f, 1.f);
    Child(TEXT("Hotbar"), TEXT("WBP_HudHotbar"), 0.5f, 0.95f, 0.5f, 1.f);
    Child(TEXT("Minimap"), TEXT("WBP_HudMinimap"), 0.99f, 0.02f, 1.f, 0.f);
    Child(TEXT("QuestTracker"), TEXT("WBP_HudQuestTracker"), 0.84f, 0.313f, 0.f, 0.f);
    Child(TEXT("Toasts"), TEXT("WBP_HudToasts"), 0.84f, 0.47f, 0.f, 0.f);
    T->RootWidget = Root;
}

struct FSpec
{
    const TCHAR* Dir;
    const TCHAR* Name;
    UClass* Parent;
    TFunction<void(UWidgetTree*)> Layout;
    TFunction<void(UObject*)> Defaults;
};

// Dependency order: a WBP that nests another is listed after it.
TArray<FSpec> Specs()
{
    auto Row = [](UObject* Cdo) { return Cast<USFListRowWidget>(Cdo); };
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
        {HudDir, TEXT("WBP_HudVitals"), USFHudVitalsWidget::StaticClass(), VitalsLayout, nullptr},
        {HudDir, TEXT("WBP_HudHotbar"), USFHudHotbarWidget::StaticClass(), HotbarLayout, nullptr},
        {HudDir, TEXT("WBP_HudMinimap"), USFHudMinimapWidget::StaticClass(), MinimapLayout, nullptr},
        {HudDir, TEXT("WBP_HudQuestTracker"), USFHudQuestTrackerWidget::StaticClass(), QuestTrackerLayout, nullptr},
        {HudDir, TEXT("WBP_HudToasts"), USFHudToastsWidget::StaticClass(), ToastsLayout, nullptr},
        {HudDir, TEXT("WBP_Hud"), USFHudWidget::StaticClass(), HudLayout, nullptr},
    };
}
}

FString USFWidgetAuthoring::BuildAll(bool bOverwrite)
{
    FString Report;
    for (const FSpec& Spec : Specs())
    {
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
            Blueprint = FWidgetBlueprintOperationUtils::CreateWidgetBlueprint(Package, FName(Spec.Name), BPTYPE_Normal, TSubclassOf<UUserWidget>(Spec.Parent), nullptr, NAME_None, false);
            if (!Blueprint) { Report += FString::Printf(TEXT("FAIL   %s (create)\n"), *PackageName); continue; }
            FAssetRegistryModule::AssetCreated(Blueprint);
        }

        Spec.Layout(Blueprint->WidgetTree);
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
    return Report;
}
#endif
