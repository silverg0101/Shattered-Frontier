#pragma once

#include "UObject/Object.h"
#include "SFWidgetAuthoring.generated.h"

// Editor-only seeding of the WBP_ assets (tools/ui/create_widgets.py calls BuildAll). It writes each Widget Blueprint with a
// first-draft layout close to the old code-built look and the BindWidget names its C++ base expects. The owner then rearranges the
// layout in the UMG Designer; an existing WBP is never touched unless bOverwrite is set, because that discards the owner's layout.
UCLASS()
class SHATTEREDFRONTIER_API USFWidgetAuthoring : public UObject
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    // Creates (or with bOverwrite rebuilds) every shared and HUD WBP under /Game/Frontier/UI/Widgets/, compiles and saves them.
    // Only (a WBP name) limits it to that one asset. Returns one line per asset.
    UFUNCTION(BlueprintCallable, Category = "SF UI|Authoring")
    static FString BuildAll(bool bOverwrite, const FString& Only = TEXT(""));
    // One line per widget of a WBP's tree (indent = depth), for checking a Designer layout from a script.
    UFUNCTION(BlueprintCallable, Category = "SF UI|Authoring")
    static FString DumpTree(const FString& AssetPath);
#endif
};
