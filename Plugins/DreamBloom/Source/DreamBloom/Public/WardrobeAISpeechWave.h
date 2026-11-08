#pragma once
#include "CoreMinimal.h"
#include "Sound/SoundWaveProcedural.h"
#include "WardrobeAISpeechWave.generated.h"
struct FWardrobeSpeechClip;
/** Immutable PCM shared with the render thread; cursor is atomic and never resets in flight. */
UCLASS()
class DREAMBLOOM_API UWardrobeAISpeechWave : public USoundWaveProcedural {
 GENERATED_BODY()
public:
 void Initialize(TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Data);
 virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio,int32 NumSamples) override;
 float GetConsumedTime() const;
 void ReleasePCM();
private:
 mutable FCriticalSection DataMutex;
 TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Clip;
 TAtomic<int32> Cursor{0};
};
