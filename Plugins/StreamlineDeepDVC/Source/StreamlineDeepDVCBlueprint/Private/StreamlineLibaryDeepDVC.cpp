/*
* Copyright (c) 2022 - 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "StreamlineLibraryDeepDVC.h"
#include "StreamlineLibrary.h"

#if WITH_STREAMLINE
#include "StreamlineNGXCommon.h"
#endif

#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"

#ifdef __INTELLISENSE__
#define WITH_STREAMLINE 1
#endif

#define LOCTEXT_NAMESPACE "FStreamlineDeepDVCBlueprintModule"
DEFINE_LOG_CATEGORY_STATIC(LogStreamlineDeepDVCBlueprint, Log, All);

#if WITH_STREAMLINE

#define TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(ReturnValueOrEmptyOrVoidPreFiveThree) \
if (!TryInitDeepDVCLibrary()) \
{ \
	UE_LOG(LogStreamlineDeepDVCBlueprint, Error, TEXT("%s should not be called before PostEngineInit"), ANSI_TO_TCHAR(__FUNCTION__)); \
	return ReturnValueOrEmptyOrVoidPreFiveThree; \
}

#else

#define TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(ReturnValueWhichCanBeEmpty) 

#endif

#if WITH_STREAMLINE

bool UStreamlineLibraryDeepDVC::bDeepDVCLibraryInitialized = false;

static bool ShowDeepDVCSDebugOnScreenMessages()
{
	return true;
}


#if !UE_BUILD_SHIPPING

UStreamlineLibraryDeepDVC::FDLSSErrorState UStreamlineLibraryDeepDVC::DLSSErrorState;
FDelegateHandle UStreamlineLibraryDeepDVC::DeepDVCOnScreenMessagesDelegateHandle;
void UStreamlineLibraryDeepDVC::GetDeepDVCOnScreenMessages(TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText>& OutMessages)
{
	check(IsInGameThread());

	// We need a valid DLSSSupport, so calling this here in case other UStreamlineLibraryDeepDVC functions which call TryInitStreamlineLibrary() haven't been called
	if (!TryInitDeepDVCLibrary())
	{
		return;
	}
}
#endif

#endif

bool UStreamlineLibraryDeepDVC::IsDeepDVCModeSupported(EStreamlineDeepDVCMode DeepDVCMode)
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(false)

		const UEnum* Enum = StaticEnum<EStreamlineDeepDVCMode>();

	if (ValidateEnumValue(DeepDVCMode, __FUNCTION__))
	{
		if (DeepDVCMode == EStreamlineDeepDVCMode::Off)
		{
			return true;
		}

		if (!IsDeepDVCSupported()) // that returns false if WITH_STREAMLINE is false 
		{
			return false;
		}

		return true;
	}

	return false;
}



TArray<EStreamlineDeepDVCMode> UStreamlineLibraryDeepDVC::GetSupportedDeepDVCModes()
{
	TArray<EStreamlineDeepDVCMode> SupportedQualityModes;

	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(SupportedQualityModes);
	{
		const UEnum* Enum = StaticEnum<EStreamlineDeepDVCMode>();
		for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums() - 1 /* avoid _MAX */; ++EnumIndex)
		{
			const int64 EnumValue = Enum->GetValueByIndex(EnumIndex);
			const EStreamlineDeepDVCMode QualityMode = EStreamlineDeepDVCMode(EnumValue);
			if (IsDeepDVCModeSupported(QualityMode))
			{
				SupportedQualityModes.Add(QualityMode);
			}
		}
	}
	return SupportedQualityModes;
}

bool UStreamlineLibraryDeepDVC::IsDeepDVCSupported()
{
	return UStreamlineLibrary::IsStreamlineFeatureSupported(TEXT("DeepDVC"));
}

EStreamlineFeatureSupport UStreamlineLibraryDeepDVC::QueryDeepDVCSupport()
{
	return UStreamlineLibrary::QueryStreamlineFeatureSupport(TEXT("DeepDVC"));
}

#if WITH_STREAMLINE
static int32 DeepDVCModeIntCvarFromEnum(EStreamlineDeepDVCMode DeepDVCMode)
{
	switch (DeepDVCMode)
	{
	case EStreamlineDeepDVCMode::Off:
		return 0;
	case EStreamlineDeepDVCMode::On:
		return 1;
	default:
		checkf(false, TEXT("dear DeepDVC plugin developer, please support new enum type!"));
		return 0;
	}
}

static EStreamlineDeepDVCMode DeepDVCModeEnumFromIntCvar(int32 DeepDVCMode)
{
	switch (DeepDVCMode)
	{
	case 0:
		return EStreamlineDeepDVCMode::Off;
	case 1:
		return EStreamlineDeepDVCMode::On;
	default:
		UE_LOG(LogStreamlineDeepDVCBlueprint, Error, TEXT("Invalid r.Streamline.DeepDVC.Enable value %d"), DeepDVCMode);
		return EStreamlineDeepDVCMode::Off;
	}
}
#endif	// WITH_STREAMLINE

void UStreamlineLibraryDeepDVC::SetDeepDVCMode(EStreamlineDeepDVCMode DeepDVCMode)
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(void());

#if WITH_STREAMLINE

	const UEnum* Enum = StaticEnum<EStreamlineDeepDVCMode>();

	if (ValidateEnumValue(DeepDVCMode, __FUNCTION__))
	{
		static auto CVarDeepDVCEnable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.DeepDVC.Enable"));
		if (CVarDeepDVCEnable)
		{
			SetCVarWithCurrentPriority(CVarDeepDVCEnable, DeepDVCModeIntCvarFromEnum(DeepDVCMode));
		}
		
		if (DeepDVCMode != EStreamlineDeepDVCMode::Off)
		{
#if !UE_BUILD_SHIPPING
			check(IsInGameThread());
			DLSSErrorState.bIsDeepDVCModeUnsupported = !IsDeepDVCModeSupported(DeepDVCMode);
			DLSSErrorState.InvalidDeepDVCMode = DeepDVCMode;
#endif 
		}
	}

#endif	// WITH_STREAMLINE
}

EStreamlineDeepDVCMode UStreamlineLibraryDeepDVC::GetDeepDVCMode()
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(EStreamlineDeepDVCMode::Off);

#if WITH_STREAMLINE
	static const auto CVarDeepDVCEnable = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.DeepDVC.Enable"));
	if (CVarDeepDVCEnable != nullptr)
	{
		return DeepDVCModeEnumFromIntCvar(CVarDeepDVCEnable->GetInt());
	}
#endif
	return EStreamlineDeepDVCMode::Off;
}

EStreamlineDeepDVCMode UStreamlineLibraryDeepDVC::GetDefaultDeepDVCMode()
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(EStreamlineDeepDVCMode::Off);
	if (UStreamlineLibraryDeepDVC::IsDeepDVCSupported())
	{
		return EStreamlineDeepDVCMode::Off;
	}
	else
	{
		return EStreamlineDeepDVCMode::Off;
	}
}

STREAMLINEDEEPDVCBLUEPRINT_API void UStreamlineLibraryDeepDVC::SetDeepDVCIntensity(float Intensity)
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(void());

#if WITH_STREAMLINE
	static const auto CVarDeepDVCIntensity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.DeepDVC.Intensity"));
	if (CVarDeepDVCIntensity)
	{
		// Quantize here so the value can snap to 0, which downstream uses to turn off DeepDVC implicitly.
		// Passing a formatted string preserves this behavior; the float setter's %g formatting does not snap to 0.
		SetCVarWithCurrentPriority(CVarDeepDVCIntensity, *FString::Printf(TEXT("%2.2f"), Intensity));
	}
#endif

}

STREAMLINEDEEPDVCBLUEPRINT_API float UStreamlineLibraryDeepDVC::GetDeepDVCIntensity()
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(0.0f);

#if WITH_STREAMLINE
	static const auto CVarDeepDVCIntensity = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.DeepDVC.Intensity"));

	if (CVarDeepDVCIntensity)
	{
		return CVarDeepDVCIntensity->GetFloat();
	}

#endif
	return 0.0f;
}

STREAMLINEDEEPDVCBLUEPRINT_API void UStreamlineLibraryDeepDVC::SetDeepDVCSaturationBoost(float SaturationBoost)
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(void());

#if WITH_STREAMLINE
	static const auto CVarDeepDVCSaturationBoost = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.DeepDVC.SaturationBoost"));
	if (CVarDeepDVCSaturationBoost)
	{
		// Quantize here so the value can snap to 0, which is useful.
		// Passing a formatted string preserves this behavior; the float setter's %g formatting does not snap to 0.
		SetCVarWithCurrentPriority(CVarDeepDVCSaturationBoost, *FString::Printf(TEXT("%2.2f"), SaturationBoost));
	}
#endif
}

STREAMLINEDEEPDVCBLUEPRINT_API float UStreamlineLibraryDeepDVC::GetDeepDVCSaturationBoost()
{
	TRY_INIT_STREAMLINE_DEEPDVC_LIBRARY_AND_RETURN(0.0f);

#if WITH_STREAMLINE
	static const auto CVarDeepDVCSaturationBoost = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.DeepDVC.SaturationBoost"));

	if (CVarDeepDVCSaturationBoost)
	{
		return CVarDeepDVCSaturationBoost->GetFloat();
	}

#endif
	return  0.0f;
}

#if WITH_STREAMLINE

// Delayed initialization, which allows this module to be available early so blueprints can be loaded before DLSS is available in PostEngineInit
bool UStreamlineLibraryDeepDVC::TryInitDeepDVCLibrary()
{
	if (bDeepDVCLibraryInitialized)
	{
		return true;
	}

#if !UE_BUILD_SHIPPING
	if (!DeepDVCOnScreenMessagesDelegateHandle.IsValid())
	{
		DeepDVCOnScreenMessagesDelegateHandle = FCoreDelegates::OnGetOnScreenMessages.AddStatic(&GetDeepDVCOnScreenMessages);
	}
#endif

	bDeepDVCLibraryInitialized = true;

	return true;
}
#endif // WITH_STREAMLINE


void UStreamlineLibraryDeepDVC::Startup()
{
#if WITH_STREAMLINE
	UStreamlineLibraryDeepDVC::TryInitDeepDVCLibrary();
#else
	UE_LOG(LogStreamlineDeepDVCBlueprint, Log, TEXT("Streamline is not supported on this platform at build time. The Streamline Blueprint library however is supported and stubbed out to ignore any calls to enable DeepDVC and will always return UStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime, regardless of the underlying hardware. This can be used to e.g. to turn off related UI elements."));
#endif
}
void UStreamlineLibraryDeepDVC::Shutdown()
{
#if WITH_STREAMLINE && !UE_BUILD_SHIPPING
	if (UStreamlineLibraryDeepDVC::DeepDVCOnScreenMessagesDelegateHandle.IsValid())
	{
		FCoreDelegates::OnGetOnScreenMessages.Remove(UStreamlineLibraryDeepDVC::DeepDVCOnScreenMessagesDelegateHandle);
		UStreamlineLibraryDeepDVC::DeepDVCOnScreenMessagesDelegateHandle.Reset();
	}
#endif
}




void FStreamlineLibraryDeepDVCBlueprintModule::StartupModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
		UE_LOG(LogStreamlineDeepDVCBlueprint, Log, TEXT("Initialization of StreamlineBlueprint is disabled."));
		return;
	}

	UStreamlineLibraryDeepDVC::Startup();
}

void FStreamlineLibraryDeepDVCBlueprintModule::ShutdownModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
		return;
	}

	UStreamlineLibraryDeepDVC::Shutdown();
}

IMPLEMENT_MODULE(FStreamlineLibraryDeepDVCBlueprintModule, StreamlineDeepDVCBlueprint)

#undef LOCTEXT_NAMESPACE
