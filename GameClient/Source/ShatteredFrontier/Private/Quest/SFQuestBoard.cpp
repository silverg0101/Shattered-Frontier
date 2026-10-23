#include "Quest/SFQuestBoard.h"

#include "Quest/SFQuestPilot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/SFHudStyle.h"
#include "Framework/SFPlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gathering/SFStationRecipes.h"
#include "InputCoreTypes.h"
#include "UI/SFItemTooltip.h"
#include "UI/SFWindowStack.h"
#include "UObject/ConstructorHelpers.h"

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
FLinearColor Greyed() { return SFHudStyle::Hex(TEXT("E8E1CE"), 0.45f); }

USFQuestBoardPanel* PanelFor(APlayerController* Local, bool bCreate)
{
    USFQuestPilotSubsystem* Quests = Local && Local->GetWorld() ? Local->GetWorld()->GetSubsystem<USFQuestPilotSubsystem>() : nullptr;
    if (!Quests) { return nullptr; }
    if (!Quests->Panel && bCreate)
    {
        const TSubclassOf<USFQuestBoardPanel> Class = LoadClass<USFQuestBoardPanel>(nullptr, USFQuestBoardPanel::AssetPath);
        if (!Class) { UE_LOG(LogTemp, Error, TEXT("QUEST_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFQuestBoardPanel::AssetPath); return nullptr; }
        Quests->Panel = CreateWidget<USFQuestBoardPanel>(Local, Class);
        if (Quests->Panel) { Quests->Panel->AddToViewport(28); Quests->Panel->SetVisibility(ESlateVisibility::Collapsed); }
    }
    return Quests->Panel;
}
}

void USFQuestBoardPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    RowClass = LoadClass<USFKitRowWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_KitRow.WBP_KitRow_C"));
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
    if (AcceptButton) { AcceptButton->SetLabel(FText::FromString(TEXT("Accept"))); AcceptButton->OnActivated.AddDynamic(this, &USFQuestBoardPanel::HandlePrimary); }
    if (TrackButton) { TrackButton->SetLabel(FText::FromString(TEXT("Track"))); TrackButton->OnActivated.AddDynamic(this, &USFQuestBoardPanel::HandleTrack); }
    if (AbandonButton)
    {
        AbandonButton->SetLabel(FText::FromString(TEXT("Hold to Abandon")));
        AbandonButton->SetHoldMode(true, 1.0f);  // the family hold time (trade, respawn)
        AbandonButton->OnActivated.AddDynamic(this, &USFQuestBoardPanel::HandleAbandon);
    }
    for (int32 Index = 0; Index < MaxRewardSlots; ++Index)
    {
        if (USFSlotWidget* RewardSlot = Cast<USFSlotWidget>(GetWidgetFromName(*FString::Printf(TEXT("Reward%d"), Index)))) { RewardSlots.Add(RewardSlot); }
    }
    if (RewardGold) { RewardGold->SetAmount(0); RewardGold->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFQuestBoardPanel::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

TArray<USFQuestBoardPanel::FRow> USFQuestBoardPanel::ListRows() const
{
    TArray<FRow> Result;
    const USFQuestPilotComponent* Quests = Log.Get();
    if (!Quests) { return Result; }
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
            (Row.bLocked ? Others : Result).Add(Row);
        }
        Others.StableSort([](const FRow& A, const FRow& B) { return A.Section < B.Section; });  // Locked before Taken
        Result.Append(Others);
    }
    else
    {
        // The journal names each active quest's zone ("Silverbrook").
        for (const FSFQuestView& View : Quests->GetViews())
        {
            if (View.bComplete) { continue; }
            const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(View.QuestId);
            FString Zone = Quest ? Quest->ZoneId : FString(); int32 Cut;
            if (Zone.FindLastChar('_', Cut)) { Zone = Zone.Mid(Cut + 1); }
            if (!Zone.IsEmpty()) { Zone[0] = FChar::ToUpper(Zone[0]); }
            FRow Row; Row.QuestId = View.QuestId; Row.Section = Zone;
            Result.Add(Row);
        }
    }
    if (Result.Num() > MaxRows) { Result.SetNum(MaxRows); }
    return Result;
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
    const TArray<FRow> QuestRows = ListRows();
    if (!QuestRows.ContainsByPredicate([&](const FRow& R) { return R.QuestId == SelectedQuest; })) { SelectedQuest = QuestRows.IsEmpty() ? FString() : QuestRows[0].QuestId; }
    const FSFQuestCatalog& Catalog = FSFQuestCatalog::Get();
    // The list: one kit row per quest, the section ("Posted", "Locked", the zone) in its cell.
    QuestList->ClearChildren(); Rows.Reset(); RowQuests.Reset();
    for (const FRow& Entry : QuestRows)
    {
        const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(Entry.QuestId);
        if (!Quest || !RowClass) { continue; }
        USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
        if (!Row) { continue; }
        QuestList->AddChild(Row);
        Row->SetEntry(FText::FromString(Quest->Title()), FText::GetEmpty());
        Row->SetIconTexture(nullptr);
        Row->SetCells(FText::FromString(Entry.Section), FText::GetEmpty());
        Row->ShowChevron(true);
        Row->SetSelected(Entry.QuestId == SelectedQuest);
        if (Entry.bLocked) { Row->SetTint(Greyed(), Greyed()); Row->SetCellTint(Greyed()); }
        Row->OnClicked.AddDynamic(this, &USFQuestBoardPanel::HandleRowClicked);
        Rows.Add(Row); RowQuests.Add(Entry.QuestId);
    }
    if (EmptyText)
    {
        EmptyText->SetText(FText::FromString(IsBoard() ? TEXT("Nothing is posted here.") : TEXT("No quests yet. Visit a notice board.")));
        EmptyText->SetVisibility(Rows.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    // The selected quest.
    const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(SelectedQuest);
    if (Detail) { Detail->SetVisibility(Quest ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    const USFQuestPilotComponent* Quests = Log.Get();
    if (!Quest)
    {
        if (AcceptButton) { AcceptButton->SetVisibility(ESlateVisibility::Collapsed); }
        if (TrackButton) { TrackButton->SetVisibility(ESlateVisibility::Collapsed); }
        if (AbandonButton) { AbandonButton->SetVisibility(ESlateVisibility::Collapsed); }
        return;
    }
    const FRow* Shown = QuestRows.FindByPredicate([&](const FRow& R) { return R.QuestId == SelectedQuest; });
    if (QuestTitle) { QuestTitle->SetText(FText::FromString(Quest->Title())); }
    if (Summary) { Summary->SetText(FText::FromString(Quest->bHasOffer ? Quest->Offer.Summary : FString())); }
    const FSFQuestView* View = Quests ? Quests->FindView(SelectedQuest) : nullptr;
    FString Note = Shown && Shown->bLocked ? Shown->Reason : FString();
    const FString& Result = Quests ? Quests->GetLastResult() : FString();
    if (Note.IsEmpty() && !Result.IsEmpty() && Result != TEXT("accepted") && Result != TEXT("abandoned")) { Note = Result == TEXT("out_of_reach") ? TEXT("Stand closer to the board") : Result.Replace(TEXT("_"), TEXT(" ")); }
    if (Reason) { Reason->SetText(FText::FromString(Note)); Reason->SetVisibility(Note.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
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
        UWidget* Row = GetWidgetFromName(*FString::Printf(TEXT("Objective%dRow"), Index));
        if (!Row) { continue; }
        const bool bShow = Lines.IsValidIndex(Index);
        Row->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bShow) { continue; }
        const bool bDone = Lines[Index].Get<1>() >= Lines[Index].Get<2>();
        if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Objective%dLabel"), Index)))) { Label->SetText(FText::FromString(Lines[Index].Get<0>())); }
        if (UTextBlock* Count = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("Objective%dCount"), Index)))) { Count->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Lines[Index].Get<1>(), Lines[Index].Get<2>()))); }
        if (UWidget* Tick = GetWidgetFromName(*FString::Printf(TEXT("Objective%dTick"), Index))) { Tick->SetVisibility(bDone ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden); }
    }
    // Rewards: item slots with their counts (and gold when a quest pays any; none does yet).
    TArray<FSFQuestRewardItem> Items;
    for (const FSFQuestStep& Step : Quest->Steps) { Items.Append(Step.RewardItems); }
    RewardIds.Reset(); RewardCounts.Reset();
    for (int32 Index = 0; Index < RewardSlots.Num(); ++Index)
    {
        const bool bShow = Items.IsValidIndex(Index);
        RewardSlots[Index]->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (!bShow) { continue; }
        RewardSlots[Index]->SetIcon(SFItemTooltip::ItemTexture(Items[Index].DefinitionId));
        RewardSlots[Index]->SetKeyText(FText::GetEmpty());
        RewardSlots[Index]->SetCount(static_cast<int32>(Items[Index].Quantity));
        RewardIds.Add(Items[Index].DefinitionId); RewardCounts.Add(static_cast<int32>(Items[Index].Quantity));
    }
    if (NoReward) { NoReward->SetVisibility(Items.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    // Buttons.
    const bool bTaken = View && !View->bComplete;
    if (IsBoard())
    {
        const bool bCanAccept = Shown && !Shown->bLocked;
        if (AcceptButton)
        {
            AcceptButton->SetVisibility(ESlateVisibility::Visible);
            AcceptButton->SetButtonEnabled(bCanAccept);
            AcceptButton->SetLabel(FText::FromString(bCanAccept ? TEXT("Accept") : bTaken ? TEXT("Accepted") : TEXT("Locked")));
        }
        if (TrackButton) { TrackButton->SetVisibility(ESlateVisibility::Collapsed); }
        if (AbandonButton) { AbandonButton->SetVisibility(ESlateVisibility::Collapsed); }
    }
    else
    {
        // Journal: Track toggles the HUD tracker, Abandon is held to confirm.
        if (AcceptButton) { AcceptButton->SetVisibility(ESlateVisibility::Collapsed); }
        if (TrackButton) { TrackButton->SetVisibility(ESlateVisibility::Visible); TrackButton->SetLabel(FText::FromString(Quests && Quests->IsTracked(SelectedQuest) ? TEXT("Untrack") : TEXT("Track"))); }
        if (AbandonButton) { AbandonButton->SetVisibility(bTaken ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
    }
}

void USFQuestBoardPanel::Show()
{
    Rebuild();
    if (ASFPlayerController* PC = Cast<ASFPlayerController>(GetOwningPlayer())) { PC->GetWindowStack()->Open(this); }
    else { SetVisibility(ESlateVisibility::Visible); }
}

void USFQuestBoardPanel::OpenJournal(USFQuestPilotComponent* InLog)
{
    Log = InLog; BoardId.Reset();
    if (Window) { Window->SetTitle(FText::FromString(TEXT("Quest Journal"))); }
    Show();
    UE_LOG(LogTemp, Display, TEXT("QUEST_JOURNAL_OPENED Rows=%d"), RowQuests.Num());
}

void USFQuestBoardPanel::OpenBoard(USFQuestPilotComponent* InLog, const FString& InBoardId, const FString& InTitle)
{
    Log = InLog; BoardId = InBoardId; SelectedQuest.Reset();
    if (Window) { Window->SetTitle(FText::FromString(InTitle)); }
    Show();
    if (APlayerController* PC = GetOwningPlayer()) { PC->SetIgnoreMoveInput(true); bFrozeMovement = true; }
    UE_LOG(LogTemp, Display, TEXT("QUEST_BOARD_OPENED Board=%s Rows=%d"), *BoardId, RowQuests.Num());
}

void USFQuestBoardPanel::Close()
{
    if (!IsOpen()) { return; }
    if (bFrozeMovement) { if (APlayerController* PC = GetOwningPlayer()) { PC->SetIgnoreMoveInput(false); } bFrozeMovement = false; }
    if (ASFPlayerController* PC = Cast<ASFPlayerController>(GetOwningPlayer())) { PC->GetWindowStack()->Close(this); }
    else { SetVisibility(ESlateVisibility::Collapsed); }
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
    UE_LOG(LogTemp, Display, TEXT("QUEST_PANEL_CLOSED Board=%s"), *BoardId);
}

void USFQuestBoardPanel::HandleRowClicked(USFSelectableWidget* Source)
{
    const int32 Index = Rows.IndexOfByPredicate([Source](const TObjectPtr<USFKitRowWidget>& Row) { return Row == Source; });
    if (RowQuests.IsValidIndex(Index)) { Select(RowQuests[Index]); }
}

void USFQuestBoardPanel::HandlePrimary(USFButtonWidget*)
{
    USFQuestPilotComponent* Quests = Log.Get();
    if (!Quests || !IsBoard() || SelectedQuest.IsEmpty()) { return; }
    UE_LOG(LogTemp, Display, TEXT("QUEST_BOARD_ACCEPT Quest=%s Board=%s"), *SelectedQuest, *BoardId);
    Quests->ServerAcceptQuest(SelectedQuest, BoardId);
}

void USFQuestBoardPanel::HandleTrack(USFButtonWidget*)
{
    USFQuestPilotComponent* Quests = Log.Get();
    if (IsBoard() || !Quests || SelectedQuest.IsEmpty()) { return; }
    Quests->SetTracked(SelectedQuest, !Quests->IsTracked(SelectedQuest));
    Rebuild();
}

void USFQuestBoardPanel::HandleAbandon(USFButtonWidget*)
{
    USFQuestPilotComponent* Quests = Log.Get();
    if (IsBoard() || !Quests || SelectedQuest.IsEmpty()) { return; }
    UE_LOG(LogTemp, Display, TEXT("QUEST_JOURNAL_ABANDON Quest=%s"), *SelectedQuest);
    Quests->ServerAbandonQuest(SelectedQuest);
}

void USFQuestBoardPanel::UpdateTooltip()
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

void USFQuestBoardPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!IsOpen()) { return; }
    if (StateKey() != LastKey) { Rebuild(); }
    UpdateTooltip();
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
    if (!Panel) { return; }
    // Esc closes the window through the window stack; a board visit that ended some other way (Esc, a modal) gives movement back here.
    if (!Panel->IsOpen()) { if (Panel->bFrozeMovement) { Panel->bFrozeMovement = false; Local->SetIgnoreMoveInput(false); } return; }
    // Walking off ends a board visit (the board freezes movement, so this covers knockback and teleports).
    if (Panel->IsBoard() && !Panel->bCaptureVisit)
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
    Panel->bCaptureVisit = Screen == TEXT("board");
    if (Screen == TEXT("board")) { Panel->OpenBoard(Quests, TEXT("board/silverbrook"), TEXT("Silverbrook Notice Board")); }
    else { Panel->OpenJournal(Quests); }
#endif
}
