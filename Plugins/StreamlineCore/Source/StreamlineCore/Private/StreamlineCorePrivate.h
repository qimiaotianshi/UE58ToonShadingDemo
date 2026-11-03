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
#pragma once

#include "CoreMinimal.h"
#include "Slate/SceneViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "StreamlineRHI.h"
#include "StreamlineNGXRHI.h"
#include "StreamlineNGXRenderer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStreamline, Log, All);
DECLARE_GPU_STAT_NAMED_EXTERN(Streamline, TEXT("Streamline"));


bool ShouldTagStreamlineBuffers();
bool ForceTagStreamlineBuffers();
bool ShouldTrackViews();
bool NeedStreamlineViewIdOverride();

#if !ENGINE_PROVIDES_UE_5_6_ID3D12DYNAMICRHI_METHODS
BEGIN_SHADER_PARAMETER_STRUCT(FDebugLayerCompatibilityShaderParameters, )
	RDG_TEXTURE_ACCESS(DebugLayerCompatibilityHelperSource, ERHIAccess::CopySrc)
	RDG_TEXTURE_ACCESS(DebugLayerCompatibilityHelperDest, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

inline void AddDebugLayerCompatibilitySetupPasses(FRDGBuilder& GraphBuilder, FDebugLayerCompatibilityShaderParameters* PassParameters)
{
	NV_RDG_EVENT_SCOPE(GraphBuilder,Streamline, "UE5.5AndOlderDebugLayerCompatibilitySetup");
	FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable);
	PassParameters->DebugLayerCompatibilityHelperSource = GraphBuilder.CreateTexture(Desc, TEXT("UE5.5AndOlderDebugLayerCompatibilityHelperSource"));
	PassParameters->DebugLayerCompatibilityHelperDest = GraphBuilder.CreateTexture(Desc, TEXT("UE5.5AndOlderDebugLayerCompatibilityHelperDest"));
	AddClearRenderTargetPass(GraphBuilder, PassParameters->DebugLayerCompatibilityHelperSource);
	AddClearRenderTargetPass(GraphBuilder, PassParameters->DebugLayerCompatibilityHelperDest);
}

inline void DebugLayerCompatibilityRHISetup(const FDebugLayerCompatibilityShaderParameters& PassParameters, FRHIStreamlineResource& Texture)
{
	check(PassParameters.DebugLayerCompatibilityHelperSource);
	PassParameters.DebugLayerCompatibilityHelperSource->MarkResourceAsUsed();

	check(PassParameters.DebugLayerCompatibilityHelperDest);
	PassParameters.DebugLayerCompatibilityHelperDest->MarkResourceAsUsed();

	Texture.DebugLayerCompatibilityHelperSource = PassParameters.DebugLayerCompatibilityHelperSource->GetRHI();
	Texture.DebugLayerCompatibilityHelperDest = PassParameters.DebugLayerCompatibilityHelperDest->GetRHI();
}

template<typename Array>
inline void DebugLayerCompatibilityRHISetup(const FDebugLayerCompatibilityShaderParameters& PassParameters, Array& Textures)
{
	for (FRHIStreamlineResource& Texture : Textures)
	{
		DebugLayerCompatibilityRHISetup(PassParameters, Texture);
	}
}
#endif 

