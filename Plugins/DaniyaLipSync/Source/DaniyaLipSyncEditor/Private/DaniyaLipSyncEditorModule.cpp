#include "Modules/ModuleManager.h"
#include "DaniyaAudioAnimation.h"
#include "ToolMenus.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "UObject/StrongObjectPtr.h"

class FDaniyaLipSyncEditorModule : public IModuleInterface
{
    TStrongObjectPtr<UDaniyaAudioAnimationTool> Settings;
public:
    virtual void StartupModule() override
    {
        if (IsRunningCommandlet()) return;
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("DaniyaAudioAnimation"),FOnSpawnTab::CreateRaw(this,&FDaniyaLipSyncEditorModule::SpawnTab))
            .SetDisplayName(FText::FromString(TEXT("Daniya Audio Animation"))).SetMenuType(ETabSpawnerMenuType::Hidden);
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FDaniyaLipSyncEditorModule::RegisterMenu));
    }
    virtual void ShutdownModule() override
    {
        if (IsRunningCommandlet()) return;
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("DaniyaAudioAnimation"));
        if (Settings.IsValid()) Settings->Cancel();
        Settings.Reset();
    }
    void RegisterMenu()
    {
        FToolMenuOwnerScoped Owner(this);
        UToolMenu* Menu=UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
        Menu->FindOrAddSection(TEXT("Daniya")).AddMenuEntry(TEXT("DaniyaAudioAnimation"),FText::FromString(TEXT("Daniya Audio Animation")),
            FText::FromString(TEXT("Generate a saved MMD mouth/expression animation directly from audio.")),FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FName(TEXT("DaniyaAudioAnimation"))); })));
    }
    TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs&)
    {
        if (!Settings.IsValid()) Settings.Reset(NewObject<UDaniyaAudioAnimationTool>());
        FDetailsViewArgs Args;
        Args.bAllowSearch=false; Args.bHideSelectionTip=true;
        auto View=FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateDetailView(Args);
        View->SetObject(Settings.Get());
        return SNew(SDockTab).TabRole(ETabRole::NomadTab)[View];
    }
};
IMPLEMENT_MODULE(FDaniyaLipSyncEditorModule,DaniyaLipSyncEditor)
