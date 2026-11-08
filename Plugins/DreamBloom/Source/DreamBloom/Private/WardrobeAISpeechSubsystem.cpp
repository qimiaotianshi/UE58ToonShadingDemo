#include "WardrobeAISpeechSubsystem.h"
#include "WardrobeStartup.h"
#include "WardrobeAISpeechTypes.h"
#include "WardrobeAISpeechPaths.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "NNEModelData.h"
#include "SpeechAnimationSolverV4.h"
#include "GuiToRawControlsUtils.h"
#include "UObject/StrongObjectPtr.h"

DEFINE_LOG_CATEGORY_STATIC(LogWardrobeAISpeech,Log,All);
struct FWardrobeSpeechWorker {
 // Created on GT; a task owns this pin until its completion, even after EndPlay/GC.
 TSharedPtr<TStrongObjectPtr<UNNEModelData>,ESPMode::ThreadSafe> Pin;
 TUniquePtr<FSpeechAnimationSolverV4> Solver;
};
struct FWardrobeSpeechRequest {
 FWardrobeSpeechJob Job;
 TAtomic<bool> Cancelled{false};
 FHttpRequestPtr HTTP;
 TArray<uint8> Body;int32 Limit=0;bool bOverflow=false;
};
namespace {
FString Encode(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
FString UTF8(const TArray<uint8>& B){FUTF8ToTCHAR C(reinterpret_cast<const ANSICHAR*>(B.GetData()),B.Num());return FString(C.Length(),C.Get());}
bool ValidHTTP(const FString& S){return S.StartsWith(TEXT("http://"))||S.StartsWith(TEXT("https://"));}
// A bounded streamed body prevents even a malfunctioning server from building an unlimited response.
FHttpRequestRef MakeHTTP(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R,const FString& URL,const FString& JSON,int32 Limit){
 R->Body.Reset();R->Limit=Limit;R->bOverflow=false;
 const auto H=FHttpModule::Get().CreateRequest();R->HTTP=H;H->SetURL(URL);H->SetVerb(TEXT("POST"));H->SetHeader(TEXT("Content-Type"),TEXT("application/json"));H->SetTimeout(R->Job.Timeout);H->SetActivityTimeout(R->Job.Timeout);
 H->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
 H->SetContentAsString(JSON);
 H->SetResponseBodyReceiveStreamDelegateV2(FHttpRequestStreamDelegateV2::CreateLambda([R](void* Ptr,int64& N){
  if(R->Cancelled.Load()||N<0||N>R->Limit-R->Body.Num()){R->bOverflow=true;N=0;return;}
  R->Body.Append(static_cast<uint8*>(Ptr),int32(N));
 }));return H;
}
}

FString UWardrobeAISpeechSubsystem::GetLocalConfigPath() const{return FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Config/WardrobeAISpeech.local.ini"));}
FString UWardrobeAISpeechSubsystem::GetResolvedTTSRoot() const{
 FString Root;GConfig->GetString(TEXT("WardrobeAISpeech"),TEXT("TTSRoot"),Root,GGameIni);
 FConfigFile Local;Local.Read(GetLocalConfigPath());Local.GetString(TEXT("WardrobeAISpeech"),TEXT("TTSRoot"),Root);
 FString Error;return WardrobeAISpeech::ResolveTTSRoot(Root,FPaths::ProjectDir(),Error);
}
UWardrobeAISpeechSubsystem::UWardrobeAISpeechSubsystem(){FaceModel=TSoftObjectPtr<UNNEModelData>(FSoftObjectPath(TEXT("/StreamingADA/xsada_face_base_fp32_v2_0_0.xsada_face_base_fp32_v2_0_0")));}
void UWardrobeAISpeechSubsystem::Initialize(FSubsystemCollectionBase& C){
 C.InitializeDependency<UWardrobeStartupSubsystem>();
 Super::Initialize(C);bEnding=false;
 const FString Path=GetLocalConfigPath();
 if(!FPaths::FileExists(Path)){
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
  FFileHelper::SaveStringToFile(TEXT("; Local only: never distribute API keys. Service defaults come from DefaultGame.ini.\n; Optional overrides: TTSRoot=Content/AI, AutoStart=true, TTSDevice=cpu, CPUThreads=4\n[WardrobeAISpeech]\nApiKey=\nReferenceAudio=\nReferenceText=\n"),*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
 }
 EnsureService();
}
void UWardrobeAISpeechSubsystem::Deinitialize(){
 bEnding=true;if(Active){Active->Cancelled.Store(true);if(Active->HTTP)Active->HTTP->CancelRequest();Active->HTTP.Reset();}
 if(LoadHandle)LoadHandle->CancelHandle();LoadHandle.Reset();Active.Reset();Worker.Reset();ApiKey.Reset();Super::Deinitialize();
}
bool UWardrobeAISpeechSubsystem::IsBusy() const{return Active.IsValid();}
void UWardrobeAISpeechSubsystem::EnsureService(){
 FConfigFile C;C.Read(GetLocalConfigPath());bool Auto=false;GConfig->GetBool(TEXT("WardrobeAISpeech"),TEXT("AutoStart"),Auto,GGameIni);C.GetBool(TEXT("WardrobeAISpeech"),TEXT("AutoStart"),Auto);if(!Auto)return;
 FString Configured;GConfig->GetString(TEXT("WardrobeAISpeech"),TEXT("TTSRoot"),Configured,GGameIni);C.GetString(TEXT("WardrobeAISpeech"),TEXT("TTSRoot"),Configured);
 FString Error;const FString Root=WardrobeAISpeech::ResolveTTSRoot(Configured,FPaths::ProjectDir(),Error);
 if(Root.IsEmpty()){UE_LOG(LogWardrobeAISpeech,Warning,TEXT("Auto-start skipped: %s"),*Error);return;}
 FString Device=TEXT("cpu");GConfig->GetString(TEXT("WardrobeAISpeech"),TEXT("TTSDevice"),Device,GGameIni);C.GetString(TEXT("WardrobeAISpeech"),TEXT("TTSDevice"),Device);if(Device!=TEXT("cuda"))Device=TEXT("cpu");
 int32 Threads=4;GConfig->GetInt(TEXT("WardrobeAISpeech"),TEXT("CPUThreads"),Threads,GGameIni);C.GetInt(TEXT("WardrobeAISpeech"),TEXT("CPUThreads"),Threads);Threads=FMath::Clamp(Threads,1,8);
 const FString Python=FPaths::Combine(Root,TEXT("runtime/python.exe"));
 const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("DreamBloom"));
 if(!Plugin||!FPaths::FileExists(Python)){UE_LOG(LogWardrobeAISpeech,Warning,TEXT("Auto-start skipped: configure a valid local TTSRoot."));return;}
 const FString Script=FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(),TEXT("Resources/AI/wardrobe_tts_server.py")));
 if(!FPaths::FileExists(Script)){UE_LOG(LogWardrobeAISpeech,Warning,TEXT("Auto-start skipped: DreamBloom Resources/AI/wardrobe_tts_server.py was not staged as a loose file."));return;}
 auto H=FHttpModule::Get().CreateRequest();H->SetURL(TEXT("http://127.0.0.1:9880/wardrobe/status"));H->SetVerb(TEXT("GET"));H->SetTimeout(2.f);
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),Python,Script,Root,Device,Threads](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  if(!Weak.IsValid()||Weak->bEnding)return;
  // Any HTTP server on the port is already owned by the user; never launch a duplicate.
  if(OK&&Response)return;
  const FString Args=FString::Printf(TEXT("\"%s\" --root \"%s\" --device %s --cpu-threads %d"),*Script,*Root,*Device,Threads);
  auto P=FPlatformProcess::CreateProc(*Python,*Args,true,true,true,nullptr,0,*Root,nullptr);
  if(P.IsValid()){FPlatformProcess::CloseProc(P);UE_LOG(LogWardrobeAISpeech,Display,TEXT("Started the local serialized TTS service. Models remain resident until the service is stopped."));}
  else UE_LOG(LogWardrobeAISpeech,Warning,TEXT("Could not start the local TTS service."));
 });H->ProcessRequest();
}
void UWardrobeAISpeechSubsystem::Prewarm(){
 check(IsInGameThread());if(bEnding||Active||Worker||bPrewarming)return;
 bPrewarming=true;
 LoadHandle=UAssetManager::GetStreamableManager().RequestAsyncLoad(FaceModel.ToSoftObjectPath(),FStreamableDelegate::CreateLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this)]{
  auto* S=Weak.Get();if(!S||S->bEnding)return;S->LoadHandle.Reset();
  auto* M=S->FaceModel.Get();
  if(!M){S->bPrewarming=false;UE_LOG(LogWardrobeAISpeech,Warning,TEXT("Startup face warmup: model is unavailable."));return;}
  S->Worker=MakeShared<FWardrobeSpeechWorker,ESPMode::ThreadSafe>();
  S->Worker->Pin=MakeShareable(new TStrongObjectPtr<UNNEModelData>(M),[](TStrongObjectPtr<UNNEModelData>* P){if(IsInGameThread())delete P;else AsyncTask(ENamedThreads::GameThread,[P]{delete P;});});
  const auto W=S->Worker;
  Async(EAsyncExecution::ThreadPool,[Weak,W]{
   W->Solver=MakeUnique<FSpeechAnimationSolverV4>(W->Pin->Get(),TEXT("NNERuntimeORTCpu"));
   const bool Ready=W->Solver->Initialize();if(!Ready)W->Solver.Reset();
   AsyncTask(ENamedThreads::GameThread,[Weak,Ready]{
    auto* Owner=Weak.Get();if(!Owner||Owner->bEnding)return;
    Owner->bPrewarming=false;if(!Ready)Owner->Worker.Reset();
    UE_LOG(LogWardrobeAISpeech,Display,TEXT("WARDROBE_STARTUP_FACE_READY success=%d"),Ready);
   });
  });
 }));
 if(!LoadHandle){bPrewarming=false;UE_LOG(LogWardrobeAISpeech,Warning,TEXT("Startup face warmup: could not queue the model."));}
}
bool UWardrobeAISpeechSubsystem::Start(const FWardrobeSpeechJob& Input){
 check(IsInGameThread());if(bEnding||Active||bPrewarming)return false;
 FConfigFile C;C.Read(GetLocalConfigPath());C.GetString(TEXT("WardrobeAISpeech"),TEXT("ApiKey"),ApiKey);
 auto R=MakeShared<FWardrobeSpeechRequest,ESPMode::ThreadSafe>();R->Job=Input;
 if(R->Job.ReferenceAudio.IsEmpty())C.GetString(TEXT("WardrobeAISpeech"),TEXT("ReferenceAudio"),R->Job.ReferenceAudio);
 if(R->Job.ReferenceText.IsEmpty())C.GetString(TEXT("WardrobeAISpeech"),TEXT("ReferenceText"),R->Job.ReferenceText);
 if(!R->Job.ReferenceAudio.IsEmpty()&&FPaths::IsRelative(R->Job.ReferenceAudio))if(auto P=IPluginManager::Get().FindPlugin(TEXT("DreamBloom")))R->Job.ReferenceAudio=FPaths::ConvertRelativePathToFull(FPaths::Combine(P->GetBaseDir(),R->Job.ReferenceAudio));
 Active=R;
 if(!ValidHTTP(Input.TTSEndpoint)||R->Job.ReferenceAudio.IsEmpty()||R->Job.ReferenceText.IsEmpty()){
  Finish(R,TEXT("Configure Reference Audio and its exact Reference Text in the DA or local INI."));return true;
 }
 if(Input.Text.IsEmpty()){
  if(!ValidHTTP(Input.Endpoint)||ApiKey.IsEmpty()){Finish(R,TEXT("Configure ApiKey in Saved/Config/WardrobeAISpeech.local.ini."));return true;}
  RequestText(R);
 }else RequestVoice(R);return true;
}
void UWardrobeAISpeechSubsystem::Cancel(UWardrobeAISpeechComponent* Owner){
 if(Active&&Active->Job.Owner.Get()==Owner){Active->Cancelled.Store(true);if(Active->HTTP)Active->HTTP->CancelRequest();}
}
void UWardrobeAISpeechSubsystem::Finish(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R,const FString& Error){
 check(IsInGameThread());R->HTTP.Reset();R->Body.Empty();
 if(Active==R)Active.Reset();
 if(bEnding||R->Cancelled.Load())return;
 if(auto* Owner=R->Job.Owner.Get())if(Owner->GetTicket()==R->Job.Ticket&&!Error.IsEmpty())Owner->Fail(R->Job.Ticket,Error);
}
void UWardrobeAISpeechSubsystem::RequestText(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R){
 const auto O=WardrobeAISpeech::BuildChatRequest(R->Job);
 auto H=MakeHTTP(R,R->Job.Endpoint,Encode(O),64*1024);H->SetHeader(TEXT("Authorization"),TEXT("Bearer ")+ApiKey);
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),R](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  auto* S=Weak.Get();if(!S||S->bEnding){R->HTTP.Reset();return;}
  if(R->Cancelled.Load()){S->Finish(R,TEXT(""));return;}
  if(!OK||!Response||Response->GetResponseCode()!=200||R->bOverflow){S->Finish(R,TEXT("Chat request failed, timed out, or exceeded the response limit."));return;}
  FString Error;if(!WardrobeAISpeech::ParseDialogue(UTF8(R->Body),R->Job.MaxCharacters,R->Job.Text,Error)){S->Finish(R,Error);return;}
  R->HTTP.Reset();R->Body.Empty();S->RequestVoice(R);
 });if(!H->ProcessRequest())Finish(R,TEXT("Could not submit the chat request."));
}
void UWardrobeAISpeechSubsystem::RequestVoice(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R){
 if(R->Cancelled.Load()){Finish(R,TEXT(""));return;}
 if(auto* Owner=R->Job.Owner.Get())Owner->SetStage(R->Job.Ticket,EWardrobeSpeechState::GeneratingVoice);
 auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("text"),R->Job.Text);O->SetStringField(TEXT("text_lang"),R->Job.Language);
 O->SetStringField(TEXT("ref_audio_path"),R->Job.ReferenceAudio);O->SetStringField(TEXT("prompt_text"),R->Job.ReferenceText);O->SetStringField(TEXT("prompt_lang"),R->Job.Language);
 O->SetStringField(TEXT("media_type"),TEXT("wav"));O->SetBoolField(TEXT("streaming_mode"),false);O->SetNumberField(TEXT("seed"),-1);O->SetNumberField(TEXT("batch_size"),1);O->SetNumberField(TEXT("parallel_infer"),false);
 auto H=MakeHTTP(R,R->Job.TTSEndpoint,Encode(O),24*1024*1024);
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),R](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  auto* S=Weak.Get();if(!S||S->bEnding){R->HTTP.Reset();return;}
  if(R->Cancelled.Load()){S->Finish(R,TEXT(""));return;}
  if(!OK||!Response||Response->GetResponseCode()!=200||R->bOverflow){S->Finish(R,TEXT("TTS request failed. Check the local service, reference audio and exact prompt text."));return;}
  R->HTTP.Reset();S->Solve(R,MoveTemp(R->Body));
 });if(!H->ProcessRequest())Finish(R,TEXT("Could not submit the TTS request."));
}
void UWardrobeAISpeechSubsystem::Solve(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R,TArray<uint8> Bytes){
 if(R->Cancelled.Load()){Finish(R,TEXT(""));return;}
 if(auto* Owner=R->Job.Owner.Get())Owner->SetStage(R->Job.Ticket,EWardrobeSpeechState::SolvingFace);
 if(!Worker){
  // Loading is async; the completion only establishes a GT lifetime pin. NNE inference runs below.
  auto Data=MakeShared<TArray<uint8>,ESPMode::ThreadSafe>(MoveTemp(Bytes));
  LoadHandle=UAssetManager::GetStreamableManager().RequestAsyncLoad(FaceModel.ToSoftObjectPath(),FStreamableDelegate::CreateLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),R,Data]{
   auto* S=Weak.Get();if(!S||S->bEnding)return;
   S->LoadHandle.Reset();
   if(R->Cancelled.Load()){S->Finish(R,TEXT(""));return;}
   auto* M=S->FaceModel.Get();if(!M){S->Finish(R,TEXT("StreamingADA face model is unavailable. Enable StreamingADA and cook its model asset."));return;}
   S->Worker=MakeShared<FWardrobeSpeechWorker,ESPMode::ThreadSafe>();
   S->Worker->Pin=MakeShareable(new TStrongObjectPtr<UNNEModelData>(M),[](TStrongObjectPtr<UNNEModelData>* P){
    if(IsInGameThread())delete P;else AsyncTask(ENamedThreads::GameThread,[P]{delete P;});
   });
   S->Solve(R,MoveTemp(*Data));
  }));return;
 }
 const auto W=Worker;const auto Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this);
 Async(EAsyncExecution::ThreadPool,[Weak,W,R,Bytes=MoveTemp(Bytes)]()mutable{
  FString Error;auto Clip=MakeShared<FWardrobeSpeechClip,ESPMode::ThreadSafe>();Clip->Text=R->Job.Text;
  const double Begin=FPlatformTime::Seconds();
  if(!R->Cancelled.Load()&&WardrobeAISpeech::DecodeWave(Bytes,R->Job.MaxDuration,Clip->PCM,Clip->SampleRate,Error)){
   Bytes.Empty();
   if(!W->Solver){W->Solver=MakeUnique<FSpeechAnimationSolverV4>(W->Pin->Get(),TEXT("NNERuntimeORTCpu"));if(!W->Solver->Initialize()){W->Solver.Reset();Error=TEXT("MetaHuman CPU speech solver initialization failed.");}}
   if(W->Solver&&Error.IsEmpty()&&!R->Cancelled.Load()){
    W->Solver->ClearCache();
    for(const auto& M:R->Job.Morphs)if(!M.Morph.IsNone())Clip->Names.AddUnique(M.Morph);
    Clip->Names.Sort(FNameLexicalLess());
    const int32 AudioFrames=FMath::DivideAndRoundUp(Clip->PCM.Num(),320),LookaheadFrames=4;
    Clip->Frames=AudioFrames+1;Clip->Values.SetNumZeroed(Clip->Frames*Clip->Names.Num());
    FSpeechAnimationAudioFrame In;In.SampleRate=16000;In.SamplesCount=320;In.NumChannels=1;In.AudioSamples.SetNumZeroed(320);In.Mood=R->Job.Mood;In.MoodIntensity=FMath::Clamp(R->Job.ExpressionIntensity,0.f,1.f);In.Lookahead=80;
    for(int32 F=0;F<AudioFrames+LookaheadFrames+1&&!R->Cancelled.Load();++F){
     for(int32 I=0;I<320;++I){const int32 At=F*320+I;In.AudioSamples[I]=Clip->PCM.IsValidIndex(At)?float(Clip->PCM[At])/32768.f:0.f;}
     In.bContiguous=F>0;In.ArrivalTime=F*.02;FSpeechAnimationFrameData Out;
     if(!W->Solver->SolveAudioFrame(In,Out)||Out.CurveNames.Num()!=81||Out.CurveValues.Num()!=81){Error=TEXT("MetaHuman speech solver returned an invalid frame.");break;}
     const int32 At=F-LookaheadFrames;if(At<0||At>=Clip->Frames)continue;
     TMap<FString,float> GUI;for(int32 I=0;I<Out.CurveNames.Num();++I)GUI.Add(Out.CurveNames[I].ToString(),Out.CurveValues[I]);
     const auto Raw=GuiToRawControlsUtils::ConvertGuiToRawControls(GUI);const auto Mapped=WardrobeAISpeech::MapFace(Raw,R->Job.Morphs);
     for(int32 I=0;I<Clip->Names.Num();++I)Clip->Values[At*Clip->Names.Num()+I]=Mapped.FindRef(Clip->Names[I]);
    }
    // Silence closes the vowels without erasing the model's expression/blink controls.
    for(int32 F=0;F<Clip->Frames;++F){
     double Energy=0;int32 Count=0;for(int32 I=FMath::Max(0,F*320-320);I<FMath::Min(Clip->PCM.Num(),F*320+640);++I){const double X=Clip->PCM[I]/32768.;Energy+=X*X;++Count;}
     const float Gate=FMath::Clamp(float((FMath::Sqrt(Count?Energy/Count:0.)-.004)/.02),0.f,1.f);
     for(int32 I=0;I<Clip->Names.Num();++I){bool Mouth=false;for(const auto& M:R->Job.Morphs)if(M.Morph==Clip->Names[I]&&M.Control.ToString().StartsWith(TEXT("Daniya."))&&M.Control!=TEXT("Daniya.Blink"))Mouth=true;
      if(Mouth)Clip->Values[F*Clip->Names.Num()+I]*=Gate;
     }
    }
   }
  }
  if(W->Solver)W->Solver->ClearCache(); // Do not retain the last sentence in the solver's audio history.
  Clip->SolveSeconds=FPlatformTime::Seconds()-Begin;
  AsyncTask(ENamedThreads::GameThread,[Weak,R,Clip,Error]{
   auto* S=Weak.Get();if(!S||S->bEnding)return;
   if(!R->Cancelled.Load()&&Error.IsEmpty())if(auto* O=R->Job.Owner.Get())O->AcceptClip(R->Job.Ticket,Clip);
   if(!R->Cancelled.Load())UE_LOG(LogWardrobeAISpeech,Display,TEXT("Speech prepared on worker: audio=%.2fs solve=%.3fs transient=%lld bytes"),Clip->Duration(),Clip->SolveSeconds,Clip->Bytes());
   S->Finish(R,Error);
  });
 });
}
