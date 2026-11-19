#include "WardrobeAISpeechSubsystem.h"
#include "WardrobeStartup.h"
#include "WardrobeAISpeechTypes.h"
#include "WardrobeAISpeechPaths.h"
#include "WardrobeAIServiceLifecycle.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
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
 TArray<uint8> Body;FString Plan;int32 Limit=0;bool bOverflow=false;
};
namespace {
FString Encode(const TSharedRef<FJsonObject>& O){FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S;}
FString UTF8(const TArray<uint8>& B){FUTF8ToTCHAR C(reinterpret_cast<const ANSICHAR*>(B.GetData()),B.Num());return FString(C.Length(),C.Get());}
bool ValidHTTP(const FString& S){return S.StartsWith(TEXT("http://"))||S.StartsWith(TEXT("https://"));}
FString ServiceKey(const TCHAR* Kind){return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir())+Kind;}
TSharedPtr<FJsonObject> ReadServiceJSON(const FHttpResponsePtr& Response){
 TSharedPtr<FJsonObject> Result;
 if(Response&&Response->GetResponseCode()==200)FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()),Result);
 return Result;
}
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

void UWardrobeAISpeechSubsystem::ConfigureService(UWardrobeAISpeechData* Profile){
 check(IsInGameThread());if(bEnding||!IsValid(Profile))return;
 const bool Changed=!bServiceConfigured||ServiceProfile.Get()!=Profile;
 ServiceProfile=Profile;bServiceConfigured=true;
 if(Changed){
  bTTSReady=!Profile->bEnabled;bAstrBotReady=!Profile->bEnabled||!Profile->bUseAstrBot;
  bVoicePrepareSubmitted=false;TTSError.Reset();AstrBotError.Reset();ServiceStartedAt=FPlatformTime::Seconds();NextServiceCheck=0;
  TTSLaunchAttempts=0;AstrBotLaunchAttempts=0;
  EnsureService(Profile);EnsureAstrBot(Profile);
 }
}
FString UWardrobeAISpeechSubsystem::GetResolvedTTSRoot() const{
 const auto* Profile=ServiceProfile.Get();if(!Profile)return {};
 FString Error;return WardrobeAISpeech::ResolveTTSRoot(Profile->TTSRoot,FPaths::ProjectDir(),Error);
}
UWardrobeAISpeechSubsystem::UWardrobeAISpeechSubsystem(){FaceModel=TSoftObjectPtr<UNNEModelData>(FSoftObjectPath(TEXT("/StreamingADA/xsada_face_base_fp32_v2_0_0.xsada_face_base_fp32_v2_0_0")));}
void UWardrobeAISpeechSubsystem::Initialize(FSubsystemCollectionBase& C){
 C.InitializeDependency<UWardrobeStartupSubsystem>();
 Super::Initialize(C);bEnding=false;bServiceConfigured=false;bServiceCheckPending=false;bServiceStarted=false;ServiceProfile.Reset();
 ServiceTicker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UWardrobeAISpeechSubsystem::TickServices));
}
void UWardrobeAISpeechSubsystem::Deinitialize(){
 bEnding=true;FTSTicker::GetCoreTicker().RemoveTicker(ServiceTicker);
 for(auto* Request:{&TTSHealth,&AstrBotHealth,&VoicePrepare})if(Request->IsValid()){
  (*Request)->OnProcessRequestComplete().Unbind();(*Request)->CancelRequest();Request->Reset();
 }
 WardrobeAIServiceLifecycle::ReleaseClient(this);
 if(Active){Active->Cancelled.Store(true);if(Active->HTTP)Active->HTTP->CancelRequest();Active->HTTP.Reset();}
 if(LoadHandle)LoadHandle->CancelHandle();LoadHandle.Reset();Active.Reset();Worker.Reset();ServiceProfile.Reset();Super::Deinitialize();
}
void UWardrobeAISpeechSubsystem::EnsureAstrBot(const UWardrobeAISpeechData* Profile){
 if(!Profile||!Profile->bEnabled||!Profile->bUseAstrBot||IsRunningCommandlet()||bAstrBotCheckPending)return;
 const FString Root=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("Plugins/AI")));
 TArray<FString> Runtimes;IFileManager::Get().FindFiles(Runtimes,*FPaths::Combine(Root,TEXT("Runtime/Python/cpython-*")),false,true);
 Runtimes.Sort([](const FString& A,const FString& B){return A.Len()>B.Len();});
 if(Runtimes.IsEmpty()||!FPaths::FileExists(FPaths::Combine(Root,TEXT("StartAstrBot.py")))){AstrBotError=TEXT("AstrBot runtime is missing in Plugins/AI.");return;}
 const FString Python=FPaths::Combine(Root,TEXT("Runtime/Python"),Runtimes[0],TEXT("python.exe"));
 WardrobeAIServiceLifecycle::RegisterClient(ServiceKey(TEXT("astrbot")),this);
 auto H=FHttpModule::Get().CreateRequest();AstrBotHealth=H;H->SetURL(TEXT("http://127.0.0.1:6186/status"));H->SetVerb(TEXT("GET"));H->SetTimeout(2.f);bAstrBotCheckPending=true;
 H->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),Expected=TWeakObjectPtr<const UWardrobeAISpeechData>(Profile),Python,Root](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  if(!Weak.IsValid()||Weak->bEnding)return;Weak->bAstrBotCheckPending=false;Weak->AstrBotHealth.Reset();
  if(Weak->ServiceProfile.Get()!=Expected.Get())return;
  Weak->bAstrBotReady=false;
  Weak->bAstrBotOnline=OK&&Response&&Response->GetResponseCode()==200;
  if(OK&&Response){
   auto J=ReadServiceJSON(Response);bool Ready=false;
   double Version=0;
   if(J&&J->TryGetNumberField(TEXT("version"),Version)&&Version>=2&&J->TryGetBoolField(TEXT("ready"),Ready)){
    Weak->bAstrBotReady=Ready;if(Ready)Weak->AstrBotError.Reset();
   }else Weak->AstrBotError=TEXT("AstrBot status is invalid; inspect Plugins/AI/AstrBot/data/logs/wardrobe-service.log.");
   return;
  }
  if(WardrobeAIServiceLifecycle::IsRunning(ServiceKey(TEXT("astrbot"))))return;
  if(Weak->AstrBotLaunchAttempts>=2){Weak->AstrBotError=TEXT("AstrBot exited during startup; inspect its service log and press R to retry.");return;}
  ++Weak->AstrBotLaunchAttempts;
  const FString Args=FString::Printf(TEXT("\"%s\" --parent-pid %u"),*FPaths::Combine(Root,TEXT("StartAstrBot.py")),FPlatformProcess::GetCurrentProcessId());
  if(WardrobeAIServiceLifecycle::Launch(ServiceKey(TEXT("astrbot")),Weak.Get(),Python,Args,Root)){UE_LOG(LogWardrobeAISpeech,Display,TEXT("Started owned AstrBot; waiting for plugins, provider and transport."));}
  else Weak->AstrBotError=TEXT("Could not start AstrBot. Check the installed Python runtime.");
 });if(!H->ProcessRequest())bAstrBotCheckPending=false;
}
void UWardrobeAISpeechSubsystem::CommitAstrBotClip(const FWardrobeSpeechClip& Clip){
 check(IsInGameThread());if(bEnding||Clip.AstrBotSession.IsEmpty()||Clip.AstrBotReceipt==TEXT("stored")||AstrBotCommits.Num()>=8)return;
 auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("session"),Clip.AstrBotSession);O->SetStringField(TEXT("receipt"),Clip.AstrBotReceipt);O->SetStringField(TEXT("clip_id"),Clip.ClipId);
 O->SetStringField(TEXT("player_name"),Clip.PlayerName);O->SetStringField(TEXT("player_text"),Clip.PlayerText);O->SetStringField(TEXT("text"),Clip.Text);
 AstrBotCommits.Add(Encode(O));PumpAstrBotCommits();
}
void UWardrobeAISpeechSubsystem::PumpAstrBotCommits(){
 if(bEnding||bAstrBotCommitPending||AstrBotCommits.IsEmpty())return;bAstrBotCommitPending=true;
 auto H=FHttpModule::Get().CreateRequest();H->SetURL(TEXT("http://127.0.0.1:6186/commit"));H->SetVerb(TEXT("POST"));H->SetHeader(TEXT("Content-Type"),TEXT("application/json"));H->SetTimeout(120.f);H->SetContentAsString(AstrBotCommits[0]);
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this)](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  auto* S=Weak.Get();if(!S||S->bEnding)return;S->bAstrBotCommitPending=false;if(!S->AstrBotCommits.IsEmpty())S->AstrBotCommits.RemoveAt(0);
  if(!OK||!Response||Response->GetResponseCode()!=200)UE_LOG(LogWardrobeAISpeech,Warning,TEXT("AstrBot memory commit failed; native save remains available."));
  S->PumpAstrBotCommits();
 });if(!H->ProcessRequest()){bAstrBotCommitPending=false;AstrBotCommits.RemoveAt(0);}
}
bool UWardrobeAISpeechSubsystem::IsBusy() const{return Active.IsValid();}
void UWardrobeAISpeechSubsystem::EnsureService(const UWardrobeAISpeechData* Profile){
 if(!Profile||!Profile->bEnabled||IsRunningCommandlet()||bServiceCheckPending)return;
 FString Error;const FString Root=Profile->bAutoStart?WardrobeAISpeech::ResolveTTSRoot(Profile->TTSRoot,FPaths::ProjectDir(),Error):FString();
 if(Profile->bAutoStart&&Root.IsEmpty()){TTSError=Error;return;}
 const FString Device=Profile->TTSDevice==EWardrobeTTSDevice::CUDA?TEXT("cuda"):TEXT("cpu");
 const int32 Threads=FMath::Clamp(Profile->CPUThreads,1,8);
 const FString Python=FPaths::Combine(Root,TEXT("runtime/python.exe"));
 const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("DreamBloom"));
 if(!Plugin||(Profile->bAutoStart&&!FPaths::FileExists(Python))){TTSError=TEXT("Configure a valid local TTSRoot.");return;}
 const FString Script=FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(),TEXT("Resources/AI/wardrobe_tts_server.py")));
 if(Profile->bAutoStart&&!FPaths::FileExists(Script)){TTSError=TEXT("wardrobe_tts_server.py is missing from DreamBloom/Resources/AI.");return;}
 if(Profile->bAutoStart)WardrobeAIServiceLifecycle::RegisterClient(ServiceKey(TEXT("tts")),this);
 const FString Health=Profile->TTSEndpoint.EndsWith(TEXT("/tts"))?Profile->TTSEndpoint.LeftChop(4)+TEXT("/wardrobe/status"):Profile->TTSEndpoint+TEXT("/wardrobe/status");
 auto H=FHttpModule::Get().CreateRequest();TTSHealth=H;H->SetURL(Health);H->SetVerb(TEXT("GET"));H->SetTimeout(2.f);
 H->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
 bServiceCheckPending=true;
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),Expected=TWeakObjectPtr<const UWardrobeAISpeechData>(Profile),Python,Script,Root,Device,Threads](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  if(!Weak.IsValid()||Weak->bEnding)return;
  Weak->bServiceCheckPending=false;Weak->TTSHealth.Reset();
  if(Weak->ServiceProfile.Get()!=Expected.Get()){Weak->EnsureService(Weak->ServiceProfile.Get());return;}
  const auto* P=Expected.Get();if(!P)return;
  Weak->bTTSOnline=OK&&Response&&Response->GetResponseCode()==200;
  if(OK&&Response){
   auto J=ReadServiceJSON(Response);double Version=0;bool Ready=false;FString Error;
   if(J&&J->TryGetNumberField(TEXT("version"),Version)&&Version>=2){
    J->TryGetBoolField(TEXT("ready"),Ready);J->TryGetStringField(TEXT("error"),Error);
    if(!Error.IsEmpty()&&Weak->bVoicePrepareSubmitted){Weak->TTSError=Error.Left(400);Weak->bTTSReady=false;return;}
    if(Weak->bVoicePrepareSubmitted&&Ready){Weak->bTTSReady=true;Weak->TTSError.Reset();return;}
    if(!Weak->bVoicePrepareSubmitted)Weak->PrepareVoice(P,false);
   }else if(!P->bAutoStart&&!Weak->bVoicePrepareSubmitted){Weak->PrepareVoice(P,true);}
   else if(P->bAutoStart){Weak->TTSError=TEXT("An old/incompatible TTS service occupies the port. Stop it and restart the game.");Weak->bTTSReady=false;}
   return;
  }
  Weak->bTTSReady=false;Weak->bVoicePrepareSubmitted=false;
  if(!P->bAutoStart){Weak->TTSError=TEXT("External TTS service is unavailable.");return;}
  if(WardrobeAIServiceLifecycle::IsRunning(ServiceKey(TEXT("tts"))))return;
  if(Weak->TTSLaunchAttempts>=2){Weak->TTSError=TEXT("TTS exited during startup; inspect Saved/AIServices/gpt-sovits.log and press R to retry.");return;}
  ++Weak->TTSLaunchAttempts;
  const FString Log=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("AIServices/gpt-sovits.log"));
  const FString Args=FString::Printf(TEXT("\"%s\" --root \"%s\" --device %s --cpu-threads %d --parent-pid %u --log-file \"%s\""),*Script,*Root,*Device,Threads,FPlatformProcess::GetCurrentProcessId(),*Log);
  if(WardrobeAIServiceLifecycle::Launch(ServiceKey(TEXT("tts")),Weak.Get(),Python,Args,Root)){UE_LOG(LogWardrobeAISpeech,Display,TEXT("Started owned TTS; waiting for reference-audio inference."));}
  else Weak->TTSError=TEXT("Could not start the local TTS service.");
 });if(!H->ProcessRequest())bServiceCheckPending=false;
}
bool UWardrobeAISpeechSubsystem::TickServices(float){
 if(bEnding)return false;
 const auto* Profile=ServiceProfile.Get();const double Now=FPlatformTime::Seconds();
 if(Profile&&Profile->bEnabled&&Now>=NextServiceCheck){
  NextServiceCheck=Now+1.;EnsureService(Profile);EnsureAstrBot(Profile);
 }
 return true;
}
bool UWardrobeAISpeechSubsystem::AreServicesReady() const{
 const auto* Profile=ServiceProfile.Get();return !Profile||!Profile->bEnabled||(bTTSReady&&(!Profile->bUseAstrBot||bAstrBotReady));
}
FString UWardrobeAISpeechSubsystem::GetServiceStatus() const{
 if(!TTSError.IsEmpty())return TEXT("语音服务准备失败，请检查配置和 Saved/AIServices/gpt-sovits.log（R 重试）");
 if(!AstrBotError.IsEmpty())return TEXT("陪伴服务准备失败，请检查 AstrBot 日志（R 重试）");
 if(!bAstrBotReady)return TEXT("正在启动陪伴与记忆服务…");
 if(!bTTSReady)return TEXT("正在加载语音模型并验证声线…");
 return TEXT("AI服务已就绪");
}
void UWardrobeAISpeechSubsystem::RetryServices(){
 TTSError.Reset();AstrBotError.Reset();bVoicePrepareSubmitted=false;NextServiceCheck=0;ServiceStartedAt=FPlatformTime::Seconds();
 TTSLaunchAttempts=0;AstrBotLaunchAttempts=0;
 if(bFaceRequired&&!IsFaceReady())Prewarm();
}
float UWardrobeAISpeechSubsystem::GetServiceProgress() const{
 const auto* Profile=ServiceProfile.Get();if(!Profile||!Profile->bEnabled)return 1.f;
 const float Voice=(float(bTTSOnline)+float(bTTSReady))*.5f;
 const float Companion=Profile->bUseAstrBot?(float(bAstrBotOnline)+float(bAstrBotReady))*.5f:1.f;
 return (Voice+Companion)*.5f;
}
void UWardrobeAISpeechSubsystem::PrepareVoice(const UWardrobeAISpeechData* Profile,bool External){
 if(!Profile||bVoicePreparePending)return;
 FString Ref=Profile->ReferenceAudio.FilePath;
 if(FPaths::IsRelative(Ref))if(auto Plugin=IPluginManager::Get().FindPlugin(TEXT("DreamBloom")))Ref=FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()/Ref);
 if(Ref.IsEmpty()||!FPaths::FileExists(Ref)||Profile->ReferenceText.IsEmpty()){
  TTSError=TEXT("Configure Reference Audio and its exact Reference Text in the AI Speech DA.");return;
 }
 FWardrobeSpeechJob Job;Job.Text=TEXT("你好。");Job.ReferenceAudio=Ref;Job.ReferenceText=Profile->ReferenceText;Job.Language=Profile->Language;
 auto Body=WardrobeAISpeech::BuildVoiceRequest(Job);
 auto H=FHttpModule::Get().CreateRequest();VoicePrepare=H;bVoicePreparePending=true;
 H->SetURL(External?Profile->TTSEndpoint:Profile->TTSEndpoint.LeftChop(4)+TEXT("/wardrobe/prepare"));H->SetVerb(TEXT("POST"));H->SetHeader(TEXT("Content-Type"),TEXT("application/json"));H->SetContentAsString(Encode(Body));H->SetTimeout(External?120.f:10.f);
 H->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),External](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  auto* S=Weak.Get();if(!S||S->bEnding)return;S->VoicePrepare.Reset();S->bVoicePreparePending=false;
  if(OK&&Response&&(Response->GetResponseCode()==200||(!External&&Response->GetResponseCode()==202))){
   S->bVoicePrepareSubmitted=true;
   if(External){const auto& Bytes=Response->GetContent();S->bTTSReady=Bytes.Num()>44&&Bytes[0]=='R'&&Bytes[1]=='I'&&Bytes[2]=='F'&&Bytes[3]=='F';}
  }else S->TTSError=TEXT("Reference-audio warmup failed. Inspect Saved/AIServices/gpt-sovits.log.");
 });
 if(!H->ProcessRequest()){VoicePrepare.Reset();bVoicePreparePending=false;TTSError=TEXT("Could not submit voice warmup.");}
}
void UWardrobeAISpeechSubsystem::Prewarm(){
 check(IsInGameThread());if(bEnding||Active||Worker||bPrewarming)return;
 bPrewarming=true;bFaceRequired=true;
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
bool UWardrobeAISpeechSubsystem::IsFaceReady() const{return !bFaceRequired||(!bPrewarming&&Worker&&Worker->Solver);}
bool UWardrobeAISpeechSubsystem::Start(const FWardrobeSpeechJob& Input){
 check(IsInGameThread());if(bEnding||Active||bPrewarming)return false;
 if(auto* Owner=Input.Owner.Get())ConfigureService(const_cast<UWardrobeAISpeechData*>(Owner->GetProfile()));
 auto R=MakeShared<FWardrobeSpeechRequest,ESPMode::ThreadSafe>();R->Job=Input;
 if(!R->Job.ReferenceAudio.IsEmpty()&&FPaths::IsRelative(R->Job.ReferenceAudio))if(auto P=IPluginManager::Get().FindPlugin(TEXT("DreamBloom")))R->Job.ReferenceAudio=FPaths::ConvertRelativePathToFull(FPaths::Combine(P->GetBaseDir(),R->Job.ReferenceAudio));
 Active=R;
 if(!Input.PlanJSON.IsEmpty()){
  if(!ValidHTTP(Input.Endpoint)){Finish(R,TEXT("Configure Chat Endpoint in the AI Speech DA."));return true;}
  RequestText(R);return true;
 }
 if(!ValidHTTP(Input.TTSEndpoint)||R->Job.ReferenceAudio.IsEmpty()||R->Job.ReferenceText.IsEmpty()){
  Finish(R,TEXT("Configure Reference Audio and its exact Reference Text in the AI Speech DA."));return true;
 }
 if(!AreServicesReady()){Finish(R,TEXT("AI services are not ready: ")+GetServiceStatus());return true;}
 if(Input.Text.IsEmpty()){
  if(!ValidHTTP(Input.Endpoint)){Finish(R,TEXT("Configure Chat Endpoint in the AI Speech DA."));return true;}
  RequestText(R);
 }else RequestVoice(R);return true;
}
void UWardrobeAISpeechSubsystem::Cancel(UWardrobeAISpeechComponent* Owner){
 if(Active&&Active->Job.Owner.Get()==Owner){Active->Cancelled.Store(true);if(Active->HTTP)Active->HTTP->CancelRequest();}
}
void UWardrobeAISpeechSubsystem::Finish(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R,const FString& Error){
 check(IsInGameThread());R->HTTP.Reset();R->Body.Empty();R->Job.ApiKey.Reset();
 if(Active==R)Active.Reset();
 if(bEnding||R->Cancelled.Load())return;
 if(R->Job.OnPlan){auto Done=MoveTemp(R->Job.OnPlan);Done(R->Plan,Error);return;}
 if(R->Job.OnPrepared){auto Done=MoveTemp(R->Job.OnPrepared);Done(nullptr,Error);return;}
 if(auto* Owner=R->Job.Owner.Get())if(Owner->GetTicket()==R->Job.Ticket&&!Error.IsEmpty())Owner->Fail(R->Job.Ticket,Error);
}
void UWardrobeAISpeechSubsystem::RequestText(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R){
 FString Authorization,AuthorizationError;if(!WardrobeAISpeech::BuildChatAuthorization(R->Job.ApiKey,Authorization,AuthorizationError)){Finish(R,AuthorizationError);return;}
 const auto O=WardrobeAISpeech::BuildChatRequest(R->Job);
 auto H=MakeHTTP(R,R->Job.Endpoint,R->Job.PlanJSON.IsEmpty()?Encode(O):R->Job.PlanJSON,64*1024);H->SetHeader(TEXT("Authorization"),Authorization);R->Job.ApiKey.Reset();
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),R](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  auto* S=Weak.Get();if(!S||S->bEnding){R->HTTP.Reset();return;}
  if(R->Cancelled.Load()){S->Finish(R,TEXT(""));return;}
  if(!OK||!Response||Response->GetResponseCode()!=200||R->bOverflow){S->Finish(R,TEXT("Chat request failed, timed out, or exceeded the response limit."));return;}
  if(R->Job.OnPlan){
   TSharedPtr<FJsonObject> Envelope;const TArray<TSharedPtr<FJsonValue>>* Choices=nullptr;const TSharedPtr<FJsonObject>* Message=nullptr;
   if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(UTF8(R->Body)),Envelope)||!Envelope||!Envelope->TryGetArrayField(TEXT("choices"),Choices)||Choices->IsEmpty()||!(*Choices)[0]->AsObject()||!(*Choices)[0]->AsObject()->TryGetObjectField(TEXT("message"),Message)||!(*Message)->TryGetStringField(TEXT("content"),R->Plan)||R->Plan.Len()>8000){S->Finish(R,TEXT("Invalid event plan response."));return;}
   S->Finish(R,TEXT(""));return;
  }
  FString Error;if(!WardrobeAISpeech::ParseCompanionReply(UTF8(R->Body),R->Job,Error)){S->Finish(R,Error);return;}
  R->HTTP.Reset();R->Body.Empty();S->RequestVoice(R);
 });if(!H->ProcessRequest())Finish(R,TEXT("Could not submit the chat request."));
}
void UWardrobeAISpeechSubsystem::RequestVoice(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R){
 R->Job.ApiKey.Reset(); // Direct TTS requests do not need chat credentials.
 if(R->Cancelled.Load()){Finish(R,TEXT(""));return;}
 if(!R->Job.OnPrepared)if(auto* Owner=R->Job.Owner.Get())Owner->SetStage(R->Job.Ticket,EWardrobeSpeechState::GeneratingVoice);
 const auto O=WardrobeAISpeech::BuildVoiceRequest(R->Job);
 if(O->GetStringField(TEXT("text")).IsEmpty()){Finish(R,TEXT("No spoken text remains after emotion tags are removed."));return;}
 R->Job.Text=O->GetStringField(TEXT("text")); // Captions and solved clips use the same clean line as TTS.
 auto H=MakeHTTP(R,R->Job.TTSEndpoint,Encode(O),24*1024*1024);
 H->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeAISpeechSubsystem>(this),R](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
  auto* S=Weak.Get();if(!S||S->bEnding){R->HTTP.Reset();return;}
  if(R->Cancelled.Load()){S->Finish(R,TEXT(""));return;}
  if(!OK||!Response||Response->GetResponseCode()!=200||R->bOverflow){
   FString Detail;TSharedPtr<FJsonObject> Failure;
   if(!R->Body.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(UTF8(R->Body)),Failure)&&Failure){
    if(!Failure->TryGetStringField(TEXT("Exception"),Detail))Failure->TryGetStringField(TEXT("message"),Detail);
   }
   const int32 Code=Response?Response->GetResponseCode():0;
   S->Finish(R,FString::Printf(TEXT("TTS failed (HTTP %d%s). %s Inspect Saved/AIServices/gpt-sovits.log."),Code,OK?TEXT(""):TEXT(", connection/timeout"),*Detail.Left(400)));return;
  }
  R->HTTP.Reset();S->Solve(R,MoveTemp(R->Body));
 });if(!H->ProcessRequest())Finish(R,TEXT("Could not submit the TTS request."));
}
void UWardrobeAISpeechSubsystem::Solve(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R,TArray<uint8> Bytes){
 if(R->Cancelled.Load()){Finish(R,TEXT(""));return;}
 if(!R->Job.OnPrepared)if(auto* Owner=R->Job.Owner.Get())Owner->SetStage(R->Job.Ticket,EWardrobeSpeechState::SolvingFace);
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
  FString Error;auto Clip=MakeShared<FWardrobeSpeechClip,ESPMode::ThreadSafe>();Clip->Text=R->Job.Text;Clip->Emotion=R->Job.Emotion;
  Clip->bRemember=R->Job.bRemember;Clip->CharacterId=R->Job.CharacterId;Clip->CampaignRevision=R->Job.CampaignRevision;
  Clip->PlayerText=R->Job.PlayerMessage;Clip->VerifiedNotes=R->Job.VerifiedNotes;
  Clip->AstrBotSession=R->Job.AstrBotSession;Clip->AstrBotReceipt=R->Job.AstrBotReceipt;Clip->PlayerName=R->Job.PlayerName;Clip->ClipId=FGuid::NewGuid().ToString(EGuidFormats::Digits);
  Clip->RecentTurnLimit=R->Job.RecentTurnLimit;Clip->MemoryNoteLimit=R->Job.MemoryNoteLimit;
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
    if(Error.IsEmpty()&&!R->Cancelled.Load())WardrobeAISpeech::ApplyEmotion(*Clip,R->Job);
   }
  }
  if(W->Solver)W->Solver->ClearCache(); // Do not retain the last sentence in the solver's audio history.
  Clip->SolveSeconds=FPlatformTime::Seconds()-Begin;
  AsyncTask(ENamedThreads::GameThread,[Weak,R,Clip,Error]{
   auto* S=Weak.Get();if(!S||S->bEnding)return;
   if(!R->Cancelled.Load()&&Error.IsEmpty()&&R->Job.OnPrepared){
    auto Done=MoveTemp(R->Job.OnPrepared);S->Finish(R,TEXT(""));Done(Clip,TEXT(""));return;
   }
   if(!R->Cancelled.Load()&&Error.IsEmpty())if(auto* O=R->Job.Owner.Get())O->AcceptClip(R->Job.Ticket,Clip);
   if(!R->Cancelled.Load())UE_LOG(LogWardrobeAISpeech,Display,TEXT("Speech prepared on worker: emotion=%s audio=%.2fs solve=%.3fs transient=%lld bytes"),*Clip->Emotion.ToString(),Clip->Duration(),Clip->SolveSeconds,Clip->Bytes());
   S->Finish(R,Error);
  });
 });
}
