#include "Gathering/SFGatheringHud.h"

#include "Framework/SFHudStyle.h"
#include "Gathering/SFGatheringProgression.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryItemDefinition.h"
#endif

void FSFToastQueue::Push(const FString& Text, double Now, double LifetimeSeconds)
{
    Items.Add({Text, Now + LifetimeSeconds});
}

TArray<FString> FSFToastQueue::Visible(double Now)
{
    Items.RemoveAll([Now](const FToast& Toast) { return Toast.ExpiresAt <= Now; });
    TArray<FString> Out;
    for (int32 I = FMath::Max(0, Items.Num() - MaxVisible); I < Items.Num(); ++I) { Out.Add(Items[I].Text); }
    return Out;
}

float USFGatheringHud::RingFraction(double RemainingSeconds, double TotalSeconds)
{
    return TotalSeconds > 0.0 ? static_cast<float>(FMath::Clamp(1.0 - RemainingSeconds / TotalSeconds, 0.0, 1.0)) : 0.f;
}

FSFRecipeView USFGatheringHud::RecipeFor(const FSFStationRecipe& Recipe, const FString& StationTitle, float RefineSeconds)
{
    FSFRecipeView View;
    if (Recipe.Inputs.IsEmpty() || Recipe.Outputs.IsEmpty()) { return View; }
    View.bValid = true; View.Id = Recipe.Id; View.Title = StationTitle; View.Seconds = RefineSeconds;
    TArray<FString> Inputs;
    for (const FSFItemStack& Input : Recipe.Inputs) { Inputs.Add(Input.Quantity > 1 ? FString::Printf(TEXT("%s x%lld"), *SFStationRecipes::ItemDisplayName(Input.DefinitionId), Input.Quantity) : SFStationRecipes::ItemDisplayName(Input.DefinitionId)); }
    View.InputLabel = FString::Join(Inputs, TEXT(" + "));
    View.OutputLabel = SFStationRecipes::ItemDisplayName(Recipe.Outputs[0].DefinitionId);
    View.InputItemPath = SFStationRecipes::ItemAssetPath(Recipe.Inputs[0].DefinitionId);
    View.OutputItemPath = SFStationRecipes::ItemAssetPath(Recipe.Outputs[0].DefinitionId);
    for (const FSFItemStack& Input : Recipe.Inputs) { View.Inputs.Emplace(SFStationRecipes::ItemDisplayName(Input.DefinitionId), static_cast<int32>(Input.Quantity)); }
    View.InputCount = static_cast<int32>(Recipe.Inputs[0].Quantity);
    View.OutputCount = static_cast<int32>(Recipe.Outputs[0].Quantity);
    return View;
}

void USFGatheringHud::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    using namespace SFHudStyle;

    if (const FSFHudKitElement* RingArt = Kit(TEXT("ring_frame"))) { RingBrush = KitBrush(*RingArt); bHasRingBrush = true; }
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
    WidgetTree->RootWidget = Root;
    auto Place = [Root](UWidget* Child, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
    {
        UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Child);
        Slot->SetAnchors(Anchors); Slot->SetAlignment(Alignment); Slot->SetPosition(Position); Slot->SetAutoSize(true);
        return Slot;
    };

    // Toast stack, 84-99% x 42-62% (hud-layout-v4.md): newest on top, max 3, kit toast card with an icon well on the left.
    UVerticalBox* ToastBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    for (int32 I = 0; I < FSFToastQueue::MaxVisible; ++I)
    {
        UBorder* Frame = KitPanel(WidgetTree, TEXT("toast_card"), 1.f, Slate(0.92f), Brass(), FMargin(64.f, 4.f, 10.f, 4.f), 3.f);
        UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Lines->AddChildToVerticalBox(Text(WidgetTree, TEXT("Skill gain"), 12, Brass()));
        UTextBlock* Label = Text(WidgetTree, TEXT(""), 16, Ivory(), true);
        Lines->AddChildToVerticalBox(Label);
        Frame->SetContent(Lines); Frame->SetVisibility(ESlateVisibility::Collapsed);
        USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        CardSize->SetHeightOverride(54.f); CardSize->SetContent(Frame);
        ToastBox->AddChildToVerticalBox(CardSize)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
        ToastFrames.Add(Frame); ToastTexts.Add(Label);
    }
    UCanvasPanelSlot* ToastSlot = Root->AddChildToCanvas(ToastBox);
    ToastSlot->SetAnchors(FAnchors(0.84f, 0.42f, 0.99f, 0.62f)); ToastSlot->SetOffsets(FMargin(0.f));

    // Progress ring labels; the ring itself is painted in NativePaint.
    RingPercent = Text(WidgetTree, TEXT(""), 15, Ivory(), true);
    RingLabel = Text(WidgetTree, TEXT(""), 15, Ivory(), true);
    RingPercent->SetVisibility(ESlateVisibility::Collapsed); RingLabel->SetVisibility(ESlateVisibility::Collapsed);
    Place(RingPercent, FAnchors(0.f, 0.f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
    Place(RingLabel, FAnchors(0.f, 0.f), FVector2D(0.5f, 0.f), FVector2D::ZeroVector);

    // Station recipe panel: inputs -> output, bottom centre above the build status.
    RecipeFrame = KitPanel(WidgetTree, TEXT("panel_frame"), 0.96f, Slate(0.94f), Brass(), FMargin(10.f, 6.f), 4.f);
    RecipeFrame->SetVisibility(ESlateVisibility::Collapsed);
    UVerticalBox* RecipeBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RecipeFrame->SetContent(RecipeBox);
    RecipeTitle = Heading(WidgetTree, TEXT(""), 22, Ivory());
    RecipeBox->AddChildToVerticalBox(RecipeTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    RecipeBox->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
    auto AddStack = [&](TObjectPtr<UImage>& Icon, TObjectPtr<UTextBlock>& Count)
    {
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        Box->SetWidthOverride(56.f); Box->SetHeightOverride(56.f);
        UBorder* Slot = KitPanel(WidgetTree, TEXT("slot_empty"), 1.f, Hex(TEXT("0E171C"), 0.85f), Hex(TEXT("3A4348")), FMargin(0.f), 3.f);
        Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Slot->SetContent(Icon); Box->SetContent(Slot);
        Row->AddChildToHorizontalBox(Box)->SetVerticalAlignment(VAlign_Center);
        Count = Text(WidgetTree, TEXT(""), 18, Ivory(), true);
        UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(Count);
        CountSlot->SetVerticalAlignment(VAlign_Center); CountSlot->SetPadding(FMargin(8.f, 0.f, 8.f, 0.f));
    };
    AddStack(RecipeInIcon, RecipeInText);
    UHorizontalBoxSlot* ArrowSlot = Row->AddChildToHorizontalBox(Text(WidgetTree, TEXT("->"), 24, Brass(), true));
    ArrowSlot->SetVerticalAlignment(VAlign_Center); ArrowSlot->SetPadding(FMargin(6.f, 0.f, 14.f, 0.f));
    AddStack(RecipeOutIcon, RecipeOutText);
    // Have / need per input, with a tick or cross glyph and words: never colour alone (hud-layout-v4.md).
    NeedRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
    RecipeBox->AddChildToVerticalBox(NeedRows)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
    RecipeFooter = Text(WidgetTree, TEXT(""), 13, Hex(TEXT("A9A593")));
    RecipeBox->AddChildToVerticalBox(RecipeFooter);
    Place(RecipeFrame, FAnchors(0.5f, 1.f, 0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -230.f));
}

void USFGatheringHud::ShowRecipe(const FSFRecipeView& Recipe)
{
    RecipeFrame->SetVisibility(Recipe.bValid ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!Recipe.bValid) { RecipeShownId.Reset(); return; }
    if (Recipe.Id == RecipeShownId) { return; }
    RecipeShownId = Recipe.Id;
    RecipeTitle->SetText(FText::FromString(Recipe.Title));
    RecipeInText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), Recipe.InputCount)));
    RecipeOutText->SetText(FText::FromString(FString::Printf(TEXT("x%d"), Recipe.OutputCount)));
    RecipeFooter->SetText(FText::FromString(FString::Printf(TEXT("%s  ->  %s  |  %.1f s"), *Recipe.InputLabel, *Recipe.OutputLabel, Recipe.Seconds)));
    NeedRows->ClearChildren(); NeedNames.Reset(); NeedAmounts.Reset(); NeedHaveTexts.Reset(); NeedGlyphs.Reset(); NeedUnderlines.Reset();
    using namespace SFHudStyle;
    for (const TPair<FString, int32>& Input : Recipe.Inputs)
    {
        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Text(WidgetTree, Input.Key, 15, Ivory()));
        NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetVerticalAlignment(VAlign_Center);
        // The short number gets a crimson underline in addition to the glyph and the "short" wording.
        UVerticalBox* HaveColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        UTextBlock* Have = Text(WidgetTree, TEXT(""), 15, Ivory(), true);
        HaveColumn->AddChildToVerticalBox(Have);
        UBorder* Underline = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
        Underline->SetBrush(Frame(Crimson(), Crimson(), 0.f)); Underline->SetVisibility(ESlateVisibility::Hidden);
        USizeBox* UnderlineSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        UnderlineSize->SetHeightOverride(2.f); UnderlineSize->SetContent(Underline);
        HaveColumn->AddChildToVerticalBox(UnderlineSize);
        UHorizontalBoxSlot* HaveSlot = Line->AddChildToHorizontalBox(HaveColumn);
        HaveSlot->SetVerticalAlignment(VAlign_Center); HaveSlot->SetPadding(FMargin(14.f, 0.f, 8.f, 0.f));
        UImage* Glyph = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
        Glyph->SetDesiredSizeOverride(FVector2D(22.f, 22.f));
        Line->AddChildToHorizontalBox(Glyph)->SetVerticalAlignment(VAlign_Center);
        NeedRows->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.f, 1.f));
        NeedNames.Add(Input.Key); NeedAmounts.Add(Input.Value); NeedHaveTexts.Add(Have); NeedGlyphs.Add(Glyph); NeedUnderlines.Add(Underline);
    }
#if SF_WITH_MINIMAL_INVENTORY
    auto SetIcon = [](UImage* Image, const FString& Path)
    {
        const UInventoryItemDefinition* Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(Path)).LoadSynchronous();
        if (UTexture2D* Texture = Definition ? Definition->Icon.LoadSynchronous() : nullptr) { Image->SetBrushFromTexture(Texture, false); }
    };
    SetIcon(RecipeInIcon, Recipe.InputItemPath); SetIcon(RecipeOutIcon, Recipe.OutputItemPath);
#endif
}

void USFGatheringHud::UpdateNeeds()
{
    if (!Pilot || NeedNames.IsEmpty()) { return; }
    using namespace SFHudStyle;
    const TArray<TPair<FText,int32>> Items = Pilot->GetInventoryItems();
    for (int32 I = 0; I < NeedNames.Num(); ++I)
    {
        int32 Have = 0;
        for (const TPair<FText,int32>& Item : Items) { const FString Name = Item.Key.ToString(); if (Name == NeedNames[I]) { Have += Item.Value; } }
        const bool bMet = NeedMet(Have, NeedAmounts[I]);
        NeedHaveTexts[I]->SetText(FText::FromString(NeedText(Have, NeedAmounts[I]) + (bMet ? FString() : TEXT("  short"))));
        NeedHaveTexts[I]->SetColorAndOpacity(FSlateColor(bMet ? Ivory() : Crimson()));
        NeedUnderlines[I]->SetVisibility(bMet ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
        if (const FSFHudKitElement* Glyph = Kit(bMet ? TEXT("status_tick") : TEXT("status_cross")))
        { if (NeedGlyphs[I]->GetBrush().GetResourceObject() != Glyph->Texture) { NeedGlyphs[I]->SetBrush(KitBrush(*Glyph)); } }
    }
}

void USFGatheringHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    APlayerController* Controller = GetOwningPlayer();
    UWorld* World = GetWorld();
    if (!Pilot || !Pilot->IsEnabled() || !Controller || !World) { return; }
    using namespace SFHudStyle;
    const FSFGatheringPilotFeedback& Feedback = Pilot->GetFeedback();
    const double Now = World->GetTimeSeconds();
    const AGameStateBase* GameState = World->GetGameState();
    const double ServerNow = GameState ? GameState->GetServerWorldTimeSeconds() : Now;

    if (Feedback.bSkillGained && Feedback.Phase == ESFGatheringPilotPhase::Committed && Feedback.Operation != LastToastOperation)
    {
        LastToastOperation = Feedback.Operation;
        Toasts.Push(SkillToastText(FSFGatheringProgressionSession::SkillName(Feedback.Skill)), Now);
    }
    const TArray<FString> Visible = Toasts.Visible(Now);
    for (int32 I = 0; I < ToastFrames.Num(); ++I)
    {
        const bool bShown = Visible.IsValidIndex(I);
        ToastFrames[I]->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (UWidget* Card = ToastFrames[I]->GetParent()) { Card->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }  // the fixed-height wrapper
        if (bShown) { ToastTexts[I]->SetText(FText::FromString(Visible[I])); }
    }

    // Ring: the total is the first remaining time seen for an operation (the replicated feedback only carries the end time).
    const bool bHarvesting = Feedback.Phase == ESFGatheringPilotPhase::Harvesting && IsValid(Feedback.Node);
    // Committing = the channel ended and the server is awaiting the ledger receipt: keep the ring full as "finishing".
    const bool bFinishing = Feedback.Phase == ESFGatheringPilotPhase::Committing && IsValid(Feedback.Node);
    FVector2D Screen = FVector2D::ZeroVector;
    bRingVisible = (bHarvesting || bFinishing) && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Feedback.Node->GetActorLocation() + FVector(0, 0, 90), Screen, false);
    if (bHarvesting)
    {
        const double Remaining = FMath::Max(0.0, Feedback.EndsAtServerSeconds - ServerNow);
        if (RingOperation != Feedback.Operation) { RingOperation = Feedback.Operation; RingTotalSeconds = Remaining; }
        RingTotalSeconds = FMath::Max(RingTotalSeconds, Remaining);
        RingProgress = RingFraction(Remaining, RingTotalSeconds);
    }
    else if (bFinishing) { RingProgress = 1.f; }
    for (UTextBlock* Label : {RingPercent.Get(), RingLabel.Get()}) { Label->SetVisibility(bRingVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (bRingVisible)
    {
        RingCenter = Screen;
        RingPercent->SetText(FText::FromString(bFinishing ? FString(TEXT("...")) : FString::Printf(TEXT("%d%%"), FMath::RoundToInt(RingProgress * 100.f))));
        RingLabel->SetText(FText::FromString(Feedback.Node->GetResourceName()));
        if (UCanvasPanelSlot* PercentSlot = Cast<UCanvasPanelSlot>(RingPercent->Slot)) { PercentSlot->SetPosition(Screen); }
        if (UCanvasPanelSlot* LabelSlot = Cast<UCanvasPanelSlot>(RingLabel->Slot)) { LabelSlot->SetPosition(Screen + FVector2D(0.0, 44.0)); }
    }

    // Recipe panel while aiming at a station with no harvest running.
    FSFRecipeView Recipe;
    if (!bHarvesting && !bFinishing)
    {
        FVector Start, Direction;
        if (!Controller->DeprojectMousePositionToWorld(Start, Direction)) { FRotator Rotation; Controller->GetPlayerViewPoint(Start, Rotation); Direction = Rotation.Vector(); }
        const ASFGatheringPilotNode* Node = Pilot->FindAimedNode(Start, Direction);
        if (Node && Node->IsRefinery())
        {
            const FString Station = USFGatheringPilotComponent::StationName(Node->Kind);
            const FSFStationRecipe* Chosen = SFStationRecipes::Find(Pilot->GetStationRecipe(Node->Kind));
            if (!Chosen) { const TArray<const FSFStationRecipe*> All = SFStationRecipes::ForStation(Station); Chosen = All.IsEmpty() ? nullptr : All[0]; }
            if (Chosen) { Recipe = RecipeFor(*Chosen, Station.Left(1).ToUpper() + Station.Mid(1), GetDefault<USFGatheringPilotSettings>()->RefineSeconds); }
        }
    }
    ShowRecipe(Recipe);
    UpdateNeeds();
}

int32 USFGatheringHud::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
    if (!bRingVisible) { return LayerId; }
    constexpr float Radius = 35.f;  // the track of the 88 px kit ring
    if (bHasRingBrush)
    {
        const FVector2f Size(88.f, 88.f), Corner(static_cast<float>(RingCenter.X) - 44.f, static_cast<float>(RingCenter.Y) - 44.f);
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Corner)), &RingBrush, ESlateDrawEffect::None, FLinearColor::White);
    }
    constexpr int32 Segments = 64;
    auto Arc = [this](float Fraction, TArray<FVector2f>& Points)
    {
        const int32 Count = FMath::Max(1, FMath::CeilToInt(Segments * Fraction));
        for (int32 I = 0; I <= Count; ++I)
        {
            const float Angle = -HALF_PI + TWO_PI * Fraction * I / Count;
            Points.Add(FVector2f(static_cast<float>(RingCenter.X) + Radius * FMath::Cos(Angle), static_cast<float>(RingCenter.Y) + Radius * FMath::Sin(Angle)));
        }
    };
    TArray<FVector2f> Track, Progress;
    Arc(1.f, Track); Arc(FMath::Max(RingProgress, 0.01f), Progress);
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(), Track, ESlateDrawEffect::None, SFHudStyle::Hex(TEXT("0E171C"), 0.85f), true, 9.f);
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 3, AllottedGeometry.ToPaintGeometry(), Progress, ESlateDrawEffect::None, SFHudStyle::Brass(), true, 5.f);
    return LayerId + 3;
}
