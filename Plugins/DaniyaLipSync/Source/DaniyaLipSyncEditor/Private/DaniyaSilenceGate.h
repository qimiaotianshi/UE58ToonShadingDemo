#pragma once
#include "CoreMinimal.h"
class USoundWave;
class UAnimSequence;
namespace DaniyaSilence
{
    bool Validate(float ThresholdDb, float CloseSeconds, float OpenSeconds, FString& Error);
    // Interleaved signed 16-bit PCM. Max channel RMS avoids cancellation of opposite-phase stereo.
    bool Analyze(TArrayView<const int16> PCM, int32 SampleRate, int32 Channels, float ThresholdDb, float CloseSeconds, float OpenSeconds, TArray<float>& Gates, FString& Error);
    bool Apply(USoundWave* Audio, UAnimSequence* Animation, float ThresholdDb, float CloseSeconds, float OpenSeconds, FString& Error);
}
