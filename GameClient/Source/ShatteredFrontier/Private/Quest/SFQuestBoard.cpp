#include "Quest/SFQuestBoard.h"

#include "Quest/SFQuestPilot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/SizeBox.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Framework/SFHudStyle.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gathering/SFStationRecipes.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

// ---- Board actor ----

ASFQuestBoard::ASFQuestBoard()
{
    bReplicates = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetMobility(EComponentMobility::Movable);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Board(TEXT("/Game/Frontier/Environment/Greenlands/Silverbrook/Meshes/SM_SilverbrookPropNoticeBoard.SM_SilverbrookPropNoticeBoard"));
    if (Board.Succeeded()) { Mesh->SetStaticMesh(Board.Object); }
    Mesh->bDisallowNanite = true;  // the prop's material instances have no Nanite usage flag; a runtime spawn cannot add one
}

FString ASFQuestBoard::DisplayName() const
{
    FString City; BoardId.Split(TEXT("/"), nullptr, &City);
    if (!City.IsEmpty()) { City[0] = FChar::ToUpper(City[0]); }
    return City + TEXT(" Notice Board");
}

ASFQuestBoard* ASFQuestBoard::FindInReach(const APlayerController* PC)
{
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn) { return nullptr; }
    const FVector Forward = FRotationMatrix(FRotator(0.f, PC->GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X);
    ASFQuestBoard* Best = nullptr; float BestDistance = GetDefault<USFQuestPilotSettings>()->BoardReachCm();
    for (TActorIterator<ASFQuestBoard> It(PC->GetWorld()); It; ++It)
    {
        const FVector Offset = (It->GetActorLocation() - Pawn->GetActorLocation()) * FVector(1, 1, 0);
        const float Distance = Offset.Size();
        if (Distance <= BestDistance && (Distance < KINDA_SMALL_NUMBER || FVector::DotProduct(Offset / Distance, Forward) > 0.2f)) { Best = *It; BestDistance = Distance; }
    }
    return Best;
}

// ---- Panel ----

namespace
{
using namespace SFHudStyle;

// 16-quest-journal.png measured at 1920x1080 (concept px x 1.1483): the book spans x 387-1534, y 106-893; children are placed in
// book-local px. Left page 0-566, right page 581-1147.
const FAnchors BookAnchors(387.f / 1920.f, 106.f / 1080.f, 1534.f / 1920.f, 893.f / 1080.f);
constexpr float RightPage = 581.f, PageWidth = 566.f, PageHeight = 787.f;
constexpr float CapLift = 0.43f;  // EB Garamond: cap top sits this far below the text block top per px of font size

UCanvasPanelSlot* Box(UCanvasPanel* Into, UWidget* Widget, float Left, float Top, float Width, float Height)
{
    UCanvasPanelSlot* Slot = Into->AddChildToCanvas(Widget);
    Slot->SetPosition(FVector2D(Left, Top)); Slot->SetSize(FVector2D(Width, Height));
    return Slot;
}

UCanvasPanelSlot* Line(UCanvasPanel* Into, UTextBlock* Label, int32 Size, float Left, float CapTop)
{
    UCanvasPanelSlot* Slot = Into->AddChildToCanvas(Label);
    Slot->SetAutoSize(true); Slot->SetPosition(FVector2D(Left, CapTop - CapLift * Size));
    return Slot;
}

UWidget* Rule(UWidgetTree* Tree, const TCHAR* Piece)
{
    USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Size->SetContent(KitPanel(Tree, Kit(Piece) ? Piece : TEXT("divider"), 1.f, Brass(), Brass(), FMargin(0.f), 0.f, false));
    return Size;
}

FLinearColor Greyed() { return Hex(TEXT("E8E1CE"), 0.45f); }

FString RewardIconFor(const FString& DefinitionId)
{
    FString Stem; DefinitionId.Split(TEXT("/"), nullptr, &Stem);
    const FString Name = TEXT("icon_") + Stem;
    if (Kit(*Name)) { return Name; }
    return Stem == TEXT("rough_timber") ? TEXT("icon_timber_board") : TEXT("icon_leather_bag");
}

USFQuestBoardPanel* PanelFor(APlayerController* Local, bool bCreate)
{
    USFQuestPilotSubsystem* Quests = Local && Local->GetWorld() ? Local->GetWorld()->GetSubsystem<USFQuestPilotSubsystem>() : nullptr;
    if (!Quests) { return nullptr; }
    if (!Quests->Panel && bCreate)
    {
        Quests->Panel = CreateWidget<USFQuestBoardPanel>(Local, USFQuestBoardPanel::StaticClass());
        if (Quests->Panel) { Quests->Panel->AddToViewport(28); Quests->Panel->SetVisibility(ESlateVisibility::Collapsed); }
    }
    return Quests->Panel;
}
}

void USFQuestBoardPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    UCanvasPanel* Book = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    UCanvasPanelSlot* BookSlot = Root->AddChildToCanvas(Book);
    BookSlot->SetAnchors(BookAnchors); BookSlot->SetOffsets(FMargin(0.f));
    for (const float PageLeft : {0.f, RightPage})
    {
        // The page art is translucent inside; an opaque backing keeps the scene from showing through the text (as the dialogue board).
        UBorder* Backing = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Backing->SetBrush(FSlateColorBrush(FLinearColor::White)); Backing->SetBrushColor(Hex(TEXT("10171A")));
        Box(Book, Backing, PageLeft + 8.f, 8.f, PageWidth - 16.f, PageHeight - 16.f);
        Box(Book, KitPanel(WidgetTree, TEXT("journal_book_frame"), 1.f, Slate(1.f), Brass(), FMargin(0.f), 6.f), PageLeft, 0.f, PageWidth, PageHeight);
    }

    // Left page: title, stag, divider, the list.
    PageTitle = Heading(WidgetTree, TEXT("Quest Journal"), 46, Ivory());
    JournalTitleFont = PageTitle->GetFont();
    PageTitle->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    UCanvasPanelSlot* TitleSlot = Box(Book, PageTitle, 61.f, 70.f - CapLift * 46.f, 370.f, 60.f);
    TitleSlot->SetAutoSize(false);
    Emblem = KitImage(WidgetTree, TEXT("icon_stag_emblem"), 84.f, 84.f);
    Box(Book, Emblem, 436.f, 52.f, 84.f, 84.f);
    Box(Book, Rule(WidgetTree, TEXT("divider")), 59.f, 145.f, 438.f, 12.f);
    List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    List->SetClipping(EWidgetClipping::ClipToBounds);
    Box(Book, List, 59.f, 168.f, 465.f, 580.f);

    // Right page: the selected quest.
    UCanvasPanel* Page = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    Box(Book, Page, 0.f, 0.f, 1147.f, PageHeight);
    Detail = Page;
    QuestTitle = Heading(WidgetTree, TEXT(""), 46, Ivory());
    QuestTitle->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Line(Page, QuestTitle, 46, 654.f, 82.f);
    Box(Page, Rule(WidgetTree, TEXT("divider")), 644.f, 152.f, 446.f, 12.f);
    Summary = Text(WidgetTree, TEXT(""), 22, Ivory());
    Summary->SetAutoWrapText(true);
    UCanvasPanelSlot* SummarySlot = Box(Page, Summary, 645.f, 190.f - CapLift * 22.f, 440.f, 64.f);
    SummarySlot->SetAutoSize(false);
    Reason = Text(WidgetTree, TEXT(""), 18, Amber());
    Line(Page, Reason, 18, 645.f, 262.f);
    Line(Page, Text(WidgetTree, TEXT("Objectives"), 26, Brass()), 26, 645.f, 296.f);
    Box(Page, Rule(WidgetTree, TEXT("section_rule")), 644.f, 326.f, 446.f, 8.f);
    for (int32 Index = 0; Index < MaxObjectiveRows; ++Index)
    {
        UCanvasPanel* Row = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
        UOverlay* Check = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
        Check->AddChildToOverlay(KitImage(WidgetTree, TEXT("objective_checkbox"), 33.f, 33.f));
        USizeBox* Tick = KitImage(WidgetTree, TEXT("status_tick"), 33.f, 33.f);
        Check->AddChildToOverlay(Tick);
        Box(Row, Check, 0.f, 0.f, 33.f, 33.f);
        UTextBlock* Label = Text(WidgetTree, TEXT(""), 22, Ivory());
        Line(Row, Label, 22, 57.f, 9.f);
        UTextBlock* Count = Num(WidgetTree, TEXT(""), 22, Ivory());
        Count->SetJustification(ETextJustify::Right);
        Box(Row, Count, 300.f, 9.f - CapLift * 22.f, 137.f, 32.f);
        Box(Page, Row, 646.f, 357.f + 50.f * Index, 440.f, 40.f);
        ObjectiveRows.Add(Row); ObjectiveLabels.Add(Label); ObjectiveCounts.Add(Count); ObjectiveTicks.Add(Tick);
    }
    Line(Page, Text(WidgetTree, TEXT("Rewards"), 26, Brass()), 26, 645.f, 514.f);
    Box(Page, Rule(WidgetTree, TEXT("section_rule")), 644.f, 544.f, 446.f, 8.f);
    // Reward lines flow left to right: icon, quantity and name, then the next (16-quest-journal.png reward row at y 569).
    UHorizontalBox* RewardRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Box(Page, RewardRow, 646.f, 569.f, 446.f, 55.f);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconBox->SetWidthOverride(52.f); IconBox->SetHeightOverride(52.f);
        UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        IconBox->SetContent(Icon);
        RewardRow->AddChildToHorizontalBox(IconBox)->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Label = Text(WidgetTree, TEXT(""), 22, Ivory());
        UHorizontalBoxSlot* LabelSlot = RewardRow->AddChildToHorizontalBox(Label);
        LabelSlot->SetVerticalAlignment(VAlign_Center); LabelSlot->SetPadding(FMargin(12.f, 0.f, 26.f, 0.f));
        RewardIcons.Add(Icon); RewardTexts.Add(Label);
    }
    NoReward = Text(WidgetTree, TEXT("No item reward"), 22, Greyed());
    Line(Page, NoReward, 22, 646.f, 589.f);

    // Footer: Track / hold-to-Abandon (journal) or Accept / Close (board).
    const FSFHudButton Primary = Button(WidgetTree, TEXT("Accept"), EButtonKind::Primary, 0.f);
    PrimaryButton = Primary.Button; PrimaryLabel = Primary.Label; PrimaryRoot = Primary.Root;
    FScriptDelegate OnPrimary; OnPrimary.BindUFunction(this, TEXT("HandlePrimary")); PrimaryButton->OnClicked.Add(OnPrimary);
    Box(Page, Primary.Root, 636.f, 683.f, 200.f, 56.f);
    const FSFHudButton Secondary = Button(WidgetTree, TEXT("Close"), EButtonKind::Secondary, 0.f);
    SecondaryButton = Secondary.Button; SecondaryLabel = Secondary.Label;
    FScriptDelegate OnSecondary; OnSecondary.BindUFunction(this, TEXT("HandleSecondary")); SecondaryButton->OnClicked.Add(OnSecondary);
    Box(Page, Secondary.Root, 636.f, 683.f, 169.f, 56.f)->SetZOrder(1);
    const FSFHudButton Abandon = Button(WidgetTree, TEXT("Abandon"), EButtonKind::Destructive, 0.f);
    AbandonButton = Abandon.Button; AbandonRoot = Abandon.Root;
    FScriptDelegate OnHold; OnHold.BindUFunction(this, TEXT("HandleHoldStart")); AbandonButton->OnPressed.Add(OnHold);
    FScriptDelegate OnRelease; OnRelease.BindUFunction(this, TEXT("HandleHoldEnd")); AbandonButton->OnReleased.Add(OnRelease);
    Box(Page, Abandon.Root, 822.f, 683.f, 179.f, 56.f);
    UCanvasPanel* Hint = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    USizeBox* Ring = KitImage(WidgetTree, TEXT("hold_confirm_ring"), 32.f, 32.f);
    HoldRing = Cast<UImage>(Ring->GetContent());
    Box(Hint, Ring, 0.f, 0.f, 32.f, 32.f);
    UTextBlock* HintText = Text(WidgetTree, TEXT("Hold to\nabandon"), 15, Greyed());
    Line(Hint, HintText, 15, 40.f, 2.f);
    Box(Page, Hint, 1010.f, 695.f, 110.f, 40.f);
    HoldHint = Hint;
}

TArray<USFQuestBoardPanel::FRow> USFQuestBoardPanel::ListRows() const
{
    TArray<FRow> Rows;
    const USFQuestPilotComponent* Quests = Log.Get();
    if (!Quests) { return Rows; }
    const FSFQuestCatalog& Catalog = FSFQuestCatalog::Get();
    if (IsBoard())
    {
        // Every quest posted here (owner decision 2026-10-22): takeable ones first, then the rest greyed with the reason.
        const TSet<FString> Active = Quests->ActiveIds(), Done = Quests->CompletedIds();
        TArray<FRow> Others;
        for (const TSharedPtr<const FSFQuestDefinition>& Quest : Catalog.ForBoard(BoardId, GetDefault<USFQuestPilotSettings>()->MaxQuestsPerBoard))
        {
            FRow Row; Row.QuestId = Quest->Id;
            const ESFQuestOfferState State = Catalog.OfferState(*Quest, Active, Done, &Row.Reason);
            Row.bLocked = State != ESFQuestOfferState::Available;
            Row.Section = State == ESFQuestOfferState::Available ? TEXT("Posted") : State == ESFQuestOfferState::Locked ? TEXT("Locked") : TEXT("Taken");
            (Row.bLocked ? Others : Rows).Add(Row);
        }
        Others.StableSort([](const FRow& A, const FRow& B) { return A.Section < B.Section; });  // Locked before Taken
        Rows.Append(Others);
    }
    else
    {
        // The journal groups active quests by their zone ("Silverbrook").
        for (const FSFQuestView& View : Quests->GetViews())
        {
            if (View.bComplete) { continue; }
            const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(View.QuestId);
            FString Zone = Quest ? Quest->ZoneId : FString(); int32 Cut;
            if (Zone.FindLastChar('_', Cut)) { Zone = Zone.Mid(Cut + 1); }
            if (!Zone.IsEmpty()) { Zone[0] = FChar::ToUpper(Zone[0]); }
            FRow Row; Row.QuestId = View.QuestId; Row.Section = Zone;
            Rows.Add(Row);
        }
    }
    if (Rows.Num() > MaxRows) { Rows.SetNum(MaxRows); }
    return Rows;
}

FString USFQuestBoardPanel::StateKey() const
{
    FString Key = BoardId + TEXT("|") + SelectedQuest;
    if (const USFQuestPilotComponent* Quests = Log.Get())
    {
        for (const FSFQuestView& View : Quests->GetViews())
        {
            Key += FString::Printf(TEXT("#%s%d%d%d"), *View.QuestId, View.StepNumber, View.bComplete, Quests->IsTracked(View.QuestId));
            for (const FSFQuestLine& Line : View.Lines) { Key += FString::Printf(TEXT(",%d"), Line.Count); }
        }
        for (const FString& Id : Quests->CompletedIds()) { Key += TEXT("!") + Id; }
        Key += TEXT("?") + Quests->GetLastResult();
    }
    return Key;
}

void USFQuestBoardPanel::Rebuild()
{
    LastKey = StateKey();
    const TArray<FRow> Rows = ListRows();
    if (!Rows.ContainsByPredicate([&](const FRow& R) { return R.QuestId == SelectedQuest; })) { SelectedQuest = Rows.IsEmpty() ? FString() : Rows[0].QuestId; }
    const FSFQuestCatalog& Catalog = FSFQuestCatalog::Get();
    // Left page.
    List->ClearChildren(); RowButtons.Reset(); RowQuests.Reset();
    static const FName Handlers[MaxRows] = {TEXT("HandleRow0"), TEXT("HandleRow1"), TEXT("HandleRow2"), TEXT("HandleRow3"), TEXT("HandleRow4"), TEXT("HandleRow5")};
    FString Section;
    for (const FRow& Row : Rows)
    {
        const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(Row.QuestId);
        if (!Quest) { continue; }
        if (Row.Section != Section)
        {
            Section = Row.Section;
            UTextBlock* Header = Text(WidgetTree, Section, 26, Brass());
            List->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, List->GetChildrenCount() == 0 ? 6.f : 16.f, 0.f, 6.f));
        }
        const bool bSelected = Row.QuestId == SelectedQuest;
        UButton* Hit = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        FButtonStyle Clear = Hit->GetStyle();
        Clear.Normal = FSlateNoResource(); Clear.Hovered = FSlateNoResource(); Clear.Pressed = FSlateNoResource();
        Clear.NormalPadding = FMargin(0.f); Clear.PressedPadding = FMargin(0.f);
        Hit->SetStyle(Clear);
        UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        SetRowSelected(Plate, bSelected);
        Plate->SetPadding(FMargin(16.f, 0.f, 14.f, 0.f)); Plate->SetVerticalAlignment(VAlign_Center);
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        Line->AddChildToHorizontalBox(KitImage(WidgetTree, Row.bLocked && Row.Section == TEXT("Locked") ? TEXT("icon_closed_padlock") : TEXT("quest_diamond"), 28.f, 28.f))->SetVerticalAlignment(VAlign_Center);
        UTextBlock* Name = Text(WidgetTree, Quest->Title(), 22, Row.bLocked ? Greyed() : Ivory());
        Name->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name);
        NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center); NameSlot->SetPadding(FMargin(20.f, 0.f, 8.f, 0.f));
        if (bSelected) { Line->AddChildToHorizontalBox(KitImage(WidgetTree, TEXT("row_chevron"), 28.f, 28.f))->SetVerticalAlignment(VAlign_Center); }
        Plate->SetContent(Line);
        Hit->SetContent(Plate);
        if (UButtonSlot* Fill = Cast<UButtonSlot>(Plate->Slot)) { Fill->SetHorizontalAlignment(HAlign_Fill); Fill->SetVerticalAlignment(VAlign_Fill); Fill->SetPadding(FMargin(0.f)); }
        USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Size->SetHeightOverride(54.f); Size->SetContent(Hit);
        List->AddChildToVerticalBox(Size)->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
        FScriptDelegate Click; Click.BindUFunction(this, Handlers[RowButtons.Num()]); Hit->OnClicked.Add(Click);
        RowButtons.Add(Hit); RowQuests.Add(Row.QuestId);
    }
    if (Rows.IsEmpty())
    { List->AddChildToVerticalBox(Text(WidgetTree, IsBoard() ? TEXT("Nothing is posted here.") : TEXT("No quests yet. Visit a notice board."), 22, Greyed()))->SetPadding(FMargin(0.f, 12.f)); }

    // Right page.
    const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(SelectedQuest);
    Detail->SetVisibility(Quest ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!Quest) { return; }
    const FRow* Shown = Rows.FindByPredicate([&](const FRow& R) { return R.QuestId == SelectedQuest; });
    QuestTitle->SetText(FText::FromString(Quest->Title()));
    Summary->SetText(FText::FromString(Quest->bHasOffer ? Quest->Offer.Summary : FString()));
    const USFQuestPilotComponent* Quests = Log.Get();
    const FSFQuestView* View = Quests ? Quests->FindView(SelectedQuest) : nullptr;
    FString Note = Shown && Shown->bLocked ? Shown->Reason : FString();
    const FString& Result = Quests ? Quests->GetLastResult() : FString();
    if (Note.IsEmpty() && !Result.IsEmpty() && Result != TEXT("accepted") && Result != TEXT("abandoned")) { Note = Result == TEXT("out_of_reach") ? TEXT("Stand closer to the board") : Result.Replace(TEXT("_"), TEXT(" ")); }
    Reason->SetText(FText::FromString(Note));
    // Objectives: the quest's objectives in step order with this character's counts (earlier steps done, later ones 0).
    TArray<TTuple<FString, int32, int32>> Lines;
    for (int32 StepIndex = 0; StepIndex < Quest->Steps.Num(); ++StepIndex)
    {
        for (int32 Index = 0; Index < Quest->Steps[StepIndex].Objectives.Num(); ++Index)
        {
            const FSFQuestObjective& Objective = Quest->Steps[StepIndex].Objectives[Index];
            const int32 Active = View ? View->StepNumber - 1 : 0;
            const int32 Count = View && (View->bComplete || StepIndex < Active) ? Objective.Count
                : View && StepIndex == Active && View->Lines.IsValidIndex(Index) ? View->Lines[Index].Count : 0;
            Lines.Add(MakeTuple(FSFQuestCatalog::ObjectiveLabel(Objective), Count, Objective.Count));
        }
    }
    // Three rows: drop finished ones first so the current step stays on the page.
    while (Lines.Num() > MaxObjectiveRows && Lines[0].Get<1>() >= Lines[0].Get<2>()) { Lines.RemoveAt(0); }
    for (int32 Index = 0; Index < MaxObjectiveRows; ++Index)
    {
        const bool bShow = Lines.IsValidIndex(Index);
        ObjectiveRows[Index]->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bShow) { continue; }
        const bool bDone = Lines[Index].Get<1>() >= Lines[Index].Get<2>();
        ObjectiveLabels[Index]->SetText(FText::FromString(Lines[Index].Get<0>()));
        ObjectiveCounts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Lines[Index].Get<1>(), Lines[Index].Get<2>())));
        ObjectiveTicks[Index]->SetVisibility(bDone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    TArray<FSFQuestRewardItem> Items;
    for (const FSFQuestStep& Step : Quest->Steps) { Items.Append(Step.RewardItems); }
    for (int32 Index = 0; Index < RewardTexts.Num(); ++Index)
    {
        const bool bShow = Items.IsValidIndex(Index);
        RewardTexts[Index]->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        RewardIcons[Index]->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bShow) { continue; }
        RewardTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%lld %s"), Items[Index].Quantity, *SFStationRecipes::ItemDisplayName(Items[Index].DefinitionId))));
        if (const FSFHudKitElement* Icon = Kit(*RewardIconFor(Items[Index].DefinitionId))) { RewardIcons[Index]->SetBrush(KitBrush(*Icon)); }
    }
    NoReward->SetVisibility(Items.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    // Footer.
    const bool bTaken = View && !View->bComplete;
    PrimaryRoot->SetVisibility(ESlateVisibility::Visible);
    if (IsBoard())
    {
        const bool bCanAccept = Shown && !Shown->bLocked;
        PrimaryButton->SetIsEnabled(bCanAccept);
        PrimaryLabel->SetText(FText::FromString(bCanAccept ? TEXT("Accept") : bTaken ? TEXT("Accepted") : TEXT("Locked")));
        SecondaryLabel->SetText(FText::FromString(TEXT("Close")));
        if (UCanvasPanelSlot* Placed = Cast<UCanvasPanelSlot>(SecondaryButton->GetParent()->Slot)) { Placed->SetPosition(FVector2D(850.f, 683.f)); Placed->SetSize(FVector2D(160.f, 56.f)); }
        PrimaryRoot->SetVisibility(ESlateVisibility::Visible);
        AbandonRoot->SetVisibility(ESlateVisibility::Collapsed); HoldHint->SetVisibility(ESlateVisibility::Collapsed);
    }
    else
    {
        // Journal: the Track toggle sits where the concept has it; Abandon is held to confirm.
        PrimaryRoot->SetVisibility(ESlateVisibility::Collapsed);
        SecondaryLabel->SetText(FText::FromString(Quests && Quests->IsTracked(SelectedQuest) ? TEXT("Untrack") : TEXT("Track")));
        if (UCanvasPanelSlot* Placed = Cast<UCanvasPanelSlot>(SecondaryButton->GetParent()->Slot)) { Placed->SetPosition(FVector2D(636.f, 683.f)); Placed->SetSize(FVector2D(169.f, 56.f)); }
        AbandonRoot->SetVisibility(bTaken ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        HoldHint->SetVisibility(bTaken ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void USFQuestBoardPanel::Show()
{
    APlayerController* PC = GetOwningPlayer();
    int32 Width = 0, Height = 0; if (PC) { PC->GetViewportSize(Width, Height); }
    const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    SetDesiredSizeInViewport(FVector2D(Width / Scale, Height / Scale));
    SetPositionInViewport(FVector2D::ZeroVector, false);
    HoldStartedAt = -1.0; HoldFraction = 0.f;
    Rebuild();
    SetVisibility(ESlateVisibility::Visible);
}

void USFQuestBoardPanel::OpenJournal(USFQuestPilotComponent* InLog)
{
    Log = InLog; BoardId.Reset();
    PageTitle->SetText(FText::FromString(TEXT("Quest Journal")));
    PageTitle->SetFont(JournalTitleFont);
    Show();
    UE_LOG(LogTemp, Display, TEXT("QUEST_JOURNAL_OPENED Rows=%d"), RowQuests.Num());
}

void USFQuestBoardPanel::OpenBoard(USFQuestPilotComponent* InLog, const FString& InBoardId, const FString& InTitle)
{
    Log = InLog; BoardId = InBoardId; SelectedQuest.Reset();
    PageTitle->SetText(FText::FromString(InTitle));
    PageTitle->SetFont(SFHudStyle::RoleFont(SFHudStyle::TextFamily(), 31, false));  // 16b-quest-board.png: regular face, clear of the stag
    Show();
    if (APlayerController* PC = GetOwningPlayer()) { PC->SetIgnoreMoveInput(true); }
    UE_LOG(LogTemp, Display, TEXT("QUEST_BOARD_OPENED Board=%s Rows=%d"), *BoardId, RowQuests.Num());
}

void USFQuestBoardPanel::Close()
{
    if (!IsOpen()) { return; }
    if (IsBoard()) { if (APlayerController* PC = GetOwningPlayer()) { PC->SetIgnoreMoveInput(false); } }
    SetVisibility(ESlateVisibility::Collapsed);
    UE_LOG(LogTemp, Display, TEXT("QUEST_PANEL_CLOSED Board=%s"), *BoardId);
}

void USFQuestBoardPanel::SelectRow(int32 Index) { if (RowQuests.IsValidIndex(Index)) { Select(RowQuests[Index]); } }

void USFQuestBoardPanel::HandlePrimary()
{
    USFQuestPilotComponent* Quests = Log.Get();
    if (!Quests || !IsBoard() || SelectedQuest.IsEmpty()) { return; }
    UE_LOG(LogTemp, Display, TEXT("QUEST_BOARD_ACCEPT Quest=%s Board=%s"), *SelectedQuest, *BoardId);
    Quests->ServerAcceptQuest(SelectedQuest, BoardId);
}

void USFQuestBoardPanel::HandleSecondary()
{
    USFQuestPilotComponent* Quests = Log.Get();
    if (IsBoard() || !Quests) { Close(); return; }
    Quests->SetTracked(SelectedQuest, !Quests->IsTracked(SelectedQuest));
    Rebuild();
}

void USFQuestBoardPanel::HandleHoldStart() { if (!IsBoard()) { HoldStartedAt = FPlatformTime::Seconds(); } }
void USFQuestBoardPanel::HandleHoldEnd() { HoldStartedAt = -1.0; HoldFraction = 0.f; }

void USFQuestBoardPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (HoldStartedAt >= 0.0)
    {
        constexpr double HoldSeconds = 1.0;  // the family hold time (trade, respawn)
        HoldFraction = float(FMath::Min(1.0, (FPlatformTime::Seconds() - HoldStartedAt) / HoldSeconds));
        if (HoldFraction >= 1.f)
        {
            HoldStartedAt = -1.0; HoldFraction = 0.f;
            if (USFQuestPilotComponent* Quests = Log.Get()) { UE_LOG(LogTemp, Display, TEXT("QUEST_JOURNAL_ABANDON Quest=%s"), *SelectedQuest); Quests->ServerAbandonQuest(SelectedQuest); }
        }
    }
    if (StateKey() != LastKey) { Rebuild(); }
}

int32 USFQuestBoardPanel::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    return SFHudStyle::PaintHoldArc(OutDrawElements, LayerId, HoldRing, HoldFraction);
}

bool USFQuestBoardPanel::TryOpenBoard(APlayerController* Local)
{
    if (!Local || !Local->IsLocalController()) { return false; }
    USFQuestPilotComponent* Quests = Local->FindComponentByClass<USFQuestPilotComponent>();
    ASFQuestBoard* Board = Quests ? ASFQuestBoard::FindInReach(Local) : nullptr;
    USFQuestBoardPanel* Panel = Board ? PanelFor(Local, true) : nullptr;
    if (!Panel) { return false; }
    if (Panel->IsOpen() && Panel->IsBoard()) { return true; }
    Panel->OpenBoard(Quests, Board->BoardId, Board->DisplayName());
    return true;
}

void USFQuestBoardPanel::TickClient(APlayerController* Local, USFQuestPilotComponent* Quests)
{
    USFQuestBoardPanel* Panel = PanelFor(Local, false);
    if (Local->WasInputKeyJustPressed(EKeys::J))
    {
        Panel = PanelFor(Local, true);
        if (Panel && Panel->IsOpen() && !Panel->IsBoard()) { Panel->Close(); }
        else if (Panel) { Panel->Close(); Panel->OpenJournal(Quests); }
        return;
    }
    if (!Panel || !Panel->IsOpen()) { return; }
    if (Local->WasInputKeyJustPressed(EKeys::Escape)) { Panel->Close(); return; }
    // Walking off ends a board visit (the board freezes movement, so this covers knockback and teleports).
    if (Panel->IsBoard())
    {
        bool bNear = false;
        for (TActorIterator<ASFQuestBoard> It(Local->GetWorld()); It && Local->GetPawn() && !bNear; ++It)
        { bNear = It->BoardId == Panel->BoardId && FVector::Dist2D(It->GetActorLocation(), Local->GetPawn()->GetActorLocation()) <= GetDefault<USFQuestPilotSettings>()->BoardReachCm() * 1.5f; }
        if (!bNear) { Panel->Close(); }
    }
}

void USFQuestBoardPanel::OpenForCapture(APlayerController* Local, const FString& Screen)
{
#if !UE_BUILD_SHIPPING
    USFQuestPilotComponent* Quests = Local ? Local->FindComponentByClass<USFQuestPilotComponent>() : nullptr;
    USFQuestBoardPanel* Panel = Quests ? PanelFor(Local, true) : nullptr;
    if (!Panel) { return; }
    Panel->Close();
    if (Screen == TEXT("board")) { Panel->OpenBoard(Quests, TEXT("board/silverbrook"), TEXT("Silverbrook Notice Board")); }
    else { Panel->OpenJournal(Quests); }
#endif
}
