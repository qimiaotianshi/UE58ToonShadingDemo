/*
* Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#if WITH_STREAMLINE

// plugin includes
#include "StreamlineLibraryDLSSG.h"

// engine includes
#include "Misc/App.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDLSSFGModeTest, "Nvidia.DLSSFG.Mode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext |
	EAutomationTestFlags::NonNullRHI | EAutomationTestFlags::ProductFilter
)

static TOptional<int32> GetExpectedFramesFromMode(EStreamlineDLSSGMode Mode)
{
	switch (Mode)
	{
	case EStreamlineDLSSGMode::Off:
		return 1;
	case EStreamlineDLSSGMode::Auto:
	case EStreamlineDLSSGMode::OnDynamic:
		// no specific value expected
		return NullOpt;
	case EStreamlineDLSSGMode::On2X:
		return 2;
	case EStreamlineDLSSGMode::On3X:
		return 3;
	case EStreamlineDLSSGMode::On4X:
		return 4;
	case EStreamlineDLSSGMode::On5X:
		return 5;
	case EStreamlineDLSSGMode::On6X:
		return 6;
	default:
		// unexpected value, this should fail the test
		return -1;
	}
}

// test that every supported FG mode can be set and the correct number of frames are presented
bool FDLSSFGModeTest::RunTest(const FString& Parameters)
{
	if (!FApp::HasFocus())
	{
		// No way to test this properly if the tests are running in the background
		AddWarning(TEXT("DLSS-FG won't run if the window doesn't have focus so we have to skip this test"));
		return true;
	}

	// back up original mode
	const EStreamlineDLSSGMode OriginalMode = UStreamlineLibraryDLSSG::GetDLSSGMode();

	TArray<EStreamlineDLSSGMode> SupportedModes = UStreamlineLibraryDLSSG::GetSupportedDLSSGModes();
	for (EStreamlineDLSSGMode SupportedMode : SupportedModes)
	{
		TOptional<int32> ExpectedFramesPresented = GetExpectedFramesFromMode(SupportedMode);
		// set DLSS-FG mode
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([SupportedMode]() { UStreamlineLibraryDLSSG::SetDLSSGMode(SupportedMode); return true; }));
		// Wait a few frames. Frames don't seem to get generated until a few frames have passed.
		ADD_LATENT_AUTOMATION_COMMAND(FWaitForEngineFramesCommand(10));

		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, SupportedMode, ExpectedFramesPresented]()
		{
			// was mode set correctly?
			EStreamlineDLSSGMode CurrentMode = UStreamlineLibraryDLSSG::GetDLSSGMode();
			TestEqual(TEXT("DLSS-FG mode"), CurrentMode, SupportedMode);

			// was correct number of frames presented?
			if (ExpectedFramesPresented.IsSet())
			{
				float Unused;
				int32 ActualFramesPresented;
				UStreamlineLibraryDLSSG::GetDLSSGFrameTiming(Unused, ActualFramesPresented);
				TestEqual(TEXT("Number of frames presented"), ActualFramesPresented, ExpectedFramesPresented.GetValue());
			}
			return true;
		}));
	}

	// restore original mode
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([OriginalMode]() { UStreamlineLibraryDLSSG::SetDLSSGMode(OriginalMode); return true; }));
	return true;
}

#endif	// WITH_STREAMLINE

