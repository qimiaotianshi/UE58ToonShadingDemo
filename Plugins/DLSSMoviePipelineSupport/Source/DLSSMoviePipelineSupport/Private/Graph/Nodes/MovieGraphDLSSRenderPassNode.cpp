/*
* Copyright (c) 2020 - 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
* NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
* property and proprietary rights in and to this material, related
* documentation and any modifications thereto. Any use, reproduction,
* disclosure or distribution of this material and related documentation
* without an express license agreement from NVIDIA CORPORATION or
* its affiliates is strictly prohibited.
*/

#include "Graph/Nodes/MovieGraphDLSSRenderPassNode.h"

//#if !UE_VERSION_OLDER_THAN(5,7,0)
#include "DLSS.h"
#include "DLSSLibrary.h"
#include "Graph/Renderers/MovieGraphDeferredPass.h"
#include "Graph/Renderers/MovieGraphImagePassBase.h"
#include "SceneView.h"
#include "MoviePipelineUtils.h"

#define LOCTEXT_NAMESPACE "MovieGraphDLSSRenderPassNode"

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

namespace DLSSMRGHelpers
{
	static UDLSSMode EMoviePipelineDLSSQualityToUDLSSMode(EMoviePipelineDLSSQuality InDLSSQuality)
	{
		// Offset matches the mapping used by the classic UMoviePipelineDLSSSetting.
		const uint8 OffsetDlssQuality = static_cast<uint8>(InDLSSQuality) + static_cast<uint8>(UDLSSMode::DLAA);
		return static_cast<UDLSSMode>(OffsetDlssQuality);
	}
}

// ---------------------------------------------------------------------------
// FMovieGraphDLSSPass — renderer-side implementation
// ---------------------------------------------------------------------------

namespace UE::MovieGraph::Rendering
{
	struct FMovieGraphDLSSPass : public FMovieGraphDeferredPass
	{
		FMovieGraphDLSSPass() = default;

		// Override CreateSceneViewFamily to inject the DLSS view extension BEFORE
		// CreateSceneView is called so that SetupView() fires on the DLSS extension.
		virtual TSharedRef<FSceneViewFamilyContext> CreateSceneViewFamily(const FViewFamilyInitData& InInitData) const override
		{
			TSharedRef<FSceneViewFamilyContext> Family = FMovieGraphDeferredPass::CreateSceneViewFamily(InInitData);

			if (InInitData.ViewModeIndex == VMI_Lit)
			{
				if (IDLSSModuleInterface* DLSSModule = FModuleManager::GetModulePtr<IDLSSModuleInterface>("DLSS"))
				{
					TSharedPtr<ISceneViewExtension> DLSSViewExt = DLSSModule->GetDLSSUpscalerViewExtension();

					if (DLSSViewExt.IsValid() && !Family->ViewExtensions.Contains(DLSSViewExt))
					{
						Family->ViewExtensions.Add(DLSSViewExt.ToSharedRef());
					}
				}
			}

			return Family;
		}

		// Override ApplyMovieGraphOverridesToViewFamily to set r.ScreenPercentage to
		// the DLSS-optimal value before the base installs FLegacyScreenPercentageDriver.
		virtual void ApplyMovieGraphOverridesToViewFamily(TSharedRef<FSceneViewFamilyContext> InOutFamily, const FViewFamilyInitData& InInitData) const override
		{
			bool bIsSupported = false;

			if (InInitData.ViewModeIndex == VMI_Lit)
			{
				float OptimalScreenPercentage = 100.f;
				FVector2D DummyRes{};
				bool bDummyFixed;
				float DummyMin, DummyMax, DummySharpness;

				UDLSSLibrary::GetDLSSModeInformation(DLSSMRGHelpers::EMoviePipelineDLSSQualityToUDLSSMode(DLSSQuality),	DummyRes, bIsSupported, OptimalScreenPercentage,bDummyFixed, DummyMin, DummyMax, DummySharpness);

				if (bIsSupported)
				{
					static IConsoleVariable* CVarScreenPercentage =	IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
					if (CVarScreenPercentage)
					{
						EConsoleVariableFlags Priority = static_cast<EConsoleVariableFlags>(CVarScreenPercentage->GetFlags() & ECVF_SetByMask);
						CVarScreenPercentage->Set(OptimalScreenPercentage, Priority);
					}
				}
			}

			// Enable/disable DLSS globally so the upscaler activates for the upcoming render.
			UDLSSLibrary::EnableDLSS(bIsSupported);

			// Call base AFTER setting the CVar so FLegacyScreenPercentageDriver reads our value.
			FMovieGraphDeferredPass::ApplyMovieGraphOverridesToViewFamily(InOutFamily, InInitData);
		}

		EMoviePipelineDLSSQuality DLSSQuality;

		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, TWeakObjectPtr<UMovieGraphImagePassBaseNode> InRenderPassNode, const FMovieGraphRenderPassLayerData& InLayer) override
		{
			FMovieGraphDeferredPass::Setup(InRenderer, InRenderPassNode, InLayer);

			UMovieGraphDLSSRenderPassNode* ParentNode = Cast<UMovieGraphDLSSRenderPassNode>(InLayer.RenderPassNode);
			DLSSQuality = ParentNode->GetDLSSQuality();
		}

	};

} // namespace UE::MovieGraph::Rendering

// ---------------------------------------------------------------------------
// UMovieGraphDLSSRenderPassNode
// ---------------------------------------------------------------------------

UMovieGraphDLSSRenderPassNode::UMovieGraphDLSSRenderPassNode()
	: DLSSQuality(EMoviePipelineDLSSQuality::EMoviePipelineDLSSQuality_Balanced)
{}

#if WITH_EDITOR
FText UMovieGraphDLSSRenderPassNode::GetNodeTitle(const bool bGetDescriptive) const
{
	static const FText DLSSPassNodeName = NSLOCTEXT("MovieGraphNodes", "NodeName_DLSSDeferredRenderPassGraphNode", "DLSS Deferred Renderer Pass");
	static const FText DLSSPassNodeDescription = NSLOCTEXT("MovieGraphNodes","NodeDescription_DLSSDeferredRenderPassGraphNode","DLSS Mode: \"{0}\"");

	if (bGetDescriptive)
	{
		return FText::Format(DLSSPassNodeDescription, FText::FromString(UEnum::GetValueAsString(DLSSQuality)));
	}

	return DLSSPassNodeName;

}

void UMovieGraphDLSSRenderPassNode::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Skip rapid updates from properties; only refresh on commit
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UMovieGraphDLSSRenderPassNode, DLSSQuality))
	{
		OnNodeChangedDelegate.Broadcast(this);
	}
}

#endif

#if !UE_VERSION_OLDER_THAN(5, 7, 0)
bool UMovieGraphDLSSRenderPassNode::GetNodeValidationErrors(const FName& InBranchName, const UMovieGraphEvaluatedConfig* InEvaluatedConfig, TArray<FText>& OutValidationErrors) const
{
	if (Super::GetNodeValidationErrors(InBranchName, InEvaluatedConfig, OutValidationErrors))
	{
		return true;
	}

	if (!UDLSSLibrary::IsDLSSSupported())
	{
		OutValidationErrors.Add(FText::Format(LOCTEXT("DLSS_Unsupported", "DLSS is not supported due to \"{0}\"."), StaticEnum<UDLSSSupport>()->GetDisplayNameTextByValue(int64(UDLSSLibrary::QueryDLSSSupport()))));
		return true;

	}
	else if (!UDLSSLibrary::IsDLSSModeSupported(DLSSMRGHelpers::EMoviePipelineDLSSQualityToUDLSSMode(DLSSQuality)))
	{
		OutValidationErrors.Add(FText::Format(LOCTEXT("DLSS_Mode_Unsupported", "\"{0}\" Mode is not supported. Please try a lower quality setting."), StaticEnum<EMoviePipelineDLSSQuality>()->GetDisplayNameTextByIndex((int64)DLSSQuality)));
		return true;
	}
	
	return false;
}
#endif

void UMovieGraphDLSSRenderPassNode::GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const
{

	FString MetadataPrefix = 
#if !UE_VERSION_OLDER_THAN(5,7,0)
	UE::MoviePipeline::GetMetadataPrefixPath(InRenderDataIdentifier);
#else
	TEXT("unreal/sampling");
#endif
	OutMergedFormatArgs.FilenameArguments.Add(TEXT("dlssQuality"), UEnum::GetValueAsString(DLSSQuality));
	OutMergedFormatArgs.FileMetadata.Add(FString::Printf(TEXT("%s/dlssQuality"), *MetadataPrefix), UEnum::GetValueAsString(DLSSQuality));
	
}

TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase>
UMovieGraphDLSSRenderPassNode::CreateInstance() const
{
	return MakeUnique<UE::MovieGraph::Rendering::FMovieGraphDLSSPass>();
}

#undef LOCTEXT_NAMESPACE
//#endif
