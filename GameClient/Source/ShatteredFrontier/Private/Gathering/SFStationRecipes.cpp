#include "Gathering/SFStationRecipes.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    bool ValidId(const FString& Id, const TCHAR* Prefix)
    {
        FString Head, Tail;
        if (!Id.Split(TEXT("/"), &Head, &Tail) || Head != Prefix || Tail.IsEmpty()) { return false; }
        for (TCHAR C : Tail) { if (!(C == '_' || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9'))) { return false; } }
        return true;
    }

    bool ParseStacks(const TSharedPtr<FJsonObject>& Recipe, const TCHAR* Field, TArray<FSFItemStack>& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
        if (!Recipe->TryGetArrayField(Field, Values) || Values->IsEmpty()) { return false; }
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject> Stack = Value ? Value->AsObject() : nullptr;
            FSFItemStack Item; double Quantity = 0;
            if (!Stack || !Stack->TryGetStringField(TEXT("definition_id"), Item.DefinitionId) || !Stack->TryGetNumberField(TEXT("quantity"), Quantity)
                || !ValidId(Item.DefinitionId, TEXT("item")) || Quantity < 1 || Quantity > 1000 || Quantity != FMath::FloorToDouble(Quantity)) { return false; }
            Item.Quantity = int64(Quantity);
            Stack->TryGetBoolField(TEXT("quality_input"), Item.bQualityInput);
            Out.Add(Item);
        }
        return true;
    }

    FString PascalTail(const FString& DefinitionId, bool bSpaced)
    {
        FString Tail; DefinitionId.Split(TEXT("/"), nullptr, &Tail);
        FString Out; bool bStart = true;
        for (TCHAR C : Tail)
        {
            if (C == '_') { if (bSpaced) { Out.AppendChar(' '); } bStart = true; continue; }
            Out.AppendChar(bStart ? FChar::ToUpper(C) : C); bStart = false;
        }
        return Out;
    }
}

FString FSFStationRecipe::Describe() const
{
    return Outputs.Num() == 1 ? FString::Printf(TEXT("%lld %s"), Outputs[0].Quantity, *SFStationRecipes::ItemDisplayName(Outputs[0].DefinitionId)) : Id;
}

bool SFStationRecipes::Parse(const FString& Json, TArray<FSFStationRecipe>& Out, FString& Error)
{
    TSharedPtr<FJsonObject> Root; const TArray<TSharedPtr<FJsonValue>>* Recipes = nullptr;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root || !Root->TryGetArrayField(TEXT("recipes"), Recipes))
    { Error = TEXT("recipes missing"); return false; }
    TArray<FSFStationRecipe> Parsed; TSet<FString> Ids;
    for (const TSharedPtr<FJsonValue>& Value : *Recipes)
    {
        const TSharedPtr<FJsonObject> R = Value ? Value->AsObject() : nullptr;
        FSFStationRecipe Recipe;
        if (!R || !R->TryGetStringField(TEXT("id"), Recipe.Id) || !R->TryGetStringField(TEXT("station"), Recipe.Station)
            || !ValidId(Recipe.Id, TEXT("recipe")) || Ids.Contains(Recipe.Id)
            || !(Recipe.Station == TEXT("sawmill") || Recipe.Station == TEXT("forge") || Recipe.Station == TEXT("workbench"))
            || !ParseStacks(R, TEXT("inputs"), Recipe.Inputs) || !ParseStacks(R, TEXT("outputs"), Recipe.Outputs))
        { Error = FString::Printf(TEXT("recipe invalid: %s"), *Recipe.Id); return false; }
        if (!R->TryGetStringField(TEXT("skill"), Recipe.Skill) || !R->TryGetNumberField(TEXT("difficulty"), Recipe.Difficulty))
        { Error = FString::Printf(TEXT("recipe skill missing: %s"), *Recipe.Id); return false; }
        const TSharedPtr<FJsonObject>* Learn = nullptr;
        if (!R->TryGetObjectField(TEXT("learn"), Learn)) { Error = TEXT("recipe learning missing"); return false; }
        const TArray<TSharedPtr<FJsonValue>>* Sources = nullptr;
        if (!(*Learn)->TryGetArrayField(TEXT("sources"), Sources)) { Error = TEXT("recipe sources missing"); return false; }
        Recipe.bDefaultKnown = Sources->ContainsByPredicate([](const TSharedPtr<FJsonValue>& V) { return V->AsString() == TEXT("default"); });
        Recipe.LearnHint = Sources->ContainsByPredicate([](const TSharedPtr<FJsonValue>& V) { return V->AsString() == TEXT("trainer"); })
            ? TEXT("Learn from a trainer") : Sources->ContainsByPredicate([](const TSharedPtr<FJsonValue>& V) { return V->AsString() == TEXT("scroll"); })
            ? TEXT("Learn from a recipe scroll") : TEXT("Discover through practice");
        const TArray<TSharedPtr<FJsonValue>>* Trainers = nullptr;
        if ((*Learn)->TryGetArrayField(TEXT("trainers"), Trainers))
        { for (const auto& Trainer : *Trainers) { Recipe.Trainers.Add(Trainer->AsString()); } }
        Ids.Add(Recipe.Id);
        Parsed.Add(MoveTemp(Recipe));
    }
    Out = MoveTemp(Parsed);
    return true;
}

const TArray<FSFStationRecipe>& SFStationRecipes::Shipped()
{
    static const TArray<FSFStationRecipe> Recipes = []()
    {
        TArray<FSFStationRecipe> Loaded; FString Text, Error;
        const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../content/crafting/recipes_greenlands_starter.json")));
        if (!FFileHelper::LoadFileToString(Text, *Path) || !Parse(Text, Loaded, Error))
        { UE_LOG(LogTemp, Warning, TEXT("STATION_RECIPES_UNAVAILABLE %s %s"), *Path, *Error); Loaded.Reset(); }
        return Loaded;
    }();
    return Recipes;
}

TArray<const FSFStationRecipe*> SFStationRecipes::ForStation(const FString& Station)
{
    TArray<const FSFStationRecipe*> Out;
    for (const FSFStationRecipe& Recipe : Shipped()) { if (Recipe.Station == Station) { Out.Add(&Recipe); } }
    return Out;
}

const FSFStationRecipe* SFStationRecipes::Find(const FString& RecipeId)
{
    return Shipped().FindByPredicate([&](const FSFStationRecipe& Recipe) { return Recipe.Id == RecipeId; });
}

FString SFStationRecipes::ItemAssetPath(const FString& DefinitionId)
{
    FString Base = DefinitionId, Grade;
    if (DefinitionId.StartsWith(TEXT("item/")))
    {
        const FString Tail = DefinitionId.Mid(5);
        FString Slug;
        if (Tail.Split(TEXT("/"), &Slug, &Grade)) { Base = TEXT("item/") + Slug; }
    }
    const FString Name = TEXT("DA_") + PascalTail(Base, false) + (Grade.IsEmpty() ? TEXT("") : TEXT("_") + PascalTail(TEXT("item/") + Grade, false));
    return FString::Printf(TEXT("/Game/Frontier/Gameplay/Pilot/%s.%s"), *Name, *Name);
}

FString SFStationRecipes::ItemDisplayName(const FString& DefinitionId)
{
    FString Base = DefinitionId, Grade;
    if (DefinitionId.StartsWith(TEXT("item/")))
    { const FString Tail = DefinitionId.Mid(5); FString Slug; if (Tail.Split(TEXT("/"), &Slug, &Grade)) { Base = TEXT("item/") + Slug; } }
    if (!Grade.IsEmpty()) { return ItemDisplayName(Base) + TEXT(" (") + PascalTail(TEXT("item/") + Grade, false) + TEXT(")"); }
    if (DefinitionId == TEXT("item/foundation_material_bundle")) { return TEXT("Foundation Bundle"); }
    if (DefinitionId == TEXT("item/wall_material_bundle")) { return TEXT("Wall Bundle"); }
    if (DefinitionId == TEXT("item/door_material_bundle")) { return TEXT("Door Bundle"); }
    if (DefinitionId == TEXT("item/roof_material_bundle")) { return TEXT("Roof Bundle"); }
    if (DefinitionId == TEXT("item/starter_tool_repair_kit")) { return TEXT("Tool Repair Kit"); }
    return PascalTail(DefinitionId, true);
}
