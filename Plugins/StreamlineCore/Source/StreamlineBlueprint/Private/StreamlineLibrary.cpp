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
#include "StreamlineLibrary.h"

#include "StreamlineExtension.h"
#include "StreamlineLibraryPrivate.h"

#include "Algo/Accumulate.h"
#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Optional.h"

#if WITH_STREAMLINE
#include "StreamlineAPI.h"
#include "StreamlineCore.h"
#include "StreamlineRHI.h"

#pragma warning(push) //TODO: Remove after sl fixes this warning on their end
#pragma warning(disable : 4471)
#include "sl.h"
#include "sl_dlss_g.h"
#include "sl_reflex.h"
#include "sl_deepdvc.h"
#pragma warning(pop)
#endif

#define LOCTEXT_NAMESPACE "FStreamlineBlueprintModule"
DEFINE_LOG_CATEGORY(LogStreamlineBlueprint);

TMap<EStreamlineFeature, FStreamlineFeatureRequirements> UStreamlineLibrary::FeatureRequirements{};
TMap<FName, EStreamlineFeature> UStreamlineLibrary::StreamlineFeatureFromName{};
bool UStreamlineLibrary::bStreamlineLibraryInitialized = false;

EStreamlineFeatureSupport ToUStreamlineFeatureSupport(Streamline::EStreamlineFeatureSupport Support)
{
	static_assert(int32(Streamline::EStreamlineFeatureSupport::NumValues) == 9, "dear NVIDIA plugin developer, please update this code to handle the new enum values ");

	switch (Support)
	{
		case Streamline::EStreamlineFeatureSupport::Supported: return EStreamlineFeatureSupport::Supported;

		default:
			/* Gotta catch them all*/
		case Streamline::EStreamlineFeatureSupport::NotSupported: return EStreamlineFeatureSupport::NotSupported;

		case Streamline::EStreamlineFeatureSupport::NotSupportedIncompatibleHardware: return EStreamlineFeatureSupport::NotSupportedIncompatibleHardware;
		case Streamline::EStreamlineFeatureSupport::NotSupportedDriverOutOfDate: return EStreamlineFeatureSupport::NotSupportedDriverOutOfDate;
		case Streamline::EStreamlineFeatureSupport::NotSupportedOperatingSystemOutOfDate: return EStreamlineFeatureSupport::NotSupportedOperatingSystemOutOfDate;
		case Streamline::EStreamlineFeatureSupport::NotSupportedHardwareSchedulingDisabled: return EStreamlineFeatureSupport::NotSupportedHardewareSchedulingDisabled;
		case Streamline::EStreamlineFeatureSupport::NotSupportedIncompatibleRHI: return EStreamlineFeatureSupport::NotSupportedByRHI;
		case Streamline::EStreamlineFeatureSupport::NotSupportedUnknownFeature: return EStreamlineFeatureSupport::NotSupportedUnknownFeature;
		case Streamline::EStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime: return EStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime;
	}
}

void UStreamlineLibrary::BreakStreamlineFeatureRequirements(EStreamlineFeatureRequirementsFlags Requirements, bool& D3D11Supported, bool& D3D12Supported, bool& VulkanSupported, bool& VSyncOffRequired, bool& HardwareSchedulingRequired)
{
	if (ValidateEnumBitFlags(Requirements, __FUNCTION__))
	{
		D3D11Supported = EnumHasAllFlags(Requirements, EStreamlineFeatureRequirementsFlags::D3D11Supported);
		D3D12Supported = EnumHasAllFlags(Requirements, EStreamlineFeatureRequirementsFlags::D3D12Supported);
		VulkanSupported = EnumHasAllFlags(Requirements, EStreamlineFeatureRequirementsFlags::VulkanSupported);
		VSyncOffRequired = EnumHasAllFlags(Requirements, EStreamlineFeatureRequirementsFlags::VSyncOffRequired);
		HardwareSchedulingRequired = EnumHasAllFlags(Requirements, EStreamlineFeatureRequirementsFlags::HardwareSchedulingRequired);
	}
}

FStreamlineFeatureRequirements UStreamlineLibrary::GetStreamlineFeatureInformation(EStreamlineFeature Feature)
{
	TRY_INIT_STREAMLINE_LIBRARY_AND_RETURN(FStreamlineFeatureRequirements{})

	if (ValidateEnumValue(Feature, __FUNCTION__))
	{
		FStreamlineFeatureRequirements* Requirements = FeatureRequirements.Find(Feature);
		if (Requirements != nullptr)
		{
			return *Requirements;
		}
	}

	return FStreamlineFeatureRequirements();
}

bool UStreamlineLibrary::IsStreamlineFeatureSupported(EStreamlineFeature Feature)
{
	TRY_INIT_STREAMLINE_LIBRARY_AND_RETURN(false)

	if (ValidateEnumValue(Feature, __FUNCTION__))
	{
		return QueryStreamlineFeatureSupport(Feature) == EStreamlineFeatureSupport::Supported;
	}

	return false;
}


EStreamlineFeatureSupport UStreamlineLibrary::QueryStreamlineFeatureSupport(EStreamlineFeature Feature)
{
#if WITH_STREAMLINE
	TRY_INIT_STREAMLINE_LIBRARY_AND_RETURN(EStreamlineFeatureSupport::NotSupported)

	if (ValidateEnumValue(Feature, __FUNCTION__))
	{
		FStreamlineFeatureRequirements* Requirements = FeatureRequirements.Find(Feature);
		if (Requirements != nullptr)
		{
			return Requirements->Support;
		}
	}

	return EStreamlineFeatureSupport::NotSupportedUnknownFeature;
#else
	return EStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime;
#endif
}

EStreamlineFeatureSupport UStreamlineLibrary::QueryStreamlineFeatureSupport(FName FeatureName)
{
#if WITH_STREAMLINE
	TRY_INIT_STREAMLINE_LIBRARY_AND_RETURN(EStreamlineFeatureSupport::NotSupported)

	EStreamlineFeature* Feature = StreamlineFeatureFromName.Find(FeatureName);
	return Feature ? QueryStreamlineFeatureSupport(*Feature) : EStreamlineFeatureSupport::NotSupportedUnknownFeature;
#else
	return EStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime;
#endif
}

void UStreamlineLibrary::Startup()
{
	// This initialization will likely not succeed if this module has been moved from PostEngineInit. It still gets called again when any BP functions are accessed.
	TryInitStreamlineLibrary();
#if !WITH_STREAMLINE
	UE_LOG(LogStreamlineBlueprint, Log, TEXT("Streamline is not supported on this platform at build time. The Streamline Blueprint library however is supported and stubbed out to ignore any calls to enable Streamline features and will always return UStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime, regardless of the underlying hardware. This can be used to e.g. to turn off related UI elements."));
#endif
}
void UStreamlineLibrary::Shutdown()
{
}

// Delayed initialization, which allows this module to be available early so blueprints can be loaded before DLSS is available in PostEngineInit
bool UStreamlineLibrary::TryInitStreamlineLibrary()
{
	if (bStreamlineLibraryInitialized)
	{
		return true;
	}

#if WITH_STREAMLINE
	// Before UE 5.6, PostSplashScreen modules like StreamlineRHI don't get loaded at all if there's no splash screen
	// (for example when running a commandlet).
	//
	// Normally we'd allow SL features to be registered up until Streamline itself is initialized by StreamlineRHI, then
	// we'd prevent new SL features from being registered after that. But if StreamlineRHI never loads, Streamline never
	// gets initialized and nothing prevents a rogue SL feature from registering itself at any point. So how can we
	// know when it's safe to query the registered SL features and initialize the enum values?
	//
	// Just not updating the enum values at all is not an option because that would break any content that uses them
	// when running a cooking commandlet.
	//
	// So in 5.5 and earlier we call CloseRegistration() here ourselves to prevent any new things from getting
	// registered after this module, and we proceed to update the dynamic enums. That should be safe as long as
	// customers don't change the module loading phases in such a way that this module gets loaded before all dynamic
	// Streamline features are loaded.
	//
	// Starting with UE 5.6 PostSplashScreen modules are loaded unconditionally so none of this is an issue there. We
	// just verify that StreamlineRHI has been loaded, which should always be the case if loading phases haven't
	// been changed.
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("StreamlineRHI")))
	{
#if !UE_VERSION_OLDER_THAN(5,6,0)
		// too early, SL not initialized yet
		return false;
#else
		if (!IsRunningDedicatedServer() && !IsRunningCommandlet())
		{
			UE_LOG(LogStreamlineBlueprint, Warning, TEXT("TryInitStreamlineLibrary called before Streamline initialized. This is unexpected. Some dynamic Streamline features may not be properly supported"));
		}
		FStreamlineExtensionModule::Get().CloseRegistration();
#endif
	}
#else
	FStreamlineExtensionModule::Get().CloseRegistration();
#endif	// WITH_STREAMLINE

	// FG is a special case, for historic reasons the feature name doesn't match the enum name
	TMap<EStreamlineFeature, FName> NameFromStreamlineFeature{ {EStreamlineFeature::DLSSG, TEXT("DLSS-FG")} };

	// Find how many SL features are registered
	int32 NumFeatures = 0;
	UEnum* Enum = StaticEnum<EStreamlineFeature>();
	const TArray<FSLFeatureDesc>& DynamicFeatures = FStreamlineExtensionModule::Get().GetRegisteredFeatures();
	if (DynamicFeatures.IsEmpty())
	{
		// The last entry is the auto-generated hidden "MAX" entry so we skip it.
		NumFeatures = Enum->NumEnums() - 1;
		for (int32 Idx = 0; Idx < NumFeatures; ++Idx)
		{
			const EStreamlineFeature Feature = static_cast<EStreamlineFeature>(Idx);
			if (!NameFromStreamlineFeature.Contains(Feature))
			{
				FName EnumName = *Enum->GetNameStringByIndex(Idx);
				NameFromStreamlineFeature.Add(Feature, EnumName);
			}
		}
	}
	else
	{
		// Add enum values for any dynamically registered features

		// strip non-alphanumerics from feature name to get enum name
		// Would turn a fictional "DLSS-XP" feature into "DLSSXP"
		auto GetEnumNameFromFeatureName = [](FString InFeatureName) -> FString
		{
			return Algo::Accumulate(InFeatureName, FString{}, [](FString Value, TCHAR Char)
				{ return FChar::IsAlnum(Char) ? Value + Char : Value; });
		};

		// First copy base names
		TArray<TPair<FName, int64>> NewNames;
		int32 Idx = 0;
		// The last entry is the auto-generated hidden "MAX" entry so we skip copying it.
		// Don't worry, a new hidden "MAX" entry will be auto-generated by the SetEnums() call below.
		int32 NumNames = Enum->NumEnums() - 1;
		while (NumNames-- > 0)
		{
			FName EnumName = *Enum->GetNameStringByIndex(Idx);
			const EStreamlineFeature Feature = static_cast<EStreamlineFeature>(Idx);
			if (!NameFromStreamlineFeature.Contains(Feature))
			{
				NameFromStreamlineFeature.Add(Feature, EnumName);
			}
			NewNames.Emplace(EnumName, Idx++);
		}

		// Next add new enum names to the end
		for (const FSLFeatureDesc& DynamicFeature : DynamicFeatures)
		{
			FString EnumString = GetEnumNameFromFeatureName(DynamicFeature.FeatureName);
			if (INDEX_NONE == Enum->GetIndexByNameString(EnumString))
			{
				NameFromStreamlineFeature.Add(static_cast<EStreamlineFeature>(Idx), *DynamicFeature.FeatureName);
				NewNames.Emplace(*EnumString, Idx++);
			}
		}
		NumFeatures = Idx;

		// Write new enum list
#if !UE_VERSION_OLDER_THAN(5,8,0)
		Enum->SetEnums(NewNames, UEnum::ECppForm::Regular, UEnum::EUnderlyingType::uint8, EEnumFlags::None, UEnum::EAddMaxKeyIfMissing::Yes);
#else
		Enum->SetEnums(NewNames, UEnum::ECppForm::Regular);
#endif

#if WITH_METADATA
		// In editor we make the new enum names look prettier.
		// For example an enum value of "DLSS-FG" would get mangled to "DLSS- FG" as the display name by default. And "DLSSFG" isn't pretty either.
		// So use the feature name as a display name since that's likely to look nicer.
		for (const FSLFeatureDesc& DynamicFeature : DynamicFeatures)
		{
			FString EnumString = GetEnumNameFromFeatureName(DynamicFeature.FeatureName);
			int32 FeatureIdx = Enum->GetIndexByNameString(EnumString);
			if (ensure(FeatureIdx != INDEX_NONE))
			{
				Enum->SetMetaData(TEXT("DisplayName"), *DynamicFeature.FeatureName, FeatureIdx);
			}
		}
#endif
	}

	// Add feature requirements
	FeatureRequirements.Reserve(NumFeatures);
#if WITH_STREAMLINE
	if (IsStreamlineSupported())
	{
		// static_assert and static_cast are best friends
#define UE_SL_ENUM_CHECK(A,B) static_assert(uint32(sl::FeatureRequirementFlags::A) == uint32(EStreamlineFeatureRequirementsFlags::B), "sl::FeatureRequirementFlags vs UStreamlineFeatureRequirementsFlags enum mismatch")
		UE_SL_ENUM_CHECK(eD3D11Supported, D3D11Supported);
		UE_SL_ENUM_CHECK(eD3D12Supported, D3D12Supported);
		UE_SL_ENUM_CHECK(eVulkanSupported, VulkanSupported);
		UE_SL_ENUM_CHECK(eVSyncOffRequired, VSyncOffRequired);
		UE_SL_ENUM_CHECK(eHardwareSchedulingRequired, HardwareSchedulingRequired);
#undef UE_SL_ENUM_CHECK

		for (int32 Idx = 0; Idx < NumFeatures; ++Idx)
		{
			const EStreamlineFeature Feature = static_cast<EStreamlineFeature>(Idx);
			const FName FeatureName = NameFromStreamlineFeature.FindRef(Feature, NAME_None);
			TOptional<sl::Feature> MaybeFeatureID = GetStreamlineFeatureID(FeatureName);
			if (!ensure(MaybeFeatureID.IsSet()))
			{
				FStreamlineFeatureRequirements Requirements;
				Requirements.Support = EStreamlineFeatureSupport::NotSupported;
				FeatureRequirements.Add(Feature, Requirements);
				continue;
			}
			sl::Feature FeatureID = MaybeFeatureID.GetValue();
			StreamlineFeatureFromName.Add(FeatureName, Feature);

			FStreamlineFeatureRequirements Requirements;
			Requirements.Support = ToUStreamlineFeatureSupport(::QueryStreamlineFeatureSupport(FeatureName));

			sl::FeatureRequirements SLRequirements;
			SLgetFeatureRequirements(FeatureID, SLRequirements);

			auto FromStreamlineVersion = [](const sl::Version& SLVersion) -> FStreamlineVersion
			{
				return FStreamlineVersion{ static_cast<int32>(SLVersion.major), static_cast<int32>(SLVersion.minor), static_cast<int32>(SLVersion.build) };
			};
			Requirements.RequiredDriverVersion = FromStreamlineVersion(SLRequirements.driverVersionRequired);
			Requirements.DetectedDriverVersion = FromStreamlineVersion(SLRequirements.driverVersionDetected);
			Requirements.RequiredOperatingSystemVersion = FromStreamlineVersion(SLRequirements.osVersionRequired);
			Requirements.DetectedOperatingSystemVersion = FromStreamlineVersion(SLRequirements.osVersionDetected);

			// strip the API support bits for those that are not implemented, but keep the other flags intact
			const sl::FeatureRequirementFlags ImplementedAPIFlags = PlatformGetAllImplementedStreamlineRHIs();
			const sl::FeatureRequirementFlags AllAPIFlags = sl::FeatureRequirementFlags::eD3D11Supported | sl::FeatureRequirementFlags::eD3D12Supported | sl::FeatureRequirementFlags::eVulkanSupported;
			const sl::FeatureRequirementFlags SLRequirementFlags = sl::FeatureRequirementFlags(SLBitwiseAnd(SLRequirements.flags, ImplementedAPIFlags) | SLBitwiseAnd(SLRequirements.flags, ~AllAPIFlags));

			Requirements.Requirements = static_cast<EStreamlineFeatureRequirementsFlags>(SLRequirementFlags);
			FeatureRequirements.Add(Feature, Requirements);
		}
	}
	else
#endif	// WITH_STREAMLINE
	{
		// Streamline isn't supported, so no features are supported
		FStreamlineFeatureRequirements Requirements;
#if WITH_STREAMLINE
		Requirements.Support = (GetPlatformStreamlineSupport() == EStreamlineSupport::NotSupportedIncompatibleRHI) ?
			EStreamlineFeatureSupport::NotSupportedByRHI : EStreamlineFeatureSupport::NotSupported;
#else
		Requirements.Support = EStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime;
#endif
		for (int32 Idx = 0; Idx < NumFeatures; ++Idx)
		{
			const EStreamlineFeature Feature = static_cast<EStreamlineFeature>(Idx);
			if (NameFromStreamlineFeature.Contains(Feature))
			{
				StreamlineFeatureFromName.Add(NameFromStreamlineFeature.FindChecked(Feature), Feature);
			}
			FeatureRequirements.Add(Feature, Requirements);
		}
	}

	bStreamlineLibraryInitialized = true;

	return true;
}

void FStreamlineBlueprintModule::StartupModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
		UE_LOG(LogStreamlineBlueprint, Log, TEXT("Initialization of StreamlineBlueprint is disabled."));
		return;
	}

	UStreamlineLibrary::Startup();
}

void FStreamlineBlueprintModule::ShutdownModule()
{
	auto CVarInitializePlugin = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streamline.InitializePlugin"));
	if (CVarInitializePlugin && !CVarInitializePlugin->GetBool())
	{
		return;
	}

	UStreamlineLibrary::Shutdown();

}


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStreamlineBlueprintModule, StreamlineBlueprint)

