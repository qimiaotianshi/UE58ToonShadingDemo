#include "WardrobeAISpeechTypes.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeAISpeechWave.h"
#include "Dom/JsonObject.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"
#include "WardrobeAISpeechPaths.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "WardrobeSpeechBubble.h"
#include "Components/AudioComponent.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSpeechBoundaries,"Wardrobe.AISpeech.WaveAndResponseBounds",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSpeechBoundaries::RunTest(const FString&){
 TArray<uint8> B;auto W16=[&](uint16 V){B.Add(V&255);B.Add(V>>8);};auto W32=[&](uint32 V){W16(V&65535);W16(V>>16);};auto Tag=[&](const char* S){B.Append(reinterpret_cast<const uint8*>(S),4);};
 Tag("RIFF");W32(36+3200*4);Tag("WAVE");Tag("fmt ");W32(16);W16(1);W16(2);W32(32000);W32(128000);W16(4);W16(16);Tag("data");W32(3200*4);
 for(int32 I=0;I<3200;++I){W16(8192);W16(8192);}
 TArray<int16> PCM;int32 Rate;FString E;
 TestTrue(TEXT("Stereo 32 kHz accepted"),WardrobeAISpeech::DecodeWave(B,1,PCM,Rate,E));TestEqual(TEXT("Resampled to 16 kHz"),Rate,16000);TestEqual(TEXT("Resampled sample count"),PCM.Num(),1600);TestTrue(TEXT("Downmix preserves gain"),FMath::Abs(PCM[800]-8192)<=2);
 B.SetNum(B.Num()-1);TestFalse(TEXT("Truncated chunk rejected"),WardrobeAISpeech::DecodeWave(B,1,PCM,Rate,E));TestEqual(TEXT("Failure does not retain decoded PCM"),PCM.Num(),0);
 FString T;TestTrue(TEXT("Compatible response accepted"),WardrobeAISpeech::ParseDialogue(TEXT("{\"choices\":[{\"message\":{\"content\":\"  hello  \"}}]}"),10,T,E));TestEqual(TEXT("Whitespace stripped"),T,FString(TEXT("hello")));
 TestFalse(TEXT("Oversized speech rejected"),WardrobeAISpeech::ParseDialogue(TEXT("{\"choices\":[{\"message\":{\"content\":\"12345678901\"}}]}"),10,T,E));TestTrue(TEXT("Rejected text cleared"),T.IsEmpty());
 TestFalse(TEXT("No content rejected"),WardrobeAISpeech::ParseDialogue(TEXT("{\"choices\":[]}"),10,T,E));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSpeechCleanup,"Wardrobe.AISpeech.InterpolationAndAudioCleanup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSpeechCleanup::RunTest(const FString&){
 auto C=MakeShared<FWardrobeSpeechClip,ESPMode::ThreadSafe>();C->PCM.Init(1234,1024);C->Names={TEXT("jaw"),TEXT("blink")};C->Frames=2;C->Values={0,1,1,0};
 TArray<float> V;WardrobeAISpeech::InterpolateFace(*C,.01,V);TestEqual(TEXT("Timestamp interpolation"),V[0],.5f);TestEqual(TEXT("Independent curves"),V[1],.5f);
 TArray<FWardrobeSpeechMorph> Map;FWardrobeSpeechMorph M;M.Control=TEXT("CTRL_expressions_jawOpen");M.Morph=TEXT("jaw");Map.Add(M);
 const auto Values=WardrobeAISpeech::MapFace({{TEXT("CTRL_expressions_jawOpen"),.75f}},Map);TestEqual(TEXT("DA raw mapping"),Values.FindRef(TEXT("jaw")),.75f);
 auto* Wave=NewObject<UWardrobeAISpeechWave>();Wave->Initialize(C);TArray<uint8> Out;TestEqual(TEXT("PCM render sample count"),Wave->OnGeneratePCMAudio(Out,512),512);TestEqual(TEXT("PCM byte count"),Out.Num(),1024);
 Wave->ReleasePCM();TestEqual(TEXT("Cancellation releases wave's clip without waiting for GC"),Wave->OnGeneratePCMAudio(Out,512),0);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSpeechChatRequestTest,"Wardrobe.AISpeech.ManualMessageAndPrompt",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSpeechChatRequestTest::RunTest(const FString&){
 FWardrobeSpeechJob Job;Job.Model=TEXT("deepseek-chat");Job.Persona=TEXT("角色身份");Job.Scene=TEXT("房间");Job.Topic=TEXT("风景");Job.Tone=TEXT("温柔");Job.MaxCharacters=42;
 const auto Automatic=WardrobeAISpeech::BuildChatRequest(Job);const auto AutomaticMessage=Automatic->GetArrayField(TEXT("messages"))[1]->AsObject()->GetStringField(TEXT("content"));
 Job.PlayerMessage=TEXT("你好，我今天有点累。\n可以安慰我一句吗？");
 const auto Manual=WardrobeAISpeech::BuildChatRequest(Job);const auto Messages=Manual->GetArrayField(TEXT("messages"));
 TestEqual(TEXT("Only one current player message; no accumulated conversation"),Messages.Num(),2);
 TestEqual(TEXT("Player input is the actual user message"),Messages[1]->AsObject()->GetStringField(TEXT("content")),Job.PlayerMessage);
 TestTrue(TEXT("Manual request replaces the automatic prompt"),AutomaticMessage!=Job.PlayerMessage);
 const auto System=Messages[0]->AsObject()->GetStringField(TEXT("content"));
 for(const FString& Required:{TEXT("角色身份"),TEXT("房间"),TEXT("温柔"),TEXT("42")})TestTrue(TEXT("Character and response constraints preserved"),System.Contains(Required));
 TestFalse(TEXT("Nonstreaming bounded request"),Manual->GetBoolField(TEXT("stream")));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSpeechPauseTest,"Wardrobe.AISpeech.AutomaticPauseAndResume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSpeechPauseTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();
 ON_SCOPE_EXIT{GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 auto* Person=W->SpawnActor<AWardrobeShowcase>();auto* Speech=Person->FindComponentByClass<UWardrobeAISpeechComponent>();
 auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);auto* Profile=NewObject<UWardrobeAISpeechData>();Profile->MinInterval=2;Profile->MaxInterval=3;Data->Characters[0].AISpeech=Profile;
 Person->InteractionData=Data;Person->InteractionProfileIndex=0;Person->SetInteractionEnabled(true);
 TestTrue(TEXT("Usable showcase fixture"),Speech->IsIdleAllowed());
 Speech->SetAutomaticPaused(true);Speech->LastProfile=Profile;Speech->State=EWardrobeSpeechState::Idle;Speech->NextRequestTime=0;
 const uint64 Ticket=Speech->GetTicket();Speech->TickComponent(.02f,LEVELTICK_All,nullptr);
 TestEqual(TEXT("Expired automatic timer does not start any request while typing"),Speech->GetTicket(),Ticket);
 TestTrue(TEXT("Pausing automatic timer does not disable explicit reply eligibility"),Speech->IsIdleAllowed());
 TestFalse(TEXT("Blank input rejected"),Speech->RequestReply(TEXT(" \n\t")));
 TestFalse(TEXT("Oversized input rejected"),Speech->RequestReply(FString::ChrN(501,TEXT('a'))));
 TestEqual(TEXT("Invalid input starts no job"),Speech->GetTicket(),Ticket);
 const double Before=FPlatformTime::Seconds();Speech->SetAutomaticPaused(false);
 TestTrue(TEXT("Close resumes with a fresh interval rather than an overdue immediate request"),Speech->NextRequestTime>=Before+2&&Speech->NextRequestTime<=Before+3.1);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSpeechPortablePathsTest,"Wardrobe.AISpeech.PortableProjectAndPackagePaths",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSpeechPortablePathsTest::RunTest(const FString&){
 const FString Temp=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Automation/TTSPaths_")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
 ON_SCOPE_EXIT{IFileManager::Get().DeleteDirectory(*Temp,false,true);};
 auto MakeBundle=[](const FString& Root){IFileManager::Get().MakeDirectory(*FPaths::Combine(Root,TEXT("runtime")),true);FFileHelper::SaveStringToFile(TEXT("fixture"),*FPaths::Combine(Root,TEXT("runtime/python.exe")));FFileHelper::SaveStringToFile(TEXT("fixture"),*FPaths::Combine(Root,TEXT("api_v2.py")));};
 FString Error;const FString Editor=FPaths::Combine(Temp,TEXT("开发工程/中文 空格"));const FString Bundle=FPaths::Combine(Editor,TEXT("Content/AI/wrapper/portable"));MakeBundle(Bundle);
 TestEqual(TEXT("Empty local override discovers the two wrapper layers"),WardrobeAISpeech::ResolveTTSRoot(TEXT(""),Editor,Error),Bundle);
 TestEqual(TEXT("Explicit relative folder is resolved from project, not executable/CWD"),WardrobeAISpeech::ResolveTTSRoot(TEXT("Content/AI/wrapper/portable"),Editor,Error),Bundle);
 TestEqual(TEXT("Absolute external bundle remains supported"),WardrobeAISpeech::ResolveTTSRoot(Bundle,Editor,Error),Bundle);
 const FString Packaged=FPaths::Combine(Temp,TEXT("MovedInstall/Windows/MyGame"));const FString PackagedBundle=FPaths::Combine(Packaged,TEXT("Content/AI/wrapper/portable"));MakeBundle(PackagedBundle);
 TestEqual(TEXT("Relocated packaged project resolves its own loose Content/AI"),WardrobeAISpeech::ResolveTTSRoot(TEXT("Content/AI"),Packaged,Error),PackagedBundle);
 const FString Other=FPaths::Combine(Packaged,TEXT("Content/AI/other"));MakeBundle(Other);
 TestTrue(TEXT("Multiple portable bundles require an explicit choice"),WardrobeAISpeech::ResolveTTSRoot(TEXT("Content/AI"),Packaged,Error).IsEmpty()&&!Error.IsEmpty());
 TestEqual(TEXT("Explicit choice still works with multiple models"),WardrobeAISpeech::ResolveTTSRoot(TEXT("Content/AI/wrapper/portable"),Packaged,Error),PackagedBundle);
 TestEqual(TEXT("UE's own relative ProjectDir is normalized before resolving the setting"),WardrobeAISpeech::ResolveTTSRoot(TEXT("Content/AI/wrapper/portable"),FPaths::ProjectSavedDir()+TEXT("Automation/")+FPaths::GetCleanFilename(Temp)+TEXT("/MovedInstall/Windows/MyGame"),Error),PackagedBundle);
 TestTrue(TEXT("A missing bundle reports a useful staging error"),WardrobeAISpeech::ResolveTTSRoot(TEXT("Content/MissingAI"),Packaged,Error).IsEmpty()&&!Error.IsEmpty());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSpeechPresentationTest,"Wardrobe.AISpeech.CaptionLifecycleAndVoiceVolume",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSpeechPresentationTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();
 ON_SCOPE_EXIT{GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 auto* Person=W->SpawnActor<AWardrobeShowcase>();auto* Speech=Person->FindComponentByClass<UWardrobeAISpeechComponent>();
 auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);auto* Profile=NewObject<UWardrobeAISpeechData>();
 Data->Characters[0].AISpeech=Profile;Person->InteractionData=Data;Person->InteractionProfileIndex=0;Person->SetInteractionEnabled(true);
 Speech->SetAutomaticPaused(true);Speech->LastProfile=Profile;
 auto Clip=MakeShared<FWardrobeSpeechClip,ESPMode::ThreadSafe>();Clip->Text=TEXT("今天的风景真好。");Clip->PCM.Init(0,16000);Clip->Names={TEXT("jaw")};Clip->Frames=2;Clip->Values={0,1};
 Profile->VoiceVolume=.5f;Speech->AcceptClip(Speech->GetTicket(),Clip);
 if(!TestNotNull(TEXT("Playback audio created"),Speech->Audio.Get()))return false;
 TestEqual(TEXT("Volume applied before playback"),Speech->Audio->VolumeMultiplier,.5f);
 Profile->VoiceVolume=100.f;Speech->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestEqual(TEXT("DA supports one hundred times voice volume"),Speech->Audio->VolumeMultiplier,100.f);
 Profile->VoiceVolume=150.f;Speech->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestEqual(TEXT("Voice volume remains bounded at one hundred"),Speech->Audio->VolumeMultiplier,100.f);
 Profile->VoiceVolume=0;Speech->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestEqual(TEXT("DA can mute an ongoing sentence"),Speech->Audio->VolumeMultiplier,0.f);
 TestEqual(TEXT("Muted speech still animates and keeps its text"),Speech->CurrentText,Clip->Text);
 Profile->VoiceVolume=std::numeric_limits<float>::quiet_NaN();Speech->TickComponent(.01f,LEVELTICK_All,nullptr);
 TestEqual(TEXT("Invalid volume cannot enter mixer"),Speech->Audio->VolumeMultiplier,1.f);
 auto* Bubble=NewObject<UWardrobeSpeechBubble>();Speech->SpeechBubble=Bubble;Bubble->ShowCaption(Person,Clip->Text);
 Speech->OnAudioFinished();
 TestTrue(TEXT("Natural completion briefly retains caption for exit fade"),Bubble->bFinishing&&Bubble->Caption==Clip->Text);
 TestTrue(TEXT("Audio PCM and component text already released"),!Speech->Audio&&!Speech->Wave&&Speech->CurrentText.IsEmpty()&&Speech->GetTemporaryBytes()==0);
 Speech->CancelSpeech();TestTrue(TEXT("Explicit cancellation immediately clears caption and speaker"),Bubble->Caption.IsEmpty()&&!Bubble->Speaker.IsValid());
 Bubble->ShowCaption(Person,TEXT("新台词"));TestFalse(TEXT("New sentence cancels previous exit fade"),Bubble->bFinishing);
 Bubble->FinishCaption(false);Bubble->ClearCaption();TestTrue(TEXT("Fade completion releases final caption"),Bubble->Caption.IsEmpty());
 return true;
}
#endif
