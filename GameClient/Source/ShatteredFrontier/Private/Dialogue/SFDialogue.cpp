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
#include "Framework/Application/SlateApplication.h"
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
        if (Node.Line.Len() > MaxLineChars) { Error = FString::Printf(TEXT("node %s line is %d characters; the board shows %d (three lines)"), *Entry.Key, Node.Line.Len(), MaxLineChars); return false; }
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

// Every position is the approved concept's (10-npc-dialogue.png at 1920x1080, menus.md section 2 acceptance), measured once:
// the board is x 486-1437, y 219-841 and children are placed in board-local px.
namespace DialogueLayout
{
const FAnchors BoardAnchors(486.f / 1920.f, 219.f / 1080.f, 1437.f / 1920.f, 841.f / 1080.f);
inline float X(float ConceptX) { return ConceptX - 486.f; }
inline float Y(float ConceptY) { return ConceptY - 219.f; }
// EB Garamond: the cap top sits this many px below the text block's top per px of font size.
constexpr float CapLift = 0.43f;

// Places a widget on its concept rectangle.
UCanvasPanelSlot* Box(UCanvasPanel* Into, UWidget* Widget, float Left, float Top, float Width, float Height)
{
    UCanvasPanelSlot* Slot = Into->AddChildToCanvas(Widget);
    Slot->SetPosition(FVector2D(X(Left), Y(Top)));
    Slot->SetSize(FVector2D(Width, Height));
    return Slot;
}

// Type sizes matched to the concept's measured text widths need fractions of a point; the helpers take whole sizes.
void Resize(UTextBlock* Label, float Size)
{
    FSlateFontInfo Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
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
    // The window_frame art is 84 % opaque inside; an opaque backing keeps the speaker from showing through the board.
    UBorder* Backing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Backing->SetBrush(FSlateColorBrush(FLinearColor::White)); Backing->SetBrushColor(Hex(TEXT("10171A")));
    UCanvasPanelSlot* BackingSlot = Root->AddChildToCanvas(Backing);
    BackingSlot->SetAnchors(BoardAnchors); BackingSlot->SetOffsets(FMargin(8.f));
    UBorder* Frame = KitPanel(WidgetTree, TEXT("window_frame"), 1.f, Slate(1.f), Brass(), FMargin(0.f), 6.f);
    Frame->SetClipping(EWidgetClipping::ClipToBounds);
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
    UOverlaySlot* WellSlot = Portrait->AddChildToOverlay(KitImage(WidgetTree, TEXT("portrait_well"), 122.f, 124.f));
    WellSlot->SetHorizontalAlignment(HAlign_Fill); WellSlot->SetVerticalAlignment(VAlign_Fill);
    USizeBox* FaceBox = nullptr;
    PortraitImage = ArtImage(WidgetTree, FaceBox, 106.f, 108.f);
    UOverlaySlot* FaceSlot = Portrait->AddChildToOverlay(FaceBox);
    FaceSlot->SetHorizontalAlignment(HAlign_Center); FaceSlot->SetVerticalAlignment(VAlign_Center);
    Box(Board, Portrait, 516.f, 267.f, 122.f, 124.f);
    NameText = Text(WidgetTree, TEXT(""), 25, Ivory());
    Resize(NameText, 27.5f);
    Line(Board, NameText, 25, 662.f, 286.f);
    LineText = Text(WidgetTree, TEXT(""), 20, Ivory());
    Resize(LineText, 20.6f);
    LineText->SetWrapTextAt(X(982.f) - X(662.f));
    LineText->SetLineHeightPercentage(0.85f);  // the concept's 31 px line pitch
    // Below the name the left column flows: speech, divider, rows. A two-line speech puts every piece on its concept rect;
    // a longer one pushes the rows down instead of running through them (tree lines are capped at three, see Parse).
    UVerticalBox* Flow = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Flow->AddChildToVerticalBox(LineText)->SetPadding(FMargin(662.f - 513.f, 0.f, 0.f, 0.f));  // speech block top 341 - 0.43 * 20 = 332
    Flow->AddChildToVerticalBox(Rule(WidgetTree))->SetPadding(FMargin(3.f, 32.f, 3.f, 27.f));  // rule 425-433, row 1 at 460

    static const TCHAR* const Keycaps[FSFDialogueTree::MaxChoices] = {TEXT("keycap_1"), TEXT("keycap_2"), TEXT("keycap_3"), TEXT("keycap_4")};
    for (int32 Index = 0; Index < FSFDialogueTree::MaxChoices; ++Index)
    {
        // A response row is the shared list row (menus.md section 1: RowBrush, selected = moss plate + brass edge) under a flat hit button.
        UButton* Control = WidgetTree->ConstructWidget<USFHudButton>(USFHudButton::StaticClass());
        FButtonStyle Flat = Control->GetStyle();
        Flat.Normal = Flat.Hovered = Flat.Pressed = Flat.Disabled = FSlateNoResource();
        Flat.NormalPadding = FMargin(0.f); Flat.PressedPadding = FMargin(0.f);
        Control->SetStyle(Flat);
        UTextBlock* RowLabel = Text(WidgetTree, TEXT(""), 21, Ivory());
        RowLabel->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        UBorder* Pad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Pad->SetBrush(FSlateNoResource()); Pad->SetPadding(FMargin(606.f - 513.f - 6.f, 0.f, 24.f, 0.f)); Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Left);  // label ink at x 606
        Pad->SetContent(RowLabel);
        UBorder* RowPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        SetRowSelected(RowPlate, false);
        RowPlate->SetPadding(FMargin(0.f)); RowPlate->SetVerticalAlignment(VAlign_Fill); RowPlate->SetHorizontalAlignment(HAlign_Fill);
        RowPlate->SetContent(Pad);
        RowPlates.Add(RowPlate);
        Control->SetContent(RowPlate);
        if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(RowPlate->Slot)) { ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill); }
        FScriptDelegate Clicked;
        Clicked.BindUFunction(this, *FString::Printf(TEXT("HandleChoice%d"), Index));
        Control->OnClicked.Add(Clicked);
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        UOverlaySlot* ControlSlot = Stack->AddChildToOverlay(Control);
        ControlSlot->SetHorizontalAlignment(HAlign_Fill); ControlSlot->SetVerticalAlignment(VAlign_Fill);
        UWidget* Cap = KitImage(WidgetTree, Keycaps[Index], 53.f);
        Cap->SetVisibility(ESlateVisibility::HitTestInvisible);
        UOverlaySlot* CapSlot = Stack->AddChildToOverlay(Cap);
        CapSlot->SetHorizontalAlignment(HAlign_Left); CapSlot->SetVerticalAlignment(VAlign_Center); CapSlot->SetPadding(FMargin(529.f - 513.f, 0.f, 0.f, 0.f));
        USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowBox->SetHeightOverride(72.f); RowBox->SetContent(Stack);
        Flow->AddChildToVerticalBox(RowBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 13.f));  // concept row pitch 85-86
        RowButtons.Add(Control); RowLabels.Add(RowLabel); RowRoots.Add(RowBox);
    }
    Box(Board, Flow, 513.f, 332.f, 472.f, 500.f);

    // Right half: the offered quest in its inset card.
    Box(Offer, KitPanel(WidgetTree, TEXT("detail_card_frame"), 0.5f, Hex(TEXT("0F1517")), Brass(), FMargin(0.f), 0.f), 1016.f, 244.f, 400.f, 522.f);
    USizeBox* WellFrame = KitImage(WidgetTree, TEXT("respawn_image_well"), 1.f, 1.f);
    WellFrame->ClearWidthOverride(); WellFrame->ClearHeightOverride();
    WellFrame->SetVisibility(ESlateVisibility::HitTestInvisible);
    Box(Offer, WellFrame, 1024.f, 244.f, 391.f, 198.f);
    USizeBox* ArtBox = nullptr;
    OfferArt = ArtImage(WidgetTree, ArtBox, 1.f, 1.f);
    ArtBox->ClearWidthOverride(); ArtBox->ClearHeightOverride();
    Box(Offer, ArtBox, 1028.f, 248.f, 383.f, 190.f);
    OfferTitle = Text(WidgetTree, TEXT(""), 23, Ivory());
    Resize(OfferTitle, 25.4f);
    Line(Offer, OfferTitle, 23, 1041.f, 457.f);
    OfferSummary = Text(WidgetTree, TEXT(""), 16, Ivory());
    Resize(OfferSummary, 17.1f);
    Line(Offer, OfferSummary, 16, 1044.f, 499.f);
    Line(Offer, Text(WidgetTree, TEXT("Objectives"), 17, Ivory()), 17, 1044.f, 549.f);
    Box(Offer, SectionRule(WidgetTree), 1159.f, 553.f, 237.f, 8.f);
    static const float ObjectiveTops[3] = {584.f, 620.f, 656.f};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Row->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("objective_checkbox"), 24.f))->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Label = Text(WidgetTree, TEXT(""), 18, Ivory());
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetPadding(FMargin(1088.f - 1068.f, 0.f, 0.f, 8.f)); LabelSlot->SetVerticalAlignment(VAlign_Center);  // EB Garamond sits low in its line box
        UTextBlock* Count = Text(WidgetTree, TEXT(""), 18, Ivory());
        UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(Count);
        CountSlot->SetVerticalAlignment(VAlign_Center); CountSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        Box(Offer, Row, 1044.f, ObjectiveTops[Index], 344.f, 24.f);
        ObjectiveLabels.Add(Label); ObjectiveCounts.Add(Count); ObjectiveRows.Add(Row);
    }
    Line(Offer, Text(WidgetTree, TEXT("Rewards"), 17, Ivory()), 17, 1044.f, 668.f);
    Box(Offer, SectionRule(WidgetTree), 1141.f, 674.f, 255.f, 8.f);
    Box(Offer, KitImage(WidgetTree, TEXT("icon_xp_roundel"), 45.f), 1044.f, 701.f, 45.f, 45.f);
    XpText = Text(WidgetTree, TEXT(""), 17, Ivory());
    Line(Offer, XpText, 17, 1107.f, 713.f);
    Box(Offer, KitImage(WidgetTree, TEXT("icon_coin_stack"), 43.f), 1231.f, 701.f, 43.f, 43.f);
    CoinText = Text(WidgetTree, TEXT(""), 17, Ivory());
    Line(Offer, CoinText, 17, 1293.f, 713.f);
    const FSFHudButton Accept = Button(WidgetTree, TEXT("Accept"), EButtonKind::Primary, 0.f);
    AcceptButton = Accept.Button; AcceptLabel = Accept.Label;
    FScriptDelegate AcceptClicked; AcceptClicked.BindUFunction(this, TEXT("HandleAccept")); AcceptButton->OnClicked.Add(AcceptClicked);
    Box(Offer, Accept.Root, 1030.f, 765.f, 205.f, 54.f);
    const FSFHudButton Decline = Button(WidgetTree, TEXT("Decline"), EButtonKind::Secondary, 0.f);
    FScriptDelegate DeclineClicked; DeclineClicked.BindUFunction(this, TEXT("HandleDecline")); Decline.Button->OnClicked.Add(DeclineClicked);
    Box(Offer, Decline.Root, 1248.f, 765.f, 164.f, 54.f);
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

// Selected row = the shared selected row (moss plate, brass edge); the others the hairline row.
void USFDialoguePanel::StyleRows()
{
    if (StyledSelected == Selected) { return; }
    StyledSelected = Selected;
    for (int32 Index = 0; Index < RowPlates.Num(); ++Index) { SetRowSelected(RowPlates[Index], Index == Selected); }
}

void USFDialoguePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    const FSFDialogueNode* Node = Session.Node();
    if (!Node) { return; }
    const FVector2D Mouse = FSlateApplication::Get().GetCursorPos();
    if (Mouse != LastMouse)
    {
        LastMouse = Mouse;
        for (int32 Index = 0; Index < Node->Choices.Num() && Index < RowButtons.Num(); ++Index)
        {
            if (RowButtons[Index]->IsHovered()) { Selected = Index; }
        }
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
