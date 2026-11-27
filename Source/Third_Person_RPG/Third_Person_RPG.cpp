#include "Third_Person_RPG.h"
#include "Modules/ModuleManager.h"
#if WITH_EDITOR
#include "UI/InlineInputPrompts.h"
#include "Misc/CoreDelegates.h"
#endif

class FThirdPersonRPGModule : public FDefaultGameModuleImpl
{
#if WITH_EDITOR
    virtual void StartupModule() override
    {
        FCoreDelegates::OnPostEngineInit.AddRaw(this, &FThirdPersonRPGModule::InstallPrompts);
    }
    virtual void ShutdownModule() override
    {
        FCoreDelegates::OnPostEngineInit.RemoveAll(this);
    }
    void InstallPrompts()
    {
        if (GIsEditor && (!IsRunningCommandlet() || FParse::Param(FCommandLine::Get(), TEXT("InstallInputPrompts"))))
            InstallDesignerInputPrompts();
    }
#endif
};
IMPLEMENT_PRIMARY_GAME_MODULE(FThirdPersonRPGModule, Third_Person_RPG, "Third_Person_RPG");
