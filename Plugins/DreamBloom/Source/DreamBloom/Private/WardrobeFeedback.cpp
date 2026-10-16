#include "WardrobeFeedback.h"
#include "WardrobeKawaiiForce.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeMenu.h"
#include "WardrobeStations.h"
#include "WardrobeLoading.h"
#include "KawaiiPhysicsLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "DrawDebugHelpers.h"

bool UWardrobeClickEffectData::WriteParameters(UWorld* World,const FVector& Position,const FVector2D& UV,bool bHit) const{
 if(!World||!Collection||!Collection->GetVectorParameterByName(WorldPositionParameter)||(!ScreenUVParameter.IsNone()&&!Collection->GetVectorParameterByName(ScreenUVParameter))||(!TimeParameter.IsNone()&&!Collection->GetScalarParameterByName(TimeParameter)))return false;
 auto* Instance=World->GetParameterCollectionInstance(Collection);if(!Instance)return false;
 Instance->SetVectorParameterValue(WorldPositionParameter,FLinearColor(Position.X,Position.Y,Position.Z,1));
 if(!ScreenUVParameter.IsNone())Instance->SetVectorParameterValue(ScreenUVParameter,FLinearColor(UV.X,UV.Y,bHit?1:0,1));
 if(!TimeParameter.IsNone())Instance->SetScalarParameterValue(TimeParameter,World->GetTimeSeconds());
 return true;
}

bool AWardrobeShowcase::TraceTouch(const FVector& Origin,const FVector& Direction,FHitResult& Hit) const{
 const auto* Profile=GetInteractionProfile();
 if(!bInteractionEnabled||!Profile||!Profile->PhysicsTouch.bEnabled||!IsValid(Character)||Direction.IsNearlyZero())return false;
 const FVector Ray=Direction.GetSafeNormal();
 FCollisionQueryParams Params(SCENE_QUERY_STAT(WardrobeTouch),false);Params.AddIgnoredActor(this);
 FHitResult Obstacle;float Best=100000.f;
 if(GetWorld()->LineTraceSingleByChannel(Obstacle,Origin,Origin+Ray*Best,ECC_Visibility,Params))Best=Obstacle.Distance;
 Character->HandleExistingParallelEvaluationTask(true,true);
 TArray<FKawaiiPhysicsReference> Nodes;FGameplayTagContainer NodeFilter;
 UKawaiiPhysicsLibrary::CollectKawaiiPhysicsNodes(Nodes,Character,NodeFilter,false);
 bool Found=false;
 for(auto& Ref:Nodes){
  auto& Node=Ref.GetAnimNode<FAnimNode_KawaiiPhysics>();
  if(Node.Alpha<=0)continue;
  const float Scale=Character->GetComponentTransform().GetMaximumAxisScale();
  for(const auto& Bone:Node.ModifyBones){
   if(Bone.ParentIndex<0)continue;
   const FVector Center=FWardrobeKawaiiClickForce::BoneWorldPosition(Node,Character->GetComponentTransform(),Bone.Location);
   const float Radius=FMath::Max(0.f,Bone.PhysicsSettings.Radius)*Scale+FMath::Max(0.f,Profile->PhysicsTouch.HitPadding);
   float Distance=0;
   if(Profile->PhysicsTouch.bDrawDebug)DrawDebugSphere(GetWorld(),Center,Radius,12,FColor::Cyan,false,1.f);
   if(FWardrobeKawaiiClickForce::RaySphere(Origin,Ray,Center,Radius,Distance)&&Distance<Best){
    Best=Distance;Found=true;Hit=FHitResult();Hit.bBlockingHit=true;Hit.Distance=Distance;Hit.ImpactPoint=Origin+Ray*Distance;Hit.Location=Hit.ImpactPoint;Hit.BoneName=Bone.BoneRef.BoneName;Hit.Component=Character;
   }
  }
 }
 return Found;
}
bool AWardrobeShowcase::ApplyPhysicsTouch(const FVector& Position,const FVector& Direction){
 const auto* Profile=GetInteractionProfile();
 if(!bInteractionEnabled||IsInteracting()||!Profile||!Profile->PhysicsTouch.bEnabled||GetWorld()->GetTimeSeconds()<PhysicsTouchReadyAt)return false;
 const auto& P=Profile->PhysicsTouch;
 if(P.Strength<=0)return false;
 StopPhysicsTouch();
 // The library mutates anim-node arrays; wait for outstanding worker evaluation first.
 Character->HandleExistingParallelEvaluationTask(true,true);
 FWardrobeKawaiiClickForce Force;Force.WorldPosition=Position;Force.WorldDirection=Direction.GetSafeNormal();
 Force.Radius=FMath::Max(1.f,P.Radius);Force.Strength=FMath::Clamp(P.Strength,0.f,2000.f);
 Force.Duration=FMath::Clamp(P.Duration,.02f,2.f);Force.Falloff=P.Falloff;Force.RadialBlend=P.RadialBlend;
 TArray<FInstancedStruct> Forces;Forces.Add(FInstancedStruct::Make<FWardrobeKawaiiClickForce>(Force));
 FGameplayTagContainer FilterTags;
 bPhysicsTouchActive=UKawaiiPhysicsLibrary::AddExternalForcesToComponent(Character,Forces,this,FilterTags,true,false);
 if(!bPhysicsTouchActive){CharacterMessage=FText::FromString(TEXT("未找到 KawaiiPhysics 节点，请检查人物 AnimBP 是否正在运行"));return false;}
 PhysicsTouchRemaining=Force.Duration;PhysicsTouchReadyAt=GetWorld()->GetTimeSeconds()+FMath::Max(0.f,P.Cooldown);
 if(P.bDrawDebug)DrawDebugSphere(GetWorld(),Position,Force.Radius,24,FColor::Cyan,false,Force.Duration);
 return true;
}
bool AWardrobeShowcase::BeginPhysicsDrag(const FVector& Origin,const FVector& Direction){
 if(IsInteracting()||HasLimbPose())return false;
 FHitResult Hit;if(!TraceTouch(Origin,Direction,Hit))return false;
 StopPhysicsTouch();
 const auto& P=GetInteractionProfile()->PhysicsTouch;
 Character->HandleExistingParallelEvaluationTask(true,true);
 TArray<FKawaiiPhysicsReference> Nodes;FGameplayTagContainer NodeFilter;
 UKawaiiPhysicsLibrary::CollectKawaiiPhysicsNodes(Nodes,Character,NodeFilter,false);
 bool Added=false;
 for(auto& Ref:Nodes){
  auto& Node=Ref.GetAnimNode<FAnimNode_KawaiiPhysics>();if(Node.Alpha<=0)continue;
  FWardrobeKawaiiClickForce Force;Force.bDragging=true;Force.DragStiffness=FMath::Clamp(P.DragStiffness,.1f,50.f);Force.MaxDragSpeed=FMath::Clamp(P.MaxDragSpeed,1.f,1000.f);
  bool Affected=false;
  for(const auto& Bone:Node.ModifyBones){
   const FVector World=FWardrobeKawaiiClickForce::BoneWorldPosition(Node,Character->GetComponentTransform(),Bone.Location);
   Force.DragOrigins.Add(World);Force.DragBoneNames.Add(Bone.BoneRef.BoneName);
   const float SurfaceDistance=FMath::Max(0.,FVector::Distance(World,Hit.ImpactPoint)-FMath::Max(0.f,Bone.PhysicsSettings.Radius)*Character->GetComponentTransform().GetMaximumAxisScale()-FMath::Max(0.f,P.HitPadding));
   const float Weight=Bone.ParentIndex<0?0:FWardrobeKawaiiClickForce::SpatialWeight(SurfaceDistance,P.Radius,P.Falloff);
   Force.DragWeights.Add(Weight);Affected|=Weight>0;
  }
  if(Affected){auto Value=FInstancedStruct::Make<FWardrobeKawaiiClickForce>(Force);Added|=UKawaiiPhysicsLibrary::AddExternalForce(Ref,Value,this,false);}
 }
 bPhysicsDragging=Added;bPhysicsTouchActive=Added;DragPlanePoint=Hit.ImpactPoint;DragPlaneNormal=Direction.GetSafeNormal();
 return Added;
}
void AWardrobeShowcase::UpdatePhysicsDrag(const FVector& Origin,const FVector& Direction){
 const auto* Profile=GetInteractionProfile();
 if(!bPhysicsDragging)return;
 if(!bInteractionEnabled||IsInteracting()||!Profile||!Profile->PhysicsTouch.bEnabled){StopPhysicsTouch();return;}
 const double Dot=FVector::DotProduct(Direction,DragPlaneNormal);if(FMath::Abs(Dot)<KINDA_SMALL_NUMBER)return;
 const double T=FVector::DotProduct(DragPlanePoint-Origin,DragPlaneNormal)/Dot;if(T<=0)return;
 const FVector Delta=(Origin+Direction*T-DragPlanePoint).GetClampedToMaxSize(FMath::Clamp(Profile->PhysicsTouch.MaxDragDistance,1.f,200.f));
 Character->HandleExistingParallelEvaluationTask(true,true);
 TArray<FKawaiiPhysicsReference> Nodes;FGameplayTagContainer NodeFilter;
 UKawaiiPhysicsLibrary::CollectKawaiiPhysicsNodes(Nodes,Character,NodeFilter,false);
 bool Found=false;
 for(auto& Ref:Nodes){auto& Node=Ref.GetAnimNode<FAnimNode_KawaiiPhysics>();for(auto& Value:Node.ExternalForces){
  auto* Force=Value.GetMutablePtr<FWardrobeKawaiiClickForce>();
  if(Force&&Force->ExternalOwner==this&&Force->bDragging){
   // A graph/LOD rebuild invalidates the captured bone list; release instead of pulling another bone.
   if(Force->DragOrigins.Num()!=Node.ModifyBones.Num()){StopPhysicsTouch();return;}
   Force->DragDelta=Delta;Found=true;
  }
 }}
 if(!Found)StopPhysicsTouch();
 if(Profile->PhysicsTouch.bDrawDebug)DrawDebugLine(GetWorld(),DragPlanePoint,DragPlanePoint+Delta,FColor::Yellow,false,.1f,0,2);
}
void AWardrobeShowcase::TickPhysicsTouch(float Dt){
 if(!bPhysicsTouchActive)return;
 const auto* P=GetInteractionProfile();if(!bInteractionEnabled||IsInteracting()||!P||!P->PhysicsTouch.bEnabled){StopPhysicsTouch();return;}
 if(!bPhysicsDragging){PhysicsTouchRemaining-=Dt;if(PhysicsTouchRemaining<=0)StopPhysicsTouch();}
}
void AWardrobeShowcase::StopPhysicsTouch(){
 if(bPhysicsTouchActive&&IsValid(Character)){
  Character->HandleExistingParallelEvaluationTask(true,true);
  FGameplayTagContainer FilterTags;UKawaiiPhysicsLibrary::RemoveExternalForcesFromComponent(Character,this,FilterTags,false);
 }
 bPhysicsDragging=false;bPhysicsTouchActive=false;PhysicsTouchRemaining=0;
}
bool UWardrobeMenu::BeginPhysicsDrag(FVector2D Position){
 EndPhysicsDrag();if(!CanPreviewInput()||!GetOwningPlayer())return false;
 auto* Person=StationController->GetActiveCharacter();if(!Person)return false;
 FVector2D Pixel,Viewport;USlateBlueprintLibrary::AbsoluteToViewport(this,Position,Pixel,Viewport);
 FVector O,D;if(!GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,O,D)||!(Person->BeginLimbDrag(O,D)||Person->BeginPinch(O,D)||Person->BeginPhysicsDrag(O,D)))return false;
 DragCharacter=Person;return true;
}
void UWardrobeMenu::UpdatePhysicsDrag(FVector2D Position){
 if(!DragCharacter.IsValid())return;
 if(!CanPreviewInput()||!GetOwningPlayer()){EndPhysicsDrag();return;}
 FVector2D Pixel,Viewport;USlateBlueprintLibrary::AbsoluteToViewport(this,Position,Pixel,Viewport);
 FVector O,D;if(GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,O,D)){if(DragCharacter->IsLimbDragging())DragCharacter->UpdateLimbDrag(O,D);else if(DragCharacter->IsPinching())DragCharacter->UpdatePinch(O,D);else DragCharacter->UpdatePhysicsDrag(O,D);}
}
void UWardrobeMenu::EndPhysicsDrag(){if(DragCharacter.IsValid()){DragCharacter->ReleaseLimbDrag();DragCharacter->ReleasePinch();DragCharacter->StopPhysicsTouch();}DragCharacter.Reset();}
bool UWardrobeMenu::HasLimbDragGesture() const{return DragCharacter.IsValid()&&DragCharacter->HasLimbPose();}
void UWardrobeMenu::StopClickEffect(){
 if(IsValid(ClickEffectPlayer)){ClickEffectPlayer->Stop();ClickEffectPlayer->RestoreState();}
 if(IsValid(ClickEffectActor))ClickEffectActor->Destroy();
 ClickEffectPlayer=nullptr;ClickEffectActor=nullptr;ClickEffectAge=0;
}
void UWardrobeMenu::PlayClickEffect(FVector2D Position){
 if(!IsMenuMap()||!Data||!Data->ClickEffect||!GetOwningPlayer())return;
 const auto& P=*Data->ClickEffect;
 if(!P.bEnabled||!P.Collection||GetWorld()->GetTimeSeconds()<NextClickEffectTime)return;
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 auto Fail=[&](const TCHAR* Reason){UE_LOG(LogTemp,Warning,TEXT("WARDROBE_CLICK_EFFECT %s"),Reason);NextClickEffectTime=GetWorld()->GetTimeSeconds()+1;};
 if(!P.Collection->GetVectorParameterByName(P.WorldPositionParameter)||(!P.ScreenUVParameter.IsNone()&&!P.Collection->GetVectorParameterByName(P.ScreenUVParameter))||(!P.TimeParameter.IsNone()&&!P.Collection->GetScalarParameterByName(P.TimeParameter))){Fail(TEXT("MPC parameter missing or wrong type"));return;}
 auto* Sequence=P.Sequence.LoadSynchronous();
 if(!Sequence||!Sequence->GetMovieScene()||Sequence->GetMovieScene()->GetPlaybackRange().IsEmpty()){Fail(TEXT("Configure a valid click effect Sequence"));return;}
 FVector2D Pixel,Viewport;USlateBlueprintLibrary::AbsoluteToViewport(this,Position,Pixel,Viewport);
 FVector Origin,Direction;
 if(!GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Origin,Direction))return;
 FHitResult Hit;bool Found=GetWorld()->LineTraceSingleByChannel(Hit,Origin,Origin+Direction*FMath::Max(100.f,P.TraceDistance),ECC_Visibility);
 auto* Person=IsValid(StationController)?StationController->GetActiveCharacter():nullptr;
 FHitResult PersonHit;
 if(Person&&Person->TraceTouch(Origin,Direction,PersonHit)&&(!Found||PersonHit.Distance<Hit.Distance)){Hit=PersonHit;Found=true;}
 FVector WorldPosition=Found?Hit.ImpactPoint:Origin+Direction*FMath::Max(1.f,P.FallbackDistance);
 if(!Found&&P.bUseCharacterPlane&&Person&&GetOwningPlayer()->PlayerCameraManager){
  const FVector Normal=GetOwningPlayer()->PlayerCameraManager->GetCameraRotation().Vector();
  const float Dot=FVector::DotProduct(Direction,Normal);
  if(FMath::Abs(Dot)>KINDA_SMALL_NUMBER){const float Distance=FVector::DotProduct(Person->GetActorLocation()-Origin,Normal)/Dot;if(Distance>0)WorldPosition=Origin+Direction*Distance;}
 }
 // One MPC holds one click: stop/restore the old effect before writing the new position.
 StopClickEffect();
 int32 Width=1,Height=1;GetOwningPlayer()->GetViewportSize(Width,Height);
 if(!P.WriteParameters(GetWorld(),WorldPosition,FVector2D(Pixel.X/FMath::Max(1,Width),Pixel.Y/FMath::Max(1,Height)),Found))return;
 FMovieSceneSequencePlaybackSettings Settings;Settings.LoopCount.Value=0;Settings.bDisableCameraCuts=true;Settings.FinishCompletionStateOverride=EMovieSceneCompletionModeOverride::ForceRestoreState;
 ALevelSequenceActor* Actor=nullptr;ClickEffectPlayer=ULevelSequencePlayer::CreateLevelSequencePlayer(this,Sequence,Settings,Actor);ClickEffectActor=Actor;
 if(!ClickEffectPlayer){StopClickEffect();return;}
 ClickEffectPlayer->Play();NextClickEffectTime=GetWorld()->GetTimeSeconds()+FMath::Max(0.f,P.MinInterval);
}

