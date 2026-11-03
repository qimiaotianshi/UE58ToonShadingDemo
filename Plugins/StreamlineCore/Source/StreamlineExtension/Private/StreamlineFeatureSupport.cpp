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

#include "StreamlineFeatureSupport.h"

#if WITH_STREAMLINE
#include "sl_result.h"
#endif

Streamline::EStreamlineFeatureSupport TranslateStreamlineResult(sl::Result Result)
{
#if WITH_STREAMLINE
	switch (Result)
	{

	case sl::Result::eOk:					return Streamline::EStreamlineFeatureSupport::Supported;
	case sl::Result::eErrorOSDisabledHWS:   return Streamline::EStreamlineFeatureSupport::NotSupportedHardwareSchedulingDisabled;
	case sl::Result::eErrorOSOutOfDate: return Streamline::EStreamlineFeatureSupport::NotSupportedOperatingSystemOutOfDate;
	case sl::Result::eErrorDriverOutOfDate: return Streamline::EStreamlineFeatureSupport::NotSupportedDriverOutOfDate;
	case sl::Result::eErrorNoSupportedAdapterFound: return Streamline::EStreamlineFeatureSupport::NotSupportedIncompatibleHardware;
	case sl::Result::eErrorAdapterNotSupported: return Streamline::EStreamlineFeatureSupport::NotSupportedIncompatibleHardware;
	case sl::Result::eErrorMissingOrInvalidAPI: return Streamline::EStreamlineFeatureSupport::NotSupportedIncompatibleRHI;

	default:
		/* Intentionally falls through*/
		return Streamline::EStreamlineFeatureSupport::NotSupported;
	}
#else
	return Streamline::EStreamlineFeatureSupport::NotSupportedByPlatformAtBuildTime;
#endif	// WITH_STREAMLINE
}

