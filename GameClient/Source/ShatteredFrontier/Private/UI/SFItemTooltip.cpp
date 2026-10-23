#include "UI/SFItemTooltip.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Framework/SFHudStyle.h"
#include "Gathering/SFStationRecipes.h"
#if SF_WITH_MINIMAL_INVENTORY
#include "InventoryItemDefinition.h"
#endif

namespace SFItemTooltip
{
UTexture2D* ItemTexture(const FString& ItemId)
{
    FString Stem = ItemId; int32 Slash = INDEX_NONE;
    if (ItemId.FindLastChar(TEXT('/'), Slash)) { Stem = ItemId.Mid(Slash + 1); }
    if (const FSFHudKitElement* Element = SFHudStyle::Kit(*(TEXT("icon_") + Stem))) { return Element->Texture; }
    return LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Frontier/UI/Icons/T_Icon_%s.T_Icon_%s"), *Stem, *Stem), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FSFInventoryEntry EntryFor(const FString& ItemId, const FString& Name, int32 Count)
{
    FSFInventoryEntry Entry;
#if SF_WITH_MINIMAL_INVENTORY
    if (const UInventoryItemDefinition* Definition = LoadObject<UInventoryItemDefinition>(nullptr, *SFStationRecipes::ItemAssetPath(ItemId), nullptr, LOAD_NoWarn | LOAD_Quiet))
    { Entry = USFInventoryWidget::MakeEntry(*Definition, Name, Count, false); }
#endif
    if (Entry.Name.IsEmpty()) { Entry.Name = FText::FromString(Name); Entry.Count = Count; }
    if (Entry.Icon.IsNull()) { if (UTexture2D* Art = ItemTexture(ItemId)) { Entry.Icon = Art; } }
    return Entry;
}

void Present(const FSFItemTooltipParts& Parts, const FSFInventoryEntry* Entry, const FVector2D& CursorPos)
{
    if (!Parts.Root) { return; }
    if (!Entry) { Parts.Root->SetVisibility(ESlateVisibility::Collapsed); return; }
    // Follow the cursor: translate the tooltip's holder (its parent) within the holder's own parent, offset down-right of the pointer.
    UWidget* Holder = Parts.Root->GetParent() ? static_cast<UWidget*>(Parts.Root->GetParent()) : Parts.Root;
    if (const UWidget* Space = Holder->GetParent())
    {
        Holder->SetRenderTranslation(Space->GetCachedGeometry().AbsoluteToLocal(CursorPos) + FVector2D(12.f, 12.f));
    }
    const FSFInventoryTooltip Tip = USFInventoryWidget::BuildTooltip(*Entry);
    auto Set = [](UTextBlock* Block, const FString& Value) { if (Block) { Block->SetText(FText::FromString(Value)); } };
    Set(Parts.Title, Tip.Title); Set(Parts.Rarity, Tip.RarityLine); Set(Parts.Type, Tip.TypeLine); Set(Parts.Description, Tip.Description); Set(Parts.Weight, Tip.WeightLine);
    if (Parts.Quantity) { Parts.Quantity->SetText(FText::FromString(FString::Printf(TEXT("Owned  %d"), Entry->Count))); }
    if (Parts.Description) { Parts.Description->SetVisibility(Tip.Description.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    if (Parts.Icon)
    {
        UTexture2D* Art = Entry->Icon.LoadSynchronous();
        if (Art) { Parts.Icon->SetBrushFromTexture(Art, false); }
        Parts.Icon->SetVisibility(Art ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    Parts.Root->SetVisibility(ESlateVisibility::HitTestInvisible);
}
}
