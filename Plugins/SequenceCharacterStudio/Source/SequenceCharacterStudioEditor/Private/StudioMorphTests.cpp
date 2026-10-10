#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SequenceCharacterTools.h"
#include "SequenceCharacterRig.h"
#include "SequenceMorphSection.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "ControlRigObjectBinding.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneChannelEditorData.h"
#include "Animation/AnimInstance.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStudioMorphTest,"SequenceCharacterStudio.MorphOnlyWithOfficialFK",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStudioMorphTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobePinch23/SK_DaniyaPinch.SK_DaniyaPinch"));if(!TestNotNull(TEXT("Mesh"),Mesh))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AActor>();auto* C=NewObject<USkeletalMeshComponent>(A);A->AddInstanceComponent(C);A->SetRootComponent(C);C->SetSkeletalMesh(Mesh);C->RegisterComponent();C->RefreshBoneTransforms();
 auto* Seq=NewObject<ULevelSequence>();Seq->Initialize();Seq->GetMovieScene()->SetDisplayRate(FFrameRate(24,1));Seq->GetMovieScene()->SetPlaybackRange(0,96000);
 auto* MorphTrack=USequenceCharacterTools::AddMorphTrack(Seq,C);if(!TestNotNull(TEXT("Morph track"),MorphTrack))return false;
 TestTrue(TEXT("Readding reuses track"),MorphTrack==USequenceCharacterTools::AddMorphTrack(Seq,C));
 auto* H=MorphTrack->GetControlRig()->GetHierarchy();int32 Bones=0;H->ForEach<FRigBoneElement>([&](FRigBoneElement*){++Bones;return true;});TestTrue(TEXT("Reference skeleton available for pose pass-through"),Bones>0);
 for(auto* Ctrl:H->GetControls())TestTrue(TEXT("Only Float controls"),Ctrl->Settings.ControlType==ERigControlType::Float);
 auto* Section=Cast<USequenceMorphSection>(MorphTrack->GetAllSections()[0]);if(!TestNotNull(TEXT("Fine float section"),Section))return false;
 const auto Metadata=Section->GetChannelProxy().GetMetaData<FMovieSceneFloatChannel>();TestTrue(TEXT("Float metadata available"),Metadata.Num()>0);
 for(const auto& M:Metadata)TestEqual(TEXT("No fixed snapping step"),M.GetPropertyMetaData(TEXT("Delta")),FString(TEXT("0")));
 const FName Morph(TEXT("DB_EyeClose_L"));TestTrue(TEXT("Tiny float key accepted"),USequenceCharacterTools::KeyMorph(Seq,MorphTrack,Morph,0,.00125f));TestFalse(TEXT("Invalid morph rejected"),USequenceCharacterTools::KeyMorph(Seq,MorphTrack,TEXT("Missing"),0,1));
 auto* Anim=NewObject<UAnimSequence>();Anim->SetSkeleton(Mesh->GetSkeleton());auto& Controller=Anim->GetController();Controller.InitializeModel();Controller.SetFrameRate(FFrameRate(30,1),false);Controller.SetNumberOfFrames(FFrameNumber(31),false);
 const FAnimationCurveIdentifier Id(Morph,ERawCurveTrackTypes::RCT_Float);Controller.AddCurve(Id,AACF_DefaultCurve,false);Controller.SetCurveKeys(Id,{FRichCurveKey(0,0,0,0,RCIM_Linear),FRichCurveKey(1,.8f,0,0,RCIM_Linear),FRichCurveKey(31.f/30,.4f,0,0,RCIM_Linear)},false);
 USequenceCharacterTools::KeyMorph(Seq,MorphTrack,Morph,96,.3f);USequenceCharacterTools::KeyMorph(Seq,MorphTrack,TEXT("DB_MouthPout"),36,.6f);
 const auto Result=USequenceCharacterTools::ImportAnimationMorphCurves(Seq,MorphTrack,Anim,24,.5f,true);TestTrue(TEXT("Morph import works"),Result.bSuccess);TestEqual(TEXT("Exact tail included"),Result.WrittenKeys,26);
 const auto Name=UFKControlRig::GetControlName(Morph,ERigElementType::Curve);
 TestTrue(TEXT("Gain and frame rate"),FMath::IsNearlyEqual(Section->EvaluateScalarParameter(FFrameTime(48000),Name).GetValue(),.4f,.001f));TestTrue(TEXT("Tail preserved"),FMath::IsNearlyEqual(Section->EvaluateScalarParameter(FFrameTime(48800),Name).GetValue(),.2f,.001f));
 TestTrue(TEXT("Outside key preserved"),FMath::IsNearlyEqual(Section->EvaluateScalarParameter(FFrameTime(96000),Name).GetValue(),.3f));TestTrue(TEXT("Other expression preserved"),FMath::IsNearlyEqual(Section->EvaluateScalarParameter(FFrameTime(36000),UFKControlRig::GetControlName(TEXT("DB_MouthPout"),ERigElementType::Curve)).GetValue(),.6f));
 Section->SetIsLocked(true);TestFalse(TEXT("Locked import rejected"),USequenceCharacterTools::ImportAnimationMorphCurves(Seq,MorphTrack,Anim,24).bSuccess);Section->SetIsLocked(false);
 // Official native FK rig, independent from the plugin, edits the body.
 const FGuid Binding=Seq->FindBindingFromObject(C,W);auto* Body=Seq->GetMovieScene()->AddTrack<UMovieSceneControlRigParameterTrack>(Binding);auto* FK=NewObject<UFKControlRig>(Body);
 UFKControlRig::FRigElementInitializationOptions Options;Options.bGenerateCurveControls=false;FK->SetInitializationOptions(Options);FK->SetObjectBinding(MakeShared<FControlRigObjectBinding>());FK->GetObjectBinding()->BindToObject(C);FK->Initialize();FK->Evaluate_AnyThread();
 auto* BodySection=Body->CreateControlRigSection(0,FK,true);BodySection->SetRange(TRange<FFrameNumber>::All());Body->SetPriorityOrder(0);MorphTrack->SetPriorityOrder(-100);
 auto* BS=Cast<UMovieSceneControlRigParameterSection>(BodySection);const FName Bone(TEXT("頭"));BS->AddTransformParameterKey(UFKControlRig::GetControlName(Bone,ERigElementType::Bone),FFrameNumber(0),FTransform::Identity);BS->AddTransformParameterKey(UFKControlRig::GetControlName(Bone,ERigElementType::Bone),FFrameNumber(48000),FTransform(FRotator(0,0,15)));
 const auto Before=C->GetSocketQuaternion(Bone);ALevelSequenceActor* SequenceActor=nullptr;FMovieSceneSequencePlaybackSettings Settings;Settings.FinishCompletionStateOverride=EMovieSceneCompletionModeOverride::ForceRestoreState;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,Settings,SequenceActor);
 Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(0.f,EUpdatePositionMethod::Jump));Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(2.f,EUpdatePositionMethod::Jump));C->TickAnimation(.033f,false);C->RefreshBoneTransforms();C->HandleExistingParallelEvaluationTask(true,true);
 TestTrue(TEXT("Official FK body survives expression rig"),Before.AngularDistance(C->GetSocketQuaternion(Bone))>.1f);
 auto* GameRig=MorphTrack->GetGameWorldControlRig(W,C);TestTrue(TEXT("Morph survives official body rig"),FMath::IsNearlyEqual(GameRig->GetControlValue(Name).Get<float>(),.4f,.01f));
 auto* MorphAsset=Mesh->FindMorphTarget(Morph);const int32* Index=C->ActiveMorphTargets.Find(MorphAsset);TestTrue(TEXT("Mesh actual morph deforms"),Index&&C->MorphTargetWeights.IsValidIndex(*Index)&&C->MorphTargetWeights[*Index]>.3f);
 AddInfo(FString::Printf(TEXT("Morph control %.3f rig curve %.3f anim curve %.3f active %d"),GameRig->GetControlValue(Name).Get<float>(),GameRig->GetHierarchy()->GetCurveValue(FRigElementKey(Morph,ERigElementType::Curve)),C->GetAnimInstance()->GetCurveValue(Morph),Index?*Index:-1));
 Player->Stop();Player->RestoreState();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
