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
#include "CoreMinimal.h"

#define UE_API STREAMLINEEXTENSION_API


namespace sl
{
	enum class Result;
}

namespace Streamline
{
	enum class EStreamlineFeatureSupport
	{
		Supported,
		NotSupported,

		NotSupportedIncompatibleHardware,
		NotSupportedHardwareSchedulingDisabled,
		NotSupportedOperatingSystemOutOfDate,
		NotSupportedDriverOutOfDate,

		NotSupportedIncompatibleRHI,
		NotSupportedByPlatformAtBuildTime,
		NotSupportedUnknownFeature,

		NumValues
	};
};

UE_API Streamline::EStreamlineFeatureSupport TranslateStreamlineResult(sl::Result Result);

#undef UE_API

