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

#include "StreamlineLibraryReflex.h"
#include "StreamlineLibrary.h"


#if WITH_STREAMLINE
#include "StreamlineReflex.h"
#endif

#include "Modules/ModuleManager.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "FStreamlineReflexBlueprintModule"
DEFINE_LOG_CATEGORY_STATIC(LogStreamlineReflexBlueprint, Log, All);


bool UStreamlineLibraryReflex::IsReflexSupported()
{
	return UStreamlineLibrary::IsStreamlineFeatureSupported(TEXT("Reflex"));
}

EStreamlineFeatureSupport UStreamlineLibraryReflex::QueryReflexSupport()
{
	return UStreamlineLibrary::QueryStreamlineFeatureSupport(TEXT("Reflex"));
}

#if WITH_STREAMLINE
static EStreamlineReflexMode ReflexModeEnumFromFlags(int32 ReflexFlags)
{

	static_assert(uint32(FStreamlineMaxTickRateHandler::ReflexFlags::ReflexOff) == uint32(EStreamlineReflexMode::Off), "enum value mismatch. Dear NVIDIA Streamline plugin developer, please update this code!");
	static_assert(uint32(FStreamlineMaxTickRateHandler::ReflexFlags::ReflexOn) == uint32(EStreamlineReflexMode::Enabled), "enum value mismatch. Dear NVIDIA Streamline plugin developer, please update this code!");
	static_assert(uint32(FStreamlineMaxTickRateHandler::ReflexFlags::ReflexBoost) == uint32(EStreamlineReflexMode::Boost), "enum value mismatch. Dear NVIDIA Streamline plugin developer, please update this code!");

	if ((uint32(FStreamlineMaxTickRateHandler::ReflexFlags::AllBits) & ReflexFlags) != uint32(FStreamlineMaxTickRateHandler::ReflexFlags::ReflexInvalidFlags))
	{
		return static_cast<EStreamlineReflexMode>(ReflexFlags);
	}
	else
	{
		UE_LOG(LogStreamlineReflexBlueprint, Error, TEXT("Invalid t.Streamline.Reflex.Mode and t.Streamline.Reflex.Auto vars yielding invalid GetFlags %d. turning Reflex off"), ReflexFlags);
		return EStreamlineReflexMode::Off;
	}

}
#endif

void UStreamlineLibraryReflex::SetReflexMode(const EStreamlineReflexMode Mode)
{
#if WITH_STREAMLINE
	if (ValidateEnumValue(Mode, __FUNCTION__))
	{
		TArray<IMaxTickRateHandlerModule*> MaxTickRateHandlerModules = IModularFeatures::Get()
			.GetModularFeatureImplementations<IMaxTickRateHandlerModule>(IMaxTickRateHandlerModule::GetModularFeatureName());
		for (IMaxTickRateHandlerModule* MaxTickRateHandler : MaxTickRateHandlerModules)
		{
			if (MaxTickRateHandler != GetStreamlineReflexMaxTickRateHandler())
			{
				continue;
			}

			MaxTickRateHandler->SetEnabled(Mode != EStreamlineReflexMode::Off);
			MaxTickRateHandler->SetFlags(static_cast<uint32>(Mode));
		}
	}
#endif
}

EStreamlineReflexMode UStreamlineLibraryReflex::GetReflexMode()
{
#if WITH_STREAMLINE
	TArray<IMaxTickRateHandlerModule*> MaxTickRateHandlerModules = IModularFeatures::Get()
		.GetModularFeatureImplementations<IMaxTickRateHandlerModule>(IMaxTickRateHandlerModule::GetModularFeatureName());
	for (IMaxTickRateHandlerModule* MaxTickRateHandler : MaxTickRateHandlerModules)
	{
		if (MaxTickRateHandler != GetStreamlineReflexMaxTickRateHandler())
		{
			continue;
		}

		if (MaxTickRateHandler->GetEnabled())
		{
			return ReflexModeEnumFromFlags(MaxTickRateHandler->GetFlags());
		}
	}
#endif
	return EStreamlineReflexMode::Off;
}


bool UStreamlineLibraryReflex::IsReflexModeSupported(EStreamlineReflexMode ReflexMode)
{
	if (ValidateEnumValue(ReflexMode, __FUNCTION__))
	{
		if (ReflexMode == EStreamlineReflexMode::Off)
		{
			return true;
		}

		if (!IsReflexSupported()) // that returns false if WITH_STREAMLINE is false 
		{
			return false;
		}

		return true; // TODO, right now Enabled and Boost
	}

	return false;
}

TArray<EStreamlineReflexMode> UStreamlineLibraryReflex::GetSupportedReflexModes()
{
	TArray<EStreamlineReflexMode> SupportedQualityModes;

	const UEnum* Enum = StaticEnum<EStreamlineReflexMode>();
	for (int32 EnumIndex = 0; EnumIndex < Enum->NumEnums() - 1 /* avoid _MAX */; ++EnumIndex)
	{
		const int64 EnumValue = Enum->GetValueByIndex(EnumIndex);
		const EStreamlineReflexMode QualityMode = EStreamlineReflexMode(EnumValue);
		if (IsReflexModeSupported(QualityMode))
		{
			SupportedQualityModes.Add(QualityMode);
		}
	}

	return SupportedQualityModes;
}

EStreamlineReflexMode UStreamlineLibraryReflex::GetDefaultReflexMode()
{
	if (UStreamlineLibraryReflex::IsReflexSupported())
	{
		return EStreamlineReflexMode::Enabled;
	}
	else
	{
		return EStreamlineReflexMode::Off;
	}
}

float UStreamlineLibraryReflex::GetGameToRenderLatencyInMs()
{
#if WITH_STREAMLINE
	TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get()
		.GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		if (LatencyMarkerModule != GetStreamlineReflexLatencyMarkerModule())
		{
			continue;
		}

		return LatencyMarkerModule->GetTotalLatencyInMs();
	}
#endif
	return 0.f;
}

float UStreamlineLibraryReflex::GetGameLatencyInMs()
{
#if WITH_STREAMLINE
	TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get()
		.GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		if (LatencyMarkerModule != GetStreamlineReflexLatencyMarkerModule())
		{
			continue;
		}

		return LatencyMarkerModule->GetGameLatencyInMs();
	}
#endif
	return 0.f;
}

float UStreamlineLibraryReflex::GetRenderLatencyInMs()
{
#if WITH_STREAMLINE
	TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get()
		.GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
	for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
	{
		if (LatencyMarkerModule != GetStreamlineReflexLatencyMarkerModule())
		{
				continue;
		}

		return LatencyMarkerModule->GetRenderLatencyInMs();
	}
#endif
	return 0.f;
}

void FStreamlineLibraryReflexBlueprintModule::StartupModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
		UE_LOG(LogStreamlineReflexBlueprint, Log, TEXT("Initialization of StreamlineBlueprint is disabled."));
		return;
	}
}

void FStreamlineLibraryReflexBlueprintModule::ShutdownModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
#if WITH_STREAMLINE
		if (FStreamlineLatencyMarkers::Get(false) != nullptr)
		{
			FStreamlineLatencyMarkers::Reset();
		}

		if (FStreamlineMaxTickRateHandler::Get(false) != nullptr)
		{
			FStreamlineMaxTickRateHandler::Reset();
		}
#endif
		return;
	}
}

IMPLEMENT_MODULE(FStreamlineLibraryReflexBlueprintModule, StreamlineReflexBlueprint)

#undef LOCTEXT_NAMESPACE