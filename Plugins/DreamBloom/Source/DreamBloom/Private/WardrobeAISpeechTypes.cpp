#include "WardrobeAISpeechTypes.h"
#include "DaniyaMouthLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace WardrobeAISpeech {
TSharedRef<FJsonObject> BuildChatRequest(const FWardrobeSpeechJob& J){
 auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("model"),J.Model);O->SetBoolField(TEXT("stream"),false);O->SetNumberField(TEXT("max_tokens"),256);O->SetNumberField(TEXT("temperature"),.85);
 auto System=MakeShared<FJsonObject>();System->SetStringField(TEXT("role"),TEXT("system"));
 System->SetStringField(TEXT("content"),J.Persona+FString::Printf(TEXT("\n只输出角色说的一句中文台词，最多%d个字符。不要标题、引号、动作括号、Markdown或解释。"),J.MaxCharacters)+TEXT("\n当前场景：")+J.Scene+TEXT("\n话题参考：")+J.Topic+TEXT("\n语气：")+J.Tone);
 auto User=MakeShared<FJsonObject>();User->SetStringField(TEXT("role"),TEXT("user"));
 User->SetStringField(TEXT("content"),J.PlayerMessage.IsEmpty()?TEXT("请主动和玩家聊一句，内容自然且有变化。"):J.PlayerMessage);
 TArray<TSharedPtr<FJsonValue>> Messages;Messages.Add(MakeShared<FJsonValueObject>(System));Messages.Add(MakeShared<FJsonValueObject>(User));O->SetArrayField(TEXT("messages"),Messages);return O;
}
bool DecodeWave(const TArray<uint8>& B,float MaxDuration,TArray<int16>& Mono,int32& Rate,FString& Error){
 Mono.Reset();Rate=16000;Error.Reset();
 auto Fail=[&](const TCHAR* E){Error=E;return false;};
 auto U16=[&](int64 P)->uint16{return uint16(B[P])|(uint16(B[P+1])<<8);};
 auto U32=[&](int64 P)->uint32{return uint32(B[P])|(uint32(B[P+1])<<8)|(uint32(B[P+2])<<16)|(uint32(B[P+3])<<24);};
 if(B.Num()<44||FMemory::Memcmp(B.GetData(),"RIFF",4)||FMemory::Memcmp(B.GetData()+8,"WAVE",4))return Fail(TEXT("TTS did not return a RIFF WAV."));
 const int64 End=int64(U32(4))+8;if(End>B.Num()||End<12)return Fail(TEXT("Truncated WAV."));
 int32 Format=0,Channels=0,Hz=0,Bits=0,Align=0;int64 Data=-1,Size=0;
 for(int64 P=12;P+8<=End;){
  const uint32 N=U32(P+4);const int64 At=P+8;if(int64(N)>End-At)return Fail(TEXT("Invalid WAV chunk length."));
  if(!FMemory::Memcmp(B.GetData()+P,"fmt ",4)){
   if(N<16)return Fail(TEXT("Invalid WAV format."));Format=U16(At);Channels=U16(At+2);Hz=U32(At+4);Align=U16(At+12);Bits=U16(At+14);
  }else if(!FMemory::Memcmp(B.GetData()+P,"data",4)&&Data<0){Data=At;Size=N;}
  P=At+N+(N&1);
 }
 if(Data<0||Channels<1||Channels>8||Hz<8000||Hz>192000||!((Format==1&&Bits==16)||(Format==3&&Bits==32))||Align!=Channels*(Bits/8)||Size%Align)
  return Fail(TEXT("WAV must be PCM16 or float32, 1-8 channels."));
 const int64 Frames=Size/Align;
 if(Frames<1||!FMath::IsFinite(MaxDuration)||Frames>int64(Hz*FMath::Clamp(MaxDuration,1.f,30.f)))return Fail(TEXT("TTS audio exceeds the configured duration limit."));
 TArray<float> Source;Source.SetNumUninitialized(Frames);
 for(int64 I=0;I<Frames;++I){double V=0;for(int32 C=0;C<Channels;++C){const int64 At=Data+I*Align+C*(Bits/8);float S;
   if(Format==1)S=float(int16(U16(At)))/32768.f;else{FMemory::Memcpy(&S,B.GetData()+At,4);if(!FMath::IsFinite(S))return Fail(TEXT("Non-finite WAV sample."));}
   V+=FMath::Clamp(S,-1.f,1.f);
  }Source[I]=V/Channels;}
 // Windowed-sinc resampling also filters frequencies above the 16 kHz Nyquist limit.
 const int32 Count=FMath::Max(1,FMath::RoundToInt(double(Frames)*Rate/Hz));Mono.SetNumUninitialized(Count);
 const double Cutoff=FMath::Min(1.,double(Rate)/Hz)*.94;
 for(int32 I=0;I<Count;++I){const double X=double(I)*Hz/Rate;const int32 Center=FMath::FloorToInt(X);double V=0,W=0;
  for(int32 J=Center-16;J<=Center+16;++J){const double D=X-J;if(FMath::Abs(D)>16)continue;
   const double A=PI*D*Cutoff;const double K=Cutoff*(FMath::Abs(A)<1.e-9?1.:FMath::Sin(A)/A)*(.5+.5*FMath::Cos(PI*D/16));
   V+=Source[FMath::Clamp(J,0,Source.Num()-1)]*K;W+=K;
  }Mono[I]=FMath::RoundToInt(FMath::Clamp(W?V/W:0.,-1.,1.)*32767.);
 }return true;
}
bool ParseDialogue(const FString& JSON,int32 MaxCharacters,FString& Text,FString& Error){
 Text.Reset();Error.Reset();TSharedPtr<FJsonObject> O;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JSON),O)||!O){Error=TEXT("Invalid chat JSON.");return false;}
 const TArray<TSharedPtr<FJsonValue>>* Choices=nullptr;const TSharedPtr<FJsonObject>* Message=nullptr;
 if(!O->TryGetArrayField(TEXT("choices"),Choices)||Choices->IsEmpty()||!(*Choices)[0]->AsObject().IsValid()||!(*Choices)[0]->AsObject()->TryGetObjectField(TEXT("message"),Message)||!(*Message)->TryGetStringField(TEXT("content"),Text)){
  Error=TEXT("Chat response has no choices[0].message.content.");return false;
 }
 Text=Text.TrimStartAndEnd();Text.ReplaceInline(TEXT("\r"),TEXT(" "));Text.ReplaceInline(TEXT("\n"),TEXT(" "));
 if(Text.IsEmpty()||Text.Len()>FMath::Clamp(MaxCharacters,5,120)){Text.Reset();Error=TEXT("Chat text is empty or exceeds Max Text Characters.");return false;}return true;
}
TMap<FName,float> MapFace(const TMap<FString,float>& C,const TArray<FWardrobeSpeechMorph>& Mapping){
 TMap<FString,float> Controls=C;
 const auto Mouth=UDaniyaMouthLibrary::BuildDaniyaMouthWeights(C,1.35f);
 Controls.Add(TEXT("Daniya.A"),Mouth.A);Controls.Add(TEXT("Daniya.I"),Mouth.I);Controls.Add(TEXT("Daniya.U"),Mouth.U);Controls.Add(TEXT("Daniya.E"),Mouth.E);Controls.Add(TEXT("Daniya.O"),Mouth.O);
 Controls.Add(TEXT("Daniya.Blink"),(C.FindRef(TEXT("CTRL_expressions_eyeBlinkL"))+C.FindRef(TEXT("CTRL_expressions_eyeBlinkR")))*.5f);
 TMap<FName,float> Out;
 for(const auto& M:Mapping)if(!M.Morph.IsNone()){
  const float V=Controls.FindRef(M.Control.ToString())*M.Gain;
  Out.FindOrAdd(M.Morph)=FMath::Clamp(Out.FindRef(M.Morph)+(FMath::IsFinite(V)?V:0.f),0.f,1.f);
 }return Out;
}
void InterpolateFace(const FWardrobeSpeechClip& C,float Time,TArray<float>& V){
 V.SetNumZeroed(C.Names.Num());if(C.Frames<1||C.FrameInterval<=0||C.Values.Num()!=C.Frames*C.Names.Num()||!FMath::IsFinite(Time))return;
 const float T=FMath::Clamp(Time/C.FrameInterval,0.f,float(C.Frames-1));const int32 A=FMath::FloorToInt(T),B=FMath::Min(A+1,C.Frames-1);
 for(int32 I=0;I<V.Num();++I)V[I]=FMath::Lerp(C.Values[A*V.Num()+I],C.Values[B*V.Num()+I],T-A);
}
}
