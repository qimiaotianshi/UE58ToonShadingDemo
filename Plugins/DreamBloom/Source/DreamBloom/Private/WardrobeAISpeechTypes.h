#pragma once
#include "CoreMinimal.h"
#include "WardrobeAISpeech.h"
#include "WardrobeAICompanion.h"

struct FWardrobeSpeechClip {
 TArray<int16> PCM;int32 SampleRate=16000;FString Text;FName Emotion;
 TArray<FName> Names;TArray<float> Values;
 int32 Frames=0;float FrameInterval=.02f;
 double SolveSeconds=0;
 FString PlayerText;TArray<FString> VerifiedNotes;FName CharacterId;
 uint64 CampaignRevision=0;bool bRemember=false;
 FString AstrBotSession,AstrBotReceipt,ClipId,PlayerName;
 int32 RecentTurnLimit=12,MemoryNoteLimit=24;
 float Duration() const{return float(PCM.Num())/SampleRate;}
 int64 Bytes() const{int64 N=PCM.GetAllocatedSize()+Values.GetAllocatedSize()+Names.GetAllocatedSize()+Text.GetAllocatedSize()+PlayerText.GetAllocatedSize()+VerifiedNotes.GetAllocatedSize();for(const auto& Note:VerifiedNotes)N+=Note.GetAllocatedSize();return N;}
};
struct FWardrobeSpeechJob {
 TWeakObjectPtr<UWardrobeAISpeechComponent> Owner;uint64 Ticket=0;
 FString Endpoint,ApiKey,Model,Persona,Scene,Topic,Tone,Text,PlayerMessage,TTSEndpoint,ReferenceAudio,ReferenceText,Language;
 EAudioDrivenAnimationMood Mood=EAudioDrivenAnimationMood::Neutral;
 TArray<FWardrobeSpeechMorph> Morphs;
 TArray<FWardrobeSpeechEmotion> Emotions;
 FName Emotion=TEXT("平静");
 FName PreviousEmotion;
 int32 MoodScore=60;FString RecentEvents;
 FString WorldContext,SpeakingStyle,PlayerName,ClockContext;
 FWardrobeCompanionMemory Memory;TArray<FString> VerifiedNotes;
 FName CharacterId;uint64 CampaignRevision=0;bool bRemember=false;
 FString AstrBotSession,AstrBotReceipt,Upstream;
 int32 RecentTurnLimit=12,MemoryNoteLimit=24;
 // Native callbacks are GT-only; clips exclusively own heap arrays.
 FString PlanJSON;
 TFunction<void(const FString&,const FString&)> OnPlan;
 TFunction<void(TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe>,const FString&)> OnPrepared;
 int32 MaxCharacters=60;float MaxDuration=15,Timeout=45,ExpressionIntensity=.65f;
};
class FJsonObject;
namespace WardrobeAISpeech {
 bool BuildChatAuthorization(const FString& ConfiguredKey,FString& Header,FString& Error);
 TSharedRef<FJsonObject> BuildChatRequest(const FWardrobeSpeechJob& Job);
 FString BuildCompanionContext(const FWardrobeSpeechJob& Job);
 void AddAstrBotMetadata(const FWardrobeSpeechJob& Job,TSharedRef<FJsonObject> Object,const FString& Mode);
 bool ParseCompanionReply(const FString& JSON,FWardrobeSpeechJob& Job,FString& Error);
 bool DecodeWave(const TArray<uint8>& Bytes,float MaxDuration,TArray<int16>& Mono,int32& Rate,FString& Error);
 bool CleanDialogue(const FString& Raw,int32 MaxCharacters,const TArray<FWardrobeSpeechEmotion>& Presets,FString& Text,FName& Emotion,FString& Error);
 bool ParseDialogue(const FString& JSON,int32 MaxCharacters,FString& Text,FString& Error,const TArray<FWardrobeSpeechEmotion>* Presets=nullptr,FName* Emotion=nullptr);
 TSharedRef<FJsonObject> BuildVoiceRequest(const FWardrobeSpeechJob& Job);
 void ApplyEmotion(FWardrobeSpeechClip& Clip,const FWardrobeSpeechJob& Job);
 TMap<FName,float> MapFace(const TMap<FString,float>& Controls,const TArray<FWardrobeSpeechMorph>& Mapping);
 void InterpolateFace(const FWardrobeSpeechClip& Clip,float Time,TArray<float>& Values);
}
