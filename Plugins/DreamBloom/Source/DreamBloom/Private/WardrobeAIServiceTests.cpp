#include "WardrobeAIServiceLifecycle.h"
#include "WardrobeAISpeechSubsystem.h"
#include "WardrobeAISpeech.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformMisc.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAIServiceReadinessTest,"DreamBloom.AI.Services.Readiness",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAIServiceReadinessTest::RunTest(const FString&)
{
 auto* GI=NewObject<UGameInstance>();auto* Service=NewObject<UWardrobeAISpeechSubsystem>(GI);auto* Profile=NewObject<UWardrobeAISpeechData>(GI);
 Service->ServiceProfile=Profile;Profile->bEnabled=true;Profile->bUseAstrBot=true;
 Service->bTTSReady=false;Service->bAstrBotReady=false;
 TestFalse(TEXT("Open ports alone never allow entry"),Service->AreServicesReady());
 Service->bTTSOnline=true;Service->bAstrBotOnline=true;
 TestEqual(TEXT("Online ports are only half the service checkpoints"),Service->GetServiceProgress(),.5f);
 Service->bTTSReady=true;TestFalse(TEXT("Companion initialization still blocks entry"),Service->AreServicesReady());
 Service->bAstrBotReady=true;TestTrue(TEXT("Both verified services permit entry"),Service->AreServicesReady());
 TestEqual(TEXT("Completed checkpoints reach one"),Service->GetServiceProgress(),1.f);
 Service->bFaceRequired=true;TestFalse(TEXT("Failed face model cannot bypass its gate"),Service->IsFaceReady());
 Service->bTTSReady=false;Profile->bEnabled=false;TestTrue(TEXT("Disabled AI needs no service process"),Service->AreServicesReady());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAIProcessClientsTest,"DreamBloom.AI.Services.SharedProcessLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAIProcessClientsTest::RunTest(const FString&)
{
#if PLATFORM_WINDOWS
 const FString Key=TEXT("Automation.SharedService"),Exe=FPlatformMisc::GetEnvironmentVariable(TEXT("SystemRoot"))/TEXT("System32/cmd.exe");
 int32 First=0,Second=0;
 WardrobeAIServiceLifecycle::RegisterClient(Key,&First);WardrobeAIServiceLifecycle::RegisterClient(Key,&Second);
 TestTrue(TEXT("Launch owned background process"),WardrobeAIServiceLifecycle::Launch(Key,&First,Exe,TEXT("/c ping -n 30 127.0.0.1 >nul"),FPaths::ProjectDir()));
 WardrobeAIServiceLifecycle::ReleaseClient(&First);TestTrue(TEXT("Other PIE client retains shared process"),WardrobeAIServiceLifecycle::IsRunning(Key));
 WardrobeAIServiceLifecycle::ReleaseClient(&Second);TestFalse(TEXT("Last client releases and stops process"),WardrobeAIServiceLifecycle::IsRunning(Key));
#endif
 return true;
}
#endif
