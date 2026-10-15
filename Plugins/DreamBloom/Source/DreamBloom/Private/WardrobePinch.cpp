#include "WardrobePinch.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeKawaiiForce.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"
#include "Components/AudioComponent.h"
#include "LevelSequence.h"
#include "UObject/PropertyTag.h"

bool FWardrobePinchSounds::SerializeFromMismatchedTag(const FPropertyTag& Tag,FStructuredArchive::FSlot Slot){
 if(Tag.Type==NAME_StructProperty&&Tag.GetType().GetParameterName(0)==FWardrobeSoundEntry::StaticStruct()->GetFName()){
  FWardrobeSoundEntry Old;FWardrobeSoundEntry::StaticStruct()->SerializeItem(Slot,&Old,nullptr);
  Sounds.Reset();if(!Old.Sound.IsNull())Sounds.Add(Old.Sound);return true;
 }
 return false;
}
FWardrobeSoundEntry AWardrobeShowcase::ChoosePinchSound(const FWardrobePinchSounds& Sounds,FName Bone,const TCHAR* Event){
 TArray<TSoftObjectPtr<USoundBase>> Choices;
 for(const auto& Sound:Sounds.Sounds)if(!Sound.IsNull())Choices.AddUnique(Sound);
 const FString Key=Bone.ToString()+TEXT(":")+Event;
 if(Choices.Num()>1)if(const auto* Last=LastPinchSoundChoices.Find(Key))Choices.RemoveAll([&](const auto& S){return S.ToSoftObjectPath()==*Last;});
 FWardrobeSoundEntry Entry;
 if(!Choices.IsEmpty()){Entry.Sound=Choices[FMath::RandRange(0,Choices.Num()-1)];LastPinchSoundChoices.Add(Key,Entry.Sound.ToSoftObjectPath());}
 return Entry;
}

FVector AWardrobeShowcase::GetPinchOffset(FName Bone) const{
 FVector Result=FVector::ZeroVector;for(const auto& State:PinchStates)if(State.Bone==Bone)Result+=State.Position;return Result;
}
void AWardrobeShowcase::PlayPinchSound(const FWardrobeSoundEntry& Entry,EWardrobeUISound Event){
 if(GetGameInstance())if(auto* Audio=GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>())Audio->PlayEntry(Event,Entry);
}
void AWardrobeShowcase::StopPinchAudio(){
 if(IsValid(PinchMoveAudio)){PinchMoveAudio->OnAudioFinished.RemoveDynamic(this,&AWardrobeShowcase::RepeatPinchMoveSound);PinchMoveAudio->Stop();PinchMoveAudio->DestroyComponent();}
 if(IsValid(PinchGrabAudio)){PinchGrabAudio->OnAudioFinished.RemoveDynamic(this,&AWardrobeShowcase::FinishPinchGrabSound);PinchGrabAudio->Stop();PinchGrabAudio->DestroyComponent();}
 PinchGrabAudio=nullptr;PinchMoveAudio=nullptr;PinchMoveVolume=0;PinchMotionAge=1.f;PinchLastMouseDelta=FVector::ZeroVector;
 bPinchGrabFinished=true;bPinchMoveStarted=false;bPinchMovePaused=false;
}
void AWardrobeShowcase::StartPinchAudio(const FWardrobePinchRegion& Region){
 StopPinchAudio();
 const auto Grab=ChoosePinchSound(Region.GrabSound,Region.Bone,TEXT("Grab"));
 const auto Move=ChoosePinchSound(Region.MoveSound,Region.Bone,TEXT("Move"));
 PinchRoundReleaseSound=ChoosePinchSound(Region.ReleaseSound,Region.Bone,TEXT("Release")).Sound;
 if(GetGameInstance())if(auto* Audio=GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>()){
  PinchGrabAudio=Audio->SpawnOwnedEntry(Grab);PinchMoveAudio=Audio->SpawnOwnedEntry(Move,false);
  bPinchGrabFinished=!IsValid(PinchGrabAudio)||!PinchGrabAudio->IsPlaying();
  if(IsValid(PinchGrabAudio))PinchGrabAudio->OnAudioFinished.AddDynamic(this,&AWardrobeShowcase::FinishPinchGrabSound);
  if(IsValid(PinchMoveAudio)){PinchMoveAudio->SetVolumeMultiplier(0);PinchMoveAudio->OnAudioFinished.AddDynamic(this,&AWardrobeShowcase::RepeatPinchMoveSound);}
 }
}
void AWardrobeShowcase::RepeatPinchMoveSound(){
 bPinchMoveStarted=false;bPinchMovePaused=false;
 if(ActivePinch!=INDEX_NONE&&bInteractionEnabled&&!IsInteracting()&&bPinchGrabFinished&&PinchMotionAge<.08f&&IsValid(PinchMoveAudio)){PinchMoveAudio->Play();bPinchMoveStarted=true;}
}
void AWardrobeShowcase::FinishPinchGrabSound(){bPinchGrabFinished=true;}
void AWardrobeShowcase::TickPinchAudio(float Dt){
 const auto* P=GetInteractionProfile();
 if(!P||!P->PinchRegions.IsValidIndex(ActivePinch)||!PinchStates.IsValidIndex(ActivePinch))return;
 PinchMotionAge+=FMath::Max(0.f,Dt);
 const bool Moving=PinchMotionAge<.08f;
 const bool Audible=Moving&&bPinchGrabFinished;
 const float Force=FMath::Clamp(float(PinchStates[ActivePinch].Position.Size())/FMath::Max(.1f,P->PinchRegions[ActivePinch].MaxDistance),0.f,1.f);
 PinchMoveVolume=FMath::Lerp(PinchMoveVolume,Audible?FMath::Lerp(.15f,1.f,Force):0.f,1.f-FMath::Exp(-30.f*FMath::Max(0.f,Dt)));
 if(IsValid(PinchMoveAudio)){
  PinchMoveAudio->SetVolumeMultiplier(PinchMoveVolume);
  if(Audible){
   if(bPinchMovePaused){PinchMoveAudio->SetPaused(false);bPinchMovePaused=false;}
   else if(!bPinchMoveStarted){PinchMoveAudio->Play();bPinchMoveStarted=true;}
  }else if(bPinchMoveStarted&&!bPinchMovePaused&&PinchMoveVolume<.01f){PinchMoveAudio->SetPaused(true);bPinchMovePaused=true;}
 }
}
bool AWardrobeShowcase::BeginPinch(const FVector& Origin,const FVector& Direction){
 const auto* Profile=GetInteractionProfile();if(!bInteractionEnabled||IsInteracting()||IsReleaseAnimationPlaying()||!Profile||!Character||Direction.IsNearlyZero())return false;
 Character->HandleExistingParallelEvaluationTask(true,true);
 FCollisionQueryParams Params(SCENE_QUERY_STAT(WardrobePinch),false);Params.AddIgnoredActor(this);
 FHitResult Blocker;float Best=100000;const FVector Ray=Direction.GetSafeNormal();
 if(GetWorld()->LineTraceSingleByChannel(Blocker,Origin,Origin+Ray*Best,ECC_Visibility,Params))Best=Blocker.Distance;
 int32 Selected=INDEX_NONE;
 for(int32 I=0;I<Profile->PinchRegions.Num();I++){
  const auto& Region=Profile->PinchRegions[I];
  if(!Region.bEnabled||Character->GetBoneIndex(Region.Bone)==INDEX_NONE)continue;
  const FName Anchor=Region.AnchorBone.IsNone()?Region.Bone:Region.AnchorBone;
  if(!Character->DoesSocketExist(Anchor))continue;
  const FTransform TM=Character->GetSocketTransform(Anchor);
  const FVector Center=TM.TransformPosition(Region.Offset);float Distance=0;
  if(FWardrobeKawaiiClickForce::RaySphere(Origin,Ray,Center,FMath::Max(.1f,Region.HitRadius)*Character->GetComponentTransform().GetMaximumAxisScale(),Distance)&&Distance<Best){Selected=I;Best=Distance;}
 }
 if(Selected==INDEX_NONE)return false;
 // A second grab replaces any held voice and repetition.
 StopPinchAudio();
 if(PinchStates.Num()!=Profile->PinchRegions.Num()){ClearPinch();PinchStates.SetNum(Profile->PinchRegions.Num());}
 const auto& Region=Profile->PinchRegions[Selected];auto& State=PinchStates[Selected];
 State.Bone=Region.Bone;State.Target=State.Position;State.bReturning=false;
 ActivePinch=Selected;PinchPlanePoint=Origin+Ray*Best;PinchPlaneNormal=Ray;PinchStartRotation=Character->GetSocketQuaternion(Region.Bone);
 State.StartComponentRotation=Character->GetComponentQuat().Inverse()*PinchStartRotation;
 PinchGrabOffset=PinchStartRotation.RotateVector(State.Position);
 StopPhysicsTouch();StartPinchAudio(Region);return true;
}
void AWardrobeShowcase::UpdatePinch(const FVector& Origin,const FVector& Direction){
 const auto* Profile=GetInteractionProfile();if(!bInteractionEnabled||IsInteracting()||!Profile||!Profile->PinchRegions.IsValidIndex(ActivePinch)||!PinchStates.IsValidIndex(ActivePinch)){ReleasePinch();return;}
 const auto& Region=Profile->PinchRegions[ActivePinch];if(!Region.bEnabled){ReleasePinch();return;}
 const double Dot=FVector::DotProduct(Direction,PinchPlaneNormal);if(FMath::Abs(Dot)<KINDA_SMALL_NUMBER)return;
 const double Distance=FVector::DotProduct(PinchPlanePoint-Origin,PinchPlaneNormal)/Dot;if(Distance<=0)return;
 auto& State=PinchStates[ActivePinch];
 const FVector MouseDelta=Origin+Direction*Distance-PinchPlanePoint;
 // Measure pointer movement before clamping stretch, so motion at maximum pull still sounds.
 if(FVector::DistSquared(MouseDelta,PinchLastMouseDelta)>=FMath::Square(.03f)){PinchMotionAge=0;PinchLastMouseDelta=MouseDelta;}
 const FVector NewOffset=PinchStartRotation.UnrotateVector(MouseDelta)*Region.AxisScale*FMath::Clamp(Region.DragSensitivity,.01f,4.f);
 State.Target=(PinchStartRotation.UnrotateVector(PinchGrabOffset)+NewOffset).GetClampedToMaxSize(FMath::Max(.1f,Region.MaxDistance));
}
void AWardrobeShowcase::ReleasePinch(){
 if(ActivePinch==INDEX_NONE)return;
 const auto* Profile=GetInteractionProfile();
 StopPinchAudio();
 for(auto& State:PinchStates){State.ReleaseReaction=State.Position;State.ReturnAge=0;State.bReturning=true;}
 if(Profile&&Profile->PinchRegions.IsValidIndex(ActivePinch)){
  const auto& R=Profile->PinchRegions[ActivePinch];FWardrobeSoundEntry Release;Release.Sound=PinchRoundReleaseSound;PlayPinchSound(Release,EWardrobeUISound::PinchRelease);PinchRoundReleaseSound.Reset();
  auto* Clip=R.ReleaseAnimation.LoadSynchronous();
  if(Clip&&Character->GetSkeletalMeshAsset()&&Clip->GetSkeleton()==Character->GetSkeletalMeshAsset()->GetSkeleton()&&Clip->GetPlayLength()>0){
   ReleaseAnimation=Clip;ReleaseAnimationAge=0;ReleaseAnimationBlendTime=FMath::Clamp(R.ReleaseBlendTime,0.f,3.f);++ReleaseAnimationSerial;
  }
 }
 ActivePinch=INDEX_NONE;for(auto& State:PinchStates)State.Target=FVector::ZeroVector;
}
void AWardrobeShowcase::ClearPinch(){
 StopPinchAudio();ReleaseAnimation=nullptr;ReleaseAnimationAge=0;PinchRoundReleaseSound.Reset();
 for(FName Name:PinchDrivenCurves)if(IsValid(Character))Character->SetMorphTarget(Name,0);PinchDrivenCurves.Reset();
 ActivePinch=INDEX_NONE;PinchStates.Reset();
}
void AWardrobeShowcase::TickPinch(float Dt){
 const auto* Profile=GetInteractionProfile();if(!bInteractionEnabled||IsInteracting()||!Profile){if(!PinchStates.IsEmpty())ClearPinch();return;}
 if(!PinchStates.IsEmpty()&&PinchStates.Num()!=Profile->PinchRegions.Num()){ClearPinch();return;}
 if(ReleaseAnimation){
  ReleaseAnimationAge+=FMath::Max(0.f,Dt);
  if(ReleaseAnimationAge>=ReleaseAnimation->GetPlayLength()+ReleaseAnimationBlendTime){ReleaseAnimation=nullptr;}
 }
 for(int32 I=0;I<PinchStates.Num();I++){
  auto& State=PinchStates[I];const auto& Region=Profile->PinchRegions[I];
  if(!Region.bEnabled)State.Target=FVector::ZeroVector;
  if(State.bReturning)State.ReturnAge+=FMath::Max(0.f,Dt);
  const bool bReleased=ActivePinch!=I;
  const float Omega=2*PI*FMath::Clamp(bReleased?Region.ReleaseFrequency*FMath::Clamp(Region.CheekRecoverySpeed,.1f,5.f):Region.Frequency,.1f,30.f),Damping=FMath::Clamp(bReleased?Region.ReleaseDampingRatio:Region.DampingRatio,.1f,2.f);
  float Remaining=FMath::Clamp(Dt,0.f,.1f);
  while(Remaining>SMALL_NUMBER){const float Step=FMath::Min(Remaining,1.f/480.f);State.Velocity+=((State.Target-State.Position)*Omega*Omega-2*Damping*Omega*State.Velocity)*Step;State.Position+=State.Velocity*Step;Remaining-=Step;}
  State.Position=State.Position.GetClampedToMaxSize(FMath::Max(.1f,Region.MaxDistance)*1.15f);
  if(State.Target.IsNearlyZero(.001f)&&State.Position.IsNearlyZero(.001f)&&State.Velocity.IsNearlyZero(.01f)){State.Position=FVector::ZeroVector;State.Velocity=FVector::ZeroVector;}
 }
 TickPinchAudio(Dt);
 TMap<FName,float> CurveValues;
 if(ReleaseAnimation){
  for(FName Name:PinchDrivenCurves)Character->SetMorphTarget(Name,0);
  PinchDrivenCurves.Reset();return;
 }
 for(int32 I=0;I<PinchStates.Num();I++){
  const auto& Region=Profile->PinchRegions[I];const auto& State=PinchStates[I];
  if(!Region.bEnabled||!Character->GetSkeletalMeshAsset())continue;
  FVector ReactionOffset=State.Position;
  if(State.bReturning){const float T=FMath::Clamp(State.ReturnAge/FMath::Max(.05f,Region.ReactionReturnTime),0.f,1.f);ReactionOffset=State.ReleaseReaction*(1.f-T*T*(3.f-2.f*T));}
  const FVector Displacement=State.StartComponentRotation.RotateVector(ReactionOffset);
  const float Force=FMath::Pow(FMath::Clamp(float(Displacement.Size())/FMath::Max(.1f,Region.MaxDistance),0.f,1.f),FMath::Clamp(Region.ReactionExponent,.1f,4.f));
  const float X=FVector::DotProduct(Displacement,Region.BodyRightAxis.GetSafeNormal()),Y=FVector::DotProduct(Displacement,Region.BodyUpAxis.GetSafeNormal()),Sum=FMath::Abs(X)+FMath::Abs(Y);
  if(Sum<KINDA_SMALL_NUMBER)continue;
  auto ApplyCurves=[&](UAnimSequence* Clip,float Weight){
   if(!Clip||Weight<=0||Clip->GetSkeleton()!=Character->GetSkeletalMeshAsset()->GetSkeleton())return;
   for(const auto& Morph:Character->GetSkeletalMeshAsset()->GetMorphTargets())if(Clip->HasCurveData(Morph->GetFName(),false)){
    const FName Name=Morph->GetFName();
    const float Value=(Clip->EvaluateCurveData(Name,FAnimExtractContext(Force*Clip->GetPlayLength()))-Clip->EvaluateCurveData(Name,FAnimExtractContext(0.)))*FMath::Clamp(Region.ReactionStrength,0.f,1.f)*Weight;
    CurveValues.FindOrAdd(Name)+=Value;
   }
  };
  ApplyCurves(X<0?Region.BodyLeft.Get():Region.BodyRight.Get(),FMath::Abs(X)/Sum);
  ApplyCurves(Y<0?Region.BodyDown.Get():Region.BodyUp.Get(),FMath::Abs(Y)/Sum);
 }
 for(FName Name:PinchDrivenCurves)if(!CurveValues.Contains(Name))Character->SetMorphTarget(Name,0);
 PinchDrivenCurves.Reset();
 for(const auto& Pair:CurveValues){Character->SetMorphTarget(Pair.Key,FMath::Clamp(Pair.Value,-1.f,1.f));PinchDrivenCurves.Add(Pair.Key);}
 if(Profile->bDrawPinchRegions)for(const auto& Region:Profile->PinchRegions){if(!Region.bEnabled)continue;const FName Anchor=Region.AnchorBone.IsNone()?Region.Bone:Region.AnchorBone;if(Character->DoesSocketExist(Anchor)){const FTransform TM=Character->GetSocketTransform(Anchor);DrawDebugSphere(GetWorld(),TM.TransformPosition(Region.Offset),Region.HitRadius*Character->GetComponentTransform().GetMaximumAxisScale(),16,FColor::Green,false,0);}}
}

FQuat AWardrobeShowcase::GetPinchReactionRotation(FName Bone) const{
 const auto* Profile=GetInteractionProfile();if(!Profile||!Character)return FQuat::Identity;
 FQuat Result=FQuat::Identity;
 for(int32 I=0;I<PinchStates.Num();I++){
  if(!Profile->PinchRegions.IsValidIndex(I))continue;
  const auto& Region=Profile->PinchRegions[I];if(!Region.bEnabled)continue;
  if(!Character->GetSkeletalMeshAsset())continue;
  const auto& State=PinchStates[I];
  FVector ReactionOffset=State.Position;
  if(State.bReturning){const float T=FMath::Clamp(State.ReturnAge/FMath::Max(.05f,Region.ReactionReturnTime),0.f,1.f);ReactionOffset=State.ReleaseReaction*(1.f-T*T*(3.f-2.f*T));}
  const FVector Displacement=State.StartComponentRotation.RotateVector(ReactionOffset);
  const float Force=FMath::Pow(FMath::Clamp(float(Displacement.Size())/FMath::Max(.1f,Region.MaxDistance),0.f,1.f),FMath::Clamp(Region.ReactionExponent,.1f,4.f));
  if(Force<KINDA_SMALL_NUMBER)continue;
  auto Apply=[&](UAnimSequence* Clip,float DirectionWeight){
   if(!Clip||Clip->GetSkeleton()!=Character->GetSkeletalMeshAsset()->GetSkeleton()||DirectionWeight<=0)return;
   const int32 Index=Clip->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Bone);if(Index==INDEX_NONE)return;
   FTransform Neutral,Pull;
   Clip->GetBoneTransform(Neutral,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(0.),false);
   Clip->GetBoneTransform(Pull,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(Force*Clip->GetPlayLength()),false);
   FQuat Delta=Pull.GetRotation()*Neutral.GetRotation().Inverse();Delta.Normalize();
   Result=FQuat::Slerp(FQuat::Identity,Delta,DirectionWeight*FMath::Clamp(Region.ReactionStrength,0.f,1.f))*Result;Result.Normalize();
  };
  if(Region.BodyLeft||Region.BodyRight||Region.BodyUp||Region.BodyDown){
   const float X=FVector::DotProduct(Displacement,Region.BodyRightAxis.GetSafeNormal()),Y=FVector::DotProduct(Displacement,Region.BodyUpAxis.GetSafeNormal());
   const float Sum=FMath::Abs(X)+FMath::Abs(Y);if(Sum>KINDA_SMALL_NUMBER){
    Apply(X<0?Region.BodyLeft.Get():Region.BodyRight.Get(),FMath::Abs(X)/Sum);
    Apply(Y<0?Region.BodyDown.Get():Region.BodyUp.Get(),FMath::Abs(Y)/Sum);
   }
  }

 }
 return Result;
}
