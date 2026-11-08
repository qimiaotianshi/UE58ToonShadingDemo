#include "WardrobeSpeechNode.h"
#include "WardrobeAISpeech.h"
#include "Animation/AnimInstance.h"
void FAnimNode_WardrobeSpeech::Initialize_AnyThread(const FAnimationInitializeContext& C){Names.Reset();Values.Reset();Weight=0;Source.Initialize(C);}
void FAnimNode_WardrobeSpeech::PreUpdate(const UAnimInstance* Instance){
 Names.Reset();Values.Reset();Weight=0;
 if(Instance&&Instance->GetOwningActor())if(auto* Speech=Instance->GetOwningActor()->FindComponentByClass<UWardrobeAISpeechComponent>())Speech->GetFaceSnapshot(Names,Values,Weight);
}
void FAnimNode_WardrobeSpeech::Evaluate_AnyThread(FPoseContext& Output){
 Source.Evaluate(Output);if(Weight<=0)return;
 for(int32 I=0;I<Names.Num()&&I<Values.Num();++I){
  const float Value=FMath::Lerp(Output.Curve.Get(Names[I]),Values[I],FMath::Clamp(Weight,0.f,1.f));
  Output.Curve.Set(Names[I],Value);Output.Curve.SetFlags(Names[I],Output.Curve.GetFlags(Names[I])|UE::Anim::ECurveElementFlags::MorphTarget);
 }
}
