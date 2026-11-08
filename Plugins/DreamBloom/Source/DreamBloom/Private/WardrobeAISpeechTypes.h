#pragma once
#include "CoreMinimal.h"
#include "WardrobeAISpeech.h"

struct FWardrobeSpeechClip {
 TArray<int16> PCM;int32 SampleRate=16000;FString Text;
 TArray<FName> Names;TArray<float> Values;
 int32 Frames=0;float FrameInterval=.02f;
 double SolveSeconds=0;
 float Duration() const{return float(PCM.Num())/SampleRate;}
 int64 Bytes() const{return PCM.GetAllocatedSize()+Values.GetAllocatedSize()+Names.GetAllocatedSize()+Text.GetAllocatedSize();}
};
struct FWardrobeSpeechJob {
 TWeakObjectPtr<UWardrobeAISpeechComponent> Owner;uint64 Ticket=0;
 FString Endpoint,Model,Persona,Scene,Topic,Tone,Text,PlayerMessage,TTSEndpoint,ReferenceAudio,ReferenceText,Language;
 EAudioDrivenAnimationMood Mood=EAudioDrivenAnimationMood::Neutral;
 TArray<FWardrobeSpeechMorph> Morphs;
 int32 MaxCharacters=60;float MaxDuration=15,Timeout=45,ExpressionIntensity=.65f;
};
class FJsonObject;
namespace WardrobeAISpeech {
 TSharedRef<FJsonObject> BuildChatRequest(const FWardrobeSpeechJob& Job);
 bool DecodeWave(const TArray<uint8>& Bytes,float MaxDuration,TArray<int16>& Mono,int32& Rate,FString& Error);
 bool ParseDialogue(const FString& JSON,int32 MaxCharacters,FString& Text,FString& Error);
 TMap<FName,float> MapFace(const TMap<FString,float>& Controls,const TArray<FWardrobeSpeechMorph>& Mapping);
 void InterpolateFace(const FWardrobeSpeechClip& Clip,float Time,TArray<float>& Values);
}
