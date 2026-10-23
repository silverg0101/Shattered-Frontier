#include "Gathering/SFStationPanel.h"

#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Gathering/SFGatheringHud.h"
#include "Gathering/SFGatheringPilot.h"
#include "Gathering/SFStationRecipes.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

FString USFStationPanel::IconStem(const FString& DefinitionId)
{
    FString Stem = DefinitionId;
    Stem.RemoveFromStart(TEXT("item/"));
    return Stem;
}

void USFStationPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetIsFocusable(false);
    RowClass = LoadClass<USFKitRowWidget>(nullptr, TEXT("/Game/Frontier/UI/Widgets/Common/WBP_KitRow.WBP_KitRow_C"));
    if (TabBar) { TabBar->SetTabs({FSFTabSpec(FText::FromString(TEXT("All"))), FSFTabSpec(FText::FromString(TEXT("Materials"))), FSFTabSpec(FText::FromString(TEXT("Parts")))}); }
    if (QuantityStepper) { QuantityStepper->SetRange(1, 1); }  // one craft per interact today
    if (CraftButton) { CraftButton->SetLabel(FText::FromString(TEXT("Craft"))); CraftButton->OnActivated.AddDynamic(this, &USFStationPanel::HandleCraft); }
    if (Tooltip) { Tooltip->SetVisibility(ESlateVisibility::Collapsed); }
}

void USFStationPanel::ApplyKitStyle()
{
    ApplyFamilyToAllText(TextStyle);
}

bool USFStationPanel::IsPointerOver() const
{
    return FSlateApplication::IsInitialized() && GetVisibility() != ESlateVisibility::Collapsed && GetCachedGeometry().IsUnderLocation(FSlateApplication::Get().GetCursorPos());
}

void USFStationPanel::HandleRecipeClicked(USFSelectableWidget* Source)
{
    const int32 Index = RecipeRows.IndexOfByPredicate([Source](const TObjectPtr<USFKitRowWidget>& Row) { return Row == Source; });
    if (RowRecipeIds.IsValidIndex(Index) && PilotRef.IsValid() && StationRef.IsValid()) { PilotRef->SelectStationRecipe(StationRef->Kind, RowRecipeIds[Index]); }
}

// The Craft button does what the interact key does: aim at the station and interact (a running craft is cancelled the same way).
void USFStationPanel::HandleCraft(USFButtonWidget*)
{
    const APlayerController* Controller = PilotRef.IsValid() ? Cast<APlayerController>(PilotRef->GetOwner()) : nullptr;
    if (!Controller || !Controller->GetPawn() || !StationRef.IsValid()) { return; }
    const FVector Start = Controller->GetPawn()->GetPawnViewLocation();
    PilotRef->InteractFromRay(Start, (StationRef->GetActorLocation() - Start).GetSafeNormal());
}

void USFStationPanel::Rebuild(const ASFGatheringPilotNode* Station, const FString& RecipeId)
{
    const FString StationId = USFGatheringPilotComponent::StationName(Station->Kind);
    const FString StationName = StationId.Left(1).ToUpper() + StationId.Mid(1);
    const TArray<const FSFStationRecipe*> Recipes = SFStationRecipes::ForStation(StationId);
    const FSFStationRecipe* Selected = SFStationRecipes::Find(RecipeId);
    if (!Selected && !Recipes.IsEmpty()) { Selected = Recipes[0]; }
    if (Window) { Window->SetTitle(FText::FromString(StationName)); }
    if (StationText) { StationText->SetText(FText::FromString(FString::Printf(TEXT("Station: %s"), *StationName))); }
    if (StationHint) { StationHint->SetText(FText::FromString(FString::Printf(TEXT("Requires a %s to craft."), *StationName))); }

    RecipeList->ClearChildren(); RecipeRows.Reset(); RowRecipeIds.Reset(); RowOutputIds.Reset();
    if (RowClass)
    {
        for (const FSFStationRecipe* Recipe : Recipes)
        {
            if (Recipe->Outputs.IsEmpty()) { continue; }
            USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
            if (!Row) { continue; }
            RecipeList->AddChild(Row);
            const FSFItemStack& Output = Recipe->Outputs[0];
            Row->SetEntry(FText::FromString(SFStationRecipes::ItemDisplayName(Output.DefinitionId)), FText::GetEmpty());
            Row->SetIconTexture(SFItemTooltip::ItemTexture(Output.DefinitionId));
            Row->SetCells(FText::AsNumber(Output.Quantity), FText::GetEmpty());  // what one craft makes
            Row->ShowChevron(true);
            Row->SetSelected(Recipe == Selected);
            Row->OnClicked.AddDynamic(this, &USFStationPanel::HandleRecipeClicked);
            RecipeRows.Add(Row); RowRecipeIds.Add(Recipe->Id); RowOutputIds.Add(Output.DefinitionId);
        }
    }
    if (EmptyText) { EmptyText->SetVisibility(RecipeRows.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }

    Ingredients->ClearChildren(); IngredientRows.Reset(); NeedIds.Reset(); NeedNames.Reset(); NeedAmounts.Reset(); HaveAmounts.Reset();
    OutputId.Reset(); OutputQuantity = 0;
    if (!Selected || Selected->Outputs.IsEmpty()) { return; }
    const FSFItemStack& Output = Selected->Outputs[0];
    OutputId = Output.DefinitionId; OutputQuantity = static_cast<int32>(Output.Quantity);
    const FString OutputDisplay = SFStationRecipes::ItemDisplayName(Output.DefinitionId);
    if (OutputName) { OutputName->SetText(FText::FromString(Output.Quantity > 1 ? FString::Printf(TEXT("%s x%lld"), *OutputDisplay, Output.Quantity) : OutputDisplay)); }
    if (OutputText) { OutputText->SetText(FText::FromString(FString::Printf(TEXT("Makes %s."), *Selected->Describe()))); }
    if (OutputIcon)
    {
        UTexture2D* Art = SFItemTooltip::ItemTexture(Output.DefinitionId);
        if (Art) { OutputIcon->SetBrushFromTexture(Art, false); }
        OutputIcon->SetVisibility(Art ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (CraftTime) { CraftTime->SetText(FText::FromString(FString::Printf(TEXT("Craft time  %s s"), *FText::AsNumber(GetDefault<USFGatheringPilotSettings>()->RefineSeconds).ToString()))); }
    if (!RowClass) { return; }
    for (const FSFItemStack& Input : Selected->Inputs)
    {
        USFKitRowWidget* Row = CreateWidget<USFKitRowWidget>(this, RowClass);
        if (!Row) { continue; }
        Ingredients->AddChild(Row);
        const FString Name = SFStationRecipes::ItemDisplayName(Input.DefinitionId);
        Row->SetEntry(FText::FromString(Name), FText::GetEmpty());
        Row->SetIconTexture(SFItemTooltip::ItemTexture(Input.DefinitionId));
        Row->SetCells(FText::FromString(USFGatheringHud::NeedText(0, static_cast<int32>(Input.Quantity))), FText::GetEmpty());
        IngredientRows.Add(Row); NeedIds.Add(Input.DefinitionId); NeedNames.Add(Name); NeedAmounts.Add(static_cast<int32>(Input.Quantity)); HaveAmounts.Add(0);
    }
}

void USFStationPanel::Present(USFGatheringPilotComponent* Pilot, const ASFGatheringPilotNode* Station)
{
    if (!Pilot || !Station) { ShownKey.Reset(); PilotRef.Reset(); StationRef.Reset(); return; }
    PilotRef = Pilot; StationRef = Station;
    const FString RecipeId = Pilot->GetStationRecipe(Station->Kind);
    const FString Key = FString::Printf(TEXT("%d|%s"), int32(Station->Kind), *RecipeId);
    if (Key != ShownKey)
    {
        ShownKey = Key; Rebuild(Station, RecipeId);
    }

    const TArray<TPair<FText, int32>> Items = Pilot->GetInventoryItems();
    for (int32 I = 0; I < NeedNames.Num() && IngredientRows.IsValidIndex(I); ++I)
    {
        int32 Have = 0;
        for (const TPair<FText, int32>& Item : Items) { if (Item.Key.ToString() == NeedNames[I]) { Have += Item.Value; } }
        HaveAmounts[I] = Have;
        // Green when the stack is covered, red when short (state tint).
        IngredientRows[I]->SetCells(FText::FromString(USFGatheringHud::NeedText(Have, NeedAmounts[I])), FText::GetEmpty());
        IngredientRows[I]->SetCellTint(USFGatheringHud::NeedMet(Have, NeedAmounts[I]) ? SFHudStyle::Hex(TEXT("8FBF6A")) : SFHudStyle::Crimson());
    }

    // Craft button: the running craft at this station fills it (the same channel the ring and the interact key show).
    if (CraftButton)
    {
        const FSFGatheringPilotFeedback& Feedback = Pilot->GetFeedback();
        const bool bCrafting = (Feedback.Phase == ESFGatheringPilotPhase::Harvesting || Feedback.Phase == ESFGatheringPilotPhase::Committing) && Feedback.Node.Get() == Station;
        float Fraction = 0.f;
        if (bCrafting)
        {
            Fraction = 1.f;
            if (Feedback.Phase == ESFGatheringPilotPhase::Harvesting)
            {
                const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
                const double Remaining = FMath::Max(0.0, Feedback.EndsAtServerSeconds - (GameState ? GameState->GetServerWorldTimeSeconds() : 0.0));
                if (CraftOperation != Feedback.Operation) { CraftOperation = Feedback.Operation; CraftTotalSeconds = Remaining; }
                CraftTotalSeconds = FMath::Max(CraftTotalSeconds, Remaining);
                Fraction = USFGatheringHud::RingFraction(Remaining, CraftTotalSeconds);
            }
        }
        CraftButton->SetProgress(Fraction);
        CraftButton->SetLabel(FText::FromString(bCrafting ? TEXT("Cancel") : TEXT("Craft")));
    }
}

void USFStationPanel::UpdateTooltip()
{
    if (!Tooltip || !FSlateApplication::IsInitialized()) { return; }
    const FVector2D Pointer = FSlateApplication::Get().GetCursorPos();
    FSFInventoryEntry Entry;
    bool bShown = false;
    for (int32 I = 0; I < RecipeRows.Num() && RowOutputIds.IsValidIndex(I) && !bShown; ++I)
    {
        if (RecipeRows[I]->GetCachedGeometry().IsUnderLocation(Pointer)) { Entry = SFItemTooltip::EntryFor(RowOutputIds[I], SFStationRecipes::ItemDisplayName(RowOutputIds[I]), 0); bShown = true; }
    }
    for (int32 I = 0; I < IngredientRows.Num() && NeedIds.IsValidIndex(I) && !bShown; ++I)
    {
        if (IngredientRows[I]->GetCachedGeometry().IsUnderLocation(Pointer)) { Entry = SFItemTooltip::EntryFor(NeedIds[I], NeedNames[I], HaveAmounts[I]); bShown = true; }
    }
    FSFItemTooltipParts Parts;
    Parts.Root = Tooltip; Parts.Icon = TipIcon; Parts.Title = TipTitle; Parts.Rarity = TipRarity; Parts.Type = TipType; Parts.Quantity = TipQuantity; Parts.Weight = TipWeight; Parts.Description = TipDescription;
    SFItemTooltip::Present(Parts, bShown ? &Entry : nullptr, Pointer);
}

void USFStationPanel::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (GetVisibility() != ESlateVisibility::Collapsed) { UpdateTooltip(); }
}
