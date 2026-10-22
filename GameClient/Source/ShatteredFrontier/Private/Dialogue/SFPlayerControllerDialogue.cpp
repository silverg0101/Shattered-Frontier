// CTL-02 NPC dialogue on the player controller: opens the v5 board for the speaker in front of the pawn, routes keys 1-4 / Esc,
// and frames the speaker through ASFE1Character::SetDialogueFramingTarget. Local presentation only; nothing here is sent to the server.
#include "Framework/SFPlayerController.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Characters/SFE1Character.h"
#include "Dialogue/SFDialogue.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

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
bool ASFPlayerController::TryOpenDialogue()
{
    if (!IsLocalController()) { return false; }
    if (IsDialogueOpen()) { return true; }
    APawn* Body = GetPawn();
    UWorld* World = GetWorld();
    if (!Body || !World || !GetLocalPlayer() || !GetLocalPlayer()->ViewportClient) { return false; }
    const FVector Forward = FRotationMatrix(FRotator(0.f, GetControlRotation().Yaw, 0.f)).GetUnitAxis(EAxis::X);
    ASFDialogueNpc* Best = nullptr; float BestDistance = TNumericLimits<float>::Max();
    for (TActorIterator<ASFDialogueNpc> It(World); It; ++It)
    {
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
        DialoguePanel = CreateWidget<USFDialoguePanel>(this, USFDialoguePanel::StaticClass());
        if (!DialoguePanel) { return false; }
        DialoguePanel->OnStep.BindUObject(this, &ASFPlayerController::HandleDialogueStep);
        DialoguePanel->AddToViewport(30);
    }
    CloseSidePanelsForModal();
    DialoguePanel->Open(Tree);
    int32 Width = 0, Height = 0; GetViewportSize(Width, Height);
    const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));
    DialoguePanel->SetDesiredSizeInViewport(FVector2D(Width / Scale, Height / Scale));  // modal root: dims the scene, places the board itself
    DialoguePanel->SetPositionInViewport(FVector2D::ZeroVector, false);
    DialoguePanel->SetVisibility(ESlateVisibility::Visible);
    DialogueNpc = Best;
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
    DialoguePanel->SetVisibility(ESlateVisibility::Collapsed);
    if (!bWasOpen) { return; }
    SetIgnoreMoveInput(false);
    if (ASFE1Character* Hero = Cast<ASFE1Character>(GetPawn())) { Hero->SetDialogueFramingTarget(nullptr); }
    DialogueNpc.Reset();
    UE_LOG(LogTemp, Display, TEXT("DIALOGUE_CLOSED"));
}

void ASFPlayerController::HandleDialogueStep(ESFDialogueStep Step)
{
    if (Step == ESFDialogueStep::Accepted) { UE_LOG(LogTemp, Display, TEXT("DIALOGUE_OFFER_ACCEPTED")); }  // no quest service owns offers yet
    if (Step == ESFDialogueStep::Closed || Step == ESFDialogueStep::Declined) { CloseDialogue(); }
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
