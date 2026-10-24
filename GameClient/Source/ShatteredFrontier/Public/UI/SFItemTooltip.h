#pragma once

#include "UI/SFInventoryWidget.h"

class UImage;
class UTextBlock;
class UTexture2D;
class UWidget;

// The item tooltip every menu window shares (inventory, market, trade, recipes): the same named parts in each WBP (Tooltip, TipIcon, TipTitle,
// TipRarity, TipType, TipQuantity, TipWeight, TipDescription), shown beside the cursor and outside the window. C++ owns the text; the WBP
// owns the look.
struct FSFItemTooltipParts
{
    UWidget* Root = nullptr;  // the Tooltip widget; its parent holder is moved to the cursor
    UImage* Icon = nullptr;
    UTextBlock* Title = nullptr; UTextBlock* Rarity = nullptr; UTextBlock* Type = nullptr;
    UTextBlock* Quantity = nullptr; UTextBlock* Weight = nullptr; UTextBlock* Description = nullptr;
};

namespace SFItemTooltip
{
    // Item art for "item/oak_log": the kit's icon_<stem> piece, else the item icon T_Icon_<stem>; null when neither exists.
    SHATTEREDFRONTIER_API UTexture2D* ItemTexture(const FString& ItemId);
    // Tooltip entry for an item id: the item definition when it exists (description, weight, category), else the name and count only.
    SHATTEREDFRONTIER_API FSFInventoryEntry EntryFor(const FString& ItemId, const FString& Name, int32 Count);
    // Shows Entry beside CursorPos (screen space); null hides the tooltip.
    SHATTEREDFRONTIER_API void Present(const FSFItemTooltipParts& Parts, const FSFInventoryEntry* Entry, const FVector2D& CursorPos);
}
