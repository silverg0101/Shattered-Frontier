#include "Modules/ModuleManager.h"
#include "Framework/SFLoadingScreen.h"

class FShatteredFrontierModule : public FDefaultGameModuleImpl
{
    virtual void StartupModule() override { SFLoadingScreen::Register(); }
    virtual void ShutdownModule() override { SFLoadingScreen::Unregister(); }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FShatteredFrontierModule, ShatteredFrontier, "ShatteredFrontier");
