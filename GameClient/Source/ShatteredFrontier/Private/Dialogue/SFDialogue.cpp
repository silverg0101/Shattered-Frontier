#include "Dialogue/SFDialogue.h"

#include "Animation/AnimInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Dom/JsonObject.h"
#include "Framework/SFHudStyle.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/ConstructorHelpers.h"

// ---- Model ----

bool FSFDialogueTree::Parse(const FString& Json, FSFDialogueTree& Out, FString& Error)
{
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()) { Error = TEXT("not valid JSON"); return false; }
    FSFDialogueTree Tree;
    Tree.Speaker = Root->GetStringField(TEXT("speaker"));
    Root->TryGetStringField(TEXT("portrait"), Tree.Portrait);
    Tree.Start = Root->GetStringField(TEXT("start"));
    const TSharedPtr<FJsonObject>* NodeObjects = nullptr;
    if (Tree.Speaker.IsEmpty() || !Root->TryGetObjectField(TEXT("nodes"), NodeObjects)) { Error = TEXT("speaker and nodes are required"); return false; }
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*NodeObjects)->Values)
    {
        const TSharedPtr<FJsonObject> Object = Entry.Value->AsObject();
        if (!Object.IsValid()) { Error = FString::Printf(TEXT("node %s is not an object"), *Entry.Key); return false; }
        FSFDialogueNode Node; Node.Id = Entry.Key; Node.Line = Object->GetStringField(TEXT("line"));
        const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
        if (!Object->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() < 1 || Choices->Num() > MaxChoices)
        { Error = FString::Printf(TEXT("node %s needs 1-%d choices"), *Entry.Key, MaxChoices); return false; }
        for (const TSharedPtr<FJsonValue>& Value : *Choices)
        {
            const TSharedPtr<FJsonObject> ChoiceObject = Value->AsObject();
            FSFDialogueChoice Choice;
            if (!ChoiceObject.IsValid() || !ChoiceObject->TryGetStringField(TEXT("label"), Choice.Label)) { Error = FString::Printf(TEXT("node %s has a choice without a label"), *Entry.Key); return false; }
            ChoiceObject->TryGetStringField(TEXT("next"), Choice.Next); ChoiceObject->TryGetStringField(TEXT("action"), Choice.Action);
            Node.Choices.Add(Choice);
        }
        Tree.Nodes.Add(Entry.Key, Node);
    }
    const TSharedPtr<FJsonObject>* OfferObject = nullptr;
    if (Root->TryGetObjectField(TEXT("offer"), OfferObject))
    {
        Tree.bHasOffer = true; FSFDialogueOffer& Offer = Tree.Offer;
        (*OfferObject)->TryGetStringField(TEXT("title"), Offer.Title); (*OfferObject)->TryGetStringField(TEXT("summary"), Offer.Summary);
        (*OfferObject)->TryGetStringField(TEXT("art"), Offer.ArtPath);
        Offer.AcceptedNode = TEXT("accepted"); (*OfferObject)->TryGetStringField(TEXT("accepted_node"), Offer.AcceptedNode);
        const TArray<TSharedPtr<FJsonValue>>* Objectives = nullptr;
        if ((*OfferObject)->TryGetArrayField(TEXT("objectives"), Objectives))
        {
            for (const TSharedPtr<FJsonValue>& Value : *Objectives)
            {
                const TSharedPtr<FJsonObject> Object = Value->AsObject(); if (!Object.IsValid()) { continue; }
                FSFDialogueObjective Objective; Objective.Label = Object->GetStringField(TEXT("label")); Objective.Need = Object->GetIntegerField(TEXT("need"));
                Offer.Objectives.Add(Objective);
            }
        }
        const TSharedPtr<FJsonObject>* Rewards = nullptr;
        if ((*OfferObject)->TryGetObjectField(TEXT("rewards"), Rewards)) { (*Rewards)->TryGetNumberField(TEXT("xp"), Offer.RewardXp); (*Rewards)->TryGetNumberField(TEXT("coin"), Offer.RewardCoin); }
        if (!Tree.Nodes.Contains(Offer.AcceptedNode)) { Error = TEXT("offer needs its accepted node"); return false; }
    }
    if (!Tree.Nodes.Contains(Tree.Start)) { Error = TEXT("start node is missing"); return false; }
    for (const TPair<FString, FSFDialogueNode>& Entry : Tree.Nodes)
    {
        for (const FSFDialogueChoice& Choice : Entry.Value.Choices)
        {
            const bool bTerminal = Choice.Action == TEXT("end") || Choice.Action == TEXT("accept");
            if (!bTerminal && !Tree.Nodes.Contains(Choice.Next)) { Error = FString::Printf(TEXT("node %s points at missing node '%s'"), *Entry.Key, *Choice.Next); return false; }
            if (Choice.Action == TEXT("accept") && !Tree.bHasOffer) { Error = FString::Printf(TEXT("node %s accepts an offer the tree does not have"), *Entry.Key); return false; }
        }
    }
    Out = MoveTemp(Tree);
    return true;
}

bool FSFDialogueTree::Load(const FString& File, FSFDialogueTree& Out, FString& Error)
{
    if (File.IsEmpty() || File.Contains(TEXT("/")) || File.Contains(TEXT("\\")) || File.Contains(TEXT(".."))) { Error = TEXT("DialogueFile must be a file name in content/dialogue"); return false; }
    FString Json;
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/dialogue"), File));
    if (!FFileHelper::LoadFileToString(Json, *Path)) { Error = FString::Printf(TEXT("cannot read %s"), *Path); return false; }
    return Parse(Json, Out, Error);
}

ESFDialogueStep FSFDialogueSession::Choose(int32 Index)
{
    const FSFDialogueNode* Current = Node();
    if (!Current || !Current->Choices.IsValidIndex(Index)) { return ESFDialogueStep::Stay; }
    const FSFDialogueChoice& Choice = Current->Choices[Index];
    if (Choice.Action == TEXT("end")) { return ESFDialogueStep::Closed; }
    if (Choice.Action == TEXT("accept")) { return Accept(); }
    NodeId = Choice.Next;
    return ESFDialogueStep::Stay;
}

ESFDialogueStep FSFDialogueSession::Accept()
{
    if (!Tree.IsValid() || !Tree->bHasOffer || bAccepted) { return ESFDialogueStep::Stay; }
    bAccepted = true;
    NodeId = Tree->Offer.AcceptedNode;
    return ESFDialogueStep::Accepted;
}

// ---- Speaker ----

ASFDialogueNpc::ASFDialogueNpc()
{
    bReplicates = false;
    Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
    SetRootComponent(Body);
    // Nothing collides with the speaker; it answers visibility traces only, so the framing camera can see it.
    Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    // The mannequin and its AnimBP idle, as the player character renders them (SFE1Character).
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Mannequin/Character/Mesh/SK_Mannequin.SK_Mannequin"));
    static ConstructorHelpers::FClassFinder<UAnimInstance> Anim(TEXT("/Game/Mannequin/Animations/ThirdPerson_AnimBP"));
    if (Mesh.Succeeded()) { Body->SetSkeletalMeshAsset(Mesh.Object); }
    if (Anim.Succeeded()) { Body->SetAnimationMode(EAnimationMode::AnimationBlueprint); Body->SetAnimInstanceClass(Anim.Class); }
    Body->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));  // the mannequin faces +Y; the actor faces +X
}

FVector ASFDialogueNpc::GetSpeakerAnchor() const { return GetActorLocation() + FVector(0.f, 0.f, 160.f); }

// ---- Panel ----

namespace
{
using namespace SFHudStyle;


UBorder* ShadeScene(UCanvasPanel* Root, UWidgetTree* Tree)
{
    UBorder* Shade = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Shade->SetBrush(FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 0.40f)));
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Shade);
    Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); Slot->SetOffsets(FMargin(0.f));
    return Shade;
}

UImage* ArtImage(UWidgetTree* Tree, USizeBox*& OutBox, float Width, float Height)
{
    OutBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    OutBox->SetWidthOverride(Width); OutBox->SetHeightOverride(Height);
    UImage* Image = Tree->ConstructWidget<UImage>(UImage::StaticClass());
    OutBox->SetContent(Image);
    return Image;
}

// The engraved horizontal divider, as wide as its slot.
UWidget* Rule(UWidgetTree* Tree)
{
    USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Box->SetHeightOverride(8.f); Box->SetContent(KitPanel(Tree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f, false));
    return Box;
}

UWidget* SectionRule(UWidgetTree* Tree)
{
    USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Box->SetHeightOverride(8.f); Box->SetContent(KitPanel(Tree, Kit(TEXT("section_rule")) ? TEXT("section_rule") : TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f, false));
    return Box;
}

UTexture2D* TextureFor(const FString& Path)
{
    if (Path.IsEmpty()) { return nullptr; }
    if (UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) { return Loaded; }
    const FSFHudKitElement* Element = Kit(*Path);
    return Element ? Element->Texture.Get() : nullptr;
}

void SetArt(UImage* Image, UTexture2D* Texture, const FVector2D& Size)
{
    FSlateBrush Brush;
    if (Texture) { Brush.SetResourceObject(Texture); } else { Brush.TintColor = FSlateColor(FLinearColor::Transparent); }
    Brush.ImageSize = Size;
    Image->SetBrush(Brush);
}
}

// Concept 10-npc-dialogue measured at 1080p: board x 486-1437, y 219-841. Rule 12 places the board at x 26-74 % y 14-78 %
// (922 x 691 at 1080p), so a concept position maps by the board's own scale around its centre; type sizes do not scale.
namespace DialogueLayout
{
constexpr float KX = 922.f / 951.f, KY = 691.f / 622.f;
inline float X(float ConceptX) { return 461.f + (ConceptX - 961.5f) * KX; }
inline float Y(float ConceptY) { return (ConceptY - 219.f) * KY; }
// EB Garamond: the cap top sits this many px below the text block's top per px of font size.
constexpr float CapLift = 0.43f;

// Places a widget by its concept rectangle; Width/Height override the mapped size when given. A fixed Height keeps the
// concept's centre line (Top + Height / 2), since positions scale with the board and fixed heights do not.
UCanvasPanelSlot* Box(UCanvasPanel* Into, UWidget* Widget, float Left, float Top, float Right, float Bottom, float Width = 0.f, float Height = 0.f)
{
    UCanvasPanelSlot* Slot = Into->AddChildToCanvas(Widget);
    Slot->SetPosition(FVector2D(X(Left), Height > 0.f ? Y(Top + Height * 0.5f) - Height * 0.5f : Y(Top)));
    Slot->SetSize(FVector2D(Width > 0.f ? Width : X(Right) - X(Left), Height > 0.f ? Height : Y(Bottom) - Y(Top)));
    return Slot;
}

// Places a text block so its cap top lands on the concept's cap top.
UCanvasPanelSlot* Line(UCanvasPanel* Into, UTextBlock* Label, int32 Size, float Left, float CapTop)
{
    UCanvasPanelSlot* Slot = Into->AddChildToCanvas(Label);
    Slot->SetAutoSize(true);
    Slot->SetPosition(FVector2D(X(Left), Y(CapTop) - CapLift * Size));
    return Slot;
}
}

void USFDialoguePanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace DialogueLayout;

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    ShadeScene(Root, WidgetTree);
    const FAnchors BoardAnchors(0.26f, 0.14f, 0.74f, 0.78f);  // family rule 12
    // The window_frame art is 84 % opaque inside; an opaque backing keeps the speaker from showing through the board.
    UBorder* Backing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Backing->SetBrush(FSlateColorBrush(FLinearColor::White)); Backing->SetBrushColor(Hex(TEXT("10171A")));
    UCanvasPanelSlot* BackingSlot = Root->AddChildToCanvas(Backing);
    BackingSlot->SetAnchors(BoardAnchors); BackingSlot->SetOffsets(FMargin(8.f));
    UBorder* Frame = KitPanel(WidgetTree, TEXT("window_frame"), 1.f, Slate(1.f), Brass(), FMargin(0.f), 6.f);
    UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
    FrameSlot->SetAnchors(BoardAnchors); FrameSlot->SetOffsets(FMargin(0.f));
    UCanvasPanel* Board = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    UCanvasPanelSlot* BoardSlot = Root->AddChildToCanvas(Board);
    BoardSlot->SetAnchors(BoardAnchors); BoardSlot->SetOffsets(FMargin(0.f));
    UCanvasPanel* Offer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    UCanvasPanelSlot* OfferSlot = Board->AddChildToCanvas(Offer);
    OfferSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); OfferSlot->SetOffsets(FMargin(0.f));
    OfferColumn = Offer;

    // Left half: portrait 124 x 128, name and speech, divider, four response rows.
    UOverlay* Portrait = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    UOverlaySlot* WellSlot = Portrait->AddChildToOverlay(KitImage(WidgetTree, TEXT("portrait_well"), 124.f, 128.f));
    WellSlot->SetHorizontalAlignment(HAlign_Fill); WellSlot->SetVerticalAlignment(VAlign_Fill);
    USizeBox* FaceBox = nullptr;
    PortraitImage = ArtImage(WidgetTree, FaceBox, 108.f, 112.f);
    UOverlaySlot* FaceSlot = Portrait->AddChildToOverlay(FaceBox);
    FaceSlot->SetHorizontalAlignment(HAlign_Center); FaceSlot->SetVerticalAlignment(VAlign_Center);
    Box(Board, Portrait, 514.f, 264.f, 0.f, 0.f, 124.f, 128.f);
    NameText = Heading(WidgetTree, TEXT(""), 25, Ivory());
    Line(Board, NameText, 25, 662.f, 286.f);
    LineText = Text(WidgetTree, TEXT(""), 20, Ivory());
    LineText->SetWrapTextAt(X(982.f) - X(662.f));
    LineText->SetLineHeightPercentage(0.93f);  // the concept's 31 px line pitch, scaled with the board (34 px)
    Line(Board, LineText, 20, 662.f, 341.f);
    Box(Board, Rule(WidgetTree), 516.f, 425.f, 982.f, 0.f, 0.f, 8.f);

    static const TCHAR* const Keycaps[FSFDialogueTree::MaxChoices] = {TEXT("keycap_1"), TEXT("keycap_2"), TEXT("keycap_3"), TEXT("keycap_4")};
    static const float RowTops[FSFDialogueTree::MaxChoices] = {458.f, 544.f, 630.f, 716.f};
    for (int32 Index = 0; Index < FSFDialogueTree::MaxChoices; ++Index)
    {
        // A response row is the kit quest_row (a plain brass-edged row), not a text button; the selected one is moss-filled.
        UButton* Control = WidgetTree->ConstructWidget<USFHudButton>(USFHudButton::StaticClass());
        UTextBlock* RowLabel = Text(WidgetTree, TEXT(""), 21, Ivory());
        RowLabel->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        UBorder* Pad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Pad->SetBrush(FSlateNoResource()); Pad->SetPadding(FMargin(X(606.f) - X(511.f) - 6.f, 0.f, 24.f, 0.f)); Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Left);
        Pad->SetContent(RowLabel);
        UBorder* Wash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());  // selection fill under the label, over the row art
        Wash->SetBrush(FSlateColorBrush(FLinearColor::White)); Wash->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f)); Wash->SetVisibility(ESlateVisibility::HitTestInvisible);
        UOverlay* Face = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UOverlaySlot* WashSlot = Face->AddChildToOverlay(Wash);
        WashSlot->SetHorizontalAlignment(HAlign_Fill); WashSlot->SetVerticalAlignment(VAlign_Fill); WashSlot->SetPadding(FMargin(4.f));
        UOverlaySlot* PadSlot = Face->AddChildToOverlay(Pad);
        PadSlot->SetHorizontalAlignment(HAlign_Fill); PadSlot->SetVerticalAlignment(VAlign_Fill);
        RowWash.Add(Wash);
        Control->SetContent(Face);
        if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Face->Slot)) { ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill); }
        FScriptDelegate Clicked;
        Clicked.BindUFunction(this, *FString::Printf(TEXT("HandleChoice%d"), Index));
        Control->OnClicked.Add(Clicked);
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UOverlaySlot* ControlSlot = Stack->AddChildToOverlay(Control);
        ControlSlot->SetHorizontalAlignment(HAlign_Fill); ControlSlot->SetVerticalAlignment(VAlign_Fill);
        UWidget* Cap = KitImage(WidgetTree, Keycaps[Index], 54.f);
        Cap->SetVisibility(ESlateVisibility::HitTestInvisible);
        UOverlaySlot* CapSlot = Stack->AddChildToOverlay(Cap);
        CapSlot->SetHorizontalAlignment(HAlign_Left); CapSlot->SetVerticalAlignment(VAlign_Center); CapSlot->SetPadding(FMargin(X(528.f) - X(511.f), 0.f, 0.f, 0.f));
        Box(Board, Stack, 511.f, RowTops[Index], 988.f, 0.f, 0.f, 75.f);
        RowButtons.Add(Control); RowLabels.Add(RowLabel); RowRoots.Add(Stack);
    }

    // Right half: the offered quest in its inset card.
    Box(Offer, KitPanel(WidgetTree, TEXT("detail_card_frame"), 1.f, Slate(0.85f), Brass(), FMargin(0.f), 0.f), 1022.f, 241.f, 1415.f, 755.f);
    USizeBox* WellFrame = KitImage(WidgetTree, TEXT("respawn_image_well"), 1.f, 1.f);
    WellFrame->ClearWidthOverride(); WellFrame->ClearHeightOverride();
    WellFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
    Box(Offer, WellFrame, 1024.f, 243.f, 1414.f, 441.f);
    USizeBox* ArtBox = nullptr;
    OfferArt = ArtImage(WidgetTree, ArtBox, 1.f, 1.f);
    ArtBox->ClearWidthOverride(); ArtBox->ClearHeightOverride();
    Box(Offer, ArtBox, 1028.f, 247.f, 1410.f, 437.f);
    OfferTitle = Heading(WidgetTree, TEXT(""), 23, Ivory());
    Line(Offer, OfferTitle, 23, 1044.f, 457.f);
    OfferSummary = Text(WidgetTree, TEXT(""), 16, Ivory());
    Line(Offer, OfferSummary, 16, 1044.f, 499.f);
    Line(Offer, Text(WidgetTree, TEXT("Objectives"), 17, Brass(), true), 17, 1044.f, 549.f);
    Box(Offer, SectionRule(WidgetTree), 1159.f, 553.f, 1396.f, 0.f, 0.f, 8.f);
    static const float ObjectiveTops[3] = {584.f, 620.f, 656.f};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Row->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("objective_checkbox"), 24.f))->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Label = Text(WidgetTree, TEXT(""), 18, Ivory());
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetPadding(FMargin(X(1088.f) - X(1068.f), 0.f, 0.f, 8.f)); LabelSlot->SetVerticalAlignment(VAlign_Center);  // EB Garamond sits low in its line box
        UTextBlock* Count = Text(WidgetTree, TEXT(""), 18, Ivory());
        UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(Count);
        CountSlot->SetVerticalAlignment(VAlign_Center); CountSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        Box(Offer, Row, 1044.f, ObjectiveTops[Index], 1386.f, 0.f, 0.f, 24.f);
        ObjectiveLabels.Add(Label); ObjectiveCounts.Add(Count); ObjectiveRows.Add(Row);
    }
    Line(Offer, Text(WidgetTree, TEXT("Rewards"), 17, Brass(), true), 17, 1044.f, 668.f);
    Box(Offer, SectionRule(WidgetTree), 1141.f, 674.f, 1396.f, 0.f, 0.f, 8.f);
    Box(Offer, KitImage(WidgetTree, TEXT("icon_xp_roundel"), 45.f), 1044.f, 701.f, 0.f, 0.f, 45.f, 45.f);
    XpText = Text(WidgetTree, TEXT(""), 17, Ivory());
    Line(Offer, XpText, 17, 1110.f, 713.f);
    Box(Offer, KitImage(WidgetTree, TEXT("icon_coin_stack"), 43.f), 1231.f, 701.f, 0.f, 0.f, 43.f, 43.f);
    CoinText = Text(WidgetTree, TEXT(""), 17, Ivory());
    Line(Offer, CoinText, 17, 1293.f, 713.f);
    const FSFHudButton Accept = Button(WidgetTree, TEXT("Accept"), EButtonKind::Primary, 0.f);
    AcceptButton = Accept.Button; AcceptLabel = Accept.Label;
    FScriptDelegate AcceptClicked; AcceptClicked.BindUFunction(this, TEXT("HandleAccept")); AcceptButton->OnClicked.Add(AcceptClicked);
    Box(Offer, Accept.Root, 1029.f, 764.f, 1235.f, 0.f, 0.f, 56.f);
    const FSFHudButton Decline = Button(WidgetTree, TEXT("Decline"), EButtonKind::Secondary, 0.f);
    FScriptDelegate DeclineClicked; DeclineClicked.BindUFunction(this, TEXT("HandleDecline")); Decline.Button->OnClicked.Add(DeclineClicked);
    Box(Offer, Decline.Root, 1247.f, 764.f, 1413.f, 0.f, 0.f, 56.f);
}

void USFDialoguePanel::Open(TSharedPtr<const FSFDialogueTree> InTree)
{
    Session.Begin(InTree);
    Selected = 0; StyledSelected = -1;
    if (!InTree.IsValid()) { return; }
    NameText->SetText(FText::FromString(InTree->Speaker));
    SetArt(PortraitImage, TextureFor(InTree->Portrait), FVector2D(108.f, 112.f));
    const FSFDialogueOffer& Offer = InTree->Offer;
    OfferColumn->SetVisibility(InTree->bHasOffer ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (InTree->bHasOffer)
    {
        SetArt(OfferArt, TextureFor(Offer.ArtPath), FVector2D(382.f, 211.f));
        OfferTitle->SetText(FText::FromString(Offer.Title)); OfferSummary->SetText(FText::FromString(Offer.Summary));
        for (int32 Index = 0; Index < ObjectiveRows.Num(); ++Index)
        {
            const bool bShown = Offer.Objectives.IsValidIndex(Index);
            ObjectiveRows[Index]->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (bShown)
            {
                ObjectiveLabels[Index]->SetText(FText::FromString(Offer.Objectives[Index].Label));
                ObjectiveCounts[Index]->SetText(FText::FromString(FString::Printf(TEXT("0 / %d"), Offer.Objectives[Index].Need)));
            }
        }
        XpText->SetText(FText::FromString(FString::Printf(TEXT("%d XP"), Offer.RewardXp)));
        CoinText->SetText(FText::FromString(FString::Printf(TEXT("%d Coin"), Offer.RewardCoin)));
    }
    Refresh();
}

void USFDialoguePanel::Refresh()
{
    const FSFDialogueNode* Node = Session.Node();
    if (!Node) { return; }
    LineText->SetText(FText::FromString(Node->Line));
    for (int32 Index = 0; Index < RowRoots.Num(); ++Index)
    {
        const bool bShown = Node->Choices.IsValidIndex(Index);
        RowRoots[Index]->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bShown) { RowLabels[Index]->SetText(FText::FromString(Node->Choices[Index].Label)); }
    }
    Selected = FMath::Clamp(Selected, 0, FMath::Max(0, Node->Choices.Num() - 1));
    StyledSelected = -1;
    if (Session.Tree.IsValid() && Session.Tree->bHasOffer)
    {
        AcceptButton->SetIsEnabled(!Session.bAccepted);
        AcceptLabel->SetText(FText::FromString(Session.bAccepted ? TEXT("Accepted") : TEXT("Accept")));
    }
    StyleRows();
}

// Selected row = moss tint and brass edge (the pressed art); the others keep the plain button art.
void USFDialoguePanel::StyleRows()
{
    if (StyledSelected == Selected) { return; }
    StyledSelected = Selected;
    for (int32 Index = 0; Index < RowButtons.Num(); ++Index)
    {
        FButtonStyle Style = RowButtons[Index]->GetStyle();
        const bool bSelected = Index == Selected;
        FSlateBrush Row = KitOrFrame(TEXT("quest_row"), bSelected ? Hex(TEXT("2B3B2C")) : Hex(TEXT("0E171C"), 0.85f), bSelected ? Brass() : Hex(TEXT("3A4348")), 3.f);
        if (RowWash.IsValidIndex(Index)) { RowWash[Index]->SetBrushColor(bSelected ? Hex(TEXT("2F3D2D")) : FLinearColor(0.f, 0.f, 0.f, 0.f)); }  // #2F3D2D, the concept's selected fill (state colour, rule 3)
        Style.Normal = Row; Style.Hovered = Row; Style.Pressed = Row;
        Style.NormalPadding = FMargin(0.f); Style.PressedPadding = FMargin(0.f);
        RowButtons[Index]->SetStyle(Style);
    }
}

void USFDialoguePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    const FSFDialogueNode* Node = Session.Node();
    if (!Node) { return; }
    for (int32 Index = 0; Index < Node->Choices.Num() && Index < RowButtons.Num(); ++Index)
    {
        if (RowButtons[Index]->IsHovered()) { Selected = Index; }
    }
    StyleRows();
}

bool USFDialoguePanel::ChooseIndex(int32 Index)
{
    const FSFDialogueNode* Node = Session.Node();
    if (!Node || !Node->Choices.IsValidIndex(Index)) { return false; }
    Selected = Index;
    Apply(Session.Choose(Index));
    return true;
}

void USFDialoguePanel::HandleAccept() { Apply(Session.Accept()); }
void USFDialoguePanel::HandleDecline() { Apply(Session.Decline()); }

void USFDialoguePanel::Apply(ESFDialogueStep Step)
{
    if (Step == ESFDialogueStep::Stay && !Session.Node()) { return; }
    if (Step != ESFDialogueStep::Closed && Step != ESFDialogueStep::Declined) { Refresh(); }
    OnStep.ExecuteIfBound(Step);
}
