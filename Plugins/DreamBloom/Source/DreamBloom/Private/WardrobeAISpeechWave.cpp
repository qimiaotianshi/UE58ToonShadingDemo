#include "WardrobeAISpeechWave.h"
#include "WardrobeAISpeechTypes.h"

void UWardrobeAISpeechWave::Initialize(TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Data){
 check(IsInGameThread());Clip=MoveTemp(Data);Cursor.Store(0);NumChannels=1;SampleByteSize=2;
 SetSampleRate(Clip->SampleRate);Duration=Clip->Duration();bLooping=false;VirtualizationMode=EVirtualizationMode::Disabled;
}
int32 UWardrobeAISpeechWave::OnGeneratePCMAudio(TArray<uint8>& Out,int32 Requested){
 TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Data;{FScopeLock Lock(&DataMutex);Data=Clip;}
 if(!Data||Requested<=0)return 0;
 const int32 Start=Cursor.Load();const int32 Count=FMath::Min(Requested,Data->PCM.Num()-Start);
 if(Count<=0)return 0;
 Out.SetNumUninitialized(Count*sizeof(int16));FMemory::Memcpy(Out.GetData(),Data->PCM.GetData()+Start,Out.Num());Cursor.Store(Start+Count);return Count;
}
float UWardrobeAISpeechWave::GetConsumedTime() const{FScopeLock Lock(&DataMutex);return Clip?float(Cursor.Load())/Clip->SampleRate:0.f;}
void UWardrobeAISpeechWave::ReleasePCM(){FScopeLock Lock(&DataMutex);Clip.Reset();}
