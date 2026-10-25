// CTL-02 NPC dialogue on the player controller: opens the v5 board for the speaker in front of the pawn, routes keys 1-4 / Esc,
// and frames the speaker through ASFE1Character::SetDialogueFramingTarget. QST-02: Accept asks the server's quest log to take the
// quest, and opening a speaker reports the talk (talk_npc turn-ins); everything else is local presentation.
#include "Framework/SFPlayerController.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Characters/SFE1Character.h"
#include "Dialogue/SFDialogue.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "Quest/SFQuestPilot.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFStationRecipes.h"
#include "Services/SFBackpackComponent.h"
#include "Services/SFGameplayServicesClient.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "UI/SFWindowStack.h"

namespace
{
TSharedPtr<const FSFDialogueTree> TreeFor(const FString& File)
{
    static TMap<FString, TSharedPtr<const FSFDialogueTree>> Cache;
    if (const TSharedPtr<const FSFDialogueTree>* Hit = Cache.Find(File)) { return *Hit; }
    TSharedRef<FSFDialogueTree> Tree = MakeShared<FSFDialogueTree>();
    FString Error;
    if (!FSFDialogueTree::Load(File, *Tree, Error)) { UE_LOG(LogTemp, Error, TEXT("DIALOGUE_LOAD_FAILED %s: %s"), *File, *Error); return nullptr; }
    Cache.Add(File, Tree);
    return Tree;
}
}

bool ASFPlayerController::IsDialogueOpen() const
{
    return DialoguePanel && DialoguePanel->GetVisibility() != ESlateVisibility::Collapsed;
}

// The nearest speaker within its own talk range and in front of the pawn; true when a board is (now) open, so E is consumed.
bool ASFPlayerController::TryOpenDialogue(ASFDialogueNpc* ExactTarget)
{
    if (!IsLocalController()) { return false; }
    if (IsDialogueOpen())
    { if (!ExactTarget || DialogueNpc == ExactTarget) { return true; } CloseDialogue(); }
    APawn* Body = GetPawn();
    UWorld* World = GetWorld();
    if (!Body || !World || !GetLocalPlayer() || !GetLocalPlayer()->ViewportClient) { return false; }
    const FVector Forward = FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X);
    ASFDialogueNpc* Best = nullptr; float BestDistance = TNumericLimits<float>::Max();
    for (TActorIterator<ASFDialogueNpc> It(World); It; ++It)
    {
        if (ExactTarget && *It != ExactTarget) { continue; }
        const FVector Offset = (*It)->GetActorLocation() - Body->GetActorLocation();
        const float Distance = FVector(Offset.X, Offset.Y, 0.f).Size();
        const bool bInFront = Distance < KINDA_SMALL_NUMBER || FVector::DotProduct(FVector(Offset.X, Offset.Y, 0.f) / Distance, Forward) > 0.2f;
        if (Distance <= (*It)->TalkRangeCm && bInFront && Distance < BestDistance) { Best = *It; BestDistance = Distance; }
    }
    if (!Best) { return false; }
    const TSharedPtr<const FSFDialogueTree> Tree = TreeFor(Best->DialogueFile);
    if (!Tree.IsValid()) { return false; }
    if (!DialoguePanel)
    {
        const TSubclassOf<USFDialoguePanel> PanelClass = LoadClass<USFDialoguePanel>(nullptr, USFDialoguePanel::AssetPath);
        if (!PanelClass) { UE_LOG(LogTemp, Error, TEXT("DIALOGUE_WBP_MISSING %s: run tools/ui/create_widgets.py"), USFDialoguePanel::AssetPath); return false; }
        DialoguePanel = CreateWidget<USFDialoguePanel>(this, PanelClass);
        if (!DialoguePanel) { return false; }
        DialoguePanel->OnStep.BindUObject(this, &ASFPlayerController::HandleDialogueStep);
        DialoguePanel->AddToViewport(30);
        DialoguePanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    CloseSidePanelsForModal();
    // The speaker's quest is offered only while the player can take it; held, finished or locked opens on the accepted line.
    USFQuestPilotComponent* Quests = FindComponentByClass<USFQuestPilotComponent>();
    const TSharedPtr<const FSFQuestDefinition> Quest = Tree->bHasOffer ? FSFQuestCatalog::Get().Find(Tree->Offer.QuestId) : nullptr;
    const bool bOffered = Quest && (!Quests || FSFQuestCatalog::Get().OfferState(*Quest, Quests->ActiveIds(), Quests->CompletedIds()) == ESFQuestOfferState::Available);
    DialoguePanel->Open(Tree, Quest.Get(), bOffered);
    if (Quests) { Quests->ServerTalkedTo(TEXT("npc/") + FPaths::GetBaseFilename(Best->DialogueFile)); }
    GetWindowStack()->Open(DialoguePanel);  // a window like the others: lines up at the left, Esc ends the conversation
    DialogueNpc = Best;
    if (bOffered)
    {
        FSFQuestView Offered; Offered.QuestId = Quest->Id; Offered.Title = Quest->Title();
        for (const FSFQuestStep& Step : Quest->Steps)
        { for (const FSFQuestObjective& Objective : Step.Objectives) { FSFQuestLine Line; Line.Label = FSFQuestCatalog::ObjectiveLabel(Objective); Line.Target = Objective.Count; Offered.Lines.Add(Line); } }
        if (USFQuestPilotSubsystem* Log = World->GetSubsystem<USFQuestPilotSubsystem>()) { Log->PresentOffer(Offered); }
    }
    SetIgnoreMoveInput(true);
    const FVector ToPlayer = Body->GetActorLocation() - Best->GetActorLocation();
    Best->SetActorRotation(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(ToPlayer.Y, ToPlayer.X)), 0.f));
    if (ASFE1Character* Hero = Cast<ASFE1Character>(Body)) { Hero->SetDialogueFramingTarget(Best); }
    UE_LOG(LogTemp, Display, TEXT("DIALOGUE_OPENED Speaker=%s"), *Tree->Speaker);
    return true;
}

void ASFPlayerController::CloseDialogue()
{
    if (!DialoguePanel) { return; }
    const bool bWasOpen = IsDialogueOpen();
    GetWindowStack()->Close(DialoguePanel);
    if (!bWasOpen) { return; }
    SetIgnoreMoveInput(false);
    if (ASFE1Character* Hero = Cast<ASFE1Character>(GetPawn())) { Hero->SetDialogueFramingTarget(nullptr); }
    DialogueNpc.Reset();
    if (USFQuestPilotSubsystem* Quests = GetWorld() ? GetWorld()->GetSubsystem<USFQuestPilotSubsystem>() : nullptr) { Quests->RestoreTracker(); }
    UE_LOG(LogTemp, Display, TEXT("DIALOGUE_CLOSED"));
}

void ASFPlayerController::HandleDialogueStep(ESFDialogueStep Step)
{
    if (Step == ESFDialogueStep::Learn && DialoguePanel && DialogueNpc.IsValid())
    {
        const FString Action = DialoguePanel->GetSession().LastAction;
        if (Action.StartsWith(TEXT("learn:"))) { ServerLearnRecipe(DialogueNpc.Get(), Action.Mid(6)); }
    }
    if (Step == ESFDialogueStep::Accepted)
    {
        const FSFDialogueSession& Session = DialoguePanel->GetSession();
        const FString QuestId = Session.Tree.IsValid() ? Session.Tree->Offer.QuestId : FString();
        const FString Giver = DialogueNpc.IsValid() ? TEXT("npc/") + FPaths::GetBaseFilename(DialogueNpc->DialogueFile) : FString();
        UE_LOG(LogTemp, Display, TEXT("DIALOGUE_OFFER_ACCEPTED Quest=%s Giver=%s"), *QuestId, *Giver);
        if (USFQuestPilotComponent* Quests = FindComponentByClass<USFQuestPilotComponent>()) { Quests->ServerAcceptQuest(QuestId, Giver); }
    }
    if (Step == ESFDialogueStep::Closed || Step == ESFDialogueStep::Declined) { CloseDialogue(); }
}

void ASFPlayerController::ServerLearnRecipe_Implementation(ASFDialogueNpc* Npc, const FString& RecipeId)
{
    const FSFStationRecipe* Recipe = SFStationRecipes::Find(RecipeId);
    APawn* Body = GetPawn();
    if (!Npc || !Body || !Recipe || Recipe->Trainers.IsEmpty() ||
        FVector::Dist2D(Npc->GetActorLocation(), Body->GetActorLocation()) > Npc->TalkRangeCm ||
        !Recipe->Trainers.Contains(TEXT("npc/") + FPaths::GetBaseFilename(Npc->DialogueFile))) { return; }
    USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(this);
    if (!Pack || !USFBackpackComponent::Services()) { return; }
    FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::LearnRecipe;
    Request.OperationId = TEXT("learn/") + FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    Request.CharacterId = Pack->GetCharacterId(); Request.RecipeId = RecipeId;
    Request.LearnSource = TEXT("trainer"); Request.TrainerNpc = TEXT("npc/") + FPaths::GetBaseFilename(Npc->DialogueFile);
    USFBackpackComponent::Services()->Call(Request, [Weak = TWeakObjectPtr<ASFPlayerController>(this), RecipeId](const FSFGameplayResponse& Response)
    {
        if (Response.Status == ESFGameplayStatus::Ok)
            if (auto* Self = Weak.Get()) if (Self->GatheringPilot) { Self->GatheringPilot->AddKnownRecipe(RecipeId); }
    });
}

void ASFPlayerController::UseRecipeScroll(const FString& RecipeId)
{ ServerUseRecipeScroll(RecipeId); }

void ASFPlayerController::ServerUseRecipeScroll_Implementation(const FString& RecipeId)
{
    const FSFStationRecipe* Recipe = SFStationRecipes::Find(RecipeId);
    if (!Recipe || Recipe->LearnHint != TEXT("Learn from a recipe scroll")) { return; }
    USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(this);
    if (!Pack || !USFBackpackComponent::Services()) { return; }
    FSFBackpackMutation Mutation; Mutation.Label = TEXT("recipe_scroll");
    Mutation.OperationId = TEXT("learn/") + FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
    Mutation.bRetryStale = false;  // a failed consume is not a committed receipt; UI may request again after a refresh.
    Mutation.Run = [Weak = TWeakObjectPtr<ASFPlayerController>(this), RecipeId](const FString& Op, int64 Expected,
        TFunction<void(ESFBackpackOutcome, int64)> Report)
    {
        ASFPlayerController* Self = Weak.Get();
        USFBackpackComponent* Owner = Self ? USFBackpackComponent::FindOrCreate(Self) : nullptr;
        if (!Owner) { Report(ESFBackpackOutcome::Unavailable, -1); return; }
        FSFGameplayRequest Request; Request.Rpc = ESFGameplayRpc::LearnRecipe;
        Request.OperationId = Op; Request.CharacterId = Owner->GetCharacterId(); Request.RecipeId = RecipeId;
        Request.LearnSource = TEXT("scroll"); Request.ScrollContainer = Owner->GetContainerId(); Request.ExpectedContainerRevision = Expected;
        USFBackpackComponent::Services()->Call(Request, [Report](const FSFGameplayResponse& Response)
        { Report(Response.Status == ESFGameplayStatus::Ok ? ESFBackpackOutcome::Committed :
            Response.Status == ESFGameplayStatus::Unavailable ? ESFBackpackOutcome::Unavailable : ESFBackpackOutcome::Rejected, -1); });
    };
    Mutation.Finished = [Weak = TWeakObjectPtr<ASFPlayerController>(this), RecipeId](ESFBackpackOutcome Result)
    {
        if (Result == ESFBackpackOutcome::Committed)
            if (auto* Self = Weak.Get())
            {
                if (Self->GatheringPilot) { Self->GatheringPilot->AddKnownRecipe(RecipeId); }
                if (auto* Owner = USFBackpackComponent::FindOrCreate(Self)) { Owner->Reconcile(); }
            }
    };
    Pack->Enqueue(MoveTemp(Mutation));
}

void ASFPlayerController::TickDialogue()
{
    if (!IsDialogueOpen()) { return; }
    const ASFDialogueNpc* Npc = DialogueNpc.Get();
    const APawn* Body = GetPawn();
    // Walking off ends the conversation (the board freezes movement, so this covers knockback and teleports).
    if (!Npc || !Body || FVector::Dist2D(Npc->GetActorLocation(), Body->GetActorLocation()) > Npc->TalkRangeCm * 1.5f) { CloseDialogue(); return; }
    static const FKey Keys[FSFDialogueTree::MaxChoices] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
    for (int32 Index = 0; Index < FSFDialogueTree::MaxChoices && IsDialogueOpen(); ++Index)
    {
        if (WasInputKeyJustPressed(Keys[Index])) { DialoguePanel->ChooseIndex(Index); }
    }
}

void ASFPlayerController::SFTalk()
{
    TryOpenDialogue();
}

void ASFPlayerController::SFDialogueChoose(int32 Index)
{
#if !UE_BUILD_SHIPPING
    if (IsDialogueOpen()) { DialoguePanel->ChooseIndex(Index); }
#endif
}

void ASFPlayerController::SFStageAt(float XMetres, float YMetres, float YawDegrees)
{
#if !UE_BUILD_SHIPPING
    APawn* Body = GetPawn();
    if (!IsLocalController() || !Body || !GetWorld()) { return; }
    const FVector Column(XMetres * 100.f, YMetres * 100.f, 0.f);
    FHitResult Ground;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SFStageAt), false, Body);
    if (!GetWorld()->LineTraceSingleByChannel(Ground, Column + FVector(0.f, 0.f, 50000.f), Column - FVector(0.f, 0.f, 50000.f), ECC_Visibility, Query)) { return; }
    Body->TeleportTo(Ground.ImpactPoint + FVector(0.f, 0.f, Body->GetSimpleCollisionHalfHeight() + 5.f), FRotator(0.f, YawDegrees, 0.f));
    SetControlRotation(FRotator(-6.f, YawDegrees, 0.f));
    UE_LOG(LogTemp, Display, TEXT("STAGE_AT %s"), *Ground.ImpactPoint.ToString());
#endif
}

void ASFPlayerController::SFSpawnDialogueNpc()
{
#if !UE_BUILD_SHIPPING
    APawn* Body = GetPawn();
    if (!IsLocalController() || !Body || !GetWorld()) { return; }
    const FVector Forward = FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X);
    FHitResult Ground;
    const FVector Spot = Body->GetActorLocation() + Forward * 220.f;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Ground, Spot + FVector(0.f, 0.f, 300.f), Spot - FVector(0.f, 0.f, 600.f), ECC_Visibility);
    const FVector At = bHit ? Ground.ImpactPoint : Spot - FVector(0.f, 0.f, 90.f);
    ASFDialogueNpc* Npc = GetWorld()->SpawnActor<ASFDialogueNpc>(At, (-Forward).Rotation());
    if (Npc) { TryOpenDialogue(); }
#endif
}
