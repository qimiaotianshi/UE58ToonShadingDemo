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
#pragma once

#include "Modules/ModuleManager.h"


#define UE_API STREAMLINEEXTENSION_API


namespace sl
{
	using Feature = uint32_t;
}

class IStreamlineFeature;
enum class EStreamlineInputType : uint32;

// Information needed to initialize a Streamline feature
struct FSLFeatureDesc
{
	// ctor covers most common cases
	//
	//  CommandLineSuffix is a lowercase version of InFeatureName with any non-alphanumeric characters removed
	//  LoadCVar is "r.Streamline.Load." + InFeatureName with any non-alphanumeric characters removed
	//
	// For example, if InFeatureName = "Reflex" then CommandLineSuffix = "reflex" and LoadCVar = "r.Streamline.Load.Reflex"
	UE_API FSLFeatureDesc(sl::Feature InSLFeature, const FString& InUEPluginName, const FString& InFeatureName);
	// default ctor in case you want to manually set all the fields yourself
	FSLFeatureDesc() = default;

	sl::Feature SLFeature;
	// Plugin that must be loaded to enable this feature
	FString UEPluginName;
	// The name that will be used at runtime to identify this feature for things like QueryStreamlineFeatureSupport()
	FString FeatureName;
	// Command line options will be "-sl" + CommandLineSuffix or "-slno" + CommandLineSuffix to control feature loading
	FString CommandLineSuffix;
	// Console variable to control feature loading
	FString LoadCVar;
	// Optional additional directories to search for NGX binaries
	TArray<FString> BinaryDirectories;
	// Whether the feature is loaded by default. Can be overridden with cvar or command line
	bool bAllowByDefault = true;
};

// This module exists for plugins to register their Streamline feature during PostConfigInit before Streamline is initialized in PostSplashScreen
class FStreamlineExtensionModule: public IModuleInterface
{
public:
	//////////////
	// Common API
	static UE_API FStreamlineExtensionModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FStreamlineExtensionModule>(FName("StreamlineExtension"));
	}

	UE_API const TArray<FSLFeatureDesc>& GetRegisteredFeatures() const;

	UE_API IStreamlineFeature* GetFeatureImplementation(FName FeatureName) const;

	// Invokes function on every Streamline feature implementation.
	UE_API void ForEachFeature(TFunctionRef<void(IStreamlineFeature&)> Callable) const;

	// Checks if predicate is true for any Streamline feature implementation.
	// For example:
	// bool bAnyFeatureActive = FStreamlineExtensionModule::Get().AnyFeature([](const IStreamlineFeature& Feature) { return Feature.IsActive(); });
	UE_API bool AnyFeature(TFunctionRef<bool(IStreamlineFeature&)> Predicate) const;

	// Returns requires input resources for all active features
	UE_API EStreamlineInputType GetRequiredInputs_RenderThread() const;

	//////////////////////////////////////////////
	// API for plugins that implement SL features

	// Must be called before streamline init. Returns false if feature failed to register.
	// Will cause the feature to be requested from Streamline, and will add the feature to the
	// EStreamlineFeature enum in Blueprints.
	UE_API bool RegisterFeature(const FSLFeatureDesc& Feature, IStreamlineFeature& Implementation);

	// Call this before destroying a feature implementation to prevent dangling pointers.
	UE_API void UnregisterFeatureImplementation(FName FeatureName);

	// Gets default base directory for extra binaries from plugin name.
	// Use this if you store binaries in the {PluginDir}\Binaries\ThirdParty\{Platform} folder.
	// Note that NGX binaries (nvngx_*.dll) are ok to store here but SL binaries (sl.*.dll) won't work due to SL limitations
	static UE_API FString GetDefaultBinaryBaseDir(const FStringView PluginName);

	////////////////////////////////////////
	// API for StreamlineRHI initialization

	UE_API void CloseRegistration();
	UE_API void SetLoadedFeatures(TArrayView<sl::Feature> LoadedFeatureIDs);

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	bool bIsRegistrationClosed = false;
	TArray<FSLFeatureDesc> FeatureDescs;
	TMap<FName, IStreamlineFeature*> FeatureImpls;
};

#undef UE_API

