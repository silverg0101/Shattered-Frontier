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
#include "Quest/SFQuestPilot.h"
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
        (*OfferObject)->TryGetStringField(TEXT("quest_id"), Offer.QuestId);
        Offer.AcceptedNode = TEXT("accepted"); (*OfferObject)->TryGetStringField(TEXT("accepted_node"), Offer.AcceptedNode);
        if (!Offer.QuestId.StartsWith(TEXT("quest/"))) { Error = TEXT("offer needs a quest_id"); return false; }
        if (!Tree.Nodes.Contains(Offer.AcceptedNode)) { Error = TEXT("offer needs its accepted node"); return false; }
    }
    if (!Tree.Nodes.Contains(Tree.Start)) { Error = TEXT("start node is missing"); return false; }
    for (const TPair<FString, FSFDialogueNode>& Entry : Tree.Nodes)
    {
        for (const FSFDialogueChoice& Choice : Entry.Value.Choices)
        {
            const bool bTerminal = Choice.Action == TEXT("end") || Choice.Action == TEXT("accept") || Choice.Action.StartsWith(TEXT("learn:recipe/"));
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
    LastAction = Choice.Action;
    if (Choice.Action == TEXT("end")) { return ESFDialogueStep::Closed; }
    if (Choice.Action == TEXT("accept")) { return Accept(); }
    if (Choice.Action.StartsWith(TEXT("learn:recipe/"))) { return ESFDialogueStep::Learn; }
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
    Box->SetHeightOverride(12.f); Box->SetContent(KitPanel(Tree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f, false));
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
// EB Garamond: the cap top sits this many px below the text block's top per 1080p px of font size.
constexpr float CapLift = 0.3225f;

// Places a widget on its concept rectangle.
UCanvasPanelSlot* Box(UCanvasPanel* Into, UWidget* Widget, float Left, float Top, float Width, float Height)
{
    UCanvasPanelSlot* Slot = Into->AddChildToCanvas(Widget);
    Slot->SetPosition(FVector2D(X(Left), Y(Top)));
    Slot->SetSize(FVector2D(Width, Height));
    return Slot;
}

// Type sizes matched to the concept's measured text widths need fractions of a px.
inline void Resize(UTextBlock* Label, float Px) { SFHudStyle::ResizePx(Label, Px); }

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
    NameText = Text(WidgetTree, TEXT(""), 37, Ivory());
    Resize(NameText, 36.7f);
    Line(Board, NameText, 33, 662.f, 286.f);
    LineText = Text(WidgetTree, TEXT(""), 27, Ivory());
    Resize(LineText, 27.5f);
    LineText->SetWrapTextAt(X(982.f) - X(662.f));
    LineText->SetLineHeightPercentage(0.85f);  // the concept's 31 px line pitch
    // Below the name the left column flows: speech, divider, rows. A two-line speech puts every piece on its concept rect;
    // a longer one pushes the rows down instead of running through them (tree lines are capped at three, see Parse).
    UVerticalBox* Flow = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Flow->AddChildToVerticalBox(LineText)->SetPadding(FMargin(662.f - 513.f, 0.f, 0.f, 0.f));  // speech block top 341 - 0.43 * 20 = 332
    Flow->AddChildToVerticalBox(Rule(WidgetTree))->SetPadding(FMargin(3.f, 26.f, 3.f, 25.f));  // rule 423-435 (12 px art box), row 1 at 460

    static const TCHAR* const Keycaps[FSFDialogueTree::MaxChoices] = {TEXT("keycap_1"), TEXT("keycap_2"), TEXT("keycap_3"), TEXT("keycap_4")};
    for (int32 Index = 0; Index < FSFDialogueTree::MaxChoices; ++Index)
    {
        // A response row is the shared list row (menus.md section 1: RowBrush, selected = moss plate + brass edge) under a flat hit button.
        UButton* Control = WidgetTree->ConstructWidget<USFHudButton>(USFHudButton::StaticClass());
        FButtonStyle Flat = Control->GetStyle();
        Flat.Normal = Flat.Hovered = Flat.Pressed = Flat.Disabled = FSlateNoResource();
        Flat.NormalPadding = FMargin(0.f); Flat.PressedPadding = FMargin(0.f);
        Control->SetStyle(Flat);
        UTextBlock* RowLabel = Text(WidgetTree, TEXT(""), 28, Ivory());
        RowLabel->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        RowLabel->SetClipping(EWidgetClipping::ClipToBounds);  // the ellipsis applies only to a clipped block
        UBorder* Pad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Pad->SetBrush(FSlateNoResource()); Pad->SetPadding(FMargin(606.f - 513.f - 6.f, 0.f, 24.f, 0.f)); Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Fill);  // label ink at x 606; a long label ends in an ellipsis inside the row
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
    OfferTitle = Text(WidgetTree, TEXT(""), 34, Ivory());
    Resize(OfferTitle, 33.9f);
    Line(Offer, OfferTitle, 31, 1041.f, 457.f);
    OfferSummary = Text(WidgetTree, TEXT(""), 23, Ivory());
    Resize(OfferSummary, 22.8f);
    Line(Offer, OfferSummary, 21, 1044.f, 499.f);
    Line(Offer, Text(WidgetTree, TEXT("Objectives"), 23, Ivory()), 23, 1044.f, 549.f);
    Box(Offer, SectionRule(WidgetTree), 1159.f, 553.f, 237.f, 8.f);
    static const float ObjectiveTops[3] = {584.f, 620.f, 656.f};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Row->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("objective_checkbox"), 24.f))->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Label = Text(WidgetTree, TEXT(""), 24, Ivory());
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetPadding(FMargin(1088.f - 1068.f, 0.f, 0.f, 8.f)); LabelSlot->SetVerticalAlignment(VAlign_Center);  // EB Garamond sits low in its line box
        UTextBlock* Count = Text(WidgetTree, TEXT(""), 24, Ivory());
        UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(Count);
        CountSlot->SetVerticalAlignment(VAlign_Center); CountSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        Box(Offer, Row, 1044.f, ObjectiveTops[Index], 344.f, 24.f);
        ObjectiveLabels.Add(Label); ObjectiveCounts.Add(Count); ObjectiveRows.Add(Row);
    }
    Line(Offer, Text(WidgetTree, TEXT("Rewards"), 23, Ivory()), 23, 1044.f, 668.f);
    Box(Offer, SectionRule(WidgetTree), 1141.f, 674.f, 255.f, 8.f);
    // Two reward pairs (icon + line) flow left to right: the second starts at the concept's x 1231, or 20 px after a long first line.
    UHorizontalBox* Rewards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    USizeBox* FirstIcon = KitImage(WidgetTree, TEXT("icon_xp_roundel"), 45.f);
    RewardIcons.Add(Cast<UImage>(FirstIcon->GetContent()));
    XpText = Text(WidgetTree, TEXT(""), 23, Ivory());
    UHorizontalBox* FirstPair = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    FirstPair->AddChildToHorizontalBox(FirstIcon)->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* XpSlot = FirstPair->AddChildToHorizontalBox(XpText);
    XpSlot->SetVerticalAlignment(VAlign_Center); XpSlot->SetPadding(FMargin(1110.f - 1089.f, 0.f, 20.f, 6.f));  // ink at x 1110; EB Garamond sits low
    USizeBox* FirstColumn = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    FirstColumn->SetMinDesiredWidth(1231.f - 1044.f); FirstColumn->SetContent(FirstPair);
    Rewards->AddChildToHorizontalBox(FirstColumn)->SetVerticalAlignment(VAlign_Center);
    USizeBox* SecondIcon = KitImage(WidgetTree, TEXT("icon_coin_stack"), 43.f);
    RewardIcons.Add(Cast<UImage>(SecondIcon->GetContent()));
    Rewards->AddChildToHorizontalBox(SecondIcon)->SetVerticalAlignment(VAlign_Center);
    CoinText = Text(WidgetTree, TEXT(""), 23, Ivory());
    CoinText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    CoinText->SetClipping(EWidgetClipping::ClipToBounds);
    UHorizontalBoxSlot* CoinSlot = Rewards->AddChildToHorizontalBox(CoinText);
    CoinSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); CoinSlot->SetVerticalAlignment(VAlign_Center); CoinSlot->SetPadding(FMargin(1293.f - 1274.f, 0.f, 0.f, 6.f));
    Box(Offer, Rewards, 1044.f, 701.f, 1396.f - 1044.f, 45.f);
    const FSFHudButton Accept = Button(WidgetTree, TEXT("Accept"), EButtonKind::Primary, 0.f);
    AcceptButton = Accept.Button; AcceptLabel = Accept.Label;
    FScriptDelegate AcceptClicked; AcceptClicked.BindUFunction(this, TEXT("HandleAccept")); AcceptButton->OnClicked.Add(AcceptClicked);
    Box(Offer, Accept.Root, 1030.f, 765.f, 205.f, 54.f);
    const FSFHudButton Decline = Button(WidgetTree, TEXT("Decline"), EButtonKind::Secondary, 0.f);
    FScriptDelegate DeclineClicked; DeclineClicked.BindUFunction(this, TEXT("HandleDecline")); Decline.Button->OnClicked.Add(DeclineClicked);
    Box(Offer, Decline.Root, 1248.f, 765.f, 164.f, 54.f);
}

void USFDialoguePanel::Open(TSharedPtr<const FSFDialogueTree> InTree, const FSFQuestDefinition* Quest, bool bOffered)
{
    Session.Begin(InTree, Quest && !bOffered);
    Selected = 0; StyledSelected = -1;
    if (!InTree.IsValid()) { return; }
    NameText->SetText(FText::FromString(InTree->Speaker));
    SetArt(PortraitImage, TextureFor(InTree->Portrait), FVector2D(108.f, 112.f));
    // QST-02: the offer renders from the quest record; a quest already held or finished is not offered again.
    const bool bShowOffer = InTree->bHasOffer && Quest && bOffered;
    OfferColumn->SetVisibility(bShowOffer ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (bShowOffer)
    {
        SetArt(OfferArt, TextureFor(Quest->Offer.Art), FVector2D(382.f, 211.f));
        OfferTitle->SetText(FText::FromString(Quest->Title())); OfferSummary->SetText(FText::FromString(Quest->Offer.Summary));
        TArray<const FSFQuestObjective*> Objectives;
        for (const FSFQuestStep& Step : Quest->Steps) { for (const FSFQuestObjective& Objective : Step.Objectives) { Objectives.Add(&Objective); } }
        for (int32 Index = 0; Index < ObjectiveRows.Num(); ++Index)
        {
            const bool bShown = Objectives.IsValidIndex(Index);
            ObjectiveRows[Index]->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (bShown)
            {
                ObjectiveLabels[Index]->SetText(FText::FromString(FSFQuestCatalog::ObjectiveLabel(*Objectives[Index])));
                ObjectiveCounts[Index]->SetText(FText::FromString(FString::Printf(TEXT("0 / %d"), Objectives[Index]->Count)));
            }
        }
        // Rewards are item lines now (xp/coin left the dialogue offer); the two reward slots show the first two.
        const TArray<FString> Rewards = Quest->RewardLines();
        XpText->SetText(FText::FromString(Rewards.IsValidIndex(0) ? Rewards[0] : FString()));
        CoinText->SetText(FText::FromString(Rewards.IsValidIndex(1) ? Rewards[1] : FString()));
        {
            const float Room = (1396.f - 1044.f) - 45.f - 21.f - 20.f - 43.f - 19.f;  // both lines together
            DialogueLayout::Resize(XpText, 23.f); DialogueLayout::Resize(CoinText, 23.f);
            const float Both = SFHudStyle::TextWidth(XpText) + SFHudStyle::TextWidth(CoinText);
            if (Both > Room) { const float Px = FMath::Max(18.f, 23.f * Room / Both); DialogueLayout::Resize(XpText, Px); DialogueLayout::Resize(CoinText, Px); }
        }
        TArray<FString> Items;
        for (const FSFQuestStep& Step : Quest->Steps) { for (const FSFQuestRewardItem& Item : Step.RewardItems) { Items.Add(Item.DefinitionId); } }
        for (int32 Index = 0; Index < RewardIcons.Num(); ++Index)
        {
            if (!RewardIcons[Index]) { continue; }
            FString Stem; if (Items.IsValidIndex(Index)) { Items[Index].Split(TEXT("/"), nullptr, &Stem); }
            const FString Piece = Stem == TEXT("rough_timber") ? FString(TEXT("icon_timber_board")) : TEXT("icon_") + Stem;
            const FSFHudKitElement* Icon = Kit(*Piece) ? Kit(*Piece) : Kit(TEXT("icon_leather_bag"));
            RewardIcons[Index]->SetVisibility(Items.IsValidIndex(Index) ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (Icon) { RewardIcons[Index]->SetBrush(KitBrush(*Icon)); }
        }
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
        if (bShown)
        {
            RowLabels[Index]->SetText(FText::FromString(Node->Choices[Index].Label));
            SFHudStyle::FitWidth(RowLabels[Index], 28.f, 985.f - 606.f - 24.f);
        }
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
