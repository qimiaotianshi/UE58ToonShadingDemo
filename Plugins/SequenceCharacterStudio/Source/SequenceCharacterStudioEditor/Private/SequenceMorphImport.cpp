#include "SequenceCharacterTools.h"
#include "SequenceCharacterRig.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "ScopedTransaction.h"
#include "Rigs/RigHierarchy.h"

FSequenceMorphImportResult USequenceCharacterTools::ImportAnimationMorphCurves(ULevelSequence* Sequence,UMovieSceneControlRigParameterTrack* Track,UAnimSequence* Animation,int32 StartDisplayFrame,float Gain,bool bReplaceRange)
{
 FSequenceMorphImportResult Result;
 auto Fail=[&](const TCHAR* Message){Result.Message=Message;return Result;};
 if(!Sequence||!Track||Track->GetTypedOuter<ULevelSequence>()!=Sequence||!Cast<USequenceCharacterRig>(Track->GetControlRig())||!Animation||!Animation->GetDataModel())return Fail(TEXT("请选择有效的 Sequence 人物控制轨道和 AnimSequence。"));
 auto* Scene=Sequence->GetMovieScene();
 auto* Section=Cast<UMovieSceneControlRigParameterSection>(Track->GetSectionToKey());
 if(!Section&&Track->GetAllSections().Num())Section=Cast<UMovieSceneControlRigParameterSection>(Track->GetAllSections()[0]);
 if(!Scene||Scene->IsReadOnly()||!Section||Section->IsLocked()||Section->IsReadOnly())return Fail(TEXT("Sequence 或控制轨道片段被锁定 / 只读。"));
 const double Duration=Animation->GetPlayLength(),FPS=Scene->GetDisplayRate().AsDecimal();
 if(!FMath::IsFinite(Duration)||Duration<=0||!FMath::IsFinite(Gain)||Gain<0||FPS<=0)return Fail(TEXT("动画时长或导入强度无效。"));
 const double Samples=FMath::CeilToDouble(Duration*FPS);
 if(Samples>1000000)return Fail(TEXT("动画过长：一次导入最多 1,000,000 个采样帧。"));
 const double StartSeconds=Scene->GetDisplayRate().AsSeconds(FFrameTime(StartDisplayFrame));
 const double TickRate=Scene->GetTickResolution().AsDecimal();
 if((StartSeconds+Duration)*TickRate>MAX_int32-2||StartSeconds*TickRate<MIN_int32+2)return Fail(TEXT("导入时间超出了 Sequencer 支持范围。"));
 const FFrameNumber Start=FFrameTime::FromDecimal(StartSeconds*TickRate).RoundToFrame();
 const FFrameNumber End=FFrameTime::FromDecimal((StartSeconds+Duration)*TickRate).RoundToFrame();
 if(!Section->GetRange().Contains(Start)||!Section->GetRange().Contains(End))return Fail(TEXT("导入区间超出了控制轨道片段范围，请先延长该片段。"));
 struct FPendingCurve {FName Name;TArray<FFrameNumber> Times;TArray<float> Values;};
 TArray<FPendingCurve> Pending;
 auto* Hierarchy=Track->GetControlRig()->GetHierarchy();
 // Build and validate all samples before touching destination keys.
 for(const FFloatCurve& Curve:Animation->GetDataModel()->GetFloatCurves()){
  const FName Morph=Curve.GetName(),Control=UFKControlRig::GetControlName(Morph,ERigElementType::Curve);
  const auto* Element=Hierarchy->Find<FRigControlElement>(FRigElementKey(Control,ERigElementType::Control));
  if(!Element||Element->Settings.ControlType!=ERigControlType::Float){Result.SkippedCurves.Add(Morph);continue;}
  if(Curve.FloatCurve.GetNumKeys()==0)continue;
  auto& Dest=Pending.AddDefaulted_GetRef();Dest.Name=Control;
  for(int32 I=0;I<=static_cast<int32>(Samples);++I){
   const double SourceTime=FMath::Min(I/FPS,Duration);
   const float Value=Curve.FloatCurve.Eval(SourceTime)*Gain;
   if(!FMath::IsFinite(Value))return Fail(TEXT("源动画存在非有限曲线值，导入已取消。"));
   const FFrameNumber Time=FFrameTime::FromDecimal((StartSeconds+SourceTime)*TickRate).RoundToFrame();
   if(Dest.Times.Num()&&Dest.Times.Last()==Time)Dest.Values.Last()=FMath::Clamp(Value,-1.f,1.f);
   else {Dest.Times.Add(Time);Dest.Values.Add(FMath::Clamp(Value,-1.f,1.f));}
  }
 }
 if(Pending.IsEmpty())return Fail(TEXT("没有可导入的表情曲线：动画曲线名称必须与当前人物的 BlendShape 名称一致。"));
 const FScopedTransaction Transaction(NSLOCTEXT("SequenceCharacterStudio","ImportMorphs","Import animation expression curves"));
 Sequence->Modify();Scene->Modify();Track->Modify();Section->Modify();
 for(const auto& Curve:Pending){
  if(bReplaceRange){
   // GetScalarParameterNamesAndCurves is const-only; section channels are writable via the proxy.
   auto Channels=Section->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();
   const auto* Map=Section->ControlChannelMap.Find(Curve.Name);
   if(Map&&Channels.IsValidIndex(Map->ChannelIndex)){
    TArray<FKeyHandle> Keys;Channels[Map->ChannelIndex]->GetKeys(TRange<FFrameNumber>::Inclusive(Start,End),nullptr,&Keys);
    Channels[Map->ChannelIndex]->DeleteKeys(Keys);
   }
  }
  for(int32 I=0;I<Curve.Times.Num();++I)Section->AddScalarParameterKey(Curve.Name,Curve.Times[I],Curve.Values[I],EMovieSceneKeyInterpolation::Linear);
  Result.WrittenKeys+=Curve.Times.Num();
 }
 Sequence->MarkPackageDirty();Result.bSuccess=true;Result.ImportedCurves=Pending.Num();
 Result.Message=FString::Printf(TEXT("已导入 %d 条表情曲线、%d 个关键帧，起始帧 %d，时长 %.2f 秒；跳过 %d 条不匹配曲线。保存 Sequence 后可继续手动调整。"),Result.ImportedCurves,Result.WrittenKeys,StartDisplayFrame,Duration,Result.SkippedCurves.Num());
 return Result;
}
