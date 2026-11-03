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

#pragma once

#include "StreamlineNGXCommon.h"
#include "Graph/Nodes/MovieGraphDeferredPassNode.h"
#include "MoviePipelineDLSSSetting.h"

#include "MovieGraphDLSSRenderPassNode.generated.h"

/**
 * Movie Render Graph node that renders using the Deferred Renderer with DLSS/DLAA upscaling.
 * Add this node to a Movie Render Graph in place of (or alongside) the standard Deferred
 * Renderer node to enable DLSS upscaling during offline rendering.
 */
UCLASS(MinimalAPI, BlueprintType)
class UMovieGraphDLSSRenderPassNode : public UMovieGraphDeferredRenderPassNode
{
	GENERATED_BODY()

public:
	UMovieGraphDLSSRenderPassNode();

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

#if !UE_VERSION_OLDER_THAN(5,7,0)
	virtual bool GetNodeValidationErrors(const FName& InBranchName, const UMovieGraphEvaluatedConfig* InEvaluatedConfig, TArray<FText>& OutValidationErrors) const override;
#endif

	/** DLSS/DLAA quality preset used during rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DLSS/DLAA settings", DisplayName = "DLSS Quality")
	TEnumAsByte<EMoviePipelineDLSSQuality> DLSSQuality;


	void GetFormatResolveArgs(FMovieGraphResolveArgs& OutMergedFormatArgs, const FMovieGraphRenderDataIdentifier& InRenderDataIdentifier) const override;

	EMoviePipelineDLSSQuality GetDLSSQuality() {return DLSSQuality;}

protected:
	virtual TUniquePtr<UE::MovieGraph::Rendering::FMovieGraphImagePassBase> CreateInstance() const override;
};
