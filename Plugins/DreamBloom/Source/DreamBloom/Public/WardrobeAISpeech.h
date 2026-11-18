#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Components/ActorComponent.h"
#include "SpeechAnimationSolverTypes.h"
#include "WardrobeAISpeech.generated.h"

class UAudioComponent;class UWardrobeAISpeechWave;class UNNEModelData;class UWardrobeAISpeechSubsystem;class UWardrobeSpeechBubble;
struct FWardrobeSpeechClip;struct FWardrobeSpeechJob;

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeSpeechTopic {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Scene;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Topic;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Tone;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) EAudioDrivenAnimationMood Mood=EAudioDrivenAnimationMood::Neutral;
};
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeSpeechMorph {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Control;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Morph;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="4")) float Gain=1;
};
/** Static facial expression layered over speech; vowel morphs are protected at runtime. */
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeSpeechEmotionMorph {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Morph;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="1",UIMin="0",UIMax="1")) float Weight=.5f;
};
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeSpeechEmotion {
 GENERATED_BODY()
 /** Exact tag the AI can return, e.g. 【开心】. Use one unique name per preset. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Emotion;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="2")) float Intensity=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(TitleProperty="Morph")) TArray<FWardrobeSpeechEmotionMorph> Morphs;
};
UENUM(BlueprintType)
enum class EWardrobeTTSDevice : uint8 { CPU, CUDA };
UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeAISpeechData : public UDataAsset {
 GENERATED_BODY()
public:
 UWardrobeAISpeechData();
 /** Append built-in expressions by name, preserving edited presets and custom entries. */
 UFUNCTION(BlueprintCallable,CallInEditor,Category="Emotions",meta=(DisplayName="补齐默认心情表情")) void AddMissingDefaultEmotions();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Speech") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") FString ChatEndpoint=TEXT("https://api.deepseek.com/chat/completions");
 /** Stored in this asset; password masking is only an editor display feature. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue",meta=(DisplayName="API Key",PasswordField="true")) FString ApiKey;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue") FString Model=TEXT("deepseek-chat");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue",meta=(MultiLine="true")) FString Persona;
 /** Shared factual setting for chat and events. Do not put credentials here. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Companion",meta=(MultiLine="true",DisplayName="世界与关系背景")) FString WorldContext;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Companion",meta=(MultiLine="true",DisplayName="说话习惯")) FString SpeakingStyle;
 /** Use the project-local AstrBot service and its installed companion/memory plugins. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Companion",meta=(DisplayName="使用 AstrBot 陪伴与记忆")) bool bUseAstrBot=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Companion",meta=(DisplayName="记住对话")) bool bRememberConversations=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Companion",AdvancedDisplay,meta=(ClampMin="4",ClampMax="32",EditCondition="bRememberConversations")) int32 RecentTurnLimit=12;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Companion",AdvancedDisplay,meta=(ClampMin="8",ClampMax="64",EditCondition="bRememberConversations")) int32 MemoryNoteLimit=24;
 /** Each unanswered automatic line lengthens the next interval, up to three steps. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Timing",AdvancedDisplay,meta=(ClampMin="1",ClampMax="3")) float UnansweredCooldownScale=1.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue",meta=(TitleProperty="Topic")) TArray<FWardrobeSpeechTopic> Topics;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dialogue",meta=(ClampMin="5",ClampMax="120")) int32 MaxTextCharacters=60;
 /** Stable ID shared by outfits of the same character, and independent of actor tags. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Events") FName CharacterId=TEXT("Daniya");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Events") bool bEventsEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Events",meta=(MultiLine="true",EditCondition="bEventsEnabled")) FString EventBackground;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Events",meta=(ClampMin="10",Units="s",EditCondition="bEventsEnabled")) float EventMinInterval=60;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Events",meta=(ClampMin="10",Units="s",EditCondition="bEventsEnabled")) float EventMaxInterval=120;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Events",meta=(ClampMin="0",ClampMax="100")) int32 InitialMood=60;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="AI Events",meta=(ClampMin="0",ClampMax="10",EditCondition="bEventsEnabled")) int32 MaxMoodChange=5;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Timing",meta=(ClampMin="1",Units="s")) float MinInterval=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Timing",meta=(ClampMin="1",Units="s")) float MaxInterval=30;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Voice") FString TTSEndpoint=TEXT("http://127.0.0.1:9880/tts");
 /** Relative to the UE project. Content/AI discovers the portable bundle in its wrapper folders. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Local TTS",meta=(DisplayName="AI Folder")) FString TTSRoot=TEXT("Content/AI");
 /** Starts the bundled local server when this profile is first selected. Disable for an external server. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Local TTS",meta=(DisplayName="Auto Start Local Service")) bool bAutoStart=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Local TTS",meta=(DisplayName="Compute Device",EditCondition="bAutoStart")) EWardrobeTTSDevice TTSDevice=EWardrobeTTSDevice::CPU;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Local TTS",meta=(DisplayName="CPU Threads",ClampMin="1",ClampMax="8",EditCondition="bAutoStart && TTSDevice == EWardrobeTTSDevice::CPU")) int32 CPUThreads=4;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Voice") FFilePath ReferenceAudio;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Voice",meta=(MultiLine="true")) FString ReferenceText;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Voice") FString Language=TEXT("zh");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Voice",meta=(ClampMin="0",ClampMax="100",UIMin="0",UIMax="100")) float VoiceVolume=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face",meta=(TitleProperty="Morph")) TArray<FWardrobeSpeechMorph> Morphs;
 /** Unsupported mesh morphs are skipped. Empty presets still provide a valid mood tag. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Emotions",meta=(TitleProperty="Emotion")) TArray<FWardrobeSpeechEmotion> Emotions;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face",meta=(ClampMin="0",ClampMax="2")) float ExpressionIntensity=.65f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Face",meta=(ClampMin="0",ClampMax="1",Units="s")) float FaceBlendTime=.15f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Advanced",AdvancedDisplay,meta=(ClampMin="1",ClampMax="30",Units="s")) float MaxAudioDuration=15;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Advanced",AdvancedDisplay,meta=(ClampMin="5",ClampMax="120",Units="s")) float RequestTimeout=45;
};

UENUM(BlueprintType)
enum class EWardrobeSpeechState:uint8 {Disabled,Idle,RequestingText,GeneratingVoice,SolvingFace,Playing,Fading,Error};

/** Game-thread coordinator. Worker threads never mutate this component or its mesh. */
UCLASS(ClassGroup=Wardrobe,meta=(BlueprintSpawnableComponent))
class DREAMBLOOM_API UWardrobeAISpeechComponent : public UActorComponent {
 GENERATED_BODY()
public:
 UWardrobeAISpeechComponent();
 UPROPERTY(Transient,BlueprintReadOnly,Category="AI Speech") EWardrobeSpeechState State=EWardrobeSpeechState::Disabled;
 UPROPERTY(Transient,BlueprintReadOnly,Category="AI Speech") FString LastError;
 UPROPERTY(Transient,BlueprintReadOnly,Category="AI Speech") FString CurrentText;
 UPROPERTY(Transient,BlueprintReadOnly,Category="AI Speech") FName CurrentEmotion;
 UFUNCTION(BlueprintCallable,Category="AI Speech") bool RequestDialogueNow();
 /** Ask the character to reply to one player message through the configured LLM. */
 UFUNCTION(BlueprintCallable,Category="AI Speech") bool RequestReply(const FString& Message);
 /** Pauses only the automatic timer; explicit player messages can still be sent. */
 UFUNCTION(BlueprintCallable,Category="AI Speech") void SetAutomaticPaused(bool Paused);
 UFUNCTION(BlueprintPure,Category="AI Speech") bool IsAutomaticPaused() const{return bAutomaticPaused;}
 UFUNCTION(BlueprintPure,Category="AI Speech") bool IsBusy() const;
 static constexpr int32 MaxPlayerMessageCharacters=500;
 /** Uses the same TTS/face/playback path for authoring tests without spending an LLM request. */
 UFUNCTION(BlueprintCallable,Category="AI Speech") bool SpeakText(const FString& Text);
 UFUNCTION(BlueprintCallable,Category="AI Speech") void CancelSpeech();
 UFUNCTION(BlueprintPure,Category="AI Speech") int32 GetTemporaryBytes() const;
 void GetFaceSnapshot(TArray<FName>& Names,TArray<float>& Values,float& Weight) const;
 void AcceptClip(uint64 Ticket,TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> InClip);
 void Fail(uint64 Ticket,const FString& Error);
 void SetStage(uint64 Ticket,EWardrobeSpeechState Stage);
 const UWardrobeAISpeechData* GetProfile() const;
 bool IsIdleAllowed() const;
 uint64 GetTicket() const{return Generation;}
 /** Copies DA/mesh settings on the game thread, without making a request. */
 bool BuildPreparationJob(FWardrobeSpeechJob& Job) const;
 void SetEventReserved(bool Reserved);
 bool PlayPreparedClip(TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Prepared);
 bool IsEventReserved() const{return bEventReserved;}
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* ThisTick) override;
private:
 friend class FWardrobeCompanionPlaybackTest;
 friend class FWardrobeSpeechPauseTest;
 friend class FWardrobeSpeechPresentationTest;
 void ReleaseSpeech(bool NaturalFinish);
 void ScheduleNext();bool Start(const FString* Text,const FString* PlayerMessage=nullptr);
 UFUNCTION() void OnAudioFinished();
 UFUNCTION() void OnPlaybackPercent(const USoundWave* Sound,float Percent);
 UPROPERTY(Transient) TObjectPtr<UAudioComponent> Audio;
 UPROPERTY(Transient) TObjectPtr<UWardrobeAISpeechWave> Wave;
 UPROPERTY(Transient) TObjectPtr<UWardrobeSpeechBubble> SpeechBubble;
 TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Clip;
 TArray<FName> FaceNames;TArray<float> FaceValues;
 TWeakObjectPtr<UWardrobeAISpeechData> LastProfile;
 uint64 Generation=0;
 double NextRequestTime=0,StartedAt=0;
 float FaceWeight=0,FadeAge=0,FadeFrom=0,PlaybackTime=0;
 int32 PreviousTopic=INDEX_NONE,UnansweredLines=0;
 uint64 LastCampaignRevision=MAX_uint64;
 FName LastCharacterId;
 FName LastPlayedEmotion;
 bool bAutomaticPaused=false,bEventReserved=false;
};
