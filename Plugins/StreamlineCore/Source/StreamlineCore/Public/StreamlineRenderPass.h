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

#include "StreamlineRHI.h"

#include "ShaderParameterMacros.h"

// add a render pass to update Streamline feature state
BEGIN_SHADER_PARAMETER_STRUCT(FSLSetStateShaderParameters, )
END_SHADER_PARAMETER_STRUCT()


// Optionally allow setting SL feature state from the same thread that calls Present().
// The present thread will be either the RHI thread or the RHI submission thread depending on engine version and options.
// See r.D3D12.PresentOnSubmissionThread
enum class ESetSLStateThread
{
	RHIThread,
	PresentThread
};

template< bool bSupportPresentThread = false, typename StateOnRenderThreadLambda, typename SetStateLambda >
void AddStreamlineStateRenderPass(const TCHAR* FeatureName, FRDGBuilder& GraphBuilder, uint32 ViewID, const FIntRect& SecondaryViewRect,
	StateOnRenderThreadLambda&& StateOnRenderThread, SetStateLambda&& SetState, ESetSLStateThread SetStateThread = ESetSLStateThread::RHIThread)
{
	FSLSetStateShaderParameters* PassParameters = GraphBuilder.AllocParameters<FSLSetStateShaderParameters>();

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Streamline %s State ViewID = % u", FeatureName, ViewID),
		PassParameters,
		ERDGPassFlags::Compute | ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass | ERDGPassFlags::NeverCull,
		[PassParameters, ViewID, SecondaryViewRect, SetState=Forward<SetStateLambda>(SetState), StateOnRenderThread=Forward<StateOnRenderThreadLambda>(StateOnRenderThread), SetStateThread](FRHICommandListImmediate& RHICmdList) mutable
		{
			auto Options = StateOnRenderThread(ViewID, SecondaryViewRect);

			RHICmdList.EnqueueLambda(
				[ViewID, Options, SetStateThread, SetState = Forward<SetStateLambda>(SetState)](FRHICommandListImmediate&) mutable
				{
					if constexpr (bSupportPresentThread)
					{
						if (SetStateThread == ESetSLStateThread::PresentThread)
						{
							FStreamlineRHI* StreamlineRHI = GetPlatformStreamlineRHI();
							if (StreamlineRHI)
							{
								// We intentionally pass this lambda through multiple queues: graphbuilder → rhi → present/submission thread.
								// By not enqueuing directly from the render thread, we avoid getting off by a frame from rendering
								StreamlineRHI->SetPrePresentCommand([ViewID, Options, SetState = Forward<SetStateLambda>(SetState)]() mutable
								{
									SetState(ViewID, Options);
								});
							}
							// we don't expect this to happen normally. But if ever no SL RHI is available, then no SL features are loaded, so we shouldn't proceed anyway.
							return;
						}
					}

					SetState(ViewID, Options);
				});
		});
}

