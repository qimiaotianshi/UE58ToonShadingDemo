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

#include "StreamlineExtension.h"

#include "StreamlineFeature.h"

#include "Algo/Accumulate.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FStreamlineExtensionModule"
DEFINE_LOG_CATEGORY_STATIC(LogStreamlineExtension, Log, All)

FStreamlineNullFeature GStreamlineNullFeature;

FSLFeatureDesc::FSLFeatureDesc(sl::Feature InSLFeature, const FString& InUEPluginName, const FString& InFeatureName)
	: SLFeature(InSLFeature)
	, UEPluginName(InUEPluginName)
	, FeatureName(InFeatureName)
{
	// LoadCVar is "r.Streamline.Load." + InFeatureName with any non-alphanumeric characters removed
	// CommandLineSuffix is a lowercase version of InFeatureName with any non-alphanumeric characters removed
	//
	// For example, a fictional "DLSS-XP" feature that levels up your character using deep learning would turn into:
	// CommandLineSuffix = "dlssxp"
	// LoadCVar = "r.Streamline.Load.DLSSXP"
	LoadCVar = TEXT("r.Streamline.Load.");
	for (auto Char : InFeatureName)
	{
		if (FChar::IsAlnum(Char))
		{
			LoadCVar += Char;
			CommandLineSuffix += FChar::ToLower(Char);
		}
	}
}

IStreamlineFeature* FStreamlineExtensionModule::GetFeatureImplementation(FName FeatureName) const
{
	IStreamlineFeature* const* FeatureImplPtr = FeatureImpls.Find(FeatureName);
	return FeatureImplPtr ? *FeatureImplPtr : nullptr;
}

void FStreamlineExtensionModule::ForEachFeature(TFunctionRef<void(IStreamlineFeature&)> Callable) const
{
	for (const TPair<FName, IStreamlineFeature*>& FeatureImpl : FeatureImpls)
	{
		if (FeatureImpl.Value)
		{
			Callable(*FeatureImpl.Value);
		}
	}
}

bool FStreamlineExtensionModule::AnyFeature(TFunctionRef<bool(IStreamlineFeature&)> Predicate) const
{
	for (const TPair<FName, IStreamlineFeature*>& FeatureImpl : FeatureImpls)
	{
		if (FeatureImpl.Value && Predicate(*FeatureImpl.Value))
		{
			return true;
		}
	}
	return false;
}

EStreamlineInputType FStreamlineExtensionModule::GetRequiredInputs_RenderThread() const
{
	return Algo::Accumulate<EStreamlineInputType>(FeatureImpls, EStreamlineInputType::None, [](EStreamlineInputType Flags, const TPair<FName, IStreamlineFeature*>& FeatureImpl)
	{
		const IStreamlineFeature* Feature = FeatureImpl.Value;
		if (Feature && Feature->IsActive())
		{
			EnumAddFlags(Flags, Feature->GetRequiredInputs_RenderThread());
		}
		return Flags;
	});
}

bool FStreamlineExtensionModule::RegisterFeature(const FSLFeatureDesc& InFeature, IStreamlineFeature& Implementation)
{
	if (bIsRegistrationClosed)
	{
		UE_LOG(LogStreamlineExtension, Error, TEXT("Failed attempt to register SL feature %s after SL already initialized"), *InFeature.FeatureName);
		return false;
	}

	if (FeatureDescs.ContainsByPredicate([&](const FSLFeatureDesc& Desc) { return Desc.FeatureName == InFeature.FeatureName; }))
	{
		UE_LOG(LogStreamlineExtension, Error, TEXT("Failed attempt to register SL feature %s twice"), *InFeature.FeatureName);
		return false;
	}

	FeatureDescs.Add(InFeature);
	FeatureImpls.Add(*InFeature.FeatureName, &Implementation);

	return true;
}

void FStreamlineExtensionModule::UnregisterFeatureImplementation(FName FeatureName)
{
	FeatureImpls.Remove(FeatureName);
}

FString FStreamlineExtensionModule::GetDefaultBinaryBaseDir(const FStringView PluginName)
{
	const FString PluginBaseDir = IPluginManager::Get().FindPlugin(PluginName)->GetBaseDir();
	const FString Platform = FPlatformProcess::GetBinariesSubdirectory();
	return FPaths::Combine(*PluginBaseDir, TEXT("Binaries"), TEXT("ThirdParty"), *Platform);
}

const TArray<FSLFeatureDesc>& FStreamlineExtensionModule::GetRegisteredFeatures() const
{
	return FeatureDescs;
}

void FStreamlineExtensionModule::CloseRegistration()
{
	bIsRegistrationClosed = true;
}

void FStreamlineExtensionModule::SetLoadedFeatures(TArrayView<sl::Feature> LoadedFeatureIDs)
{
	for (const FSLFeatureDesc& FeatureDesc : FeatureDescs)
	{
		if (LoadedFeatureIDs.Contains(FeatureDesc.SLFeature))
		{
			IStreamlineFeature** Impl = FeatureImpls.Find(FName(*FeatureDesc.FeatureName));
			if (Impl && *Impl)
			{
				(*Impl)->SetLoaded();
			}
		}
	}
}

void FStreamlineExtensionModule::StartupModule()
{
}

void FStreamlineExtensionModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStreamlineExtensionModule, StreamlineExtension)

