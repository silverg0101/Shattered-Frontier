#include "Characters/SFGraphAuthoring.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "UObject/UObjectHash.h"

UEdGraph* USFGraphAuthoring::FindGraph(UObject* Asset)
{
    TArray<UObject*> Inner;
    if (Asset) { GetObjectsWithOuter(Asset, Inner); }
    for (UObject* Object : Inner) { if (UEdGraph* Graph = Cast<UEdGraph>(Object)) { return Graph; } }
    return nullptr;
}

TArray<UEdGraphNode*> USFGraphAuthoring::GetNodes(UEdGraph* Graph)
{
    return Graph ? TArray<UEdGraphNode*>(Graph->Nodes) : TArray<UEdGraphNode*>();
}

UEdGraphNode* USFGraphAuthoring::AddNode(UEdGraph* Graph, UClass* NodeClass, FVector2D Position)
{
    if (!Graph || !NodeClass || !NodeClass->IsChildOf<UEdGraphNode>()) { return nullptr; }
    Graph->Modify();
    UEdGraphNode* Node = NewObject<UEdGraphNode>(Graph, NodeClass, NAME_None, RF_Transactional);
    Node->CreateNewGuid();
    Node->NodePosX = Position.X;
    Node->NodePosY = Position.Y;
    Graph->AddNode(Node, false, false);
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
    return Node;
}

void USFGraphAuthoring::RemoveNode(UEdGraphNode* Node)
{
    if (Node && Node->GetGraph()) { Node->GetGraph()->RemoveNode(Node); }
}

void USFGraphAuthoring::ReconstructNode(UEdGraphNode* Node)
{
    if (Node) { Node->ReconstructNode(); }
}

FString USFGraphAuthoring::DescribePins(UEdGraphNode* Node)
{
    FString Out;
    for (const UEdGraphPin* Pin : Node ? Node->Pins : TArray<UEdGraphPin*>())
    {
        Out += FString::Printf(TEXT("%s %s %s\n"), Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"), *Pin->PinName.ToString(), *Pin->PinType.PinCategory.ToString());
    }
    return Out;
}

bool USFGraphAuthoring::Link(UEdGraphNode* From, const FString& FromPin, UEdGraphNode* To, const FString& ToPin)
{
    UEdGraphPin* A = From ? From->FindPin(*FromPin) : nullptr;
    UEdGraphPin* B = To ? To->FindPin(*ToPin) : nullptr;
    if (!A || !B)
    {
        UE_LOG(LogTemp, Warning, TEXT("SFGraphAuthoring: no pin %s.%s or %s.%s"), *GetNameSafe(From), *FromPin, *GetNameSafe(To), *ToPin);
        return false;
    }
    const bool bLinked = From->GetSchema()->TryCreateConnection(A, B);
    if (!bLinked) { UE_LOG(LogTemp, Warning, TEXT("SFGraphAuthoring: schema refused %s.%s -> %s.%s"), *GetNameSafe(From), *FromPin, *GetNameSafe(To), *ToPin); }
    return bLinked;
}
#endif
