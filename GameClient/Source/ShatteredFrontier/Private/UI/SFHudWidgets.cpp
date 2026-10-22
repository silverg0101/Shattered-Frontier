#include "UI/SFHudWidgets.h"

#include "Characters/SFE1VitalsHud.h"
#include "Combat/SFCombatPilot.h"
#include "Framework/SFPlayerController.h"
#include "Townsfolk/SFTownsfolk.h"
#include "Components/WrapBox.h"
#include "Framework/SFScreenArt.h"
#include "Gathering/SFGatheringPilot.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

using namespace SFHudStyle;

void FSFToastQueue::Push(const FString& Message, double Now, double LifetimeSeconds)
{
    Items.Add({Message, Now + LifetimeSeconds});
}

TArray<FString> FSFToastQueue::Visible(double Now)
{
    Items.RemoveAll([Now](const FToast& Toast) { return Toast.ExpiresAt <= Now; });
    TArray<FString> Out;
    for (int32 I = FMath::Max(0, Items.Num() - MaxVisible); I < Items.Num(); ++I) { Out.Add(Items[I].Text); }
    return Out;
}

// ---- unit frame

FText USFUnitFrameWidget::ValueText(int32 Current, int32 Max)
{
    return FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::Max(0, Current), Max));
}

UMaterialInstanceDynamic* USFUnitFrameWidget::FillMaterial(UImage* Bar)
{
    return Bar && Bar->GetBrush().GetResourceObject() ? Bar->GetDynamicMaterial() : nullptr;
}

void USFUnitFrameWidget::SetHealth(int32 Current, int32 Max)
{
    if (UMaterialInstanceDynamic* Mid = FillMaterial(HealthBar)) { Mid->SetScalarParameterValue(TEXT("Percent"), FSFE1VitalsModel::HealthFraction(Current, Max)); }
    if (HealthText) { HealthText->SetText(ValueText(Current, Max)); }
}

void USFUnitFrameWidget::SetStamina(int32 Current, int32 Max)
{
    if (UMaterialInstanceDynamic* Mid = FillMaterial(StaminaBar)) { Mid->SetScalarParameterValue(TEXT("Percent"), FSFE1VitalsModel::HealthFraction(Current, Max)); }
    if (StaminaText) { StaminaText->SetText(ValueText(Current, Max)); }
}

void USFUnitFrameWidget::SetLevel(int32 Level)
{
    // No level (a target that carries none): the numeral collapses instead of showing a made-up one.
    if (!LevelText) { return; }
    LevelText->SetVisibility(Level > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (Level > 0) { LevelText->SetText(FText::AsNumber(Level)); }
}

void USFUnitFrameWidget::SetName(const FText& Name)
{
    if (NameText) { NameText->SetText(Name); }
}

void USFUnitFrameWidget::ApplyParts()
{
    // Off = Collapsed. On = restored only when a toggle collapsed it, so Designer visibility (e.g. hit-test settings) is left alone.
    auto Show = [](UWidget* Part, bool bShow)
    {
        if (!Part) { return; }
        if (!bShow) { Part->SetVisibility(ESlateVisibility::Collapsed); }
        else if (Part->GetVisibility() == ESlateVisibility::Collapsed) { Part->SetVisibility(ESlateVisibility::SelfHitTestInvisible); }
    };
    Show(BadgeRoot ? Cast<UWidget>(BadgeRoot) : Cast<UWidget>(LevelText), bShowBadge);
    Show(NameText, bShowName);
    Show(StaminaRow, bShowStamina);
    if (!StaminaRow) { Show(StaminaBar, bShowStamina); }
    Show(StaminaText, bShowStamina && bShowValues);
    Show(HealthText, bShowValues);
}

void USFUnitFrameWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    ApplyParts();  // at creation too: PreConstruct only runs once the widget is built for Slate
}

void USFUnitFrameWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    ApplyParts();
}

void USFUnitFrameWidget::ApplyKitStyle()
{
    // A bar image the owner already gave a material keeps it; otherwise it gets the kit's masked-fill instance.
    auto StyleOne = [](UImage* Bar, const TSoftObjectPtr<UMaterialInterface>& Material)
    {
        if (Bar && !Bar->GetBrush().GetResourceObject()) { if (UMaterialInterface* Loaded = Material.LoadSynchronous()) { Bar->SetBrushResourceObject(Loaded); } }
    };
    StyleOne(HealthBar, HealthFillMaterial);
    StyleOne(StaminaBar, StaminaFillMaterial);
    // Text: only the project font family where the Designer left the default (the runtime composite font cannot be picked in the Designer);
    // the Designer's size and colour stay.
    const FSFUiTextStyle FamilyOnly;
    for (UTextBlock* Text : {HealthText.Get(), StaminaText.Get(), LevelText.Get(), NameText.Get()}) { FamilyOnly.Apply(Text); }
}

// ---- hotbar

TArray<FSFHotbarSlot> USFHudHotbarWidget::Slots()
{
    return {{1, TEXT("build_wall"), TEXT("Wall")}, {2, TEXT("build_foundation"), TEXT("Foundation")}, {3, TEXT("build_roof"), TEXT("Roof")},
        {4, TEXT("build_door"), TEXT("Door")}, {5, TEXT("iron_axe"), TEXT("Axe")}, {6, TEXT("iron_pickaxe"), TEXT("Pickaxe")}};
}

int32 USFHudHotbarWidget::ActiveToolKey(const TArray<TPair<FText, int32>>& InventoryItems)
{
    for (const TPair<FText, int32>& Item : InventoryItems)
    {
        const FString Name = Item.Key.ToString();
        if (!Name.EndsWith(TEXT(" (equipped)"))) { continue; }
        if (Name.Contains(TEXT("Pickaxe"))) { return 6; }
        if (Name.Contains(TEXT("Axe"))) { return 5; }
    }
    return 0;
}

int32 USFHudHotbarWidget::StackCount(const TArray<TPair<FText, int32>>& InventoryItems, const FString& Label)
{
    int32 Total = 0;
    for (const TPair<FText, int32>& Item : InventoryItems) { if (Item.Key.ToString().Contains(Label)) { Total += Item.Value; } }
    return Total;
}

void USFHudHotbarWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!SlotRow) { return; }
    const TArray<FSFHotbarSlot> Defs = Slots();
    for (UWidget* Child : SlotRow->GetAllChildren())
    {
        USFSlotWidget* Cell = Cast<USFSlotWidget>(Child);
        if (!Cell || !Defs.IsValidIndex(Cells.Num())) { continue; }
        const FSFHotbarSlot& Def = Defs[Cells.Num()];
        Cell->SetKeyText(FText::AsNumber(Def.Key));
        Cell->SetIcon(LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Icons/T_Icon_%s.T_Icon_%s"), *Def.IconStem, *Def.IconStem), nullptr, LOAD_NoWarn | LOAD_Quiet));
        Cells.Add(Cell);
    }
}

void USFHudHotbarWidget::Present(const TArray<TPair<FText, int32>>& InventoryItems)
{
    const int32 Key = ActiveToolKey(InventoryItems);
    if (Key != ActiveKey)
    {
        ActiveKey = Key;
        for (int32 Index = 0; Index < Cells.Num(); ++Index) { Cells[Index]->SetSelected(Index + 1 == Key); }
    }
    const TArray<FSFHotbarSlot> Defs = Slots();
    for (int32 Index = 0; Index < Cells.Num() && Index < Defs.Num(); ++Index) { Cells[Index]->SetCount(StackCount(InventoryItems, Defs[Index].Label)); }
}

// ---- minimap

FString USFHudMinimapWidget::ZoneFromMapName(const FString& MapName)
{
    int32 Underscore = INDEX_NONE;
    return MapName.FindLastChar(TEXT('_'), Underscore) ? MapName.Mid(Underscore + 1) : MapName;
}

FString USFHudMinimapWidget::FooterText(const FString& Zone, int32 Metres)
{
    return Metres < 0 ? Zone : FString::Printf(TEXT("%s · %d m"), *Zone, Metres);
}

void USFHudMinimapWidget::SetFooter(const FString& Line)
{
    if (ZoneText) { ZoneText->SetText(FText::FromString(Line)); }
}

void USFHudMinimapWidget::ApplyKitStyle()
{
    ZoneStyle.Apply(ZoneText);
}

void USFHudMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    // Footer: "<zone> · <n> m" while a pawn exists (distance to the nearest gathering node), else the zone. Once a second.
    // ponytail: quest objectives name a node id that the node actors do not expose, so the nearest node stands in for the tracked one.
    APlayerController* Controller = GetOwningPlayer();
    UWorld* World = GetWorld();
    if (!ZoneText || !Controller || !World || World->GetTimeSeconds() < NextFooterSeconds) { return; }
    NextFooterSeconds = World->GetTimeSeconds() + 1.0;
    if (ZoneName.IsEmpty()) { ZoneName = ZoneFromMapName(UGameplayStatics::GetCurrentLevelName(this, true)); }
    int32 Metres = -1;
    if (const APawn* Pawn = Controller->GetPawn())
    {
        // Gather objective: nearest gathering node; otherwise the nearest node of any kind, else the world origin as the zone centre.
        const USFQuestPilotComponent* Quest = Controller->FindComponentByClass<USFQuestPilotComponent>();
        const bool bGatherObjective = Quest && Quest->ActiveObjectiveKind() == TEXT("gather");
        const FVector PawnAt = Pawn->GetActorLocation();
        double Nearest = TNumericLimits<double>::Max();
        for (TActorIterator<ASFGatheringPilotNode> It(World); It; ++It)
        { if (!bGatherObjective || !It->IsRefinery()) { Nearest = FMath::Min(Nearest, FVector::Dist2D(It->GetActorLocation(), PawnAt)); } }
        if (Nearest == TNumericLimits<double>::Max()) { Nearest = FVector::Dist2D(FVector::ZeroVector, PawnAt); }  // ponytail: no zone-centre marker exists; the world origin stands in
        Metres = FMath::RoundToInt(Nearest / 100.0);
    }
    SetFooter(FooterText(ZoneName, Metres));
}

// ---- quest tracker

void USFHudQuestTrackerWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!RowClass) { RowClass = LoadClass<USFListRowWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Hud/WBP_HudQuestRow.WBP_HudQuestRow_C")); }
    SetVisibility(ESlateVisibility::Collapsed);
}

int32 USFHudQuestTrackerWidget::RowCount() const { return Rows ? Rows->GetChildrenCount() : 0; }

void USFHudQuestTrackerWidget::Present(const TArray<FSFQuestView>& Shown)
{
    if (!Rows || !RowClass) { return; }
    FString Key;
    for (const FSFQuestView& View : Shown)
    {
        Key += FString::Printf(TEXT("#%s|%s|%d|%d|%d"), *View.Title, *View.Step, View.StepNumber, View.bComplete, View.bRewardWaiting);
        for (const FSFQuestLine& L : View.Lines) { Key += FString::Printf(TEXT("|%s=%d/%d"), *L.Label, L.Count, L.Target); }
        for (const FString& Reward : View.RewardLines) { Key += TEXT("|") + Reward; }
    }
    if (Key == LastKey) { return; }
    LastKey = Key;
    // One line per shown quest; the runners match "Step=<n> Complete=<0|1>" for the pilot chain.
    for (const FSFQuestView& View : Shown)
    {
        FString Counts; for (const FSFQuestLine& L : View.Lines) { Counts += FString::Printf(TEXT("%s%d/%d"), Counts.IsEmpty() ? TEXT("") : TEXT(","), L.Count, L.Target); }
        UE_LOG(LogTemp, Display, TEXT("QUEST_PILOT_CLIENT_VIEW Step=%d Complete=%d Quest=%s Counts=%s Waiting=%d"), View.StepNumber, View.bComplete, *View.QuestId, *Counts, View.bRewardWaiting);
    }
    SetVisibility(Shown.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    Rows->ClearChildren();
    auto AddRow = [this](const FString& Label, const FString& Count, bool bDone, bool bHeading, const FLinearColor& Tint)
    {
        USFListRowWidget* Row = CreateWidget<USFListRowWidget>(this, RowClass);
        if (!Row) { return; }
        Row->SetEntry(FText::FromString(Label), FText::FromString(Count), bDone, bHeading);
        if (!bHeading) { Row->SetTint(Tint, Tint); }
        Rows->AddChildToVerticalBox(Row);
    };
    for (const FSFQuestView& View : Shown)
    {
        AddRow(View.Title, FString(), false, true, Ivory());
        for (const FSFQuestLine& L : View.Lines) { const bool bDone = L.Count >= L.Target; AddRow(L.Label, FString::Printf(TEXT("%d / %d"), L.Count, L.Target), bDone, false, bDone ? Moss() : Ivory()); }
        if (View.bComplete) { AddRow(TEXT("Complete"), FString(), true, false, Moss()); }
        if (View.bRewardWaiting) { AddRow(TEXT("Reward waiting: make room"), FString(), false, false, Amber()); }
        for (const FString& Reward : View.RewardLines) { AddRow(TEXT("Received ") + Reward, FString(), true, false, Moss()); }
    }
}

// ---- toasts

const TCHAR* USFHudToastsWidget::IconStem(const FString& ToastText)
{
    if (ToastText.StartsWith(TEXT("Mining"))) { return TEXT("iron_pickaxe"); }
    if (ToastText.StartsWith(TEXT("Herbalism"))) { return TEXT("river_herb"); }
    if (ToastText.StartsWith(TEXT("Fishing"))) { return TEXT("trout"); }
    return TEXT("woodcutting_axes");
}

void USFHudToastsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!CardRows) { return; }
    for (UWidget* Child : CardRows->GetAllChildren())
    {
        if (USFListRowWidget* Card = Cast<USFListRowWidget>(Child)) { Card->SetCaption(CaptionText); Card->SetVisibility(ESlateVisibility::Collapsed); Cards.Add(Card); }
    }
}

void USFHudToastsWidget::PushToast(const FString& Message)
{
    Queue.Push(Message, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
    Refresh(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

int32 USFHudToastsWidget::VisibleCards() const
{
    int32 Count = 0;
    for (const USFListRowWidget* Card : Cards) { Count += Card->GetVisibility() != ESlateVisibility::Collapsed ? 1 : 0; }
    return Count;
}

void USFHudToastsWidget::Refresh(double Now)
{
    const TArray<FString> Visible = Queue.Visible(Now);
    for (int32 I = 0; I < Cards.Num(); ++I)
    {
        const bool bShown = Visible.IsValidIndex(I);
        Cards[I]->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (!bShown) { continue; }
        const FString& Toast = Visible[Visible.Num() - 1 - I];  // newest on top
        FString Name, Delta;
        if (!Toast.Split(TEXT(" "), &Name, &Delta, ESearchCase::CaseSensitive, ESearchDir::FromEnd)) { Name = Toast; }
        Cards[I]->SetEntry(FText::FromString(Name), FText::FromString(Delta));
        Cards[I]->SetTint(Ivory(), Moss());
        Cards[I]->SetRowIcon(SFScreenArt::Icon(IconStem(Toast), 48.f));
    }
}

void USFHudToastsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (GetWorld()) { Refresh(GetWorld()->GetTimeSeconds()); }
}

// ---- root

void USFHudWidget::PresentVitals(int32 Health, int32 MaxHealth, int32 Stamina, int32 MaxStamina)
{
    if (Vitals) { Vitals->SetVitals(Health, MaxHealth, Stamina, MaxStamina); }
}

void USFHudWidget::PresentLevel(int32 Level)
{
    if (Vitals) { Vitals->SetLevel(Level); }
}

void USFHudWidget::SyncLevel()
{
    if (Pilot && Pilot->GetCharacterLevel() != ShownLevel) { ShownLevel = Pilot->GetCharacterLevel(); PresentLevel(ShownLevel); }
}

void USFHudWidget::PresentTarget(bool bShow, const FString& Name, int32 Level, int32 Health, int32 MaxHealth)
{
    if (!Target) { return; }
    Target->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bShow) { return; }
    Target->SetName(FText::FromString(Name));
    Target->SetLevel(Level);
    Target->SetHealth(Health, MaxHealth);
}

USFUnitFrameWidget* USFHudWidget::MakeFrame(TSubclassOf<USFUnitFrameWidget>& Cached, const TCHAR* Path)
{
    if (!Cached) { Cached = LoadClass<USFUnitFrameWidget>(nullptr, Path); }
    return Cached ? CreateWidget<USFUnitFrameWidget>(this, Cached) : nullptr;
}

void USFHudWidget::AddPartyMember(const FString& Name, int32 Level, int32 Health, int32 MaxHealth, int32 Stamina, int32 MaxStamina, bool bRaid)
{
    USFUnitFrameWidget* Frame = bRaid ? MakeFrame(RaidFrameClass, TEXT("/Game/Frontier/UI/Widgets/Unit/WBP_RaidFrame.WBP_RaidFrame_C"))
                                      : MakeFrame(PartyFrameClass, TEXT("/Game/Frontier/UI/Widgets/Unit/WBP_PartyFrame.WBP_PartyFrame_C"));
    UPanelWidget* Host = bRaid ? static_cast<UPanelWidget*>(RaidGrid) : static_cast<UPanelWidget*>(PartyList);
    if (!Frame || !Host) { return; }
    Frame->SetName(FText::FromString(Name));
    Frame->SetLevel(Level);
    Frame->SetVitals(Health, MaxHealth, Stamina, MaxStamina);
    Host->AddChild(Frame);
    Host->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void USFHudWidget::ClearParty()
{
    if (PartyList) { PartyList->ClearChildren(); PartyList->SetVisibility(ESlateVisibility::Collapsed); }
    if (RaidGrid) { RaidGrid->ClearChildren(); RaidGrid->SetVisibility(ESlateVisibility::Collapsed); }
}

int32 USFHudWidget::PartyCount() const
{
    return (PartyList ? PartyList->GetChildrenCount() : 0) + (RaidGrid ? RaidGrid->GetChildrenCount() : 0);
}

void USFHudWidget::PresentQuests(const TArray<FSFQuestView>& Shown)
{
    if (QuestTracker) { QuestTracker->Present(Shown); }
}

void USFHudWidget::PushToast(const FString& Message)
{
    if (Toasts) { Toasts->PushToast(Message); }
}

void USFHudWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    SyncLevel();
    if (Hotbar) { Hotbar->Present(Pilot && Pilot->IsEnabled() ? Pilot->GetInventoryItems() : TArray<TPair<FText, int32>>()); }
    // Target frame: the controller's selected combat target (boar or townsfolk), hidden while the station panel is open.
    const ASFPlayerController* Player = Cast<ASFPlayerController>(GetOwningPlayer());
    const AActor* CombatTarget = Player ? Player->GetSelectedCombatTarget() : nullptr;
    int32 Health = 0, MaxHealth = 0;
    FString Name;
    if (const ASFPilotBoar* Boar = Cast<ASFPilotBoar>(CombatTarget))
    { Health = Boar->Health(); MaxHealth = GetDefault<USFCombatPilotSettings>()->BoarHealth; Name = TEXT("Boar"); }
    else if (const ASFTownsfolkNpc* Npc = Cast<ASFTownsfolkNpc>(CombatTarget))
    { Health = Npc->Health(); MaxHealth = GetDefault<USFTownsfolkSettings>()->ResidentHealth; Name = Npc->Id(); }
    // Targets carry no level yet: level 0 collapses the target badge numeral.
    PresentTarget(CombatTarget && !Player->IsStationPanelOpen(), Name, 0, Health, MaxHealth);
}
