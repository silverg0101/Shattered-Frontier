#include "Dialogue/SFDialogue.h"

#include "Animation/AnimInstance.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Dom/JsonObject.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/SFHudStyle.h"
#include "Misc/FileHelper.h"
#include "Gathering/SFStationRecipes.h"
#include "Quest/SFQuestPilot.h"
#include "UI/SFItemTooltip.h"
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
    for (const auto& Entry : (*NodeObjects)->Values)
    {
        const FString Id(*Entry.Key);
        const TSharedPtr<FJsonObject> Object = Entry.Value->AsObject();
        if (!Object.IsValid()) { Error = FString::Printf(TEXT("node %s is not an object"), *Entry.Key); return false; }
        FSFDialogueNode Node; Node.Id = Id; Node.Line = Object->GetStringField(TEXT("line"));
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
        Tree.Nodes.Add(Id, Node);
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
// The portrait or offer art: an asset path, else a HUD kit piece name (portrait_millwarden_oswin); null when neither exists.
UTexture2D* TextureFor(const FString& Path)
{
    if (Path.IsEmpty()) { return nullptr; }
    if (UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet)) { return Loaded; }
    const FSFHudKitElement* Element = SFHudStyle::Kit(*Path);
    return Element ? Element->Texture.Get() : nullptr;
}

void SetArt(UImage* Image, UTexture2D* Texture)
{
    if (!Image) { return; }
    if (Texture) { Image->SetBrushFromTexture(Texture, false); }
    Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
}

void USFDialoguePanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    RowClass = LoadClass<USFKitRowWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_KitRow.WBP_KitRow_C"));
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
    // The response rows: key number in the cell, the label as the name; hidden until the node has that many choices.
    if (RowClass)
    {
        for (int32 Index = 0; Index < FSFDialogueTree::MaxChoices; ++Index)
        {
            USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
            if (!Row) { continue; }
            Choices->AddChild(Row);
            Row->SetIconTexture(nullptr);
            Row->SetCells(FText::AsNumber(Index + 1), FText::GetEmpty());
            Row->ShowChevron(true);
            Row->OnClicked.AddDynamic(this, &USFDialoguePanel::HandleChoiceClicked);
            ChoiceRows.Add(Row);
        }
    }
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (USFSlotWidget* RewardSlot = Cast<USFSlotWidget>(GetWidgetFromName(*FString::Printf(TEXT("Reward%d"), Index)))) { RewardSlots.Add(RewardSlot); }
    }
    if (AcceptButton) { AcceptButton->SetLabel(FText::FromString(TEXT("Accept"))); AcceptButton->OnActivated.AddDynamic(this, &USFDialoguePanel::HandleAccept); }
    if (DeclineButton) { DeclineButton->SetLabel(FText::FromString(TEXT("Decline"))); DeclineButton->OnActivated.AddDynamic(this, &USFDialoguePanel::HandleDecline); }
    if (OfferColumn) { OfferColumn->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFDialoguePanel::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

void USFDialoguePanel::Open(TSharedPtr<const FSFDialogueTree> InTree, const FSFQuestDefinition* Quest, bool bOffered)
{
    Session.Begin(InTree, Quest && !bOffered);
    if (!InTree.IsValid()) { return; }
    if (Window) { Window->SetTitle(FText::FromString(InTree->Speaker)); }
    SetArt(Portrait, TextureFor(InTree->Portrait));
    // QST-02: the offer renders from the quest record; a quest already held or finished is not offered again.
    const bool bShowOffer = InTree->bHasOffer && Quest && bOffered;
    if (OfferColumn) { OfferColumn->SetVisibility(bShowOffer ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    RewardIds.Reset(); RewardCounts.Reset();
    if (bShowOffer)
    {
        SetArt(OfferArt, TextureFor(Quest->Offer.Art));
        if (OfferTitle) { OfferTitle->SetText(FText::FromString(Quest->Title())); }
        if (OfferSummary) { OfferSummary->SetText(FText::FromString(Quest->Offer.Summary)); }
        TArray<const FSFQuestObjective*> Objectives;
        for (const FSFQuestStep& Step : Quest->Steps) { for (const FSFQuestObjective& Objective : Step.Objectives) { Objectives.Add(&Objective); } }
        for (int32 Index = 0; Index < 3; ++Index)
        {
            UWidget* Row = GetWidgetFromName(*FString::Printf(TEXT("Objective%dRow"), Index));
            if (!Row) { continue; }
            const bool bShown = Objectives.IsValidIndex(Index);
            Row->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
            if (!bShown) { continue; }
            if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Objective%dLabel"), Index)))) { Label->SetText(FText::FromString(FSFQuestCatalog::ObjectiveLabel(*Objectives[Index]))); }
            if (UTextBlock* Count = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Objective%dCount"), Index)))) { Count->SetText(FText::FromString(FString::Printf(TEXT("0 / %d"), Objectives[Index]->Count))); }
        }
        TArray<FSFQuestRewardItem> Items;
        for (const FSFQuestStep& Step : Quest->Steps) { Items.Append(Step.RewardItems); }
        for (int32 Index = 0; Index < RewardSlots.Num(); ++Index)
        {
            const bool bShown = Items.IsValidIndex(Index);
            RewardSlots[Index]->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            if (!bShown) { continue; }
            RewardSlots[Index]->SetIcon(SFItemTooltip::ItemTexture(Items[Index].DefinitionId));
            RewardSlots[Index]->SetKeyText(FText::GetEmpty());
            RewardSlots[Index]->SetCount(static_cast<int32>(Items[Index].Quantity));
            RewardIds.Add(Items[Index].DefinitionId); RewardCounts.Add(static_cast<int32>(Items[Index].Quantity));
        }
    }
    Refresh();
}

void USFDialoguePanel::Refresh()
{
    const FSFDialogueNode* Node = Session.Node();
    if (!Node) { return; }
    if (LineText) { LineText->SetText(FText::FromString(Node->Line)); }
    for (int32 Index = 0; Index < ChoiceRows.Num(); ++Index)
    {
        const bool bShown = Node->Choices.IsValidIndex(Index);
        ChoiceRows[Index]->SetVisibility(bShown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bShown) { ChoiceRows[Index]->SetEntry(FText::FromString(Node->Choices[Index].Label), FText::GetEmpty()); }
    }
    if (AcceptButton && Session.Tree.IsValid() && Session.Tree->bHasOffer)
    {
        AcceptButton->SetButtonEnabled(!Session.bAccepted);  // HandleAccept is a no-op once accepted; the face shows it disabled
        AcceptButton->SetLabel(FText::FromString(Session.bAccepted ? TEXT("Accepted") : TEXT("Accept")));
    }
}

void USFDialoguePanel::HandleChoiceClicked(USFSelectableWidget* Source)
{
    const int32 Index = ChoiceRows.IndexOfByPredicate([Source](const TObjectPtr<USFKitRowWidget>& Row) { return Row == Source; });
    if (Index != INDEX_NONE) { ChooseIndex(Index); }
}

void USFDialoguePanel::UpdateTooltip()
{
    if (!Tooltip || !FSlateApplication::IsInitialized()) { return; }
    FSFInventoryEntry Entry;
    bool bShown = false;
    for (int32 Index = 0; Index < RewardSlots.Num() && RewardIds.IsValidIndex(Index) && !bShown; ++Index)
    {
        if (RewardSlots[Index]->GetVisibility() != ESlateVisibility::Collapsed && RewardSlots[Index]->IsPointerOver())
        { Entry = SFItemTooltip::EntryFor(RewardIds[Index], SFStationRecipes::ItemDisplayName(RewardIds[Index]), RewardCounts[Index]); bShown = true; }
    }
    FSFItemTooltipParts Parts;
    Parts.Root = Tooltip; Parts.Icon = TipIcon; Parts.Title = TipTitle; Parts.Rarity = TipRarity; Parts.Type = TipType; Parts.Quantity = TipQuantity; Parts.Weight = TipWeight; Parts.Description = TipDescription;
    SFItemTooltip::Present(Parts, bShown ? &Entry : nullptr, FSlateApplication::Get().GetCursorPos());
}

void USFDialoguePanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (GetVisibility() != ESlateVisibility::Collapsed) { UpdateTooltip(); }
}

bool USFDialoguePanel::ChooseIndex(int32 Index)
{
    const FSFDialogueNode* Node = Session.Node();
    if (!Node || !Node->Choices.IsValidIndex(Index)) { return false; }
    Apply(Session.Choose(Index));
    return true;
}

void USFDialoguePanel::HandleAccept(USFButtonWidget*) { Apply(Session.Accept()); }
void USFDialoguePanel::HandleDecline(USFButtonWidget*) { Apply(Session.Decline()); }

void USFDialoguePanel::Apply(ESFDialogueStep Step)
{
    if (Step == ESFDialogueStep::Stay && !Session.Node()) { return; }
    if (Step != ESFDialogueStep::Closed && Step != ESFDialogueStep::Declined) { Refresh(); }
    OnStep.ExecuteIfBound(Step);
}
