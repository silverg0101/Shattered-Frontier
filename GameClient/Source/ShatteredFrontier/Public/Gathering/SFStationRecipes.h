#pragma once

#include "CoreMinimal.h"

// Typed station recipes from content/crafting (the same file CraftingContent and Gameplay.Host load), and the pilot
// item catalogue rule that maps a definition id to its bag asset. Code reads the data; content owns the numbers.
struct FSFItemStack { FString DefinitionId; int64 Quantity = 0; bool bQualityInput = false; };

struct FSFStationRecipe
{
    FString Id;          // recipe/iron_nails
    FString Station;     // sawmill | forge | workbench
    FString Skill;
    int32 Difficulty = 0;
    bool bDefaultKnown = true;
    FString LearnHint;
    TArray<FString> Trainers;
    TArray<FSFItemStack> Inputs;
    TArray<FSFItemStack> Outputs;
    // The single output's display line, e.g. "8 Iron Nails".
    FString Describe() const;
};

namespace SFStationRecipes
{
    // Parses and validates recipe JSON: ids, stations, positive quantities, unique recipe ids.
    SHATTEREDFRONTIER_API bool Parse(const FString& Json, TArray<FSFStationRecipe>& Out, FString& Error);
    // content/crafting/recipes_greenlands_starter.json, loaded once per process; empty when the file is missing.
    SHATTEREDFRONTIER_API const TArray<FSFStationRecipe>& Shipped();
    // Recipes for one station in content order (the first is the station default).
    SHATTEREDFRONTIER_API TArray<const FSFStationRecipe*> ForStation(const FString& Station);
    SHATTEREDFRONTIER_API const FSFStationRecipe* Find(const FString& RecipeId);
    // item/iron_nails -> /Game/Frontier/Gameplay/Pilot/DA_IronNails.DA_IronNails
    SHATTEREDFRONTIER_API FString ItemAssetPath(const FString& DefinitionId);
    // item/iron_nails -> Iron Nails
    SHATTEREDFRONTIER_API FString ItemDisplayName(const FString& DefinitionId);
}
