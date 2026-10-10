#include "SequenceCharacterTools.h"
#include "SequenceCharacterRig.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieScenePossessable.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "ControlRigObjectBinding.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "SequenceMorphSection.h"
#include "ScopedTransaction.h"
#include "Rigs/RigHierarchy.h"
UMovieSceneControlRigParameterTrack* USequenceCharacterTools::AddMorphTrack(ULevelSequence* Sequence,USkeletalMeshComponent* Component){
 if(!Sequence||!Component||!Component->GetSkeletalMeshAsset()||!Component->GetOwner())return nullptr;
 auto* Scene=Sequence->GetMovieScene();if(!Scene||Scene->IsReadOnly())return nullptr;
 const FScopedTransaction Transaction(FText::FromString(TEXT("Add Sequence Character Studio Track")));Sequence->Modify();Scene->Modify();
 auto* Actor=Component->GetOwner();auto* World=Actor->GetWorld();FGuid ActorId=Sequence->FindBindingFromObject(Actor,World);
 if(!ActorId.IsValid()){ActorId=Scene->AddPossessable(Actor->GetActorLabel(),Actor->GetClass());Sequence->BindPossessableObject(ActorId,*Actor,World);}
 FGuid Id=Sequence->FindBindingFromObject(Component,World);
 if(!Id.IsValid()){Id=Scene->AddPossessable(Component->GetName(),Component->GetClass());Scene->FindPossessable(Id)->SetParent(ActorId,Scene);Sequence->BindPossessableObject(Id,*Component,Actor);}
 for(auto* Existing:Scene->FindTracks(UMovieSceneControlRigParameterTrack::StaticClass(),Id,NAME_None))if(auto* T=Cast<UMovieSceneControlRigParameterTrack>(Existing))if(Cast<USequenceCharacterRig>(T->GetControlRig())){
  // Upgrade legacy expression sections without changing their curve keys or tangents.
  // Legacy mixed sections must first transfer their transform channels to an official rig.
  for(auto* S:T->GetAllSections())if(auto* Old=Cast<UMovieSceneControlRigParameterSection>(S))if(!Cast<USequenceMorphSection>(Old)&&Old->GetTransformParameterNamesAndCurves().Num())return nullptr;
  T->Modify();const auto Sections=T->GetAllSections();
  for(auto* S:Sections)if(auto* Old=Cast<UMovieSceneControlRigParameterSection>(S))if(!Cast<USequenceMorphSection>(Old)){
   auto* Fine=NewObject<USequenceMorphSection>(T,NAME_None,RF_Transactional);Fine->RecreateWithThisControlRig(T->GetControlRig(),true);Fine->GetScalarParameterNamesAndCurves()=Old->GetScalarParameterNamesAndCurves();Fine->SetRange(Old->GetRange());Fine->SetBlendType(Old->GetBlendType().IsValid()?Old->GetBlendType().Get():EMovieSceneBlendType::Absolute);Fine->SetRowIndex(Old->GetRowIndex());Fine->SetOverlapPriority(Old->GetOverlapPriority());Fine->SetIsActive(Old->IsActive());Fine->SetIsLocked(Old->IsLocked());Fine->EvalOptions=Old->EvalOptions;T->RemoveSection(*Old);T->AddSection(*Fine);T->SetSectionToKey(Fine);Fine->ReconstructChannelProxy();
  }
  T->SetPriorityOrder(-100);T->SetDisplayName(FText::FromString(TEXT("BlendShapes · 表情")));Sequence->MarkPackageDirty();return T;
 }
 // UE 5.8 GetAssetReference only recognizes generated rig classes and the exact built-in FK class.
 // A saved generated subclass is essential for game-world instances and cooked playback.
 auto* RigClass=LoadClass<USequenceCharacterRig>(nullptr,TEXT("/SequenceCharacterStudio/CR_SequenceCharacter.CR_SequenceCharacter_C"));
 if(!RigClass)return nullptr;
 auto* Track=Scene->AddTrack<UMovieSceneControlRigParameterTrack>(Id);if(!Track)return nullptr;
 auto* Rig=NewObject<USequenceCharacterRig>(Track,RigClass,NAME_None,RF_Transactional);Rig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());Rig->GetObjectBinding()->BindToObject(Component);Rig->SetApplyMode(EControlRigFKRigExecuteMode::Replace);Rig->Initialize();Rig->Evaluate_AnyThread();
 Track->SetPriorityOrder(-100);Track->SetTrackName(TEXT("SequenceCharacterStudio"));Track->SetDisplayName(FText::FromString(TEXT("BlendShapes · 表情")));
 auto* Section=Track->CreateControlRigSection(Scene->GetPlaybackRange().GetLowerBoundValue(),Rig,true);if(!Section)return nullptr;Track->RemoveSection(*Section);auto* FineSection=NewObject<USequenceMorphSection>(Track,NAME_None,RF_Transactional);FineSection->RecreateWithThisControlRig(Rig,true);FineSection->SetBlendType(EMovieSceneBlendType::Absolute);Track->AddSection(*FineSection);Section=FineSection;Section->SetRange(TRange<FFrameNumber>::All());Section->EvalOptions.CompletionMode=EMovieSceneCompletionMode::RestoreState;
 Sequence->MarkPackageDirty();return Track;
}
bool USequenceCharacterTools::KeyMorph(ULevelSequence* Sequence,UMovieSceneControlRigParameterTrack* Track,FName Morph,int32 DisplayFrame,float Value){
 if(!Sequence||!Track||Track->GetTypedOuter<ULevelSequence>()!=Sequence||!Track->GetControlRig())return false;
 const FName Name=UFKControlRig::GetControlName(Morph,ERigElementType::Curve);if(!Track->GetControlRig()->GetHierarchy()->Find<FRigControlElement>(FRigElementKey(Name,ERigElementType::Control)))return false;
 const FScopedTransaction T(FText::FromString(TEXT("Key BlendShape")));Sequence->Modify();Track->Modify();for(auto* S:Track->GetAllSections())S->Modify();
 const FFrameNumber Time=FFrameRate::TransformTime(FFrameTime(DisplayFrame),Sequence->GetMovieScene()->GetDisplayRate(),Sequence->GetMovieScene()->GetTickResolution()).RoundToFrame();
 auto* Section=Cast<UMovieSceneControlRigParameterSection>(Track->GetSectionToKey());if(!Section&&Track->GetAllSections().Num())Section=Cast<UMovieSceneControlRigParameterSection>(Track->GetAllSections()[0]);if(!Section)return false;
 Value=FMath::Clamp(Value,-1.f,1.f);Section->AddScalarParameterKey(Name,Time,Value);Track->GetControlRig()->SetControlValue<float>(Name,Value,false,EControlRigSetKey::Never);Sequence->MarkPackageDirty();return true;
}


