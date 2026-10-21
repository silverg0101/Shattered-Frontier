#include "Quest/SFQuestPilot.h"

#include "Quest/SFQuestPilotEvents.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Characters/SFE1Character.h"
#include "Combat/SFCombatPilot.h"
#include "Construction/SFConstructionGeometry.h"
#include "Construction/SFConstructionNetwork.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "Framework/SFHudStyle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryComponent.h"
#include "InventoryItemDefinition.h"
#include "InventoryTransactionLibrary.h"
#endif

SFQuestPilotEvents::FGathered& SFQuestPilotEvents::OnGathered() { static FGathered D; return D; }
SFQuestPilotEvents::FStructurePlaced& SFQuestPilotEvents::OnStructurePlaced() { static FStructurePlaced D; return D; }
SFQuestPilotEvents::FBoarDefeated& SFQuestPilotEvents::OnBoarDefeated() { static FBoarDefeated D; return D; }

namespace
{
    constexpr int32 MaxObjectiveCount = 500;  // QuestTuning default; same bounds as the backend model

    bool ValidId(const FString& Id, const TCHAR* Prefix)
    {
        FString Head, Tail;
        if (!Id.Split(TEXT("/"), &Head, &Tail) || Head != Prefix || Tail.IsEmpty()) { return false; }
        for (TCHAR C : Head) { if (!(C == '_' || (C >= 'a' && C <= 'z'))) { return false; } }
        for (TCHAR C : Tail) { if (!(C == '_' || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9'))) { return false; } }
        return true;
    }

    bool ValidSlug(const FString& S)
    {
        if (S.IsEmpty() || !(S[0] >= 'a' && S[0] <= 'z')) { return false; }
        for (TCHAR C : S) { if (!(C == '_' || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9'))) { return false; } }
        return true;
    }

    // "item/oak_log" or "harvest_oak" -> "Oak Log" / "Harvest Oak"
    FString Humanize(const FString& Id)
    {
        FString Name = Id; int32 Slash;
        if (Id.FindLastChar('/', Slash)) { Name = Id.Mid(Slash + 1); }
        FString Out; bool bStart = true;
        for (TCHAR C : Name)
        {
            if (C == '_') { Out.AppendChar(' '); bStart = true; }
            else { Out.AppendChar(bStart ? FChar::ToUpper(C) : C); bStart = false; }
        }
        return Out;
    }

    FString Verb(const FString& Kind)
    { return Kind == TEXT("gather") ? TEXT("Gather") : Kind == TEXT("craft") ? TEXT("Craft") : Kind == TEXT("build") ? TEXT("Build") : TEXT("Defeat"); }
}

bool FSFQuestDefinition::Parse(const FString& Json, FSFQuestDefinition& Out, FString& Error)
{
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root) { Error = TEXT("not valid JSON"); return false; }
    FSFQuestDefinition Q;
    double Rev = 0;
    Root->TryGetStringField(TEXT("id"), Q.Id); Root->TryGetStringField(TEXT("zone_id"), Q.ZoneId); Root->TryGetNumberField(TEXT("revision"), Rev);
    Q.Revision = int32(Rev);
    if (!ValidId(Q.Id, TEXT("quest")) || Q.Revision < 1) { Error = TEXT("quest id/revision invalid"); return false; }
    if (!ValidId(Q.ZoneId, TEXT("zone"))) { Error = TEXT("zone_id invalid"); return false; }
    const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
    if (!Root->TryGetArrayField(TEXT("steps"), Steps) || Steps->Num() < 1 || Steps->Num() > 20) { Error = TEXT("steps count invalid"); return false; }
    TSet<FString> Seen;
    for (const TSharedPtr<FJsonValue>& StepValue : *Steps)
    {
        const TSharedPtr<FJsonObject> StepObj = StepValue ? StepValue->AsObject() : nullptr;
        if (!StepObj) { Error = TEXT("step null"); return false; }
        FSFQuestStep Step; StepObj->TryGetStringField(TEXT("id"), Step.Id);
        if (!ValidSlug(Step.Id) || Seen.Contains(Step.Id)) { Error = FString::Printf(TEXT("step id invalid/duplicate: %s"), *Step.Id); return false; }
        const TArray<TSharedPtr<FJsonValue>>* Pre = nullptr;
        if (StepObj->TryGetArrayField(TEXT("prerequisites"), Pre))
        {
            for (const auto& P : *Pre)
            {
                const FString Id = P ? P->AsString() : FString();
                if (!Seen.Contains(Id) || Step.Prerequisites.Contains(Id)) { Error = FString::Printf(TEXT("step %s: prerequisites must be unique earlier steps"), *Step.Id); return false; }
                Step.Prerequisites.Add(Id);
            }
        }
        Seen.Add(Step.Id);
        const TArray<TSharedPtr<FJsonValue>>* Objs = nullptr;
        if (!StepObj->TryGetArrayField(TEXT("objectives"), Objs) || Objs->IsEmpty()) { Error = FString::Printf(TEXT("step %s: objectives missing"), *Step.Id); return false; }
        for (const auto& OV : *Objs)
        {
            const TSharedPtr<FJsonObject> O = OV ? OV->AsObject() : nullptr;
            if (!O) { Error = FString::Printf(TEXT("step %s: objective null"), *Step.Id); return false; }
            FSFQuestObjective Obj; double Count = 0;
            O->TryGetStringField(TEXT("key"), Obj.Key); O->TryGetStringField(TEXT("kind"), Obj.Kind); O->TryGetStringField(TEXT("target_id"), Obj.TargetId);
            O->TryGetNumberField(TEXT("count"), Count); Obj.Count = int32(Count);
            Obj.bHasNode = O->TryGetStringField(TEXT("node_id"), Obj.NodeId);
            const TCHAR* Prefix = Obj.Kind == TEXT("gather") || Obj.Kind == TEXT("craft") ? TEXT("item")
                : Obj.Kind == TEXT("defeat") ? TEXT("npc") : Obj.Kind == TEXT("build") ? TEXT("structure_tag") : TEXT("");
            const bool bDuplicate = Step.Objectives.ContainsByPredicate([&](const FSFQuestObjective& E) { return E.Key == Obj.Key; });
            if (!ValidSlug(Obj.Key) || bDuplicate) { Error = FString::Printf(TEXT("step %s: objective key invalid/duplicate"), *Step.Id); return false; }
            if (!*Prefix || !ValidId(Obj.TargetId, Prefix)) { Error = FString::Printf(TEXT("step %s/%s: kind/target invalid"), *Step.Id, *Obj.Key); return false; }
            if (Obj.bHasNode && (Obj.Kind != TEXT("gather") || !ValidId(Obj.NodeId, TEXT("gathering_node")))) { Error = FString::Printf(TEXT("step %s/%s: node_id invalid"), *Step.Id, *Obj.Key); return false; }
            if (Obj.Count < 1 || Obj.Count > MaxObjectiveCount) { Error = FString::Printf(TEXT("step %s/%s: count out of range"), *Step.Id, *Obj.Key); return false; }
            Step.Objectives.Add(Obj);
        }
        const TArray<TSharedPtr<FJsonValue>>* Rewards = nullptr;
        if (StepObj->TryGetArrayField(TEXT("rewards"), Rewards))
        {
            for (const auto& RV : *Rewards)
            {
                FString Bundle; const TSharedPtr<FJsonObject> R = RV ? RV->AsObject() : nullptr;
                if (!R || !R->TryGetStringField(TEXT("bundle_id"), Bundle) || !ValidId(Bundle, TEXT("reward")) || Step.Rewards.Contains(Bundle))
                { Error = FString::Printf(TEXT("step %s: rewards invalid/duplicate"), *Step.Id); return false; }
                Step.Rewards.Add(Bundle);
            }
        }
        Q.Steps.Add(MoveTemp(Step));
    }
    Out = MoveTemp(Q);
    return true;
}

bool FSFQuestDefinition::Load(const FString& Path, FSFQuestDefinition& Out, FString& Error)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path)) { Error = FString::Printf(TEXT("cannot read %s"), *Path); return false; }
    return Parse(Text, Out, Error);
}

ESFQuestApply FSFQuestProgress::Apply(const FSFQuestDefinition& Quest, const FString& Kind, const FString& TargetId, const FString& NodeId,
    int32 Quantity, const FString& OperationId, TArray<FString>& NewRewards)
{
    if (IsComplete(Quest)) { return ESFQuestApply::Completed; }
    const FSFQuestStep& Step = Quest.Steps[StepIndex];
    if (OperationId.IsEmpty() || Operations.Contains(OperationId)) { return ESFQuestApply::Duplicate; }
    if (Quantity < 1) { return ESFQuestApply::Mismatch; }
    const FSFQuestObjective* Match = Step.Objectives.FindByPredicate([&](const FSFQuestObjective& O)
    { return O.Kind == Kind && O.TargetId == TargetId && (!O.bHasNode || O.NodeId == NodeId); });
    if (!Match || Counts.FindRef(Match->Key) >= Match->Count) { return ESFQuestApply::Mismatch; }
    Operations.Add(OperationId);
    int32& Count = Counts.FindOrAdd(Match->Key);
    Count = int32(FMath::Min<int64>(Match->Count, int64(Count) + Quantity));
    const bool bDone = !Step.Objectives.ContainsByPredicate([&](const FSFQuestObjective& O) { return Counts.FindRef(O.Key) < O.Count; });
    if (bDone)
    {
        bool bFirst = false; GrantedSteps.Add(Step.Id, &bFirst);
        if (!bFirst) { NewRewards.Append(Step.Rewards); }
        ++StepIndex; Counts.Reset();
    }
    return ESFQuestApply::Accepted;
}

// ---------------------------------------------------------------- component

USFQuestPilotComponent::USFQuestPilotComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.25f;
}

void USFQuestPilotComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(USFQuestPilotComponent, View, COND_OwnerOnly);
}

void USFQuestPilotComponent::Init(TSharedPtr<const FSFQuestDefinition> InQuest)
{
    Quest = InQuest;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Development fixture for the network runners: start at a named step, earlier steps credited, no rewards due.
    FString StartStep;
    if (Quest && FParse::Value(FCommandLine::Get(), TEXT("QuestPilotStartStep="), StartStep))
    {
        const int32 Index = Quest->Steps.IndexOfByPredicate([&](const FSFQuestStep& S) { return S.Id == StartStep; });
        for (int32 I = 0; I < Index; ++I) { Progress.GrantedSteps.Add(Quest->Steps[I].Id); }
        Progress.StepIndex = FMath::Max(0, Index);
    }
#endif
    RebuildView();
}

bool USFQuestPilotComponent::Observe(const FString& Kind, const FString& TargetId, const FString& NodeId, const FString& OperationId)
{
    if (!Quest || !GetOwner() || !GetOwner()->HasAuthority()) { return false; }
    TArray<FString> Rewards;
    const ESFQuestApply Result = Progress.Apply(*Quest, Kind, TargetId, NodeId, 1, OperationId, Rewards);
    UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_EVENT Kind=%s Target=%s Op=%s Result=%d Step=%d Rewards=%d"),
        *Kind, *TargetId, *OperationId, int32(Result), Progress.StepIndex, Rewards.Num());
    if (Result != ESFQuestApply::Accepted) { return false; }
    PendingRewards.Append(Rewards);
    RebuildView();
    TryDeliverRewards();
    if (Progress.IsComplete(*Quest)) { UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_COMPLETE Quest=%s"), *Quest->Id); }
    return true;
}

void USFQuestPilotComponent::RebuildView()
{
    if (!Quest) { return; }
    FSFQuestView Next;
    Next.Title = Humanize(Quest->Id); Next.StepTotal = Quest->Steps.Num();
    Next.bComplete = Progress.IsComplete(*Quest);
    Next.StepNumber = FMath::Min(Progress.StepIndex + 1, Next.StepTotal);
    if (!Next.bComplete)
    {
        const FSFQuestStep& Step = Quest->Steps[Progress.StepIndex];
        Next.Step = Humanize(Step.Id);
        for (const FSFQuestObjective& O : Step.Objectives)
        {
            FSFQuestLine Line; Line.Label = Verb(O.Kind) + TEXT(" ") + Humanize(O.TargetId);
            Line.Count = Progress.Counts.FindRef(O.Key); Line.Target = O.Count;
            Next.Lines.Add(Line);
        }
    }
    View = MoveTemp(Next);
}

void USFQuestPilotComponent::TryDeliverRewards()
{
    // Only the completion bundle is authored; the pilot pays it in a configured pilot item through the same
    // bag transaction the combat pilot uses for hides. Undelivered bundles stay pending, so nothing repeats or is lost.
#if SF_WITH_MINIMAL_INVENTORY
    const auto* Settings = GetDefault<USFQuestPilotSettings>();
    auto* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UInventoryComponent>() : nullptr;
    while (Inventory && !PendingRewards.IsEmpty())
    {
        FInventoryItemQuantity Pay;
        Pay.Item = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(Settings->CompletionRewardItemPath));
        Pay.Quantity = Settings->CompletionRewardUnits;
        if (UInventoryTransactionLibrary::ExchangeInventoryItems(Inventory, {}, {Pay}) != EInventoryTransactionResult::Success) { return; }
        UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_REWARD Bundle=%s Units=%d"), *PendingRewards[0], Pay.Quantity);
        PendingRewards.RemoveAt(0);
    }
#endif
}

void USFQuestPilotComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    if (GetOwner() && GetOwner()->HasAuthority() && !PendingRewards.IsEmpty()) { TryDeliverRewards(); }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!FParse::Param(FCommandLine::Get(), TEXT("QuestFullScenario"))) { return; }
    if (GetOwner() && GetOwner()->HasAuthority()) { TickFullServer(); }
    else { TickFullClient(GetWorld()->GetTimeSeconds()); }
#endif
}

// ---- Full-chain scenario (development): the server only places the pawn beside the next target, like the
// other scenarios; every action itself is made by the client through its normal entry points and judged by the server.

namespace
{
    // First unmet objective of the current step, by the stable content ids.
    const FSFQuestObjective* NextObjective(const FSFQuestDefinition& Quest, const FSFQuestProgress& Progress)
    {
        if (Progress.IsComplete(Quest)) { return nullptr; }
        for (const FSFQuestObjective& O : Quest.Steps[Progress.StepIndex].Objectives)
        { if (Progress.Counts.FindRef(O.Key) < O.Count) { return &O; } }
        return nullptr;
    }

    bool NodeKindFor(const FString& Kind, const FString& Target, ESFGatheringPilotKind& Out)
    {
        if (Kind == TEXT("gather") && Target == TEXT("item/oak_log")) { Out = ESFGatheringPilotKind::Oak; return true; }
        if (Kind == TEXT("gather") && Target == TEXT("item/iron_ore")) { Out = ESFGatheringPilotKind::Iron; return true; }
        if (Kind == TEXT("craft") && Target == TEXT("item/rough_timber")) { Out = ESFGatheringPilotKind::Sawmill; return true; }
        if (Kind == TEXT("craft") && Target == TEXT("item/iron_ingot")) { Out = ESFGatheringPilotKind::Forge; return true; }
        return false;
    }

    ASFGatheringPilotNode* NearestNode(UWorld* World, ESFGatheringPilotKind Kind, const FVector& To)
    {
        ASFGatheringPilotNode* Best = nullptr;
        for (TActorIterator<ASFGatheringPilotNode> It(World); It; ++It)
        { if (It->Kind == Kind && (!Best || FVector::Dist2D(It->GetActorLocation(), To) < FVector::Dist2D(Best->GetActorLocation(), To))) { Best = *It; } }
        return Best;
    }

    ASFPilotBoar* NearestBoar(UWorld* World, const FVector& To)
    {
        ASFPilotBoar* Best = nullptr;
        for (TActorIterator<ASFPilotBoar> It(World); It; ++It)
        { if (It->Health() > 0 && (!Best || FVector::Dist2D(It->GetActorLocation(), To) < FVector::Dist2D(Best->GetActorLocation(), To))) { Best = *It; } }
        return Best;
    }
}

void USFQuestPilotComponent::TickFullServer()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const auto* PC = Cast<APlayerController>(GetOwner());
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    const FSFQuestObjective* Objective = Quest ? NextObjective(*Quest, Progress) : nullptr;
    if (!Pawn || !Objective) { return; }
    // Gather and node steps: the node nearest the world origin (oak, iron) or the only station; boar: the nearest live boar.
    FVector Target = FVector::ZeroVector; ESFGatheringPilotKind Kind = ESFGatheringPilotKind::Oak;
    if (NodeKindFor(Objective->Kind, Objective->TargetId, Kind))
    {
        const auto* Node = NearestNode(GetWorld(), Kind, FVector::ZeroVector);
        if (!Node) { return; }
        Target = Node->GetActorLocation();
    }
    else if (Objective->Kind == TEXT("defeat"))
    {
        const auto* Boar = NearestBoar(GetWorld(), Pawn->GetActorLocation());
        if (!Boar) { return; }
        Target = Boar->GetActorLocation();
    }
    else if (Objective->Kind == TEXT("build"))
    {
        // The dev claim pad: stand on it beside the first cell (BLD-02 anchored construction).
        const ASFConstructionNetworkState* Claim = nullptr;
        for (TActorIterator<ASFConstructionNetworkState> It(GetWorld()); It; ++It) { Claim = *It; }
        if (!Claim || Claim->GetAnchorCell() == FIntVector::ZeroValue) { return; }
        Target = Claim->GetAnchorOffsetCm() + FVector(25, 25, 25);
    }
    else { return; }
    const FString Key = Quest->Steps[Progress.StepIndex].Id + TEXT("/") + Objective->Key;
    if (Key == FullTeleportKey && FVector::Dist2D(Pawn->GetActorLocation(), Target) < 600) { return; }
    // Re-place after a respawn or a boar that wandered off; otherwise once per objective.
    const FVector Where = Objective->Kind == TEXT("build") ? Target + FVector(-175, -125, 95)
        : Target + (-Target * FVector(1, 1, 0)).GetSafeNormal() * 200 + FVector(0, 0, 80);
    Pawn->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
    FullTeleportKey = Key;
    UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_PLACED Objective=%s"), *Key);
#endif
}

void USFQuestPilotComponent::TickFullClient(double Now)
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    auto* PC = Cast<APlayerController>(GetOwner());
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!PC || !PC->IsLocalController() || !Pawn || View.Title.IsEmpty() || bFullDone) { return; }
    if (View.bComplete)
    { bFullDone = true; UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_DONE Steps=%d"), View.StepTotal); return; }
    const FSFQuestLine* Next = View.Lines.FindByPredicate([](const FSFQuestLine& L) { return L.Count < L.Target; });
    if (!Next) { return; }  // e.g. the wall step before the dev skip lands
    const FString Key = View.Step + TEXT("/") + Next->Label;
    if (Key != FullAttemptKey)
    { FullAttemptKey = Key; FullEquipAt = 0; FullStableSince = 0; FullAttemptAt = 0; FullLastSwingAt = 0; }
    const auto Fail = [&](const TCHAR* Why)
    { UE_LOG(LogTemp, Error, TEXT("QUEST_FULL_FAILED Objective=%s Why=%s"), *Key, Why); FPlatformMisc::RequestExitWithStatus(false, 1); };

    if (Next->Label == TEXT("Build Wall"))
    {
        auto* Builder = Cast<ASFConstructionNetworkController>(PC);
        const ASFConstructionNetworkState* Claim = nullptr;
        for (TActorIterator<ASFConstructionNetworkState> It(GetWorld()); It; ++It) { Claim = *It; }
        if (!Builder || !Claim || Claim->GetAnchorCell() == FIntVector::ZeroValue) { return; }
        if (FVector::Dist2D(Pawn->GetActorLocation(), Claim->GetAnchorOffsetCm() + FVector(25, 25, 25)) > 300) { return; }  // not on the pad yet
        const auto* Character = Cast<ACharacter>(Pawn);
        if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetVelocity().IsNearlyZero(1.0)
            || FVector::DistSquared(Pawn->GetActorLocation(), FullLastPosition) > 1.0)
        { FullStableSince = 0; FullLastPosition = Pawn->GetActorLocation(); return; }
        if (FullStableSince == 0) { FullStableSince = Now; }
        if (Now - FullStableSince < 1.0) { return; }
        if (FullAttemptAt == 0)
        {
            // The same placement RPC the cursor sends: shape 0 (cube) on the first claim cell.
            FullAttemptAt = Now;
            Builder->ServerPlace(FIntVector(0, 0, 0), Claim->GetSnapshot().Revision, 0, uint8(SFConstructionGeometry::IdentityRotation()));
            UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_ACTION Objective=%s Input=ServerPlace"), *Key);
        }
        else if (Now - FullAttemptAt > 20) { Fail(TEXT("wall was not accepted in time")); }
        return;
    }
    if (Next->Label == TEXT("Defeat Boar"))
    {
        auto* Boar = NearestBoar(GetWorld(), Pawn->GetActorLocation());
        if (!Boar || FVector::Dist2D(Pawn->GetActorLocation(), Boar->GetActorLocation()) > 500) { return; }
        const FVector ToBoar = (Boar->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
        // The server judges facing from the replicated control rotation: aim on one tick, swing once it has arrived.
        if (FVector::DotProduct(PC->GetControlRotation().Vector().GetSafeNormal2D(), ToBoar) < 0.9) { PC->SetControlRotation(ToBoar.Rotation()); return; }
        if (FullAttemptAt == 0) { FullAttemptAt = Now; }
        if (Now - FullAttemptAt > 90) { Fail(TEXT("boar not defeated in time")); return; }
        if (Now - FullLastSwingAt >= 1.0)
        {
            FullLastSwingAt = Now;
            // PilotAttack is what the left mouse button invokes (and the combat scenario calls); it sends the server RPC.
            if (auto* Fighter = Cast<ASFE1Character>(Pawn)) { Fighter->PilotAttack(); }
            UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_ACTION Objective=%s Input=PilotAttack Boar=%.0fcm"), *Key,
                FVector::Dist2D(Pawn->GetActorLocation(), Boar->GetActorLocation()));
        }
        return;
    }

    ESFGatheringPilotKind Kind = ESFGatheringPilotKind::Oak;
    if (Next->Label == TEXT("Gather Oak Log")) { Kind = ESFGatheringPilotKind::Oak; }
    else if (Next->Label == TEXT("Gather Iron Ore")) { Kind = ESFGatheringPilotKind::Iron; }
    else if (Next->Label == TEXT("Craft Rough Timber")) { Kind = ESFGatheringPilotKind::Sawmill; }
    else if (Next->Label == TEXT("Craft Iron Ingot")) { Kind = ESFGatheringPilotKind::Forge; }
    else { return; }
    auto* Gathering = PC->FindComponentByClass<USFGatheringPilotComponent>();
    ASFGatheringPilotNode* Node = NearestNode(GetWorld(), Kind, Pawn->GetActorLocation());
    if (!Gathering || !Gathering->IsEnabled() || !Node || FVector::Dist2D(Pawn->GetActorLocation(), Node->GetActorLocation()) > 350) { return; }
    if (!Node->IsRefinery() && Node->GetState().Charges <= 0) { return; }  // initial replication of the node
    if (Kind == ESFGatheringPilotKind::Oak || Kind == ESFGatheringPilotKind::Iron)
    {
        if (FullEquipAt == 0)
        {
            if (Kind == ESFGatheringPilotKind::Oak) { Gathering->EquipAxe(); } else { Gathering->EquipPickaxe(); }
            FullEquipAt = Now;
        }
        if (Now - FullEquipAt < 2.0) { return; }  // the equip RPC and replication
    }
    const auto* Character = Cast<ACharacter>(Pawn);
    if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetVelocity().IsNearlyZero(1.0)
        || FVector::DistSquared(Pawn->GetActorLocation(), FullLastPosition) > 1.0)
    { FullStableSince = 0; FullLastPosition = Pawn->GetActorLocation(); return; }
    if (FullStableSince == 0) { FullStableSince = Now; }
    if (Now - FullStableSince < 1.0) { return; }
    if (FullAttemptAt == 0)
    {
        // One interaction per objective: a second one during the channel would cancel it.
        const FVector Eye = Pawn->GetPawnViewLocation();
        Gathering->InteractFromRay(Eye, (Node->GetActorLocation() - Eye).GetSafeNormal());
        FullAttemptAt = Now;
        UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_ACTION Objective=%s Input=InteractFromRay"), *Key);
    }
    else if (Now - FullAttemptAt > 20) { Fail(TEXT("objective did not complete in time")); }
#endif
}

// ---------------------------------------------------------------- tracker widget

void USFQuestTrackerHud::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuestCanvas"));
    WidgetTree->RootWidget = Canvas;
    // Under the minimap: right edge, 25% down.
    USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Box->SetWidthOverride(280.f);
    UCanvasPanelSlot* TrackerSlot = Canvas->AddChildToCanvas(Box);
    TrackerSlot->SetAnchors(FAnchors(1.f, 0.25f)); TrackerSlot->SetAlignment(FVector2D(1.f, 0.f));
    TrackerSlot->SetPosition(FVector2D(-24.f, 0.f)); TrackerSlot->SetAutoSize(true);
    UBorder* Plate = Panel(WidgetTree, Slate(0.88f), Brass(), FMargin(12.f, 9.f), 4.f);
    Box->SetContent(Plate);
    UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Plate->SetContent(Stack);
    TitleText = Text(WidgetTree, TEXT(""), 16, Ivory(), true);
    Stack->AddChildToVerticalBox(TitleText);
    StepText = Text(WidgetTree, TEXT(""), 12, Brass());
    Stack->AddChildToVerticalBox(StepText)->SetPadding(FMargin(0.f, 1.f, 0.f, 4.f));
    USizeBox* Rule = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Rule->SetHeightOverride(1.f); Rule->SetContent(Panel(WidgetTree, Brass(), Brass(), FMargin(0.f), 0.f));
    Stack->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    Stack->AddChildToVerticalBox(Rows);
}

void USFQuestTrackerHud::Present(const FSFQuestView& View)
{
    if (!Rows) { return; }
    FString Key = FString::Printf(TEXT("%s|%s|%d|%d"), *View.Title, *View.Step, View.StepNumber, View.bComplete);
    for (const FSFQuestLine& L : View.Lines) { Key += FString::Printf(TEXT("|%s=%d/%d"), *L.Label, L.Count, L.Target); }
    if (Key == LastKey) { return; }
    LastKey = Key;
    UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_CLIENT_VIEW Step=%d Complete=%d Key=%s"), View.StepNumber, View.bComplete, *Key);
    using namespace SFHudStyle;
    TitleText->SetText(FText::FromString(View.Title));
    StepText->SetText(FText::FromString(View.bComplete ? FString(TEXT("Chain complete")) : FString::Printf(TEXT("Step %d of %d: %s"), View.StepNumber, View.StepTotal, *View.Step)));
    Rows->ClearChildren();
    auto AddRow = [this](const FString& Label, const FString& Count, bool bDone)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        // Tick box: moss-filled when done, empty brass frame otherwise.
        USizeBox* Tick = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Tick->SetWidthOverride(12.f); Tick->SetHeightOverride(12.f);
        Tick->SetContent(Panel(WidgetTree, bDone ? Moss() : Hex(TEXT("0E171C"), 1.f), Brass(), FMargin(0.f), 2.f));
        UHorizontalBoxSlot* TickSlot = Row->AddChildToHorizontalBox(Tick);
        TickSlot->SetVerticalAlignment(VAlign_Center); TickSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Text(WidgetTree, Label, 13, bDone ? Moss() : Ivory()));
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Row->AddChildToHorizontalBox(Text(WidgetTree, Count, 13, bDone ? Moss() : Ivory(), true));
        Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
    };
    for (const FSFQuestLine& L : View.Lines) { AddRow(L.Label, FString::Printf(TEXT("%d/%d"), L.Count, L.Target), L.Count >= L.Target); }
    if (View.bComplete) { AddRow(TEXT("Frontier founded"), TEXT(""), true); }
}

// ---------------------------------------------------------------- subsystem

bool USFQuestPilotSubsystem::IsRequested()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return FParse::Param(FCommandLine::Get(), TEXT("QuestPilot"))
        && (FParse::Param(FCommandLine::Get(), TEXT("GatheringPilot")) || FParse::Param(FCommandLine::Get(), TEXT("CombatPilot")));
#else
    return false;
#endif
}

void USFQuestPilotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client || !IsRequested()
        || World->GetOutermost()->GetName() != TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook")) { return; }
    FSFQuestDefinition Def; FString Error;
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/quests/greenlands_frontier_founder.json")));
    if (!FSFQuestDefinition::Load(Path, Def, Error) || Def.ZoneId != TEXT("zone/greenlands_silverbrook"))
    { UE_LOG(LogTemp, Error, TEXT("QUEST_PILOT_DISABLED %s"), Error.IsEmpty() ? TEXT("zone mismatch") : *Error); return; }
    Quest = MakeShared<FSFQuestDefinition>(MoveTemp(Def));
    GatheredHandle = SFQuestPilotEvents::OnGathered().AddWeakLambda(this, [this](APlayerController* PC, ESFGatheringPilotKind Kind)
    {
        switch (Kind)
        {
        case ESFGatheringPilotKind::Oak: Route(PC, TEXT("gather"), TEXT("item/oak_log"), TEXT("gathering_node/oak_tree"), TEXT("oak")); break;
        case ESFGatheringPilotKind::Iron: Route(PC, TEXT("gather"), TEXT("item/iron_ore"), TEXT("gathering_node/iron_vein"), TEXT("iron")); break;
        case ESFGatheringPilotKind::Sawmill: Route(PC, TEXT("craft"), TEXT("item/rough_timber"), FString(), TEXT("timber")); break;
        case ESFGatheringPilotKind::Forge: Route(PC, TEXT("craft"), TEXT("item/iron_ingot"), FString(), TEXT("ingot")); break;
        default: break;
        }
    });
    PlacedHandle = SFQuestPilotEvents::OnStructurePlaced().AddWeakLambda(this, [this](APlayerController* PC, uint64 Revision)
    { Route(PC, TEXT("build"), TEXT("structure_tag/wall"), FString(), FString::Printf(TEXT("build:%llu"), Revision)); });
    BoarHandle = SFQuestPilotEvents::OnBoarDefeated().AddWeakLambda(this, [this](APlayerController* PC, const FString& DeathId)
    { Route(PC, TEXT("defeat"), TEXT("npc/boar"), FString(), TEXT("boar:") + DeathId); });
    UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_READY Quest=%s Steps=%d"), *Quest->Id, Quest->Steps.Num());
}

void USFQuestPilotSubsystem::Deinitialize()
{
    SFQuestPilotEvents::OnGathered().Remove(GatheredHandle);
    SFQuestPilotEvents::OnStructurePlaced().Remove(PlacedHandle);
    SFQuestPilotEvents::OnBoarDefeated().Remove(BoarHandle);
    if (Hud) { Hud->RemoveFromParent(); Hud = nullptr; }
    Super::Deinitialize();
}

void USFQuestPilotSubsystem::Route(APlayerController* PC, const FString& Kind, const FString& Target, const FString& Node, const FString& Operation)
{
    auto* Component = IsValid(PC) && PC->GetWorld() == GetWorld() ? PC->FindComponentByClass<USFQuestPilotComponent>() : nullptr;
    if (!Component) { return; }
    // Gather/craft broadcasts carry no id of their own; a per-controller sequence keeps each accepted grant distinct.
    const bool bHasOwnId = Operation.Contains(TEXT(":"));
    Component->Observe(Kind, Target, Node, bHasOwnId ? Operation : FString::Printf(TEXT("%s:%d"), *Operation, Component->GetProgress().Operations.Num()));
}

void USFQuestPilotSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World) { return; }
    if (World->GetNetMode() != NM_Client && Quest)
    {
        for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (IsValid(PC) && !PC->FindComponentByClass<USFQuestPilotComponent>())
            {
                auto* Component = NewObject<USFQuestPilotComponent>(PC, TEXT("QuestPilot"));
                Component->Init(Quest);
                Component->RegisterComponent();
            }
        }
    }
    APlayerController* Local = World->GetFirstPlayerController();
    auto* Component = Local && Local->IsLocalController() ? Local->FindComponentByClass<USFQuestPilotComponent>() : nullptr;
    if (!Component || Component->GetView().Title.IsEmpty()) { return; }
    if (!Hud)
    {
        Hud = CreateWidget<USFQuestTrackerHud>(Local, USFQuestTrackerHud::StaticClass());
        if (!Hud) { return; }
        Hud->AddToViewport(14);
        Hud->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    Hud->Present(Component->GetView());
}
