#include "Quest/SFQuestPilot.h"
#include "Framework/SFPlayerController.h"

#include "Quest/SFQuestBoard.h"
#include "Quest/SFQuestPilotEvents.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Characters/SFE1Character.h"
#include "Dialogue/SFDialogue.h"
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
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "Framework/SFHudStyle.h"
#include "Framework/SFPlayerController.h"
#include "UI/SFHudWidgets.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Net/UnrealNetwork.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Townsfolk/SFTownsfolk.h"
#include "Inventory/SFInventory.h"

SFQuestPilotEvents::FGathered& SFQuestPilotEvents::OnGathered() { static FGathered D; return D; }
SFQuestPilotEvents::FStructurePlaced& SFQuestPilotEvents::OnStructurePlaced() { static FStructurePlaced D; return D; }
SFQuestPilotEvents::FBoarDefeated& SFQuestPilotEvents::OnBoarDefeated() { static FBoarDefeated D; return D; }

namespace
{
    constexpr int32 MaxObjectiveCount = 500;  // QuestTuning default; same bounds as the backend model
    constexpr int32 MaxOfferLinks = 8;        // boards[] and requires_completed[] (QST-02 content)

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

    FString Stem(const FString& Id) { FString Tail; return Id.Split(TEXT("/"), nullptr, &Tail) ? Tail : Id; }

    bool IsProgressionKind(const FString& Kind) { return Kind == TEXT("gather") || Kind == TEXT("craft") || Kind == TEXT("defeat"); }

    bool ReadIds(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const TCHAR* Prefix, TArray<FString>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Object->TryGetArrayField(Field, Values)) { return true; }
        if (Values->Num() > MaxOfferLinks) { return false; }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const FString Id = Value ? Value->AsString() : FString();
            if (!QuestValidId(Id, Prefix) || Out.Contains(Id)) { return false; }
            Out.Add(Id);
        }
        return true;
    }
}

FString FSFQuestDefinition::Title() const { return bHasOffer && !Offer.Title.IsEmpty() ? Offer.Title : Humanize(Id); }

TArray<FString> FSFQuestDefinition::RewardLines() const
{
    TArray<FString> Lines;
    for (const FSFQuestStep& Step : Steps)
    {
        for (const FSFQuestRewardItem& Item : Step.RewardItems)
        { Lines.Add(FString::Printf(TEXT("%lld %s"), Item.Quantity, *SFStationRecipes::ItemDisplayName(Item.DefinitionId))); }
    }
    return Lines;
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
    const TSharedPtr<FJsonObject>* OfferObject = nullptr;
    if (Root->TryGetObjectField(TEXT("offer"), OfferObject))
    {
        Q.bHasOffer = true; FSFQuestOffer& O = Q.Offer;
        (*OfferObject)->TryGetStringField(TEXT("title"), O.Title); (*OfferObject)->TryGetStringField(TEXT("summary"), O.Summary);
        (*OfferObject)->TryGetStringField(TEXT("art"), O.Art); (*OfferObject)->TryGetStringField(TEXT("giver_npc"), O.GiverNpc);
        if (O.Title.IsEmpty() || O.Summary.IsEmpty()) { Error = TEXT("offer title/summary missing"); return false; }
        if (!ReadIds(*OfferObject, TEXT("boards"), TEXT("board"), O.Boards)) { Error = TEXT("offer boards invalid"); return false; }
        if (!ReadIds(*OfferObject, TEXT("requires_completed"), TEXT("quest"), O.RequiresCompleted) || O.RequiresCompleted.Contains(Q.Id)) { Error = TEXT("offer requires_completed invalid"); return false; }
        if (!O.GiverNpc.IsEmpty() && !QuestValidId(O.GiverNpc, TEXT("npc"))) { Error = TEXT("offer giver_npc invalid"); return false; }
        if (O.Boards.IsEmpty() && O.GiverNpc.IsEmpty()) { Error = TEXT("offer unreachable: no board and no giver"); return false; }
    }
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
                : Obj.Kind == TEXT("defeat") || Obj.Kind == TEXT("talk_npc") ? TEXT("npc") : Obj.Kind == TEXT("build") ? TEXT("structure_tag")
                : Obj.Kind == TEXT("talk_resident") ? TEXT("resident_role") : TEXT("");
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
                const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
                if (R->TryGetArrayField(TEXT("items"), Items))
                {
                    for (const auto& IV : *Items)
                    {
                        const TSharedPtr<FJsonObject> I = IV ? IV->AsObject() : nullptr;
                        FSFQuestRewardItem Item; double Quantity = 0;
                        if (!I || !I->TryGetStringField(TEXT("definition_id"), Item.DefinitionId) || !I->TryGetNumberField(TEXT("quantity"), Quantity)
                            || !QuestValidId(Item.DefinitionId, TEXT("item")) || Quantity < 1 || Quantity > 1000000)
                        { Error = FString::Printf(TEXT("step %s: reward items invalid"), *Step.Id); return false; }
                        Item.Quantity = int64(Quantity);
                        Step.RewardItems.Add(Item);
                    }
                }
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

const FSFQuestObjective* FSFQuestProgress::Wants(const FSFQuestDefinition& Quest, const FString& Kind, const FString& TargetId, const FString& NodeId) const
{
    if (IsComplete(Quest)) { return nullptr; }
    const FSFQuestObjective* Match = Quest.Steps[StepIndex].Objectives.FindByPredicate([&](const FSFQuestObjective& O)
    { return O.Kind == Kind && O.TargetId == TargetId && (!O.bHasNode || O.NodeId == NodeId); });
    return Match && Counts.FindRef(Match->Key) < Match->Count ? Match : nullptr;
}

ESFQuestApply FSFQuestProgress::Apply(const FSFQuestDefinition& Quest, const FString& Kind, const FString& TargetId, const FString& NodeId,
    int32 Quantity, const FString& OperationId, TArray<FString>& NewRewards)
{
    if (IsComplete(Quest)) { return ESFQuestApply::Completed; }
    const FSFQuestStep& Step = Quest.Steps[StepIndex];
    if (OperationId.IsEmpty() || Operations.Contains(OperationId)) { return ESFQuestApply::Duplicate; }
    if (Quantity < 1) { return ESFQuestApply::Mismatch; }
    const FSFQuestObjective* Match = Wants(Quest, Kind, TargetId, NodeId);
    if (!Match) { return ESFQuestApply::Mismatch; }
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

// ---------------------------------------------------------------- catalog

const FSFQuestCatalog& FSFQuestCatalog::Get()
{
    static const FSFQuestCatalog Catalog = []
    {
        FSFQuestCatalog Out;
        const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/quests")));
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(Folder, TEXT("*.json")), true, false);
        Files.Sort();
        for (const FString& File : Files)
        {
            FSFQuestDefinition Def; FString Error;
            if (!FSFQuestDefinition::Load(FPaths::Combine(Folder, File), Def, Error)) { UE_LOG(LogTemp, Error, TEXT("QUEST_CONTENT_REJECTED %s: %s"), *File, *Error); continue; }
            Out.Quests.Add(MakeShared<const FSFQuestDefinition>(MoveTemp(Def)));
        }
        UE_LOG(LogTemp, Display, TEXT("QUEST_CATALOG_LOADED Quests=%d"), Out.Quests.Num());
        return Out;
    }();
    return Catalog;
}

TSharedPtr<const FSFQuestDefinition> FSFQuestCatalog::Find(const FString& Id) const
{
    const TSharedPtr<const FSFQuestDefinition>* Hit = Quests.FindByPredicate([&](const TSharedPtr<const FSFQuestDefinition>& Q) { return Q->Id == Id; });
    return Hit ? *Hit : nullptr;
}

FString FSFQuestCatalog::ObjectiveLabel(const FSFQuestObjective& O)
{
    if (O.Kind == TEXT("gather") || O.Kind == TEXT("craft"))
    {
        FString Name = SFStationRecipes::ItemDisplayName(O.TargetId);
        if (O.Count > 1 && !Name.EndsWith(TEXT("s"))) { Name += TEXT("s"); }
        return (O.Kind == TEXT("gather") ? TEXT("Gather ") : TEXT("Craft ")) + Name;
    }
    if (O.Kind == TEXT("talk_npc"))
    {
        // The NPC's own dialogue names it ("Millwarden Oswin").
        static TMap<FString, FString> Speakers;
        if (const FString* Hit = Speakers.Find(O.TargetId)) { return TEXT("Talk to ") + *Hit; }
        FSFDialogueTree Tree; FString Error;
        const FString Name = FSFDialogueTree::Load(Stem(O.TargetId) + TEXT(".json"), Tree, Error) ? Tree.Speaker : Humanize(O.TargetId);
        return TEXT("Talk to ") + Speakers.Add(O.TargetId, Name);
    }
    if (O.Kind == TEXT("talk_resident")) { return TEXT("Talk to the ") + Humanize(O.TargetId); }
    return (O.Kind == TEXT("build") ? TEXT("Build ") : TEXT("Defeat ")) + Humanize(O.TargetId);
}

ESFQuestOfferState FSFQuestCatalog::OfferState(const FSFQuestDefinition& Quest, const TSet<FString>& Active, const TSet<FString>& Done, FString* Reason) const
{
    if (Active.Contains(Quest.Id)) { if (Reason) { *Reason = TEXT("In your journal"); } return ESFQuestOfferState::Active; }
    if (Done.Contains(Quest.Id)) { if (Reason) { *Reason = TEXT("Completed"); } return ESFQuestOfferState::Completed; }
    for (const FString& Needed : Quest.Offer.RequiresCompleted)
    {
        if (Done.Contains(Needed)) { continue; }
        if (Reason) { const TSharedPtr<const FSFQuestDefinition> First = Find(Needed); *Reason = FString::Printf(TEXT("Finish %s first"), First ? *First->Title() : *Humanize(Needed)); }
        return ESFQuestOfferState::Locked;
    }
    if (Reason) { Reason->Reset(); }
    return ESFQuestOfferState::Available;
}

TArray<TSharedPtr<const FSFQuestDefinition>> FSFQuestCatalog::ForBoard(const FString& BoardId, int32 Max) const
{
    TArray<TSharedPtr<const FSFQuestDefinition>> Out;
    for (const TSharedPtr<const FSFQuestDefinition>& Q : Quests)
    { if (Out.Num() < Max && Q->bHasOffer && Q->Offer.Boards.Contains(BoardId)) { Out.Add(Q); } }
    return Out;
}

TSharedPtr<const FSFQuestDefinition> FSFQuestCatalog::ForGiver(const FString& NpcId) const
{
    const TSharedPtr<const FSFQuestDefinition>* Hit = Quests.FindByPredicate([&](const TSharedPtr<const FSFQuestDefinition>& Q) { return Q->bHasOffer && Q->Offer.GiverNpc == NpcId; });
    return Hit ? *Hit : nullptr;
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
    DOREPLIFETIME_CONDITION(USFQuestPilotComponent, Views, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(USFQuestPilotComponent, Completed, COND_OwnerOnly);
}

TSet<FString> USFQuestPilotComponent::ActiveIds() const
{
    TSet<FString> Ids;
    for (const FSFQuestView& View : Views) { if (!View.bComplete) { Ids.Add(View.QuestId); } }
    return Ids;
}

void USFQuestPilotComponent::AddEntry(TSharedPtr<const FSFQuestDefinition> Quest, int64 Revision)
{
    Entries.RemoveAll([&](const FEntry& E) { return E.Quest->Id == Quest->Id; });
    FEntry& Entry = Entries.AddDefaulted_GetRef();
    Entry.Quest = Quest; Entry.Revision = Revision;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Development fixture for the network runners: start the pilot chain at a named step, earlier steps credited, no rewards due.
    FString StartStep;
    if (!USFBackpackComponent::Services() && FParse::Value(FCommandLine::Get(), TEXT("QuestPilotStartStep="), StartStep))
    {
        const int32 Index = Quest->Steps.IndexOfByPredicate([&](const FSFQuestStep& S) { return S.Id == StartStep; });
        for (int32 I = 0; I < Index; ++I) { Entry.Progress.GrantedSteps.Add(Quest->Steps[I].Id); }
        Entry.Progress.StepIndex = FMath::Max(0, Index);
    }
#endif
    RebuildView();
}

FString USFQuestPilotComponent::CheckAccept(const FString& QuestId, const FString& SourceId) const
{
    const FSFQuestCatalog& Catalog = FSFQuestCatalog::Get();
    const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(QuestId);
    if (!Quest || !Quest->bHasOffer) { return TEXT("unknown_quest"); }
    if (USFBackpackComponent::Services() && !bRestored) { return TEXT("not_ready"); }
    const APlayerController* PC = Cast<APlayerController>(GetOwner());
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn) { return TEXT("out_of_reach"); }
    bool bOffered = false, bInReach = false;
    if (SourceId.StartsWith(TEXT("board/")))
    {
        bOffered = Quest->Offer.Boards.Contains(SourceId);
        for (TActorIterator<ASFQuestBoard> It(GetWorld()); It && bOffered; ++It)
        { bInReach |= It->BoardId == SourceId && FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation()) <= GetDefault<USFQuestPilotSettings>()->BoardReachCm(); }
    }
    else if (SourceId.StartsWith(TEXT("npc/")))
    {
        bOffered = Quest->Offer.GiverNpc == SourceId;
        for (TActorIterator<ASFDialogueNpc> It(GetWorld()); It && bOffered; ++It)
        { bInReach |= It->DialogueFile == Stem(SourceId) + TEXT(".json") && FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation()) <= GetDefault<USFQuestPilotSettings>()->OfferReachCm(); }
    }
    if (!bOffered) { return TEXT("not_offered"); }
    if (!bInReach) { return TEXT("out_of_reach"); }
    switch (Catalog.OfferState(*Quest, ActiveIds(), CompletedIds()))
    {
    case ESFQuestOfferState::Active: return TEXT("duplicate");
    case ESFQuestOfferState::Completed: return TEXT("completed");
    case ESFQuestOfferState::Locked: return TEXT("prerequisite_missing");
    default: return FString();
    }
}

void USFQuestPilotComponent::ServerAcceptQuest_Implementation(const FString& QuestId, const FString& SourceId)
{
    const FString Refusal = CheckAccept(QuestId, SourceId);
    UE_LOG(LogTemp, Display, TEXT("QUEST_ACCEPT_REQUEST Quest=%s Source=%s Result=%s"), *QuestId, *SourceId, Refusal.IsEmpty() ? TEXT("allowed") : *Refusal);
    if (!Refusal.IsEmpty()) { ClientQuestResult(QuestId, Refusal); return; }
    Accept(QuestId, SourceId);
}

void USFQuestPilotComponent::Accept(const FString& QuestId, const FString& SourceId)
{
    const TSharedPtr<const FSFQuestDefinition> Quest = FSFQuestCatalog::Get().Find(QuestId);
    if (!Quest) { return; }
    if (USFBackpackComponent* Pack = USFBackpackComponent::Services() ? USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetOwner())) : nullptr)
    {
        // The Progression service owns the row; the source id names this accept attempt so a retry replays it.
        ServiceQueue.Add({TEXT("quest"), TEXT("accept"), QuestId, FString(), FString(),
            FString::Printf(TEXT("qa:%s:%s:%s"), *Pack->GetCharacterId(), *QuestId, *FGuid::NewGuid().ToString(EGuidFormats::Short))});
        PumpServices();
        return;
    }
    AddEntry(Quest, 0);
    UE_LOG(LogTemp, Display, TEXT("QUEST_ACCEPTED Quest=%s Source=%s"), *QuestId, *SourceId);
    ClientQuestResult(QuestId, TEXT("accepted"));
}

void USFQuestPilotComponent::ServerAbandonQuest_Implementation(const FString& QuestId)
{
    const FEntry* Entry = Find(QuestId);
    if (!Entry || Entry->Progress.IsComplete(*Entry->Quest)) { ClientQuestResult(QuestId, TEXT("not_active")); return; }
    Abandon(QuestId);
}

void USFQuestPilotComponent::Abandon(const FString& QuestId)
{
    if (USFBackpackComponent* Pack = USFBackpackComponent::Services() ? USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetOwner())) : nullptr)
    {
        ServiceQueue.Add({TEXT("quest"), TEXT("abandon"), QuestId, FString(), FString(),
            FString::Printf(TEXT("qx:%s:%s:%s"), *Pack->GetCharacterId(), *QuestId, *FGuid::NewGuid().ToString(EGuidFormats::Short))});
        PumpServices();
        return;
    }
    Entries.RemoveAll([&](const FEntry& E) { return E.Quest->Id == QuestId; });
    RebuildView();
    UE_LOG(LogTemp, Display, TEXT("QUEST_ABANDONED Quest=%s"), *QuestId);
    ClientQuestResult(QuestId, TEXT("abandoned"));
}

void USFQuestPilotComponent::ClientQuestResult_Implementation(const FString& QuestId, const FString& Result)
{
    LastResult = Result;
    UE_LOG(LogTemp, Display, TEXT("QUEST_CLIENT_RESULT Quest=%s Result=%s"), *QuestId, *Result);
}

void USFQuestPilotComponent::ServerTalkedTo_Implementation(const FString& NpcId)
{
    const APlayerController* PC = Cast<APlayerController>(GetOwner());
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn || !QuestValidId(NpcId, TEXT("npc"))) { return; }
    bool bNear = false;
    for (TActorIterator<ASFDialogueNpc> It(GetWorld()); It && !bNear; ++It)
    { bNear = It->DialogueFile == Stem(NpcId) + TEXT(".json") && FVector::Dist2D(It->GetActorLocation(), Pawn->GetActorLocation()) <= GetDefault<USFQuestPilotSettings>()->OfferReachCm(); }
    if (!bNear) { UE_LOG(LogTemp, Display, TEXT("QUEST_TALK_REFUSED Npc=%s Why=out_of_reach"), *NpcId); return; }
    Observe(TEXT("talk_npc"), NpcId, FString(), FString::Printf(TEXT("talk_npc:%s:%s"), *Stem(NpcId), *FGuid::NewGuid().ToString(EGuidFormats::Short)));
}

bool USFQuestPilotComponent::Observe(const FString& Kind, const FString& TargetId, const FString& NodeId, const FString& OperationId,
    int32 EligiblePartySize, int32 ReferencePartyLevel)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) { return false; }
    TArray<FEntry*> Matches;
    for (FEntry& Entry : Entries) { if (Entry.Progress.Wants(*Entry.Quest, Kind, TargetId, NodeId)) { Matches.Add(&Entry); } }
    if (USFBackpackComponent::Services())
    {
        // INT-04: the Progression service decides; the local model only mirrors accepted replies (decision 5). The first quest
        // carries the action's progression receipt; any other quest the outcome counts for gets its own quest-only receipt.
        const bool bProgression = IsProgressionKind(Kind);
        if (Matches.IsEmpty() && bProgression)
        {
            FServiceEvent& Event = ServiceQueue.AddDefaulted_GetRef();
            Event.ReceiptKind = Kind; Event.TargetId = TargetId; Event.NodeId = NodeId; Event.OperationId = OperationId;
            Event.EligiblePartySize = EligiblePartySize; Event.ReferencePartyLevel = ReferencePartyLevel;
        }
        for (int32 Index = 0; Index < Matches.Num(); ++Index)
        {
            const FString& QuestId = Matches[Index]->Quest->Id;
            ServiceQueue.Add({Index == 0 && bProgression ? Kind : FString(TEXT("quest")), Kind, QuestId, TargetId, NodeId,
                Index == 0 ? OperationId : OperationId + TEXT("#") + QuestId});
            if (Index == 0 && bProgression)
            { ServiceQueue.Last().EligiblePartySize = EligiblePartySize; ServiceQueue.Last().ReferencePartyLevel = ReferencePartyLevel; }
        }
        PumpServices();
        return !Matches.IsEmpty();
    }
    for (FEntry* Entry : Matches)
    {
        const int32 Before = Entry->Progress.StepIndex;
        TArray<FString> Rewards;
        const ESFQuestApply Result = Entry->Progress.Apply(*Entry->Quest, Kind, TargetId, NodeId, 1, OperationId, Rewards);
        UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_EVENT Kind=%s Target=%s Op=%s Result=%d Step=%d Rewards=%d Quest=%s"),
            *Kind, *TargetId, *OperationId, int32(Result), Entry->Progress.StepIndex, Rewards.Num(), *Entry->Quest->Id);
        for (const FString& Bundle : Rewards) { UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_REWARD Bundle=%s Quest=%s"), *Bundle, *Entry->Quest->Id); }
        if (!Rewards.IsEmpty())
        { for (const FSFQuestRewardItem& Item : Entry->Quest->Steps[Before].RewardItems) { PendingRewards.Add({Entry->Quest->Id, Item}); } }
        if (Entry->Progress.IsComplete(*Entry->Quest))
        { Completed.AddUnique(Entry->Quest->Id); UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_COMPLETE Quest=%s"), *Entry->Quest->Id); }
    }
    RebuildView();
    TryDeliverRewards();
    return !Matches.IsEmpty();
}

void USFQuestPilotComponent::PumpServices()
{
    auto* PC = Cast<APlayerController>(GetOwner());
    USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(PC);
    if (bServiceBusy || ServiceQueue.IsEmpty() || !Pack) { return; }
    const FServiceEvent Event = ServiceQueue[0];
    const FEntry* Entry = Event.QuestId.IsEmpty() ? nullptr : Find(Event.QuestId);
    const bool bLifecycle = Event.QuestKind == TEXT("accept") || Event.QuestKind == TEXT("abandon");
    // Quest evidence names the server's current step; an entry gone or finished since it was queued has nothing left to count.
    if (!Event.QuestId.IsEmpty() && !bLifecycle && (!Entry || Entry->Progress.IsComplete(*Entry->Quest)) && Event.ReceiptKind == TEXT("quest"))
    { ServiceQueue.RemoveAt(0); PumpServices(); return; }
    bServiceBusy = true;
    // Receipt kinds: harvest, craft and defeat carry a progression payload; the rest are quest evidence only.
    FSFGameplayRequest Q; Q.Rpc = ESFGameplayRpc::ApplyReceipt; Q.OperationId = Event.OperationId; Q.CharacterId = Pack->GetCharacterId();
    Q.Kind = Event.ReceiptKind; Q.ExpectedProgressionRevision = ProgressionRevision; Q.ExpectedQuestRevision = Entry ? Entry->Revision : 0;
    if (Event.ReceiptKind == TEXT("gather"))
    {
        Q.SkillId = Event.TargetId == TEXT("item/oak_log") ? TEXT("skill/logging") : TEXT("skill/mining");
        Q.Difficulty = 1; Q.ActionSeconds = GetDefault<USFGatheringPilotSettings>()->HarvestSeconds; Q.ContextHash = GetTypeHash(Event.NodeId);
    }
    else if (Event.ReceiptKind == TEXT("craft"))
    { Q.SkillId = TEXT("skill/smithing"); Q.Difficulty = 1; Q.ActionSeconds = GetDefault<USFGatheringPilotSettings>()->RefineSeconds; Q.ContextHash = GetTypeHash(Event.TargetId); }
    else if (Event.ReceiptKind == TEXT("defeat"))
    { Q.EnemyLevel = 2; Q.EnemyTierMultiplier = 1; Q.PartySize = Event.EligiblePartySize; Q.ReferencePartyLevel = Event.ReferencePartyLevel; }
    if (!Event.QuestId.IsEmpty() && (bLifecycle || (Entry && !Entry->Progress.IsComplete(*Entry->Quest))))
    {
        LastOccurredMs = FMath::Max(LastOccurredMs + 1, FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond());
        Q.bHasQuest = true; Q.QuestId = Event.QuestId; Q.QuestKind = Event.QuestKind; Q.OccurredAtUnixMs = LastOccurredMs;
        if (!bLifecycle) { Q.StepId = Entry->Quest->Steps[Entry->Progress.StepIndex].Id; Q.TargetId = Event.TargetId; Q.NodeId = Event.NodeId; Q.Quantity = 1; }
        else { Q.Quantity = 0; }
    }
    USFBackpackComponent::Services()->Call(Q, [Weak = TWeakObjectPtr<USFQuestPilotComponent>(this), Event](const FSFGameplayResponse& R)
    {
        USFQuestPilotComponent* Self = Weak.Get();
        if (!Self) { return; }
        Self->ServiceQueue.RemoveAt(0);
        Self->bServiceBusy = false;
        FEntry* Entry = Event.QuestId.IsEmpty() ? nullptr : Self->Find(Event.QuestId);
        if (R.Status == ESFGameplayStatus::Ok)
        {
            if (R.bProgressionApplied)
            {
                if (auto* Pilot = Self->GetOwner()->FindComponentByClass<USFGatheringPilotComponent>())
                { Pilot->RestoreLevel(R.Level); Pilot->RestoreRestedMinutes(R.RestedMinutes); }
            }
            if (R.ProgressionRevision > 0) { Self->ProgressionRevision = R.ProgressionRevision; }
            if (R.Level > 0)
            { if (auto* Pilot = Self->GetOwner()->FindComponentByClass<USFGatheringPilotComponent>()) { Pilot->RestoreLevel(R.Level); } }
            if (Event.QuestKind == TEXT("accept") || Event.QuestKind == TEXT("abandon"))
            {
                const bool bAccept = Event.QuestKind == TEXT("accept");
                FString Result = R.bQuestAccepted ? FString(bAccept ? TEXT("accepted") : TEXT("abandoned")) : (R.QuestReason.IsEmpty() ? FString(TEXT("refused")) : R.QuestReason);
                if (R.bQuestAccepted && bAccept) { Self->AddEntry(FSFQuestCatalog::Get().Find(Event.QuestId), R.QuestRevision); }
                if (R.bQuestAccepted && !bAccept) { Self->Entries.RemoveAll([&](const FEntry& E) { return E.Quest->Id == Event.QuestId; }); Self->RebuildView(); }
                if (!R.bQuestAccepted) { Self->RequestRefresh(0.f); }  // the service knows better (another session accepted, say)
                UE_LOG(LogTemp, Display, TEXT("QUEST_%s Quest=%s Result=%s Revision=%lld Source=service"), bAccept ? TEXT("ACCEPTED") : TEXT("ABANDONED"), *Event.QuestId, *Result, R.QuestRevision);
                Self->ClientQuestResult(Event.QuestId, Result);
            }
            else if (Entry)
            {
                if (R.QuestRevision > 0) { Entry->Revision = R.QuestRevision; }
                if (R.bQuestAccepted)
                {
                    // Mirror the accepted evidence for the tracker counts, then take the server's step as the truth.
                    TArray<FString> Ignored;
                    Entry->Progress.Apply(*Entry->Quest, Event.QuestKind, Event.TargetId, Event.NodeId, 1, Event.OperationId, Ignored);
                    const int32 Active = R.bQuestCompleted ? Entry->Quest->Steps.Num()
                        : Entry->Quest->Steps.IndexOfByPredicate([&](const FSFQuestStep& S) { return S.Id == R.ActiveStepId; });
                    if (Active != INDEX_NONE && Active != Entry->Progress.StepIndex)
                    {
                        UE_LOG(LogTemp, Warning, TEXT("QUEST_SERVICE_STEP_CORRECTED Quest=%s Local=%d Server=%d"), *Event.QuestId, Entry->Progress.StepIndex, Active);
                        Entry->Progress.StepIndex = Active; Entry->Progress.Counts.Reset();
                    }
                    if (R.bQuestCompleted) { Self->Completed.AddUnique(Event.QuestId); UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_COMPLETE Quest=%s Source=service"), *Event.QuestId); }
                }
                UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_EVENT Kind=%s Target=%s Op=%s Result=%d Step=%d Rewards=%d Source=service Active=%s Reason=%s Quest=%s"),
                    *Event.QuestKind, *Event.TargetId, *Event.OperationId, R.bQuestAccepted ? 0 : 2, Entry->Progress.StepIndex, R.RewardBundleIds.Num(), *R.ActiveStepId, *R.QuestReason, *Event.QuestId);
                for (const FString& Bundle : R.RewardBundleIds) { UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_REWARD Bundle=%s Source=service"), *Bundle); }
                for (const TPair<FString, int64>& Item : R.GrantedItems)
                {
                    UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_PAYOUT Item=%s Quantity=%lld"), *Item.Key, Item.Value);
                    Entry->RewardLines.AddUnique(FString::Printf(TEXT("%lld %s"), Item.Value, *SFStationRecipes::ItemDisplayName(Item.Key)));
                }
                // The granted lines are the committed payout; without them (payout still pending) the reward waits and is re-read.
                if (!R.RewardBundleIds.IsEmpty() && R.GrantedItems.IsEmpty()) { Entry->bRewardWaiting = true; }
                if (USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(Self->GetOwner())))
                { if (!R.GrantedItems.IsEmpty()) { Pack->ApplyToBag(R.GrantedItems); } else if (!R.RewardBundleIds.IsEmpty()) { Pack->Reconcile(); } }
                Self->RebuildView();
            }
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
                if (State.Status == ESFGameplayStatus::Ok)
                {
                    Again->ProgressionRevision = State.ProgressionRevision;
                    if (auto* Pilot = Again->GetOwner()->FindComponentByClass<USFGatheringPilotComponent>())
                    { Pilot->RestoreLevel(State.Level); Pilot->RestoreRestedMinutes(State.RestedMinutes); }
                    for (const FSFGameplayResponse::FQuestState& Row : State.Quests)
                    { if (FEntry* Stale = Again->Find(Row.QuestId); Stale && Row.QuestId == Event.QuestId) { Stale->Revision = Row.Revision; } }
                }
                FServiceEvent Retry = Event; Retry.bRetried = true;
                Again->ServiceQueue.Insert(Retry, 0);
                Again->PumpServices();
            });
            return;
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_EVENT Kind=%s Target=%s Op=%s Result=3 Rewards=0 Source=service Code=%d Quest=%s"),
                *Event.QuestKind, *Event.TargetId, *Event.OperationId, R.GrpcCode, *Event.QuestId);
            // A refused payout (full backpack) still committed the step: re-read what the service holds.
            if (!Event.QuestId.IsEmpty()) { Self->RequestRefresh(GetDefault<USFQuestPilotSettings>()->RewardRetrySeconds); }
            if (Event.QuestKind == TEXT("accept") || Event.QuestKind == TEXT("abandon")) { Self->ClientQuestResult(Event.QuestId, TEXT("service_unavailable")); }
        }
        Self->PumpServices();
    });
}

void USFQuestPilotComponent::RequestRefresh(float DelaySeconds)
{
    if (!USFBackpackComponent::Services()) { return; }
    const double At = GetWorld()->GetTimeSeconds() + DelaySeconds;
    NextRefreshAt = RefreshesDue > 0 ? FMath::Min(NextRefreshAt, At) : At;
    RefreshesDue = FMath::Max(RefreshesDue, 1);
}

void USFQuestPilotComponent::Refresh()
{
    USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetOwner()));
    if (bRefreshBusy || !Pack || !USFBackpackComponent::Services()) { return; }
    bRefreshBusy = true;
    FSFGameplayRequest Get; Get.Rpc = ESFGameplayRpc::GetCharacterState; Get.CharacterId = Pack->GetCharacterId();
    USFBackpackComponent::Services()->Call(Get, [Weak = TWeakObjectPtr<USFQuestPilotComponent>(this)](const FSFGameplayResponse& State)
    {
        USFQuestPilotComponent* Self = Weak.Get();
        if (!Self) { return; }
        Self->bRefreshBusy = false;
        // Evidence still in flight would be undone by an older read; its reply corrects the mirror itself.
        if (State.Status == ESFGameplayStatus::Ok && !Self->HasPendingServiceWork())
        {
            Self->Restore(State.Quests, State.ProgressionRevision);
            if (auto* Gathering = Self->GetOwner()->FindComponentByClass<USFGatheringPilotComponent>())
            { Gathering->RestoreSkills(State.Skills); Gathering->RestoreLevel(State.Level); Gathering->RestoreRestedMinutes(State.RestedMinutes); }
        }
    });
}

void USFQuestPilotComponent::Restore(const TArray<FSFGameplayResponse::FQuestState>& Rows, int64 InProgressionRevision)
{
    const FSFQuestCatalog& Catalog = FSFQuestCatalog::Get();
    ProgressionRevision = InProgressionRevision;
    TSet<FString> Listed;
    for (const FSFGameplayResponse::FQuestState& Row : Rows)
    {
        const TSharedPtr<const FSFQuestDefinition> Quest = Catalog.Find(Row.QuestId);
        if (!Quest || Row.State == TEXT("abandoned")) { continue; }
        Listed.Add(Row.QuestId);
        FEntry* Entry = Find(Row.QuestId);
        const bool bDone = Row.State == TEXT("completed");
        if (bDone) { Completed.AddUnique(Row.QuestId); }
        // A finished quest stays on the tracker only while its reward waits (or when it finished in this session).
        if (!Entry && bDone && !Row.bRewardPending) { continue; }
        if (!Entry) { Entry = &Entries.AddDefaulted_GetRef(); Entry->Quest = Quest; }
        const int32 Found = Quest->Steps.IndexOfByPredicate([&](const FSFQuestStep& Step) { return Step.Id == Row.ActiveStepId; });
        const int32 Step = bDone ? Quest->Steps.Num() : FMath::Max(Found, 0);
        if (Step != Entry->Progress.StepIndex || (!bDone && Row.Counts.OrderIndependentCompareEqual(Entry->Progress.Counts) == false))
        {
            Entry->Progress = FSFQuestProgress();
            Entry->Progress.StepIndex = Step;
            if (!bDone && Found != INDEX_NONE) { Entry->Progress.Counts = Row.Counts; }
            for (int32 Index = 0; Index < Step; ++Index) { Entry->Progress.GrantedSteps.Add(Quest->Steps[Index].Id); }
        }
        Entry->Revision = Row.Revision;
        if (Entry->bRewardWaiting && !Row.bRewardPending)
        {
            // The recovery sweep granted it: show what landed and re-read the backpack.
            Entry->RewardLines = Quest->RewardLines();
            if (USFBackpackComponent* Pack = USFBackpackComponent::FindOrCreate(Cast<APlayerController>(GetOwner()))) { Pack->Reconcile(); }
            UE_LOG(LogTemp, Display, TEXT("QUEST_REWARD_DELIVERED Quest=%s"), *Row.QuestId);
        }
        Entry->bRewardWaiting = Row.bRewardPending;
        UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_RESTORED Quest=%s Step=%d Active=%s Counts=%d Completed=%d Waiting=%d"), *Row.QuestId,
            Entry->Progress.StepIndex, *Row.ActiveStepId, Entry->Progress.Counts.Num(), bDone, Entry->bRewardWaiting);
    }
    Entries.RemoveAll([&](const FEntry& E) { return !Listed.Contains(E.Quest->Id); });
    bRestored = true;
    UE_LOG(LogTemp, Display, TEXT("QUEST_LOG_RESTORED Rows=%d Active=%d Completed=%d"), Rows.Num(), ActiveIds().Num(), Completed.Num());
    RebuildView();
}

void USFQuestPilotComponent::RebuildView()
{
    TArray<FSFQuestView> Next;
    for (const FEntry& Entry : Entries)
    {
        const FSFQuestDefinition& Quest = *Entry.Quest;
        FSFQuestView& View = Next.AddDefaulted_GetRef();
        View.QuestId = Quest.Id; View.Title = Quest.Title(); View.StepTotal = Quest.Steps.Num();
        View.bComplete = Entry.Progress.IsComplete(Quest);
        View.bRewardWaiting = Entry.bRewardWaiting;
        View.RewardLines = Entry.RewardLines;
        View.StepNumber = FMath::Min(Entry.Progress.StepIndex + 1, View.StepTotal);
        if (View.bComplete) { continue; }
        const FSFQuestStep& Step = Quest.Steps[Entry.Progress.StepIndex];
        View.Step = Humanize(Step.Id);
        for (const FSFQuestObjective& O : Step.Objectives)
        {
            FSFQuestLine Line; Line.Label = FSFQuestCatalog::ObjectiveLabel(O);
            Line.Count = Entry.Progress.Counts.FindRef(O.Key); Line.Target = O.Count;
            View.Lines.Add(Line);
        }
    }
    Views = MoveTemp(Next);
}

void USFQuestPilotComponent::TryDeliverRewards()
{
    // Without gameplay services the server pays the quest record's items straight into the bag. An item that does not fit
    // stays pending (the tracker says so) and is retried every RewardRetrySeconds, so nothing repeats or is lost.
    auto* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<USFInventoryComponent>() : nullptr;
    bool bChanged = false;
    while (Inventory && !PendingRewards.IsEmpty())
    {
        const TPair<FString, FSFQuestRewardItem> Due = PendingRewards[0];
        FEntry* Entry = Find(Due.Key);
        FSFItemQuantity Pay;
        Pay.Item = TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Due.Value.DefinitionId)));
        Pay.Quantity = int32(Due.Value.Quantity);
        if (SFInventoryTransactions::ExchangeInventoryItems(Inventory, {}, {Pay}) != ESFInventoryResult::Success)
        {
            if (Entry && !Entry->bRewardWaiting) { Entry->bRewardWaiting = true; bChanged = true; }
            break;
        }
        UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_PAYOUT Item=%s Quantity=%lld Quest=%s Source=local"), *Due.Value.DefinitionId, Due.Value.Quantity, *Due.Key);
        PendingRewards.RemoveAt(0);
        if (Entry)
        {
            Entry->RewardLines.Add(FString::Printf(TEXT("%lld %s"), Due.Value.Quantity, *SFStationRecipes::ItemDisplayName(Due.Value.DefinitionId)));
            Entry->bRewardWaiting = PendingRewards.ContainsByPredicate([&](const TPair<FString, FSFQuestRewardItem>& P) { return P.Key == Due.Key; }) && Entry->bRewardWaiting;
            bChanged = true;
        }
    }
    if (bChanged) { RebuildView(); }
}

bool USFQuestPilotComponent::CaptureGatherReceipt(FSFGameplayRequest& Q, const FString& TargetId, const FString& NodeId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) { return false; }
    Q.ExpectedProgressionRevision = ProgressionRevision;
    Q.SkillId = TargetId == TEXT("item/oak_log") ? TEXT("skill/logging") : TEXT("skill/mining");
    Q.Difficulty = 1;
    Q.ActionSeconds = GetDefault<USFGatheringPilotSettings>()->HarvestSeconds;
    Q.ContextHash = GetTypeHash(NodeId);
    // ponytail: one follow-up receipt carries one quest's evidence, so a harvest counts for the first active quest that wants it;
    // a second quest wanting the same harvest needs a second receipt from the ledger outbox.
    const FEntry* Entry = Entries.FindByPredicate([&](const FEntry& E) { return E.Progress.Wants(*E.Quest, TEXT("gather"), TargetId, NodeId) != nullptr; });
    if (Entry)
    {
        LastOccurredMs = FMath::Max(LastOccurredMs + 1, FDateTime::UtcNow().ToUnixTimestamp() * 1000 + FDateTime::UtcNow().GetMillisecond());
        Q.ExpectedQuestRevision = Entry->Revision;
        Q.bHasQuest = true; Q.QuestId = Entry->Quest->Id; Q.StepId = Entry->Quest->Steps[Entry->Progress.StepIndex].Id;
        Q.QuestKind = TEXT("gather"); Q.TargetId = TargetId; Q.NodeId = NodeId;
        Q.Quantity = 1; Q.OccurredAtUnixMs = LastOccurredMs;
    }
    // The ledger outbox relays progression even when no quest wants this harvest.
    RequestRefresh(3.f); RefreshesDue = 3;
    return true;
}

void USFQuestPilotComponent::TickTalk()
{
    // AI Townsfolk doc section 9 step 4: the server hands off a resident of the objective's census role near the player
    // and holds it; reaching it counts the objective and lets it return to its day once the player moves on.
    const APlayerController* PC = Cast<APlayerController>(GetOwner());
    const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    USFTownsfolkSubsystem* Town = GetWorld()->GetSubsystem<USFTownsfolkSubsystem>();
    if (!Pawn || !Town) { return; }
    const FSFQuestObjective* Talk = nullptr;
    for (const FEntry& Entry : Entries)
    {
        if (Entry.Progress.IsComplete(*Entry.Quest)) { continue; }
        Talk = Entry.Quest->Steps[Entry.Progress.StepIndex].Objectives.FindByPredicate([&](const FSFQuestObjective& O)
        { return O.Kind == TEXT("talk_resident") && Entry.Progress.Counts.FindRef(O.Key) < O.Count; });
        if (Talk) { break; }
    }
    if (!Talk) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Talker.IsValid())
    {
        if (Now < NextTalkRequest) { return; }
        NextTalkRequest = Now + 5;  // none of that role free: ask again shortly
        FString Role; Talk->TargetId.Split(TEXT("/"), nullptr, &Role);
        Talker = Town->RequestQuestHandoff(FName(*Role), Pawn->GetActorLocation());
        if (Talker.IsValid()) { UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_TALK_HELD Role=%s Id=%s Serial=%d"), *Role, *Talker->Id(), Talker->Serial()); }
        return;
    }
    if (FVector::Dist2D(Talker->GetActorLocation(), Pawn->GetActorLocation()) > GetDefault<USFQuestPilotSettings>()->TalkRangeMetres * 100.0f) { return; }
    const FString Target = Talk->TargetId, Operation = FString::Printf(TEXT("talk:%s:%d"), *Talker->Id(), Talker->Serial());
    Town->ReleaseQuestHandoff(Talker.Get()); Talker.Reset();
    Observe(TEXT("talk_resident"), Target, FString(), Operation);
}

void USFQuestPilotComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    // A player leaving mid-objective lets go of its share of the held resident.
    if (USFTownsfolkSubsystem* Town = GetWorld() ? GetWorld()->GetSubsystem<USFTownsfolkSubsystem>() : nullptr; Town && Talker.IsValid()) { Town->ReleaseQuestHandoff(Talker.Get()); }
    Talker.Reset();
    Super::EndPlay(Reason);
}

void USFQuestPilotComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        const double Now = GetWorld()->GetTimeSeconds();
        if (!USFBackpackComponent::Services()) { bRestored = true; }
        static double NextLocalReward = 0;
        if (!PendingRewards.IsEmpty() && Now >= NextLocalReward) { NextLocalReward = Now + GetDefault<USFQuestPilotSettings>()->RewardRetrySeconds; TryDeliverRewards(); }
        // A waiting reward is re-read every RewardRetrySeconds until the service reports it delivered.
        if (Entries.ContainsByPredicate([](const FEntry& E) { return E.bRewardWaiting; }) && RefreshesDue == 0)
        { RequestRefresh(GetDefault<USFQuestPilotSettings>()->RewardRetrySeconds); }
        if (RefreshesDue > 0 && Now >= NextRefreshAt && bRestored)
        { --RefreshesDue; NextRefreshAt = Now + 5.0; Refresh(); }
        TickTalk();
        TickDevAccept();
    }
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FParse::Param(FCommandLine::Get(), TEXT("QuestBoardScenario")) || FParse::Param(FCommandLine::Get(), TEXT("QuestNpcScenario")))
    { TickBoardScenario(GetWorld()->GetTimeSeconds()); return; }
    if (!FParse::Param(FCommandLine::Get(), TEXT("QuestFullScenario"))) { return; }
    if (GetOwner() && GetOwner()->HasAuthority()) { TickFullServer(); }
    else { TickFullClient(GetWorld()->GetTimeSeconds()); }
#endif
}

void USFQuestPilotComponent::TickDevAccept()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Development fixture: the network runners start with quests already taken (-QuestAutoAccept=<id>[,<id>]); the older
    // -QuestPilot / -QuestFullScenario runners imply the pilot chain. Same accept path as the board, without the reach check.
    if (bDevAcceptDone || !bRestored) { return; }
    bDevAcceptDone = true;
    FString List;
    if (!FParse::Value(FCommandLine::Get(), TEXT("QuestAutoAccept="), List, false)
        && (FParse::Param(FCommandLine::Get(), TEXT("QuestPilot")) || FParse::Param(FCommandLine::Get(), TEXT("QuestFullScenario"))))
    { List = TEXT("quest/frontier_founder"); }
    TArray<FString> Ids; List.ParseIntoArray(Ids, TEXT(","));
    for (const FString& Id : Ids)
    {
        if (!ActiveIds().Contains(Id) && !Completed.Contains(Id) && FSFQuestCatalog::Get().Find(Id))
        { UE_LOG(LogTemp, Display, TEXT("QUEST_DEV_AUTO_ACCEPT Quest=%s"), *Id); Accept(Id, TEXT("dev")); }
    }
#endif
}

// ---- Full-chain scenario (development): the server only places the pawn beside the next target, like the
// other scenarios; every action itself is made by the client through its normal entry points and judged by the server.

namespace
{
    // The chain the scenario drivers walk: the pilot chain, or the board scenario's mill quest.
    FString ScenarioQuestId() { return FParse::Param(FCommandLine::Get(), TEXT("QuestBoardScenario")) ? TEXT("quest/mills_supply") : TEXT("quest/frontier_founder"); }

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

    int64 BagQuantity(const AActor* Owner, const TCHAR* Item, bool bOneGrade = false)
    {
        auto* Bag = Owner ? Owner->FindComponentByClass<USFInventoryComponent>() : nullptr;
        if (!Bag) { return 0; }
        int64 Quantity = SFInventoryTransactions::GetItemQuantity(Bag,
            TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(Item))));
        int64 Maximum = Quantity;
        for (const TCHAR* Grade : {TEXT("crude"), TEXT("fine"), TEXT("superior"), TEXT("masterwork")})
        {
            const FString GradedId = FString(Item) + TEXT("/") + Grade;
            const int64 Count = SFInventoryTransactions::GetItemQuantity(Bag,
                TSoftObjectPtr<USFItemDefinition>(FSoftObjectPath(SFStationRecipes::ItemAssetPath(GradedId))));
            Quantity += Count; Maximum = FMath::Max(Maximum, Count);
        }
        return bOneGrade ? Maximum : Quantity;
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
        const bool bTimber = BagQuantity(Owner, TEXT("item/rough_timber"), true) >= Timber, bNails = BagQuantity(Owner, TEXT("item/iron_nails")) >= Nails;
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
    const FEntry* Entry = Find(ScenarioQuestId());
    const FSFQuestObjective* Objective = Entry ? NextObjective(*Entry->Quest, Entry->Progress) : nullptr;
    if (!Objective && Entry && Entry->Progress.IsComplete(*Entry->Quest) &&
        FParse::Param(FCommandLine::Get(), TEXT("Gat01GrowthRestart")) && Pawn)
    {
        // Development fixture: keep the completed-quest player beside the same iron node for depletion.
        if (const auto* Iron = NearestNode(GetWorld(), ESFGatheringPilotKind::Iron, FVector::ZeroVector))
        {
            const FVector Target = Iron->GetActorLocation();
            if (FVector::Dist2D(Pawn->GetActorLocation(), Target) > 350)
            { Pawn->SetActorLocation(Target + (-Target * FVector(1, 1, 0)).GetSafeNormal() * 200 + FVector(0, 0, 80), false, nullptr, ETeleportType::TeleportPhysics); }
        }
        return;
    }
    // A restoring character keeps its restored position; the fixture places it once progress is known.
    if (!Pawn || !Objective || USFCharacterSessionComponent::IsRestoring(PC) ||
        (Cast<ASFE1Character>(Pawn) && Cast<ASFE1Character>(Pawn)->CombatHealth() <= 0)) { return; }
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
    const FString Key = Entry->Quest->Steps[Entry->Progress.StepIndex].Id + TEXT("/") + Objective->Key + TEXT("/") + Next.Describe();
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
    const FSFQuestView* View = FindView(ScenarioQuestId());
    if (!PC || !PC->IsLocalController() || !Pawn || !View ||
        (Cast<ASFE1Character>(Pawn) && Cast<ASFE1Character>(Pawn)->CombatHealth() <= 0)) { return; }
    if (View->bComplete)
    {
        if (!bFullDone) { bFullDone = true; FullEquipAt = 0; FullLastSwingAt = 0;
            UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_DONE Steps=%d"), View->StepTotal); }
        if (!FParse::Param(FCommandLine::Get(), TEXT("Gat01GrowthRestart"))) { return; }
        auto* Gathering = PC->FindComponentByClass<USFGatheringPilotComponent>();
        ASFGatheringPilotNode* Iron = NearestNode(GetWorld(), ESFGatheringPilotKind::Iron, Pawn->GetActorLocation());
        if (!Gathering || !Gathering->IsEnabled() || !Iron ||
            FVector::Dist2D(Pawn->GetActorLocation(), Iron->GetActorLocation()) > 350 || Iron->GetState().Charges <= 0) { return; }
        if (FullEquipAt == 0) { Gathering->EquipPickaxe(); FullEquipAt = Now; return; }
        if (Now - FullEquipAt < 2 || Now - FullLastSwingAt < 4 ||
            Gathering->GetFeedback().Phase == ESFGatheringPilotPhase::Harvesting ||
            Gathering->GetFeedback().Phase == ESFGatheringPilotPhase::Committing) { return; }
        const FVector Eye = Pawn->GetPawnViewLocation();
        const FBox Bounds = Iron->GetComponentsBoundingBox(true);
        for (const FVector Aim : {Iron->GetActorLocation(), Bounds.GetCenter(), Bounds.GetCenter() + FVector(0, 0, Bounds.GetExtent().Z * 0.5)})
        { if (Gathering->InteractFromRay(Eye, (Aim - Eye).GetSafeNormal())) { break; } }
        FullLastSwingAt = Now;
        return;
    }
    if (bFullDone) { return; }
    const FSFQuestLine* Line = View->Lines.FindByPredicate([](const FSFQuestLine& L) { return L.Count < L.Target; });
    const TSharedPtr<const FSFQuestDefinition> Quest = FSFQuestCatalog::Get().Find(ScenarioQuestId());
    if (!Line || !Quest) { return; }
    // The tracker line names the objective; recover its content ids from the same label rule the server used.
    FString Kind, Target;
    for (const FSFQuestStep& Step : Quest->Steps)
    { for (const FSFQuestObjective& O : Step.Objectives) { if (FSFQuestCatalog::ObjectiveLabel(O) == Line->Label) { Kind = O.Kind; Target = O.TargetId; } } }
    const FFullTarget Next = Resolve(Kind, Target, PC, FParse::Param(FCommandLine::Get(), TEXT("QuestFullEconomy")));
    const FString Key = View->Step + TEXT("/") + Line->Label + TEXT("/") + Next.Describe();
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
        auto* Builder = Cast<ASFPlayerController>(PC);
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
        const FBox Bounds = NodeActor->GetComponentsBoundingBox(true);
        bool bHit = false;
        for (const FVector Aim : {NodeActor->GetActorLocation(), Bounds.GetCenter(), Bounds.GetCenter() + FVector(0, 0, Bounds.GetExtent().Z * 0.5)})
        {
            if (Gathering->InteractFromRay(Eye, (Aim - Eye).GetSafeNormal())) { bHit = true; break; }
        }
        FullAttemptAt = Now;
        UE_LOG(LogTemp, Display, TEXT("QUEST_FULL_ACTION Objective=%s Input=InteractFromRay Hit=%d"), *Key, bHit);
    }
#endif
}

void USFQuestPilotComponent::TickBoardScenario(double Now)
{
#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Development fixture (Test-QuestPilot.ps1 -Leg Board). -QuestBoardScenario: the server stands the pawn at the Silverbrook
    // board; the client opens it and presses Accept on The Mill's Supply, then harvests three oak through the full driver.
    // -QuestNpcScenario (after a restart): the client abandons the restored quest (the journal's RPC), talks to Oswin and picks
    // "I'll help" (the dialogue's own accept). The server only places the pawn; every request goes through the normal entry points.
    const FString Mill = TEXT("quest/mills_supply");
    const bool bBoard = FParse::Param(FCommandLine::Get(), TEXT("QuestBoardScenario"));
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn) { return; }
    AActor* Source = nullptr;
    if (bBoard) { for (TActorIterator<ASFQuestBoard> It(GetWorld()); It && !Source; ++It) { Source = *It; } }
    else { for (TActorIterator<ASFDialogueNpc> It(GetWorld()); It && !Source; ++It) { if (It->DialogueFile == TEXT("millwarden_oswin.json")) { Source = *It; } } }
    if (!Source) { return; }
    const FVector Front = Source->GetActorRotation().RotateVector(bBoard ? FVector(0, 1, 0) : FVector(1, 0, 0));
    if (GetOwner()->HasAuthority())
    {
        if (bBoard && Find(Mill)) { TickFullServer(); return; }
        if (FullTeleportKey == TEXT("source") || USFCharacterSessionComponent::IsRestoring(PC) || !bRestored) { return; }
        Pawn->SetActorLocation(Source->GetActorLocation() + Front * 200 + FVector(0, 0, 100), false, nullptr, ETeleportType::TeleportPhysics);
        FullTeleportKey = TEXT("source");
        UE_LOG(LogTemp, Display, TEXT("QUEST_SCENARIO_PLACED Source=%s At=%s"), bBoard ? TEXT("board") : TEXT("npc"), *Pawn->GetActorLocation().ToString());
        return;
    }
    if (!PC->IsLocalController() || bFullDone) { return; }
    const FSFQuestView* View = FindView(Mill);
    if (bBoard && View)
    {
        if (View->Lines.IsValidIndex(0) && View->Lines[0].Count >= 3)
        { bFullDone = true; UE_LOG(LogTemp, Display, TEXT("QUEST_BOARD_SCENARIO_HARVESTED Count=%d"), View->Lines[0].Count); return; }
        TickFullClient(Now);
        return;
    }
    if (FVector::Dist2D(Pawn->GetActorLocation(), Source->GetActorLocation()) > 300) { return; }  // not placed yet
    PC->SetControlRotation((Source->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D().Rotation());
    if (Now < FullStableSince) { return; }
    FullStableSince = Now + 10;  // one try every 10 s until the replicated log shows the result
    if (bBoard)
    {
        if (!USFQuestBoardPanel::TryOpenBoard(PC)) { UE_LOG(LogTemp, Display, TEXT("QUEST_SCENARIO_WAIT Board=not_in_reach")); return; }
        USFQuestPilotSubsystem* Quests = GetWorld()->GetSubsystem<USFQuestPilotSubsystem>();
        if (!Quests || !Quests->Panel) { return; }
        Quests->Panel->Select(Mill);
        Quests->Panel->CallFunctionByNameWithArguments(TEXT("HandlePrimary"), *GLog, nullptr, true);  // the Accept button
        UE_LOG(LogTemp, Display, TEXT("QUEST_SCENARIO_ACTION Input=BoardAccept"));
        return;
    }
    auto* Player = Cast<ASFPlayerController>(PC);
    if (!Player) { return; }
    if (View && View->Lines.IsValidIndex(0) && View->Lines[0].Count > 0 && FullAttempts == 0)
    { ++FullAttempts; ServerAbandonQuest(Mill); UE_LOG(LogTemp, Display, TEXT("QUEST_SCENARIO_ACTION Input=JournalAbandon Count=%d"), View->Lines[0].Count); return; }
    if (View && FullAttempts > 0) { bFullDone = true; UE_LOG(LogTemp, Display, TEXT("QUEST_NPC_SCENARIO_DONE Counts=%d/%d"), View->Lines[0].Count, View->Lines[0].Target); return; }
    if (View || FullAttempts == 0) { return; }
    Player->SFTalk();
    Player->SFDialogueChoose(1);  // greeting: "I'll help"
    UE_LOG(LogTemp, Display, TEXT("QUEST_SCENARIO_ACTION Input=DialogueAccept"));
#endif
}

FString USFQuestPilotComponent::ActiveObjectiveKind() const
{
    for (const FSFQuestView& View : Views)
    {
        if (View.bComplete || !IsTracked(View.QuestId)) { continue; }
        const TSharedPtr<const FSFQuestDefinition> Quest = FSFQuestCatalog::Get().Find(View.QuestId);
        if (!Quest || !Quest->Steps.IsValidIndex(View.StepNumber - 1)) { continue; }
        const TArray<FSFQuestObjective>& Objectives = Quest->Steps[View.StepNumber - 1].Objectives;
        for (int32 Index = 0; Index < Objectives.Num() && Index < View.Lines.Num(); ++Index)
        { if (View.Lines[Index].Count < View.Lines[Index].Target) { return Objectives[Index].Kind; } }
    }
    return FString();
}

// ---------------------------------------------------------------- subsystem

bool USFQuestPilotSubsystem::IsRequested()
{
    return GetDefault<USFQuestPilotSettings>()->bEnabled;  // GM-01: typed config, no launch flag
}

bool USFQuestPilotSubsystem::IsQuestMap(const UWorld* World)
{
    return World && World->GetOutermost()->GetName() == TEXT("/Game/Frontier/Maps/Greenlands/Greenlands_Silverbrook");
}

void USFQuestPilotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UWorld* World = GetWorld();
    bActive = IsRequested() && IsQuestMap(World);
    if (!bActive || World->GetNetMode() == NM_Client) { return; }
    UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_READY Quests=%d"), FSFQuestCatalog::Get().Quests.Num());
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
    BoarHandle = SFQuestPilotEvents::OnBoarDefeated().AddWeakLambda(this, [this](APlayerController* PC, const FString& DeathId,
        int32 EligiblePartySize, int32 ReferencePartyLevel)
    { Route(PC, TEXT("defeat"), TEXT("npc/boar"), FString(), TEXT("boar:") + DeathId, EligiblePartySize, ReferencePartyLevel); });
}

void USFQuestPilotSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    if (bActive) { PlaceFromLayout(InWorld); }
}

void USFQuestPilotSubsystem::PlaceFromLayout(UWorld& World)
{
    // QST-02: the town layout data places the quest boards and quest givers (silverbrook-town-layout.json quest_boards,
    // quest_givers), on the server and on each client alike, so map rebuilds keep them and no binary map edit is needed.
    const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("RuntimeData/Greenlands/silverbrook-town-layout.json")));
    FString Json; TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(Json, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root)
    { UE_LOG(LogTemp, Error, TEXT("QUEST_LAYOUT_MISSING %s"), *Path); return; }
    const auto Read = [](const TSharedPtr<FJsonObject>& Row, const TCHAR* Field, FVector2D& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
        if (!Row->TryGetArrayField(Field, Pair) || Pair->Num() != 2) { return false; }
        Out = FVector2D((*Pair)[0]->AsNumber(), (*Pair)[1]->AsNumber()) * 100.0;
        return true;
    };
    const auto Place = [&World](AActor* Actor, const FVector2D& At, const FVector2D& Facing, const FVector& Front)
    {
        // Rest on the first static surface under the column; the actor's front axis turns to the facing point.
        FHitResult Ground; FCollisionQueryParams Query(SCENE_QUERY_STAT(SFQuestPlace), false, Actor);
        const FVector Column(At.X, At.Y, 0.0);
        const bool bHit = World.LineTraceSingleByChannel(Ground, Column + FVector(0, 0, 100000), Column - FVector(0, 0, 100000), ECC_WorldStatic, Query);
        const FVector2D Look = Facing - At;
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Look.Y, Look.X)) - FMath::RadiansToDegrees(FMath::Atan2(Front.Y, Front.X));
        Actor->SetActorLocationAndRotation(bHit ? Ground.ImpactPoint : Column, FRotator(0.f, Yaw, 0.f));
        return bHit;
    };
    const TArray<TSharedPtr<FJsonValue>>* Boards = nullptr;
    if (Root->TryGetArrayField(TEXT("quest_boards"), Boards))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Boards)
        {
            const TSharedPtr<FJsonObject> Row = Value->AsObject(); FVector2D At, Facing; FString Id;
            if (!Row || !Row->TryGetStringField(TEXT("id"), Id) || !QuestValidId(Id, TEXT("board")) || !Read(Row, TEXT("at"), At) || !Read(Row, TEXT("facing"), Facing)) { continue; }
            ASFQuestBoard* Board = World.SpawnActor<ASFQuestBoard>();
            if (!Board) { continue; }
            Board->BoardId = Id;
            const bool bGround = Place(Board, At, Facing, FVector(0, 1, 0));  // the imported notice board shows its notices on +Y
            UE_LOG(LogTemp, Display, TEXT("QUEST_BOARD_PLACED Id=%s At=%s Ground=%d"), *Id, *Board->GetActorLocation().ToString(), bGround);
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* Givers = nullptr;
    if (Root->TryGetArrayField(TEXT("quest_givers"), Givers))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Givers)
        {
            const TSharedPtr<FJsonObject> Row = Value->AsObject(); FVector2D At, Facing; FString Npc;
            if (!Row || !Row->TryGetStringField(TEXT("npc"), Npc) || !QuestValidId(Npc, TEXT("npc")) || !Read(Row, TEXT("at"), At) || !Read(Row, TEXT("facing"), Facing)) { continue; }
            ASFDialogueNpc* Giver = World.SpawnActor<ASFDialogueNpc>();
            if (!Giver) { continue; }
            Giver->DialogueFile = Stem(Npc) + TEXT(".json");
            const bool bGround = Place(Giver, At, Facing, FVector(1, 0, 0));
            UE_LOG(LogTemp, Display, TEXT("QUEST_GIVER_PLACED Npc=%s At=%s Ground=%d"), *Npc, *Giver->GetActorLocation().ToString(), bGround);
        }
    }
}

void USFQuestPilotSubsystem::Deinitialize()
{
    SFQuestPilotEvents::OnGathered().Remove(GatheredHandle);
    SFQuestPilotEvents::OnStructurePlaced().Remove(PlacedHandle);
    SFQuestPilotEvents::OnBoarDefeated().Remove(BoarHandle);
    Super::Deinitialize();
}

void USFQuestPilotSubsystem::Route(APlayerController* PC, const FString& Kind, const FString& Target, const FString& Node,
    const FString& Operation, int32 EligiblePartySize, int32 ReferencePartyLevel)
{
    auto* Component = IsValid(PC) && PC->GetWorld() == GetWorld() ? PC->FindComponentByClass<USFQuestPilotComponent>() : nullptr;
    if (!Component) { return; }
    // Gather/craft broadcasts carry no id of their own; a GUID keeps each accepted grant distinct.
    const bool bHasOwnId = Operation.Contains(TEXT(":")) || Operation.Contains(TEXT("/"));  // "boar:<death>", "gather/<guid>"
    Component->Observe(Kind, Target, Node, bHasOwnId ? Operation : FString::Printf(TEXT("%s:%s"), *Operation, *FGuid::NewGuid().ToString(EGuidFormats::Short)),
        EligiblePartySize, ReferencePartyLevel);
}

void USFQuestPilotSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World || !bActive) { return; }
    if (World->GetNetMode() != NM_Client)
    {
        for (auto It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (IsValid(PC) && !PC->FindComponentByClass<USFQuestPilotComponent>())
            {
                auto* Component = NewObject<USFQuestPilotComponent>(PC, TEXT("QuestPilot"));
                Component->RegisterComponent();
            }
        }
    }
    APlayerController* Local = World->GetFirstPlayerController();
    auto* Component = Local && Local->IsLocalController() ? Local->FindComponentByClass<USFQuestPilotComponent>() : nullptr;
    if (!Component) { return; }
    USFQuestBoardPanel::TickClient(Local, Component);
    // Up to three tracked quests, unfinished first; an offer the dialogue board shows takes the tracker while it is open.
    TArray<FSFQuestView> Shown;
    if (bOfferShown) { Shown.Add(OfferView); }
    for (int32 Pass = 0; Pass < 2 && !bOfferShown; ++Pass)
    {
        for (const FSFQuestView& View : Component->GetViews())
        { if (View.bComplete == (Pass == 1) && Component->IsTracked(View.QuestId) && Shown.Num() < USFQuestPilotComponent::MaxTracked) { Shown.Add(View); } }
    }
    if (const ASFPlayerController* SFPlayer = Cast<ASFPlayerController>(Local)) { if (USFHudWidget* HudWidget = SFPlayer->GetHudWidget()) { HudWidget->PresentQuests(Shown); } }
}
