#include "Gathering/SFStationPanel.h"

#include "Gathering/SFGatheringHud.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFStationRecipes.h"
#include "Framework/SFScreenArt.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameStateBase.h"
#include "InputAction.h"

namespace
{
template <typename T> T* StationNew(UWidgetTree* Tree) { return Tree->ConstructWidget<T>(T::StaticClass()); }

USizeBox* StationSized(UWidgetTree* Tree, UWidget* Content, float Width, float Height)
{
    USizeBox* Box = StationNew<USizeBox>(Tree);
    if (Width > 0) { Box->SetWidthOverride(Width); }
    if (Height > 0) { Box->SetHeightOverride(Height); }
    Box->SetContent(Content);
    return Box;
}

// Kit-v5 9-slice piece; the procedural frame when the kit is not imported.
UBorder* StationFrame(UWidgetTree* Tree, const TCHAR* Art, const FMargin& Padding)
{
    UBorder* Border = StationNew<UBorder>(Tree);
    Border->SetBrush(SFHudStyle::KitOrFrame(Art, SFHudStyle::Hex(TEXT("0E171C"), 0.7f), SFHudStyle::Hex(TEXT("5E5038")), 0.f));
    const float Inset = SFHudStyle::Kit(Art) ? SFHudStyle::Kit(Art)->InsetPixels : 0.f;  // clear the 9-slice border
    Border->SetPadding(FMargin(Padding.Left + Inset, Padding.Top, Padding.Right + Inset, Padding.Bottom));
    return Border;
}

UImage* StationIcon(UWidgetTree* Tree, const FString& Stem, float Side)
{
    UImage* Image = StationNew<UImage>(Tree);
    Image->SetBrush(SFScreenArt::Icon(Stem, Side));
    return Image;
}

UBorder* StationRule(UWidgetTree* Tree)
{
    UBorder* Rule = StationNew<UBorder>(Tree);
    Rule->SetBrush(SFHudStyle::Frame(SFHudStyle::Hex(TEXT("A98A55"), 0.55f), FLinearColor::Transparent, 0.f));
    return Rule;
}

// First keyboard or mouse key mapped to IA_Interact, so a rebind updates the Craft key cap; "E" when unavailable.
FString StationInteractKey(const UUserWidget* Widget)
{
    const ULocalPlayer* Player = Widget->GetOwningLocalPlayer();
    const UEnhancedInputLocalPlayerSubsystem* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    const UInputAction* Interact = LoadObject<UInputAction>(nullptr, TEXT("/Game/Frontier/Input/IA_Interact.IA_Interact"));
    if (Input && Interact) { for (const FKey& Key : Input->QueryKeysMappedToAction(Interact)) { if (!Key.IsGamepadKey()) { return Key.GetDisplayName().ToString(); } } }
    return TEXT("E");
}
}

FString USFStationPanel::IconStem(const FString& DefinitionId)
{
    FString Stem = DefinitionId;
    Stem.RemoveFromStart(TEXT("item/"));
    return Stem;
}

void USFStationPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;
    UBorder* Window = KitPanel(WidgetTree, TEXT("panel_frame"), 0.94f, Slate(0.9f), Brass(), FMargin(4.f, 0.f, 4.f, 6.f), 0.f);
    WidgetTree->RootWidget = Window;
    UVerticalBox* Stack = StationNew<UVerticalBox>(WidgetTree);
    Window->SetContent(Stack);
    Title = Heading(WidgetTree, TEXT(""), 36, Ivory());
    Stack->AddChildToVerticalBox(Title)->SetPadding(FMargin(14.f, 0.f, 0.f, 10.f));
    Stack->AddChildToVerticalBox(StationSized(WidgetTree, StationRule(WidgetTree), 0.f, 2.f));

    UHorizontalBox* Body = StationNew<UHorizontalBox>(WidgetTree);
    UVerticalBoxSlot* BodySlot = Stack->AddChildToVerticalBox(Body);
    BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // Left column: search, tabs, recipe rows.
    UVerticalBox* Left = StationNew<UVerticalBox>(WidgetTree);
    Body->AddChildToHorizontalBox(StationSized(WidgetTree, Left, 318.f, 0.f))->SetPadding(FMargin(10.f, 22.f, 16.f, 0.f));
    UBorder* Search = StationFrame(WidgetTree, TEXT("search_field"), FMargin(12.f, 4.f));
    UHorizontalBox* SearchLine = StationNew<UHorizontalBox>(WidgetTree);
    UImage* Glass = StationNew<UImage>(WidgetTree);
    Glass->SetBrush(SFScreenArt::KitImage(TEXT("icon_search"), FVector2D(26.f, 26.f)));
    SearchLine->AddChildToHorizontalBox(Glass)->SetVerticalAlignment(VAlign_Center);
    SearchLine->AddChildToHorizontalBox(Text(WidgetTree, TEXT("Search recipes"), 18, Hex(TEXT("9C9886"))))->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
    Search->SetContent(SearchLine);
    Left->AddChildToVerticalBox(StationSized(WidgetTree, Search, 0.f, 42.f));
    UHorizontalBox* Tabs = StationNew<UHorizontalBox>(WidgetTree);
    const TCHAR* TabLabels[] = {TEXT("All"), TEXT("Materials"), TEXT("Parts")};
    int32 TabIndex = 0;
    for (const TCHAR* Label : TabLabels)
    {
        UBorder* Tab = StationFrame(WidgetTree, TabIndex == 0 ? TEXT("tab_selected") : TEXT("tab_normal"), FMargin(0.f));
        UTextBlock* TabText = Text(WidgetTree, Label, 18, Ivory());
        Tab->SetContent(TabText); Tab->SetHorizontalAlignment(HAlign_Center); Tab->SetVerticalAlignment(VAlign_Center);
        UHorizontalBoxSlot* TabSlot = Tabs->AddChildToHorizontalBox(StationSized(WidgetTree, Tab, 0.f, 44.f));
        TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); TabSlot->SetPadding(FMargin(TabIndex == 0 ? 0.f : 6.f, 0.f, 0.f, 0.f));
        ++TabIndex;
    }
    Left->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 16.f, 0.f, 14.f));
    RecipeList = StationNew<UVerticalBox>(WidgetTree);
    Left->AddChildToVerticalBox(RecipeList);

    UBorder* Divider = StationRule(WidgetTree);
    Body->AddChildToHorizontalBox(StationSized(WidgetTree, Divider, 2.f, 0.f));

    // Right column: output, ingredients, stepper, Craft.
    UVerticalBox* Right = StationNew<UVerticalBox>(WidgetTree);
    UHorizontalBoxSlot* RightSlot = Body->AddChildToHorizontalBox(Right);
    RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); RightSlot->SetPadding(FMargin(22.f, 16.f, 14.f, 0.f));
    OutputName = Heading(WidgetTree, TEXT(""), 30, Ivory());
    Right->AddChildToVerticalBox(OutputName);
    OutputText = Text(WidgetTree, TEXT(""), 19, Hex(TEXT("C9C2AE")));
    OutputText->SetAutoWrapText(true);
    Right->AddChildToVerticalBox(OutputText)->SetPadding(FMargin(2.f, 2.f, 0.f, 0.f));
    OutputIcon = StationNew<UImage>(WidgetTree);
    UVerticalBoxSlot* IconSlot = Right->AddChildToVerticalBox(StationSized(WidgetTree, OutputIcon, 136.f, 136.f));
    IconSlot->SetHorizontalAlignment(HAlign_Center); IconSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 6.f));
    Right->AddChildToVerticalBox(Text(WidgetTree, TEXT("Ingredients"), 18, Ivory()))->SetPadding(FMargin(2.f, 0.f, 0.f, 6.f));
    Ingredients = StationNew<UVerticalBox>(WidgetTree);
    Right->AddChildToVerticalBox(Ingredients);

    // Quantity stepper: one craft per interact today, so the value stays 1 and both steps are drawn disabled.
    UHorizontalBox* Stepper = StationNew<UHorizontalBox>(WidgetTree);
    auto AddStep = [&](const TCHAR* Sign)
    {
        UBorder* Step = StationFrame(WidgetTree, TEXT("stepper_field"), FMargin(0.f));
        UImage* Glyph = StationNew<UImage>(WidgetTree);
        Glyph->SetBrush(SFScreenArt::KitImage(Sign, FVector2D(24.f, 24.f)));
        Glyph->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.5f));  // disabled
        Step->SetContent(Glyph);
        Step->SetHorizontalAlignment(HAlign_Center); Step->SetVerticalAlignment(VAlign_Center);
        Stepper->AddChildToHorizontalBox(StationSized(WidgetTree, Step, 52.f, 46.f));
    };
    AddStep(TEXT("minus"));
    UBorder* Value = StationFrame(WidgetTree, TEXT("stepper_field"), FMargin(0.f));
    Value->SetContent(Num(WidgetTree, TEXT("1"), 20, Ivory(), false));
    Value->SetHorizontalAlignment(HAlign_Center); Value->SetVerticalAlignment(VAlign_Center);
    Stepper->AddChildToHorizontalBox(StationSized(WidgetTree, Value, 90.f, 46.f))->SetPadding(FMargin(6.f, 0.f));
    AddStep(TEXT("plus"));
    UVerticalBoxSlot* StepperSlot = Right->AddChildToVerticalBox(Stepper);
    StepperSlot->SetHorizontalAlignment(HAlign_Center); StepperSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 6.f));
    CraftTime = Text(WidgetTree, TEXT(""), 18, Hex(TEXT("BDB6A2")));
    Right->AddChildToVerticalBox(CraftTime)->SetHorizontalAlignment(HAlign_Center);

    // The shared Primary button (IMPLEMENTATION-FIX G3): no hold ring, crafting is not irreversible. The interact key cap
    // sits inside its left end: the Kit-v5 keycap_<key> art when the kit has that key, else the blank cap with the key as text.
    const FSFHudButton CraftButton = SFHudStyle::Button(WidgetTree, TEXT("Craft"), EButtonKind::Primary, 370.f);
    UOverlay* CraftFace = StationNew<UOverlay>(WidgetTree);
    CraftFace->AddChildToOverlay(CraftButton.Root);
    UOverlay* Cap = StationNew<UOverlay>(WidgetTree);
    CraftCap = StationNew<UImage>(WidgetTree);
    Cap->AddChildToOverlay(CraftCap);
    CraftKey = Num(WidgetTree, TEXT(""), 18, Ivory());
    UOverlaySlot* KeySlot = Cap->AddChildToOverlay(CraftKey);
    KeySlot->SetHorizontalAlignment(HAlign_Center); KeySlot->SetVerticalAlignment(VAlign_Center);
    UOverlaySlot* CapSlot = CraftFace->AddChildToOverlay(StationSized(WidgetTree, Cap, 40.f, 36.f));
    CapSlot->SetHorizontalAlignment(HAlign_Left); CapSlot->SetVerticalAlignment(VAlign_Center); CapSlot->SetPadding(FMargin(20.f, 0.f, 0.f, 0.f));
    UVerticalBoxSlot* CraftSlot = Right->AddChildToVerticalBox(CraftFace);
    CraftSlot->SetHorizontalAlignment(HAlign_Center); CraftSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 12.f));

    // Queue: the craft in progress at this station (one at a time).
    Stack->AddChildToVerticalBox(StationSized(WidgetTree, StationRule(WidgetTree), 0.f, 2.f));
    QueueTitle = Text(WidgetTree, TEXT(""), 18, Ivory());
    Stack->AddChildToVerticalBox(QueueTitle)->SetPadding(FMargin(16.f, 10.f, 0.f, 6.f));
    QueueRow = StationFrame(WidgetTree, TEXT("queue_row"), FMargin(14.f, 2.f, 22.f, 2.f));
    UHorizontalBox* QueueLine = StationNew<UHorizontalBox>(WidgetTree);
    QueueIcon = StationNew<UImage>(WidgetTree);
    QueueLine->AddChildToHorizontalBox(StationSized(WidgetTree, QueueIcon, 44.f, 44.f))->SetVerticalAlignment(VAlign_Center);
    QueueName = Text(WidgetTree, TEXT(""), 20, Ivory());
    QueueLine->AddChildToHorizontalBox(StationSized(WidgetTree, QueueName, 230.f, 0.f))->SetPadding(FMargin(18.f, 0.f, 0.f, 0.f));
    // Kit-v5 framed channel bar; the styled UProgressBar when the kit is missing.
    QueueFramed = MakeFramedBar(WidgetTree, TEXT("bar_channel"), 360.f);
    if (QueueFramed.Root) { QueueLine->AddChildToHorizontalBox(QueueFramed.Root)->SetVerticalAlignment(VAlign_Center); }
    else
    {
        QueueBar = StationNew<UProgressBar>(WidgetTree);
        StyleBar(QueueBar, Moss());
        UHorizontalBoxSlot* QueueBarSlot = QueueLine->AddChildToHorizontalBox(StationSized(WidgetTree, QueueBar, 0.f, 10.f));
        QueueBarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); QueueBarSlot->SetVerticalAlignment(VAlign_Center);
    }
    QueuePercent = Num(WidgetTree, TEXT(""), 18, Ivory(), false);
    QueuePercent->SetJustification(ETextJustify::Right);
    QueueLine->AddChildToHorizontalBox(StationSized(WidgetTree, QueuePercent, 70.f, 0.f))->SetVerticalAlignment(VAlign_Center);
    for (UWidget* Child : QueueLine->GetAllChildren()) { if (UHorizontalBoxSlot* S = Cast<UHorizontalBoxSlot>(Child->Slot)) { S->SetVerticalAlignment(VAlign_Center); } }
    QueueRow->SetContent(QueueLine);
    Stack->AddChildToVerticalBox(StationSized(WidgetTree, QueueRow, 0.f, 54.f))->SetPadding(FMargin(12.f, 0.f, 12.f, 4.f));
    QueueEmpty = Text(WidgetTree, TEXT("Nothing crafting"), 18, Hex(TEXT("9C9886")));
    Stack->AddChildToVerticalBox(QueueEmpty)->SetPadding(FMargin(16.f, 10.f, 0.f, 22.f));
}

void USFStationPanel::Rebuild(USFGatheringPilotComponent* Pilot, const ASFGatheringPilotNode* Station, const FString& RecipeId)
{
    using namespace SFHudStyle;
    const FString StationId = USFGatheringPilotComponent::StationName(Station->Kind);
    Title->SetText(FText::FromString(StationId.Left(1).ToUpper() + StationId.Mid(1)));
    const TArray<const FSFStationRecipe*> Recipes = SFStationRecipes::ForStation(StationId);
    const FSFStationRecipe* Selected = SFStationRecipes::Find(RecipeId);
    if (!Selected && !Recipes.IsEmpty()) { Selected = Recipes[0]; }

    RecipeList->ClearChildren();
    for (const FSFStationRecipe* Recipe : Recipes)
    {
        if (Recipe->Outputs.IsEmpty()) { continue; }
        const bool bSelected = Recipe == Selected;
        // Selected: the row tinted moss (state colour), with a brighter brass edge from the tint's lift.
        UBorder* Row = StationFrame(WidgetTree, TEXT("recipe_row"), FMargin(14.f, 2.f, 10.f, 2.f));
        if (bSelected) { Row->SetBrushColor(FLinearColor(0.55f, 0.85f, 0.5f)); }
        UHorizontalBox* Line = StationNew<UHorizontalBox>(WidgetTree);
        Line->AddChildToHorizontalBox(StationSized(WidgetTree, StationIcon(WidgetTree, IconStem(Recipe->Outputs[0].DefinitionId), 40.f), 40.f, 40.f))->SetVerticalAlignment(VAlign_Center);
        UTextBlock* RecipeName = Text(WidgetTree, SFStationRecipes::ItemDisplayName(Recipe->Outputs[0].DefinitionId), 18, Ivory());
        RecipeName->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(RecipeName);
        NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center); NameSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
        Row->SetContent(Line);
        UWidget* RowWidget = StationSized(WidgetTree, Row, 0.f, 56.f);
        if (bSelected)
        {
            // A small brass chevron notch on the panel's left edge points at the selected row.
            UOverlay* Marked = StationNew<UOverlay>(WidgetTree);
            Marked->AddChildToOverlay(RowWidget);
            USizeBox* Notch = SFHudStyle::KitImage(WidgetTree, TEXT("row_chevron"), 14.f);
            Notch->SetRenderTransformAngle(180.f); Notch->SetVisibility(ESlateVisibility::HitTestInvisible);
            UOverlaySlot* NotchSlot = Marked->AddChildToOverlay(Notch);
            NotchSlot->SetHorizontalAlignment(HAlign_Left); NotchSlot->SetVerticalAlignment(VAlign_Center); NotchSlot->SetPadding(FMargin(-18.f, 0.f, 0.f, 0.f));
            RowWidget = Marked;
        }
        RecipeList->AddChildToVerticalBox(RowWidget)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    }

    Ingredients->ClearChildren(); HaveTexts.Reset(); Glyphs.Reset(); Underlines.Reset(); NeedNames.Reset(); NeedAmounts.Reset();
    if (!Selected || Selected->Outputs.IsEmpty()) { return; }
    const FSFItemStack& Output = Selected->Outputs[0];
    OutputName->SetText(FText::FromString(SFStationRecipes::ItemDisplayName(Output.DefinitionId)));
    OutputText->SetText(FText::FromString(FString::Printf(TEXT("Makes %s."), *Selected->Describe())));
    OutputIcon->SetBrush(SFScreenArt::Icon(IconStem(Output.DefinitionId), 136.f));
    CraftTime->SetText(FText::FromString(FString::Printf(TEXT("Craft time  %s s"), *FText::AsNumber(GetDefault<USFGatheringPilotSettings>()->RefineSeconds).ToString())));
    for (const FSFItemStack& Input : Selected->Inputs)
    {
        UBorder* Row = StationNew<UBorder>(WidgetTree);  // flat inset row, no brass frame
        Row->SetBrush(Frame(Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), 0.f)); Row->SetPadding(FMargin(14.f, 2.f, 16.f, 2.f));
        UHorizontalBox* Line = StationNew<UHorizontalBox>(WidgetTree);
        Line->AddChildToHorizontalBox(StationSized(WidgetTree, StationIcon(WidgetTree, IconStem(Input.DefinitionId), 40.f), 40.f, 40.f))->SetVerticalAlignment(VAlign_Center);
        const FString Name = SFStationRecipes::ItemDisplayName(Input.DefinitionId);
        UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Text(WidgetTree, Name, 20, Ivory()));
        NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center); NameSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
        // The short number carries a 2 px crimson underline; the glyph at row end says met or short.
        UVerticalBox* HaveColumn = StationNew<UVerticalBox>(WidgetTree);
        UTextBlock* Have = Num(WidgetTree, TEXT("0"), 19, Ivory(), false);
        HaveColumn->AddChildToVerticalBox(Have);
        UBorder* Underline = StationNew<UBorder>(WidgetTree);
        Underline->SetBrush(Frame(Crimson(), Crimson(), 0.f)); Underline->SetVisibility(ESlateVisibility::Hidden);
        HaveColumn->AddChildToVerticalBox(StationSized(WidgetTree, Underline, 0.f, 2.f));
        Line->AddChildToHorizontalBox(HaveColumn)->SetVerticalAlignment(VAlign_Center);
        Line->AddChildToHorizontalBox(Num(WidgetTree, FString::Printf(TEXT(" / %lld"), Input.Quantity), 19, Ivory(), false))->SetVerticalAlignment(VAlign_Center);
        UImage* Glyph = StationNew<UImage>(WidgetTree);
        UHorizontalBoxSlot* GlyphSlot = Line->AddChildToHorizontalBox(StationSized(WidgetTree, Glyph, 28.f, 28.f));
        GlyphSlot->SetVerticalAlignment(VAlign_Center); GlyphSlot->SetPadding(FMargin(28.f, 0.f, 0.f, 0.f));
        Row->SetContent(Line);
        Ingredients->AddChildToVerticalBox(StationSized(WidgetTree, Row, 0.f, 44.f))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        HaveTexts.Add(Have); Glyphs.Add(Glyph); Underlines.Add(Underline); NeedNames.Add(Name); NeedAmounts.Add(static_cast<int32>(Input.Quantity));
    }
}

void USFStationPanel::ShowCraftKey(const FString& Key)
{
    const FString Piece = TEXT("keycap_") + Key.ToLower();
    const bool bBaked = SFHudStyle::Kit(*Piece) != nullptr;
    CraftCap->SetBrush(SFScreenArt::KitImage(bBaked ? *Piece : TEXT("keycap_blank"), FVector2D(40.f, 36.f)));
    CraftKey->SetText(FText::FromString(bBaked ? FString() : Key));
}

void USFStationPanel::Present(USFGatheringPilotComponent* Pilot, const ASFGatheringPilotNode* Station)
{
    SetVisibility(Pilot && Station ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!Pilot || !Station) { ShownKey.Reset(); return; }
    using namespace SFHudStyle;
    const FString RecipeId = Pilot->GetStationRecipe(Station->Kind);
    const FString Key = FString::Printf(TEXT("%d|%s"), int32(Station->Kind), *RecipeId);
    if (Key != ShownKey) { ShownKey = Key; Rebuild(Pilot, Station, RecipeId); ShowCraftKey(StationInteractKey(this)); }

    const TArray<TPair<FText, int32>> Items = Pilot->GetInventoryItems();
    for (int32 I = 0; I < NeedNames.Num(); ++I)
    {
        int32 Have = 0;
        for (const TPair<FText, int32>& Item : Items) { if (Item.Key.ToString() == NeedNames[I]) { Have += Item.Value; } }
        const bool bMet = USFGatheringHud::NeedMet(Have, NeedAmounts[I]);
        HaveTexts[I]->SetText(FText::AsNumber(Have));
        HaveTexts[I]->SetColorAndOpacity(FSlateColor(bMet ? Ivory() : Crimson()));
        Underlines[I]->SetVisibility(bMet ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
        if (const FSFHudKitElement* Glyph = Kit(bMet ? TEXT("status_tick") : TEXT("status_cross")))
        { if (Glyphs[I]->GetBrush().GetResourceObject() != Glyph->Texture) { Glyphs[I]->SetBrush(KitBrush(*Glyph)); } }
    }

    // Queue row: the station's craft while the pilot is refining it.
    const FSFGatheringPilotFeedback& Feedback = Pilot->GetFeedback();
    const bool bCrafting = (Feedback.Phase == ESFGatheringPilotPhase::Harvesting || Feedback.Phase == ESFGatheringPilotPhase::Committing) && Feedback.Node.Get() == Station;
    QueueTitle->SetText(FText::FromString(FString::Printf(TEXT("Crafting Queue  (%d/1)"), bCrafting ? 1 : 0)));
    QueueRow->GetParent()->SetVisibility(bCrafting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    QueueEmpty->SetVisibility(bCrafting ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (!bCrafting) { return; }
    const FSFStationRecipe* Recipe = SFStationRecipes::Find(RecipeId);
    if (!Recipe) { const TArray<const FSFStationRecipe*> All = SFStationRecipes::ForStation(USFGatheringPilotComponent::StationName(Station->Kind)); Recipe = All.IsEmpty() ? nullptr : All[0]; }
    if (Recipe && !Recipe->Outputs.IsEmpty())
    {
        QueueName->SetText(FText::FromString(SFStationRecipes::ItemDisplayName(Recipe->Outputs[0].DefinitionId)));
        QueueIcon->SetBrush(SFScreenArt::Icon(IconStem(Recipe->Outputs[0].DefinitionId), 44.f));
    }
    float Fraction = 1.f;
    if (Feedback.Phase == ESFGatheringPilotPhase::Harvesting)
    {
        const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
        const double Remaining = FMath::Max(0.0, Feedback.EndsAtServerSeconds - (GameState ? GameState->GetServerWorldTimeSeconds() : 0.0));
        if (QueueOperation != Feedback.Operation) { QueueOperation = Feedback.Operation; QueueTotalSeconds = Remaining; }
        QueueTotalSeconds = FMath::Max(QueueTotalSeconds, Remaining);
        Fraction = USFGatheringHud::RingFraction(Remaining, QueueTotalSeconds);
    }
    if (QueueFramed.Root) { QueueFramed.SetFraction(Fraction); } else if (QueueBar) { QueueBar->SetPercent(Fraction); }
    QueuePercent->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Fraction * 100.f))));
}
