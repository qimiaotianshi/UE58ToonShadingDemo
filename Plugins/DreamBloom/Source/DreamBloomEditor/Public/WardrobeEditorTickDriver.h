#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"

/**
 * Drives AWardrobeEditorTickActor instances every editor frame.
 *
 * Why FTSTicker instead of FTickableEditorObject: UEditorEngine::Tick calls
 * FTickableEditorObject::TickObjects() from inside the tickable registry lock
 * (EditorEngine.cpp:1933). Running blueprint code there that touches a
 * material parameter collection deadlocks the editor, because the render state
 * update has to round-trip through the render thread while the game thread is
 * still inside that lock. FTSTicker runs at the end of FEngineLoop::Tick, well
 * past that lock, and after the editor world has finished ticking.
 *
 * This also keeps the editor path independent of LEVELTICK_ViewportsOnly,
 * viewport realtime state, focus and background CPU throttling.
 *
 * Registered by the DreamBloomEditor module (UncookedOnly), so it never ships
 * in a packaged build.
 */
class FWardrobeEditorTickDriver
{
public:
 /** Starts the ticker if it is not running yet. Safe to call more than once. */
 static void Startup();
 /** Removes the ticker. */
 static void Shutdown();
 static bool IsDriverActive();

private:
 static bool Tick(float DeltaSeconds);
 static FTSTicker::FDelegateHandle TickerHandle;
};
