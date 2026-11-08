#include "WardrobeStartup.h"
#include "WardrobeStartupInternal.h"
#include "Engine/GameInstance.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStartupGateTest,"DreamBloom.Startup.ClickGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStartupGateTest::RunTest(const FString&)
{
 auto* Instance=NewObject<UGameInstance>();auto* Startup=NewObject<UWardrobeStartupSubsystem>(Instance);
 Startup->Visual=MakeShared<FWardrobeStartupVisual>();Startup->bActive=true;
 TestFalse(TEXT("Cannot skip opening video"),Startup->EnterGame());
 Startup->Visual->SceneReady.store(true);
 TestFalse(TEXT("Scene ready does not skip video"),Startup->EnterGame());
 Startup->Visual->SceneReady.store(false);Startup->Visual->MovieDone.store(true);
 TestFalse(TEXT("Finished movie does not reveal an unprepared scene"),Startup->EnterGame());
 Startup->Visual->SceneReady.store(true);
 TestTrue(TEXT("One click enters after movie and warmup"),Startup->EnterGame());
 TestFalse(TEXT("Double click cannot enter twice"),Startup->EnterGame());
 TestTrue(TEXT("Remember intro completion for this game instance"),Startup->bCompleted);
 TestFalse(TEXT("Movie state released"),Startup->Visual.IsValid());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStartupAspectTest,"DreamBloom.Startup.Letterbox",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStartupAspectTest::RunTest(const FString&)
{
 TestTrue(TEXT("16:9 fills exact screen"),WardrobeStartup::FitVideo(FVector2D(1920,1080),16.f/9.f).Equals(FVector2D(1920,1080),.01));
 TestTrue(TEXT("16:10 screen has top and bottom black bars"),WardrobeStartup::FitVideo(FVector2D(1920,1200),16.f/9.f).Equals(FVector2D(1920,1080),.01));
 TestTrue(TEXT("Ultrawide preserves video with side bars"),WardrobeStartup::FitVideo(FVector2D(2560,1080),16.f/9.f).Equals(FVector2D(1920,1080),.01));
 TestEqual(TEXT("Minimized viewport is safe"),WardrobeStartup::FitVideo(FVector2D::ZeroVector,16.f/9.f),FVector2D::ZeroVector);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStartupPathsTest,"DreamBloom.Startup.PortableMoviePath",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStartupPathsTest::RunTest(const FString&)
{
 const FString Content=FPaths::ProjectContentDir();
 const FString Expected=FPaths::ConvertRelativePathToFull(Content/TEXT("Movies/开场.mp4"));
 TestEqual(TEXT("Project-relative movie migrates"),WardrobeStartup::ResolveMovie(TEXT("Content/Movies/开场.mp4"),Content),Expected);
 TestEqual(TEXT("Content-relative path works"),WardrobeStartup::ResolveMovie(TEXT("Movies/开场.mp4"),Content),Expected);
 TestTrue(TEXT("External paths rejected for staging"),WardrobeStartup::ResolveMovie(TEXT("Movies/../../Other/clip.mp4"),Content).IsEmpty());
 TestTrue(TEXT("Only MP4 startup movies accepted"),WardrobeStartup::ResolveMovie(TEXT("Movies/clip.txt"),Content).IsEmpty());
 return true;
}
#endif
