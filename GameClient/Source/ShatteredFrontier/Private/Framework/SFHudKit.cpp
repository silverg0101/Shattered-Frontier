#include "Framework/SFHudKit.h"

#include "UObject/StrongObjectPtr.h"

const USFHudKit* USFHudKit::Get()
{
    static TStrongObjectPtr<USFHudKit> Kit;
    static bool bTried = false;
    if (!bTried && !Kit.IsValid() && GIsRunning)
    {
        bTried = true;
        Kit.Reset(LoadObject<USFHudKit>(nullptr, TEXT("/Game/Frontier/UI/Kit/DA_HudKit.DA_HudKit")));
    }
    return Kit.Get();
}
