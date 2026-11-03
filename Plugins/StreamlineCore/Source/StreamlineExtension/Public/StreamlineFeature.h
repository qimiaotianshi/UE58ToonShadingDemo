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

#include "StreamlineFeatureSupport.h"

#define UE_API STREAMLINEEXTENSION_API

// IStreamlineFeature allows you define an implementation of a Streamline feature.
// Use this in conjunction with an associated FSLFeatureDesc to register a dynamic Streamline feature.
//
// ** Porting guide **
//
// Implement IStreamlineFeature to port a hard-coded StreamlineCore feature over to the new interface.
// For example, a fictional "DLSS-XP" feature that levels up your character using deep learning would follow these porting steps:
//
// - Create a new PostConfigInit-loaded module that will register the feature. It will likely depend on Streamline, StreamlineExtension, and StreamlineCore modules at a minimum.
// - Make a new FStreamlineDLSSXPFeature class which implements IStreamlineFeature.
// - Replace the old QueryStreamlineDLSSXPSupport() function with an implementation of FStreamlineDLSSXPFeature::QuerySupport().
//   - Replace any RHI-specific implementation of FStreamlineRHI::IsDLSSXPSupported() with a direct test of RHIGetInterfaceType() in the new QuerySupport() function.
// - (optional) Implement SetLoaded to record whether the SL feature was successfully loaded, if needed to determine things like IsActive().
// - Replace the old IsDLSSXPActive() function with an implementation of FStreamlineDLSSXPFeature::IsActive().
// - Implement RequiresStreamlineViewExtension(). If the feature does require the Streamline view extension it should also check if the feature is supported and was successfully loaded.
// - (optional) If the feature requires any buffers from the Streamline view extension, implement GetRequiredInputs_RenderThread().
// - (optional) If the feature needs to do work in the render pass, implement RenderPass_RenderThread() and move any render pass code from StreamlineCore to the new module.
// - During module startup instantiate a FStreamlineDLSSXPFeature, create an associated FSLFeatureDesc, and register the feature with FStreamlineExtensionModule::Get().RegisterFeature().

using FRDGTextureRef = class FRDGTexture*;

enum class EStreamlineInputType: uint32
{
	None = 0,
	Color = 1<<0,
	SceneDepth = 1<<1,
	Velocity = 1<<2,
};
ENUM_CLASS_FLAGS(EStreamlineInputType);

struct FStreamlineRenderPassInputs
{
	class FRDGBuilder& GraphBuilder;
	const class FViewInfo& ViewInfo;
	const struct FScreenPassTexture& SceneColor;
	FRDGTextureRef SceneDepth = nullptr;
	FRDGTextureRef SLVelocity = nullptr;
	class FStreamlineRHI* StreamlineRHIExtensions = nullptr;
	uint32 ViewID;
};

class IStreamlineFeature
{
public:
	virtual ~IStreamlineFeature() {}

	// Called only if the Streamline feature was successfully loaded
	virtual void SetLoaded() {};

	// Feature is supported by current hardware/driver/RHI/etc
	virtual Streamline::EStreamlineFeatureSupport QuerySupport() const = 0;
	bool IsSupported() const
	{
		return QuerySupport() == Streamline::EStreamlineFeatureSupport::Supported;
	}

	// Whether feature is active this frame
	virtual bool IsActive() const = 0;

	// Whether feature requires Streamline view extension. Called once at startup.
	virtual bool RequiresStreamlineViewExtension() const = 0;

	// Override this method if the feature requires input resources.
	// Will be called every frame the feature is active in the post-processing pass.
	virtual EStreamlineInputType GetRequiredInputs_RenderThread() const
	{
		return EStreamlineInputType::None;
	}

	// Override this method to implement a render pass.
	// Will be called every frame in the post-processing pass even if feature is not active.
	// But input resources specified by GetRequiredInputs_RenderThread() might not be valid if IsActive returned false.
	virtual void RenderPass_RenderThread(struct FStreamlineRenderPassInputs& Inputs) {}

};

// Use GStreamlineNullFeature to register a feature on platforms that don't support the feature
class FStreamlineNullFeature final : public IStreamlineFeature
{
	virtual Streamline::EStreamlineFeatureSupport QuerySupport() const override final
	{
		return Streamline::EStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime;
	}

	virtual bool IsActive() const override final
	{
		return false;
	}

	virtual bool RequiresStreamlineViewExtension() const override final
	{
		return false;
	}
};

extern UE_API FStreamlineNullFeature GStreamlineNullFeature;

#undef UE_API

