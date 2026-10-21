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
#include "Services/SFBackpackComponent.h"
#include "Services/SFCharacterSession.h"
#include "Services/SFGameplayServicesClient.h"
#include "Gathering/SFStationRecipes.h"
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
#include "Townsfolk/SFTownsfolk.h"
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

    bool QuestValidId(const FString& Id, const TCHAR* Prefix)
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
    if (!QuestValidId(Q.Id, TEXT("quest")) || Q.Revision < 1) { Error = TEXT("quest id/revision invalid"); return false; }
    if (!QuestValidId(Q.ZoneId, TEXT("zone"))) { Error = TEXT("zone_id invalid"); return false; }
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
            if (!*Prefix || !QuestValidId(Obj.TargetId, Prefix)) { Error = FString::Printf(TEXT("step %s/%s: kind/target invalid"), *Step.Id, *Obj.Key); return false; }
            if (Obj.bHasNode && (Obj.Kind != TEXT("gather") || !QuestValidId(Obj.NodeId, TEXT("gathering_node")))) { Error = FString::Printf(TEXT("step %s/%s: node_id invalid"), *Step.Id, *Obj.Key); return false; }
            if (Obj.Count < 1 || Obj.Count > MaxObjectiveCount) { Error = FString::Printf(TEXT("step %s/%s: count out of range"), *Step.Id, *Obj.Key); return false; }
            Step.Objectives.Add(Obj);
        }
        const TArray<TSharedPtr<FJsonValue>>* Rewards = nullptr;
        if (StepObj->TryGetArrayField(TEXT("rewards"), Rewards))
        {
            for (const auto& RV : *Rewards)
            {
                FString Bundle; const TSharedPtr<FJsonObject> R = RV ? RV->AsObject() : nullptr;
                if (!R || !R->TryGetStringField(TEXT("bundle_id"), Bundle) || !QuestValidId(Bundle, TEXT("reward")) || Step.Rewards.Contains(Bundle))
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
    if (USFBackpackComponent::Services())
    {
        // INT-04: the Progression service decides; the local model only mirrors accepted replies (decision 5).
        ServiceQueue.Add({Kind, TargetId, NodeId, OperationId});
        PumpServices();
        return true;
    }
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

void USFQuestPilotComponent::PumpServices()
{
    auto* PC = Cast<APlayerController>(GetOwner());
    USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(PC);
    if (bServiceBusy || ServiceQueue.IsEmpty() || !Pack) { return; }
    bServiceBusy = true;
    const FServiceEvent Event = ServiceQueue[0];
    // Receipt kinds: harvest, craft and defeat carry a progression payload; building is quest evidence only.
    FSFGameplayRequest Q; Q.Rpc = ESFGameplayRpc::ApplyReceipt; Q.OperationId = Event.OperationId; Q.CharacterId = Pack->GetCharacterId();
    Q.Kind = Event.Kind == TEXT("build") ? FString(TEXT("quest")) : Event.Kind;
    Q.ExpectedProgressionRevision = ProgressionRevision; Q.ExpectedQuestRevision = QuestRevision;
    if (Event.Kind == TEXT("gather"))
    {
        Q.SkillId = Event.TargetId == TEXT("item/oak_log") ? TEXT("skill/logging") : TEXT("skill/mining");
        Q.Difficulty = 1; Q.ActionSeconds = GetDefault<USFGatheringPilotSettings>()->HarvestSeconds; Q.ContextHash = GetTypeHash(Event.NodeId);
    }
    else if (Event.Kind == TEXT("craft"))
    { Q.SkillId = TEXT("skill/smithing"); Q.Difficulty = 1; Q.ActionSeconds = GetDefault<USFGatheringPilotSettings>()->RefineSeconds; Q.ContextHash = GetTypeHash(Event.TargetId); }
    else if (Event.Kind == TEXT("defeat")) { Q.EnemyLevel = 2; Q.EnemyTierMultiplier = 1; Q.PartySize = 1; }
    if (!Progress.IsComplete(*Quest))
    {
        // Quest evidence names the server's current step; an event for another step is simply not accepted.
        LastOccurredMs = FMath::Max(LastOccurredMs + 1, FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond());
        Q.bHasQuest = true; Q.QuestId = Quest->Id; Q.StepId = Quest->Steps[Progress.StepIndex].Id; Q.QuestKind = Event.Kind;
        Q.TargetId = Event.TargetId; Q.NodeId = Event.NodeId; Q.Quantity = 1; Q.OccurredAtUnixMs = LastOccurredMs;
    }
    USFBackpackComponent::Services()->Call(Q, [Weak = TWeakObjectPtr<USFQuestPilotComponent>(this), Event](const FSFGameplayResponse& R)
    {
        USFQuestPilotComponent* Self = Weak.Get();
        if (!Self) { return; }
        Self->ServiceQueue.RemoveAt(0);
        Self->bServiceBusy = false;
        if (R.Status == ESFGameplayStatus::Ok)
        {
            if (R.ProgressionRevision > 0) { Self->ProgressionRevision = R.ProgressionRevision; }
            if (R.QuestRevision > 0) { Self->QuestRevision = R.QuestRevision; }
            if (R.bQuestAccepted)
            {
                // Mirror the accepted evidence for the tracker counts, then take the server's step as the truth.
                TArray<FString> Ignored;
                Self->Progress.Apply(*Self->Quest, Event.Kind, Event.TargetId, Event.NodeId, 1, Event.OperationId, Ignored);
                const int32 Active = R.bQuestCompleted ? Self->Quest->Steps.Num()
                    : Self->Quest->Steps.IndexOfByPredicate([&](const FSFQuestStep& S) { return S.Id == R.ActiveStepId; });
                if (Active != INDEX_NONE && Active != Self->Progress.StepIndex)
                {
                    UE_LOG(LogTemp, Warning, TEXT("QUEST_SERVICE_STEP_CORRECTED Local=%d Server=%d"), Self->Progress.StepIndex, Active);
                    Self->Progress.StepIndex = Active; Self->Progress.Counts.Reset();
                }
                Self->RebuildView();
            }
            UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_EVENT Kind=%s Target=%s Op=%s Result=%d Step=%d Rewards=%d Source=service Active=%s"),
                *Event.Kind, *Event.TargetId, *Event.OperationId, R.bQuestAccepted ? 0 : 2, Self->Progress.StepIndex, R.RewardBundleIds.Num(), *R.ActiveStepId);
            for (const FString& Bundle : R.RewardBundleIds) { UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_REWARD Bundle=%s Source=service"), *Bundle); }
            if (R.bQuestCompleted && R.bQuestAccepted) { UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_COMPLETE Quest=%s Source=service"), *Self->Quest->Id); }
            for (const TPair<FString, int64>& Item : R.GrantedItems)
            {
                UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_PAYOUT Item=%s Quantity=%lld"), *Item.Key, Item.Value);
                Self->RewardLines.AddUnique(FString::Printf(TEXT("%lld %s"), Item.Value, *SFStationRecipes::ItemDisplayName(Item.Key)));
            }
            // The granted lines are the committed payout; without them (payout still pending) re-read the backpack.
            if (USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner())))
            { if (!R.GrantedItems.IsEmpty()) { Pack->ApplyToBag(R.GrantedItems); } else if (!R.RewardBundleIds.IsEmpty()) { Pack->Reconcile(); } }
            if (!R.GrantedItems.IsEmpty()) { Self->RebuildView(); }
        }
        else if (R.Status == ESFGameplayStatus::Rejected && R.GrpcCode == 9 && !Event.bRetried)
        {
            // Stale progression or quest revision: read the character state and retry once with the same source id.
            // The same id matters here: progression and quest are separate receipts, so a retry must replay, not re-apply.
            USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner()));
            FSFGameplayRequest Get; Get.Rpc = ESFGameplayRpc::GetCharacterState; Get.CharacterId = Pack ? Pack->GetCharacterId() : FString();
            USFBackpackComponent::Services()->Call(Get, [Weak, Event](const FSFGameplayResponse& State)
            {
                USFQuestPilotComponent* Again = Weak.Get();
                if (!Again) { return; }
                if (State.Status == ESFGameplayStatus::Ok) { Again->ProgressionRevision = State.ProgressionRevision; Again->QuestRevision = State.QuestRevision; }
                FServiceEvent Retry = Event; Retry.bRetried = true;
                Again->ServiceQueue.Insert(Retry, 0);
                Again->PumpServices();
            });
            return;
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_EVENT Kind=%s Target=%s Op=%s Result=3 Step=%d Rewards=0 Source=service Code=%d"),
                *Event.Kind, *Event.TargetId, *Event.OperationId, Self->Progress.StepIndex, R.GrpcCode);
        }
        Self->PumpServices();
    });
}

void USFQuestPilotComponent::Restore(const FString& ActiveStepId, bool bCompleted, const TMap<FString,int32>& Counts, int64 InProgressionRevision, int64 InQuestRevision)
{
    if (!Quest) { return; }
    const int32 Found = Quest->Steps.IndexOfByPredicate([&](const FSFQuestStep& Step) { return Step.Id == ActiveStepId; });
    Progress = FSFQuestProgress();
    Progress.StepIndex = bCompleted ? Quest->Steps.Num() : FMath::Max(Found, 0);
    if (!bCompleted && Found != INDEX_NONE) { Progress.Counts = Counts; }
    for (int32 Index = 0; Index < Progress.StepIndex; ++Index) { Progress.GrantedSteps.Add(Quest->Steps[Index].Id); }
    ProgressionRevision = InProgressionRevision; QuestRevision = InQuestRevision;
    UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_RESTORED Quest=%s Step=%d Active=%s Counts=%d Completed=%d"), *Quest->Id, Progress.StepIndex, *ActiveStepId, Progress.Counts.Num(), bCompleted);
    RebuildView();
}

void USFQuestPilotComponent::RebuildView()
{
    if (!Quest) { return; }
    FSFQuestView Next;
    Next.Title = Humanize(Quest->Id); Next.StepTotal = Quest->Steps.Num();
    Next.bComplete = Progress.IsComplete(*Quest);
    Next.RewardLines = RewardLines;
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
#if !UE_BUILD_SHIPPING
    // Stub objective kind "talk_resident", off unless -QuestResidentStub: the first quest player holds one census labourer
    // through the townsfolk hand-off (AI Townsfolk doc section 9 step 4). No content uses the kind yet.
    static bool bResidentStub = FParse::Param(FCommandLine::Get(), TEXT("QuestResidentStub"));
    if (bResidentStub && GetOwner() && GetOwner()->HasAuthority()) { if (USFTownsfolkSubsystem* Town = GetWorld()->GetSubsystem<USFTownsfolkSubsystem>(); Town && Town->RequestQuestHandoff(TEXT("labourer"), GetOwner()->GetActorLocation())) { bResidentStub = false; } }
#endif
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

    // What the driver does next: work a node or station (with a recipe), place the wall, or fight the boar.
    enum class EFullAction : uint8 { None, Node, Build, Boar };
    struct FFullTarget
    {
        EFullAction Action = EFullAction::None;
        ESFGatheringPilotKind Kind = ESFGatheringPilotKind::Oak;
        FString Recipe;
        FString Describe() const
        {
            return Action == EFullAction::Build ? TEXT("wall") : Action == EFullAction::Boar ? TEXT("boar")
                : Action == EFullAction::Node ? FString::Printf(TEXT("%s%s%s"), *UEnum::GetValueAsString(Kind), Recipe.IsEmpty() ? TEXT("") : TEXT(":"), *Recipe) : TEXT("none");
        }
    };

    FFullTarget Node(ESFGatheringPilotKind Kind, const TCHAR* Recipe = TEXT("")) { FFullTarget T; T.Action = EFullAction::Node; T.Kind = Kind; T.Recipe = Recipe; return T; }

    int64 BagQuantity(const AActor* Owner, const TCHAR* Item)
    {
#if SF_WITH_MINIMAL_INVENTORY
        auto* Bag = Owner ? Owner->FindComponentByClass<UInventoryComponent>() : nullptr;
        return Bag ? UInventoryTransactionLibrary::GetItemQuantity(Bag, TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Item)))) : 0;
#else
        return 0;
#endif
    }

    // Economy mode (gameplay services on): a wall costs a wall bundle, so walk back from the bundle recipe to what the bag lacks.
    FString BagLine(const AActor* Owner)
    {
        return FString::Printf(TEXT("oak=%lld timber=%lld ore=%lld ingot=%lld nails=%lld bundle=%lld"), BagQuantity(Owner, TEXT("item/oak_log")),
            BagQuantity(Owner, TEXT("item/rough_timber")), BagQuantity(Owner, TEXT("item/iron_ore")), BagQuantity(Owner, TEXT("item/iron_ingot")),
            BagQuantity(Owner, TEXT("item/iron_nails")), BagQuantity(Owner, TEXT("item/wall_material_bundle")));
    }

    FFullTarget WallEconomy(const AActor* Owner)
    {
        const FSFStationRecipe* Bundle = SFStationRecipes::Find(TEXT("recipe/wall_material_bundle"));
        if (!Bundle || BagQuantity(Owner, TEXT("item/wall_material_bundle")) >= 1) { FFullTarget T; T.Action = EFullAction::Build; return T; }
        int64 Timber = 0, Nails = 0;
        for (const FSFItemStack& Input : Bundle->Inputs)
        { (Input.DefinitionId == TEXT("item/rough_timber") ? Timber : Nails) = Input.Quantity; }
        const bool bTimber = BagQuantity(Owner, TEXT("item/rough_timber")) >= Timber, bNails = BagQuantity(Owner, TEXT("item/iron_nails")) >= Nails;
        if (bTimber && bNails) { return Node(ESFGatheringPilotKind::Workbench, TEXT("recipe/wall_material_bundle")); }
        if (!bNails)
        {
            if (BagQuantity(Owner, TEXT("item/iron_ingot")) >= 1) { return Node(ESFGatheringPilotKind::Forge, TEXT("recipe/iron_nails")); }
            if (BagQuantity(Owner, TEXT("item/iron_ore")) >= 1) { return Node(ESFGatheringPilotKind::Forge, TEXT("recipe/iron_ingot")); }
            return Node(ESFGatheringPilotKind::Iron);
        }
        return BagQuantity(Owner, TEXT("item/oak_log")) >= 1 ? Node(ESFGatheringPilotKind::Sawmill, TEXT("recipe/rough_timber")) : Node(ESFGatheringPilotKind::Oak);
    }

    FFullTarget Resolve(const FString& Kind, const FString& Target, const AActor* Owner, bool bEconomy)
    {
        if (Kind == TEXT("gather")) { return Node(Target == TEXT("item/oak_log") ? ESFGatheringPilotKind::Oak : ESFGatheringPilotKind::Iron); }
        if (Kind == TEXT("craft"))
        { return Target == TEXT("item/rough_timber") ? Node(ESFGatheringPilotKind::Sawmill, TEXT("recipe/rough_timber")) : Node(ESFGatheringPilotKind::Forge, TEXT("recipe/iron_ingot")); }
        if (Kind == TEXT("build")) { if (bEconomy) { return WallEconomy(Owner); } FFullTarget T; T.Action = EFullAction::Build; return T; }
        if (Kind == TEXT("defeat")) { FFullTarget T; T.Action = EFullAction::Boar; return T; }
        return FFullTarget();
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

    const ASFConstructionNetworkState* FindClaim(UWorld* World)
    {
        // The zone's first build plot (lowest claim id), the same on server and client.
        const ASFConstructionNetworkState* Claim = nullptr;
        for (TActorIterator<ASFConstructionNetworkState> It(World); It; ++It)
        { if (It->GetAnchorCell() != FIntVector::ZeroValue && (!Claim || It->GetClaimId() < Claim->GetClaimId())) { Claim = *It; } }
        return Claim;
    }

    bool HeldStill(const APawn* Pawn, FVector& LastPosition, double& StableSince, double Now)
    {
        const auto* Character = Cast<ACharacter>(Pawn);
        if (!Character || !Character->GetCharacterMovement()->IsMovingOnGround() || !Character->GetVelocity().IsNearlyZero(1.0)
            || FVector::DistSquared(Pawn->GetActorLocation(), LastPosition) > 1.0)
        { StableSince = 0; LastPosition = Pawn->GetActorLocation(); return false; }
        if (StableSince == 0) { StableSince = Now; }
        return Now - StableSince >= 1.0;
    }
}

void USFQuestPilotComponent::TickFullServer()
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const auto* PC = Cast<APlayerController>(GetOwner());
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    const FSFQuestObjective* Objective = Quest ? NextObjective(*Quest, Progress) : nullptr;
    // A restoring character keeps its restored position; the fixture places it once progress is known.
    if (!Pawn || !Objective || USFCharacterSessionComponent::IsRestoring(PC)) { return; }
    // Nodes: the one nearest the world origin (oak, iron) or the only station; the claim pad for the wall; the nearest boar.
    const FFullTarget Next = Resolve(Objective->Kind, Objective->TargetId, PC, USFBackpackComponent::Services().IsValid());
    FVector Target = FVector::ZeroVector, Where = FVector::ZeroVector;
    if (Next.Action == EFullAction::Node)
    {
        const auto* NodeActor = NearestNode(GetWorld(), Next.Kind, FVector::ZeroVector);
        if (!NodeActor) { return; }
        Target = NodeActor->GetActorLocation();
        Where = Target + (-Target * FVector(1, 1, 0)).GetSafeNormal() * 200 + FVector(0, 0, 80);
    }
    else if (Next.Action == EFullAction::Boar)
    {
        const auto* Boar = NearestBoar(GetWorld(), Pawn->GetActorLocation());
        if (!Boar) { return; }
        Target = Boar->GetActorLocation();
        Where = Target + (-Target * FVector(1, 1, 0)).GetSafeNormal() * 200 + FVector(0, 0, 80);
    }
    else if (Next.Action == EFullAction::Build)
    {
        // The dev claim pad: stand on it beside the first cell (BLD-02 anchored construction).
        const ASFConstructionNetworkState* Claim = FindClaim(GetWorld());
        if (!Claim) { return; }
        Target = Claim->GetAnchorOffsetCm() + FVector(25, 25, 25);
        Where = Target + FVector(-175, -125, 95);
    }
    else { return; }
    // Players sharing a target stand side by side, not inside each other (two-client reconnect runner).
    int32 Index = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It && It->Get() != PC; ++It) { ++Index; }
    Where += FVector::CrossProduct((Target - Where).GetSafeNormal2D(), FVector::UpVector) * (Index % 3) * 110.0;
    const FString Key = Quest->Steps[Progress.StepIndex].Id + TEXT("/") + Objective->Key + TEXT("/") + Next.Describe();
    if (Key == FullTeleportKey && FVector::Dist2D(Pawn->GetActorLocation(), Target) < 600) { return; }
    // Re-place after a respawn or a boar that wandered off; otherwise once per target.
    Pawn->SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
    FullTeleportKey = Key;
    UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_PLACED Objective=%s Bag=%s"), *Key, *BagLine(PC));
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
    const FSFQuestLine* Line = View.Lines.FindByPredicate([](const FSFQuestLine& L) { return L.Count < L.Target; });
    if (!Line) { return; }
    // The tracker line names the objective; recover its content ids from the same label rule the server used.
    FString Kind, Target;
    static const TCHAR* const Objectives[][2] = { {TEXT("gather"), TEXT("item/oak_log")}, {TEXT("gather"), TEXT("item/iron_ore")},
        {TEXT("craft"), TEXT("item/rough_timber")}, {TEXT("craft"), TEXT("item/iron_ingot")}, {TEXT("build"), TEXT("structure_tag/wall")}, {TEXT("defeat"), TEXT("npc/boar")} };
    for (const auto& Pair : Objectives)
    { if (Line->Label == Verb(Pair[0]) + TEXT(" ") + Humanize(Pair[1])) { Kind = Pair[0]; Target = Pair[1]; } }
    const FFullTarget Next = Resolve(Kind, Target, PC, FParse::Param(FCommandLine::Get(), TEXT("QuestFullEconomy")));
    const FString Key = View.Step + TEXT("/") + Line->Label + TEXT("/") + Next.Describe();
    if (Key != FullAttemptKey)
    { UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_TARGET Objective=%s Bag=%s"), *Key, *BagLine(PC)); FullAttemptKey = Key; FullEquipAt = 0; FullStableSince = 0; FullAttemptAt = 0; FullLastSwingAt = 0; FullAttempts = 0; }
    const auto Fail = [&](const TCHAR* Why)
    { UE_LOG(LogTemp, Error, TEXT("QUEST_FULL_FAILED Objective=%s Why=%s"), *Key, Why); FPlatformMisc::RequestExitWithStatus(false, 1); };
    // An attempt that produced nothing (for example a host-down refusal) is retried; a few in a row fail the run.
    const auto Expired = [&](double Limit) -> bool
    {
        if (FullAttemptAt == 0 || Now - FullAttemptAt <= Limit) { return false; }
        if (++FullAttempts >= 4) { Fail(TEXT("objective did not complete in time")); return true; }
        UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_RETRY Objective=%s Attempt=%d"), *Key, FullAttempts + 1);
        FullAttemptAt = 0; FullStableSince = 0; FullEquipAt = 0;
        return true;
    };

    if (Next.Action == EFullAction::Build)
    {
        auto* Builder = Cast<ASFConstructionNetworkController>(PC);
        const ASFConstructionNetworkState* Claim = FindClaim(GetWorld());
        if (!Builder || !Claim) { return; }
        if (FVector::Dist2D(Pawn->GetActorLocation(), Claim->GetAnchorOffsetCm() + FVector(25, 25, 25)) > 300) { return; }  // not on the pad yet
        if (!HeldStill(Pawn, FullLastPosition, FullStableSince, Now) || Expired(20)) { return; }
        if (FullAttemptAt == 0)
        {
            // The same placement RPC the cursor sends: shape 0 (cube) on the first claim cell.
            FullAttemptAt = Now;
            Builder->ServerPlace(FIntVector(0, 0, 0), Claim->GetSnapshot().Revision, 0, uint8(SFConstructionGeometry::IdentityRotation()), Claim->GetClaimId());
            UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_ACTION Objective=%s Input=ServerPlace"), *Key);
        }
        return;
    }
    if (Next.Action == EFullAction::Boar)
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
    if (Next.Action != EFullAction::Node) { return; }
    auto* Gathering = PC->FindComponentByClass<USFGatheringPilotComponent>();
    ASFGatheringPilotNode* NodeActor = NearestNode(GetWorld(), Next.Kind, Pawn->GetActorLocation());
    if (!Gathering || !Gathering->IsEnabled() || !NodeActor || FVector::Dist2D(Pawn->GetActorLocation(), NodeActor->GetActorLocation()) > 350) { return; }
    if (!NodeActor->IsRefinery() && NodeActor->GetState().Charges <= 0) { return; }  // initial replication of the node
    if (FullEquipAt == 0)
    {
        // Tools for resources; the recipe choice for a station (the same RPC the C key sends).
        if (Next.Kind == ESFGatheringPilotKind::Oak) { Gathering->EquipAxe(); }
        else if (Next.Kind == ESFGatheringPilotKind::Iron) { Gathering->EquipPickaxe(); }
        else if (!Next.Recipe.IsEmpty()) { Gathering->SelectStationRecipe(Next.Kind, Next.Recipe); }
        FullEquipAt = Now;
    }
    if (Now - FullEquipAt < 2.0) { return; }  // the equip or selection RPC and its replication
    if (!Next.Recipe.IsEmpty() && Gathering->GetStationRecipe(Next.Kind) != Next.Recipe) { return; }
    if (!HeldStill(Pawn, FullLastPosition, FullStableSince, Now) || Expired(20)) { return; }
    if (FullAttemptAt == 0)
    {
        // One interaction per attempt: a second one during the channel would cancel it.
        const FVector Eye = Pawn->GetPawnViewLocation();
        Gathering->InteractFromRay(Eye, (NodeActor->GetActorLocation() - Eye).GetSafeNormal());
        FullAttemptAt = Now;
        UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_ACTION Objective=%s Input=InteractFromRay"), *Key);
    }
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
    TrackerSlot->SetAnchors(FAnchors(0.84f, 0.25f, 0.99f, 0.40f)); TrackerSlot->SetOffsets(FMargin(0.f));  // hud-layout-v4: 84-99% x 25-40%
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
    for (const FString& Reward : View.RewardLines) { Key += TEXT("|") + Reward; }
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
    for (const FString& Reward : View.RewardLines) { AddRow(TEXT("Received ") + Reward, TEXT(""), true); }
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
    GatheredHandle = SFQuestPilotEvents::OnGathered().AddWeakLambda(this, [this](APlayerController* PC, ESFGatheringPilotKind Kind, const FString& Output, const FString& Operation)
    {
        // A station commits its recipe's output (craft evidence); a resource node its harvest (gather evidence).
        const bool bStation = !USFGatheringPilotComponent::StationName(Kind).IsEmpty();
        const FString Node = Kind == ESFGatheringPilotKind::Oak ? TEXT("gathering_node/oak_tree") : Kind == ESFGatheringPilotKind::Iron ? TEXT("gathering_node/iron_vein") : TEXT("");
        FString Short; Output.Split(TEXT("/"), nullptr, &Short);
        Route(PC, bStation ? TEXT("craft") : TEXT("gather"), Output, Node, Operation.IsEmpty() ? Short : Operation);
    });
    PlacedHandle = SFQuestPilotEvents::OnStructurePlaced().AddWeakLambda(this, [this](APlayerController* PC, uint64 Revision, const FString& Operation)
    { Route(PC, TEXT("build"), TEXT("structure_tag/wall"), FString(), Operation.IsEmpty() ? FString::Printf(TEXT("build:%llu"), Revision) : Operation); });
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
    const bool bHasOwnId = Operation.Contains(TEXT(":")) || Operation.Contains(TEXT("/"));  // "boar:<death>", "gather/<guid>"
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
