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
    Body->SetCollisionProfileName(TEXT("NoCollision"));
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

FLinearColor Dim() { return Hex(TEXT("A9A593")); }

UBorder* AddDim(UCanvasPanel* Root, UWidgetTree* Tree)
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

// A section heading with an engraved rule filling the rest of the row ("Objectives ----").
UWidget* SectionHeading(UWidgetTree* Tree, const FString& Label)
{
    UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Row->AddChildToHorizontalBox(Text(Tree, Label, 22, Brass(), true))->SetVerticalAlignment(VAlign_Center);
    UHorizontalBoxSlot* Line = Row->AddChildToHorizontalBox(Rule(Tree));
    Line->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Line->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f)); Line->SetVerticalAlignment(VAlign_Center);
    return Row;
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

void USFDialoguePanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);

    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    AddDim(Root, WidgetTree);
    UBorder* Frame = KitPanel(WidgetTree, TEXT("window_frame"), 1.f, Slate(0.94f), Brass(), FMargin(10.f, 6.f), 6.f);
    UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);
    FrameSlot->SetAnchors(FAnchors(0.26f, 0.14f, 0.74f, 0.78f)); FrameSlot->SetOffsets(FMargin(0.f));  // family rule 12
    UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Frame->SetContent(Columns);

    // Left half: portrait + name + line, engraved divider, four numbered response rows.
    UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    UOverlay* Portrait = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
    Portrait->AddChildToOverlay(KitImage(WidgetTree, TEXT("portrait_well"), 120.f));
    USizeBox* FaceBox = nullptr;
    PortraitImage = ArtImage(WidgetTree, FaceBox, 104.f, 104.f);
    UOverlaySlot* FaceSlot = Portrait->AddChildToOverlay(FaceBox);
    FaceSlot->SetHorizontalAlignment(HAlign_Center); FaceSlot->SetVerticalAlignment(VAlign_Center);
    Head->AddChildToHorizontalBox(Portrait)->SetVerticalAlignment(VAlign_Top);
    UVerticalBox* Speech = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    NameText = Heading(WidgetTree, TEXT(""), 32, Ivory());
    Speech->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));
    LineText = Text(WidgetTree, TEXT(""), 24, Ivory());
    LineText->SetAutoWrapText(true);
    Speech->AddChildToVerticalBox(LineText);
    UHorizontalBoxSlot* SpeechSlot = Head->AddChildToHorizontalBox(Speech);
    SpeechSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); SpeechSlot->SetPadding(FMargin(22.f, 0.f, 0.f, 0.f));
    Left->AddChildToVerticalBox(Head)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
    Left->AddChildToVerticalBox(Rule(WidgetTree))->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));

    static const TCHAR* const Keycaps[FSFDialogueTree::MaxChoices] = {TEXT("keycap_1"), TEXT("keycap_2"), TEXT("keycap_3"), TEXT("keycap_4")};
    for (int32 Index = 0; Index < FSFDialogueTree::MaxChoices; ++Index)
    {
        // A response row is the kit quest_row (a plain brass-edged 56 px row), not a text button; the selected one is moss-tinted.
        UButton* Control = WidgetTree->ConstructWidget<USFHudButton>(USFHudButton::StaticClass());
        UTextBlock* RowLabel = Text(WidgetTree, TEXT(""), 24, Ivory());
        RowLabel->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        UBorder* Pad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Pad->SetBrush(FSlateNoResource()); Pad->SetPadding(FMargin(78.f, 0.f, 24.f, 0.f)); Pad->SetVerticalAlignment(VAlign_Center); Pad->SetHorizontalAlignment(HAlign_Left);
        Pad->SetContent(RowLabel);
        Control->SetContent(Pad);
        if (UButtonSlot* PadSlot = Cast<UButtonSlot>(Pad->Slot)) { PadSlot->SetHorizontalAlignment(HAlign_Fill); PadSlot->SetVerticalAlignment(VAlign_Fill); }
        FScriptDelegate Clicked;
        Clicked.BindUFunction(this, *FString::Printf(TEXT("HandleChoice%d"), Index));
        Control->OnClicked.Add(Clicked);
        USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        RowBox->SetHeightOverride(56.f); RowBox->SetContent(Control);
        UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        Stack->AddChildToOverlay(RowBox)->SetHorizontalAlignment(HAlign_Fill);
        UBorder* Wash = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());  // selection wash over the row art
        Wash->SetBrush(FSlateColorBrush(FLinearColor::White)); Wash->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f)); Wash->SetVisibility(ESlateVisibility::HitTestInvisible);
        UOverlaySlot* WashSlot = Stack->AddChildToOverlay(Wash);
        WashSlot->SetHorizontalAlignment(HAlign_Fill); WashSlot->SetVerticalAlignment(VAlign_Fill); WashSlot->SetPadding(FMargin(4.f));
        RowWash.Add(Wash);
        UWidget* Cap = KitImage(WidgetTree, Keycaps[Index], 40.f);
        Cap->SetVisibility(ESlateVisibility::HitTestInvisible);
        UOverlaySlot* CapSlot = Stack->AddChildToOverlay(Cap);
        CapSlot->SetHorizontalAlignment(HAlign_Left); CapSlot->SetVerticalAlignment(VAlign_Center); CapSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
        Left->AddChildToVerticalBox(Stack)->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));
        RowButtons.Add(Control); RowLabels.Add(RowLabel); RowRoots.Add(Stack);
    }
    UHorizontalBoxSlot* LeftSlot = Columns->AddChildToHorizontalBox(Left);
    LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LeftSlot->SetPadding(FMargin(14.f, 6.f, 14.f, 0.f));

    // Engraved vertical rule between the halves.
    USizeBox* Split = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Split->SetWidthOverride(2.f); Split->SetContent(KitPanel(WidgetTree, TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f));
    Columns->AddChildToHorizontalBox(Split)->SetPadding(FMargin(0.f, 6.f, 0.f, 6.f));

    // Right half: the offered quest.
    UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    OfferColumn = Right;
    USizeBox* ArtBox = nullptr;
    OfferArt = ArtImage(WidgetTree, ArtBox, 340.f, 206.f);
    Right->AddChildToVerticalBox(ArtBox)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    OfferTitle = Heading(WidgetTree, TEXT(""), 30, Ivory());
    Right->AddChildToVerticalBox(OfferTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    OfferSummary = Text(WidgetTree, TEXT(""), 20, Ivory());
    OfferSummary->SetAutoWrapText(true);
    Right->AddChildToVerticalBox(OfferSummary)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
    Right->AddChildToVerticalBox(SectionHeading(WidgetTree, TEXT("Objectives")))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Row->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("objective_checkbox"), 24.f))->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Label = Text(WidgetTree, TEXT(""), 20, Ivory());
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); LabelSlot->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f)); LabelSlot->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Count = Num(WidgetTree, TEXT(""), 20, Ivory(), false);
        Row->AddChildToHorizontalBox(Count)->SetVerticalAlignment(VAlign_Center);
        Right->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        ObjectiveLabels.Add(Label); ObjectiveCounts.Add(Count); ObjectiveRows.Add(Row);
    }
    Right->AddChildToVerticalBox(SectionHeading(WidgetTree, TEXT("Rewards")))->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));
    UHorizontalBox* Rewards = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Rewards->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_xp_roundel"), 44.f))->SetVerticalAlignment(VAlign_Center);
    XpText = Num(WidgetTree, TEXT(""), 20, Ivory(), false);
    Rewards->AddChildToHorizontalBox(XpText)->SetPadding(FMargin(12.f, 0.f, 32.f, 0.f));
    Cast<UHorizontalBoxSlot>(XpText->Slot)->SetVerticalAlignment(VAlign_Center);
    Rewards->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("icon_coin_stack"), 44.f))->SetVerticalAlignment(VAlign_Center);
    CoinText = Num(WidgetTree, TEXT(""), 20, Ivory(), false);
    Rewards->AddChildToHorizontalBox(CoinText)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
    Cast<UHorizontalBoxSlot>(CoinText->Slot)->SetVerticalAlignment(VAlign_Center);
    Right->AddChildToVerticalBox(Rewards)->SetPadding(FMargin(0.f, 0.f, 0.f, 14.f));
    UVerticalBox* Spacer = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Right->AddChildToVerticalBox(Spacer)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    const FSFHudButton Accept = Button(WidgetTree, TEXT("Accept"), EButtonKind::Primary, 150.f);
    AcceptButton = Accept.Button; AcceptLabel = Accept.Label;
    FScriptDelegate AcceptClicked; AcceptClicked.BindUFunction(this, TEXT("HandleAccept")); AcceptButton->OnClicked.Add(AcceptClicked);
    UHorizontalBoxSlot* AcceptSlot = Buttons->AddChildToHorizontalBox(Accept.Root);
    AcceptSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); AcceptSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    const FSFHudButton Decline = Button(WidgetTree, TEXT("Decline"), EButtonKind::Secondary, 150.f);
    FScriptDelegate DeclineClicked; DeclineClicked.BindUFunction(this, TEXT("HandleDecline")); Decline.Button->OnClicked.Add(DeclineClicked);
    Buttons->AddChildToHorizontalBox(Decline.Root)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Right->AddChildToVerticalBox(Buttons);
    USizeBox* RightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    RightBox->SetWidthOverride(340.f); RightBox->SetContent(Right);  // the art well sets the column width; the speech side takes the rest
    Columns->AddChildToHorizontalBox(RightBox)->SetPadding(FMargin(16.f, 6.f, 14.f, 0.f));
}

void USFDialoguePanel::Open(TSharedPtr<const FSFDialogueTree> InTree)
{
    Session.Begin(InTree);
    Selected = 0; StyledSelected = -1;
    if (!InTree.IsValid()) { return; }
    NameText->SetText(FText::FromString(InTree->Speaker));
    SetArt(PortraitImage, TextureFor(InTree->Portrait), FVector2D(104.f, 104.f));
    const FSFDialogueOffer& Offer = InTree->Offer;
    OfferColumn->SetVisibility(InTree->bHasOffer ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (InTree->bHasOffer)
    {
        SetArt(OfferArt, TextureFor(Offer.ArtPath), FVector2D(340.f, 206.f));
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
        if (RowWash.IsValidIndex(Index)) { RowWash[Index]->SetBrushColor(bSelected ? FLinearColor(Moss().R, Moss().G, Moss().B, 0.55f) : FLinearColor(0.f, 0.f, 0.f, 0.f)); }  // moss wash (state colour, rule 3)
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
