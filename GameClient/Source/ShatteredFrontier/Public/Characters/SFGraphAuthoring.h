#pragma once

#include "UObject/Object.h"
#include "SFGraphAuthoring.generated.h"

class UEdGraph;
class UEdGraphNode;

// Editor-only node-graph wiring for scripts. Python can make graph nodes and set their properties but not add pins or link them;
// tools/Create-PlayerOutfit.py builds the player's Mutable Customizable Object graph through these.
UCLASS()
class SHATTEREDFRONTIER_API USFGraphAuthoring : public UObject
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    // The first node graph owned (at any depth) by Asset, e.g. a Customizable Object's source graph.
    UFUNCTION(BlueprintCallable, Category = "SF Graph|Authoring")
    static UEdGraph* FindGraph(UObject* Asset);
    UFUNCTION(BlueprintCallable, Category = "SF Graph|Authoring")
    static TArray<UEdGraphNode*> GetNodes(UEdGraph* Graph);
    // A new node of NodeClass with its default pins, placed at Position.
    UFUNCTION(BlueprintCallable, Category = "SF Graph|Authoring")
    static UEdGraphNode* AddNode(UEdGraph* Graph, UClass* NodeClass, FVector2D Position);
    UFUNCTION(BlueprintCallable, Category = "SF Graph|Authoring")
    static void RemoveNode(UEdGraphNode* Node);
    // Rebuilds the node's pins after a property change (a mesh's sections, a count of LODs).
    UFUNCTION(BlueprintCallable, Category = "SF Graph|Authoring")
    static void ReconstructNode(UEdGraphNode* Node);
    // One line per pin: "in|out <name> <category>", for finding pin names from a script.
    UFUNCTION(BlueprintCallable, Category = "SF Graph|Authoring")
    static FString DescribePins(UEdGraphNode* Node);
    // Connects From's pin to To's pin through the graph's schema (which enforces the pin types); false with a log line on refusal.
    UFUNCTION(BlueprintCallable, Category = "SF Graph|Authoring")
    static bool Link(UEdGraphNode* From, const FString& FromPin, UEdGraphNode* To, const FString& ToPin);
#endif
};
