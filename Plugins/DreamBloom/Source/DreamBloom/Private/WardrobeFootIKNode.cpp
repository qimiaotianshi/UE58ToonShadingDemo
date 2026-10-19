#include "WardrobeFootIKNode.h"
#include "WardrobeLocomotion.h"
#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "TwoBoneIK.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"

bool FAnimNode_WardrobeFootIK::BuildContactData(USkeletalMesh* Mesh){
 ContactMesh=Mesh;LeftSolePoints.Reset();RightSolePoints.Reset();
 if(!Mesh)return false;
 const auto& Ref=Mesh->GetRefSkeleton();const int Feet[]={Ref.FindBoneIndex(LeftFoot.BoneName),Ref.FindBoneIndex(RightFoot.BoneName)};
 if(Feet[0]==INDEX_NONE||Feet[1]==INDEX_NONE)return false;
 auto ComponentRef=[&](int Index){FTransform T=Ref.GetRefBonePose()[Index];while((Index=Ref.GetParentIndex(Index))!=INDEX_NONE)T=T*Ref.GetRefBonePose()[Index];return T;};
 const FTransform FootRef[2]={ComponentRef(Feet[0]),ComponentRef(Feet[1])};
 LeftReferenceHeight=FootRef[0].GetLocation().Z;RightReferenceHeight=FootRef[1].GetLocation().Z;
 LeftSoleNormal=FootRef[0].InverseTransformVectorNoScale(FVector::UpVector);RightSoleNormal=FootRef[1].InverseTransformVectorNoScale(FVector::UpVector);
 auto* Render=Mesh->GetResourceForRendering();if(!Render||Render->LODRenderData.IsEmpty())return false;
 const auto& LOD=Render->LODRenderData[0];const auto& Positions=LOD.StaticVertexBuffers.PositionVertexBuffer;const auto& W=LOD.SkinWeightVertexBuffer;
 const auto* WeightData=W.GetDataVertexBuffer();
 if(!Positions.GetVertexData()||!WeightData||!WeightData->GetWeightData())return false;
 TArray<FVector> Candidates[2];float MinZ[2]={BIG_NUMBER,BIG_NUMBER};
 for(const auto& Section:LOD.RenderSections)for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;V++){
  float FootWeight[2]={0,0};
  for(uint32 I=0;I<W.GetMaxBoneInfluences();I++){
   const uint32 Weight=W.GetBoneWeight(V,I);if(!Weight)continue;
   const int MapIndex=W.GetBoneIndex(V,I);if(!Section.BoneMap.IsValidIndex(MapIndex))continue;
   for(int Side=0;Side<2;Side++){int Bone=Section.BoneMap[MapIndex];while(Bone!=INDEX_NONE){if(Bone==Feet[Side]){FootWeight[Side]+=Weight;break;}Bone=Ref.GetParentIndex(Bone);}}
  }
  const FVector P(Positions.VertexPosition(V));
  int Side=FootWeight[0]>FootWeight[1]?0:1;
  // MMD export can use parallel foot deform bones rather than descendants of the
  // control ankle. Resolve low shoe geometry spatially in that case.
  if(FootWeight[Side]<32767){
   Side=FVector::DistSquaredXY(P,FootRef[0].GetLocation())<FVector::DistSquaredXY(P,FootRef[1].GetLocation())?0:1;
   if(P.Z>FootRef[Side].GetLocation().Z+2.f||FVector::DistSquaredXY(P,FootRef[Side].GetLocation())>400.f)continue;
  }
  Candidates[Side].Add(P);MinZ[Side]=FMath::Min(MinZ[Side],float(P.Z));
 }
 for(int Side=0;Side<2;Side++){
  TArray<FVector>& Points=Side==0?LeftSolePoints:RightSolePoints;
  // A small spatial grid retains heel and toe edge samples without a per-frame mesh scan.
  TMap<FIntPoint,FVector> Grid;
  for(const auto& P:Candidates[Side])if(P.Z<=MinZ[Side]+2.f){const FIntPoint Cell(FMath::FloorToInt(P.X/1.5),FMath::FloorToInt(P.Y/1.5));auto* Old=Grid.Find(Cell);if(!Old||P.Z<Old->Z)Grid.Add(Cell,P);}
  for(const auto& Pair:Grid)Points.Add(FootRef[Side].InverseTransformPosition(Pair.Value));
 }
 return !LeftSolePoints.IsEmpty()&&!RightSolePoints.IsEmpty();
}

void FAnimNode_WardrobeFootIK::Initialize_AnyThread(const FAnimationInitializeContext& C){
 FAnimNode_SkeletalControlBase::Initialize_AnyThread(C);PelvisOffset=0;bSnap=true;MeshWorld=FTransform::Identity;
 for(int I=0;I<2;I++){SmoothedOffset[I]=FVector::ZeroVector;SmoothedRotation[I]=FQuat::Identity;Ground[I]=FGroundSample();bFootLocked[I]=false;LockedFootWorld[I]=FVector::ZeroVector;LockForward[I]=FVector::ForwardVector;bLockReleasing[I]=false;bLockCanAcquire[I]=true;LockReleaseOffset[I]=FVector::ZeroVector;LockReleaseElapsed[I]=0;}
}
FVector FAnimNode_WardrobeFootIK::UpdateFootLock(int I,const FVector& WorldFoot,float Lock,bool bCanLock){
 if(I<0||I>=2)return FVector::ZeroVector;
 Lock=FMath::Clamp(Lock,0.f,1.f);
 if(Lock<.15f)bLockCanAcquire[I]=true;
 const float Scale=MeshWorld.GetScale3D().GetAbsMax();const FVector Forward=MeshWorld.GetUnitAxis(EAxis::X);
 const bool bUnsafe=!bCanLock||FVector::DotProduct(Forward,LockForward[I])<.94f||FVector::DistSquaredXY(WorldFoot,LockedFootWorld[I])>FMath::Square(MaxFootLockDistance*Scale);
 // Never zero the correction at a threshold or recapture repeatedly within the
 // same contact. Start from the last applied component-space correction, then
 // ease it away while the authored animation is free to lift and swing the foot.
 if(bFootLocked[I]&&(Lock<.9f||bUnsafe)){
  bFootLocked[I]=false;bLockReleasing[I]=true;bLockCanAcquire[I]=Lock<.15f;
  LockReleaseOffset[I]=FVector(SmoothedOffset[I].X,SmoothedOffset[I].Y,0);LockReleaseElapsed[I]=0;
  return LockReleaseOffset[I];
 }
 if(bLockReleasing[I]){
  LockReleaseElapsed[I]+=FMath::Max(0.f,DeltaTime);
  const float T=FMath::Clamp(LockReleaseElapsed[I]/FMath::Max(.01f,FootLockReleaseTime),0.f,1.f);
  const float Ease=T*T*T*(T*(T*6.f-15.f)+10.f);
  const FVector Result=LockReleaseOffset[I]*(1.f-Ease);if(T>=1.f)bLockReleasing[I]=false;
  return Result;
 }
 if(!bFootLocked[I]&&bLockCanAcquire[I]&&bCanLock&&Lock>=.9f){
  LockedFootWorld[I]=WorldFoot;LockForward[I]=Forward;bFootLocked[I]=true;bLockCanAcquire[I]=false;
 }
 if(!bFootLocked[I])return FVector::ZeroVector;
 FVector Target=WorldFoot;Target.X=LockedFootWorld[I].X;Target.Y=LockedFootWorld[I].Y;
 const FVector Offset=MeshWorld.InverseTransformPosition(Target)-MeshWorld.InverseTransformPosition(WorldFoot);
 return FVector(Offset.X,Offset.Y,0);
}
void FAnimNode_WardrobeFootIK::UpdateInternal(const FAnimationUpdateContext& C){DeltaTime=C.GetDeltaTime();}
void FAnimNode_WardrobeFootIK::InitializeBoneReferences(const FBoneContainer& B){
 Pelvis.Initialize(B);LeftFoot.Initialize(B);RightFoot.Initialize(B);LeftToe.Initialize(B);RightToe.Initialize(B);
}
bool FAnimNode_WardrobeFootIK::IsValidToEvaluate(const USkeleton*,const FBoneContainer& B){
 if(!Pelvis.IsValidToEvaluate(B)||!LeftFoot.IsValidToEvaluate(B)||!RightFoot.IsValidToEvaluate(B))return false;
 // FBoneReference validity does not guarantee its cached compact index is usable.
 auto ValidIndex=[&](FCompactPoseBoneIndex I){return I.GetInt()>=0&&I.GetInt()<B.GetCompactPoseNumBones();};
 const auto PelvisIndex=Pelvis.GetCompactPoseIndex(B);if(!ValidIndex(PelvisIndex))return false;
 for(const auto* Foot:{&LeftFoot,&RightFoot}){
  const auto FootIndex=Foot->GetCompactPoseIndex(B);if(!ValidIndex(FootIndex))return false;
  auto Knee=B.GetParentBoneIndex(FootIndex);if(!ValidIndex(Knee))return false;
  auto Hip=B.GetParentBoneIndex(Knee);if(!ValidIndex(Hip))return false;
  auto Ancestor=Hip;bool Found=false;
  while(ValidIndex(Ancestor)){if(Ancestor==PelvisIndex){Found=true;break;}Ancestor=B.GetParentBoneIndex(Ancestor);}
  if(!Found)return false;
 }
 return true;
}
void FAnimNode_WardrobeFootIK::PreUpdate(const UAnimInstance* Instance){
 RuntimeGroundAlpha=1;bRuntimeInAir=false;
 if(const auto* Locomotion=Cast<UWardrobeLocomotionAnim>(Instance)){RuntimeGroundAlpha=Locomotion->GroundIKAlpha;bRuntimeInAir=Locomotion->bInAir;}
 DragSuppression[0]=DragSuppression[1]=0;
 if(Instance)if(auto* A=Cast<AWardrobeShowcase>(Instance->GetOwningActor())){DragSuppression[0]=A->GetLimbGroundSuppression(LeftFoot.BoneName);DragSuppression[1]=A->GetLimbGroundSuppression(RightFoot.BoneName);}
 for(auto& G:Ground)G.bHit=false;
 auto* Mesh=Instance?Instance->GetSkelMeshComponent():nullptr;
 if(!Mesh||!Mesh->GetWorld()||!bEnabled||bRuntimeInAir)return;
 MeshWorld=Mesh->GetComponentTransform();
 // Reject bad component transforms before socket queries, traces or worker snapshots.
 if(MeshWorld.ContainsNaN()||!MeshWorld.GetRotation().IsNormalized())return;
 if(ContactMesh!=Mesh->GetSkeletalMeshAsset())BuildContactData(Mesh->GetSkeletalMeshAsset());
 const float Scale=MeshWorld.GetScale3D().GetAbsMax();
 FCollisionQueryParams Params(SCENE_QUERY_STAT(WardrobeFootGround),false,Mesh->GetOwner());Params.AddIgnoredComponent(Mesh);
 TArray<AActor*> Attached;if(Mesh->GetOwner()){Mesh->GetOwner()->GetAttachedActors(Attached);Params.AddIgnoredActors(Attached);}
 const FBoneReference* Feet[]={&LeftFoot,&RightFoot};const FBoneReference* Toes[]={&LeftToe,&RightToe};
 for(int I=0;I<2;I++){
  if(Mesh->GetBoneIndex(Feet[I]->BoneName)==INDEX_NONE)continue;
  const FVector Ankle=Mesh->GetSocketLocation(Feet[I]->BoneName);
  FVector Direction=MeshWorld.GetUnitAxis(EAxis::X);float Length=12.f*Scale;
  if(Mesh->GetBoneIndex(Toes[I]->BoneName)!=INDEX_NONE){Direction=Mesh->GetSocketLocation(Toes[I]->BoneName)-Ankle;Direction.Z=0;Length=FMath::Clamp(Direction.Size(),4.f*Scale,20.f*Scale);Direction.Normalize();}
  Direction.Z=0;if(!Direction.Normalize())Direction=FVector::ForwardVector;
  float Best=-BIG_NUMBER;FVector Normal=FVector::ZeroVector;int Hits=0;
  for(int J=0;J<3;J++){
   const float Along=J==0?0.f:(J==1?Length:-Length*.4f);
   const FVector Center=Ankle+Direction*Along;
   const FVector Start=Center+FVector::UpVector*TraceDistance*Scale,End=Center-FVector::UpVector*TraceDistance*Scale;
   FHitResult Hit;const bool HitGround=Mesh->GetWorld()->LineTraceSingleByChannel(Hit,Start,End,TraceChannel,Params)&&Hit.ImpactNormal.Z>=FMath::Cos(FMath::DegreesToRadians(MaxSlopeAngle));
   if(bDebugDraw){DrawDebugLine(Mesh->GetWorld(),Start,End,HitGround?FColor::Green:FColor::Red,false,0,0,1);if(HitGround)DrawDebugSphere(Mesh->GetWorld(),Hit.ImpactPoint,2.f,8,FColor::Yellow,false,0);}
   if(HitGround){const float AtAnkle=Hit.ImpactPoint.Z-(Hit.ImpactNormal.X*(Ankle.X-Hit.ImpactPoint.X)+Hit.ImpactNormal.Y*(Ankle.Y-Hit.ImpactPoint.Y))/Hit.ImpactNormal.Z;Best=FMath::Max(Best,AtAnkle);Normal+=Hit.ImpactNormal;Hits++;}
  }
  if(Hits){Ground[I].bHit=true;Ground[I].Point=FVector(Ankle.X,Ankle.Y,Best);Ground[I].Normal=Normal.GetSafeNormal();}
 }
}
void FAnimNode_WardrobeFootIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& O,TArray<FBoneTransform>& Out){
 const auto& Bones=O.Pose.GetPose().GetBoneContainer();const FBoneReference* Feet[]={&LeftFoot,&RightFoot};
 const auto PelvisIndex=Pelvis.GetCompactPoseIndex(Bones);
 if(!O.Pose.GetPose().IsValidIndex(PelvisIndex))return;
 if(bRuntimeInAir){
  PelvisOffset=0;for(int I=0;I<2;I++){SmoothedOffset[I]=FVector::ZeroVector;SmoothedRotation[I]=FQuat::Identity;bFootLocked[I]=bLockReleasing[I]=false;bLockCanAcquire[I]=true;}return;
 }
 const float A=bSnap||BlendTime<=0?1.f:1.f-FMath::Exp(-DeltaTime*4.6f/BlendTime);
 // PreUpdate can be skipped during reinitialization. Never transform through an
 // invalid snapshot; existing finite ground offsets fade back to the source pose.
 const bool bWorldValid=!MeshWorld.ContainsNaN()&&MeshWorld.GetRotation().IsNormalized();
 float RequestedPelvis=0;bool Any=false;
 for(int I=0;I<2;I++){
  const auto F=Feet[I]->GetCompactPoseIndex(Bones);
  if(!O.Pose.GetPose().IsValidIndex(F))continue;
  const FTransform& Foot=O.Pose.GetComponentSpaceTransform(F);
  FVector Offset=FVector::ZeroVector;FQuat Rotation=FQuat::Identity;
  LastPlant[I]=0;
  if(bEnabled&&bWorldValid&&Ground[I].bHit){
   const FVector WorldFoot=MeshWorld.TransformPosition(Foot.GetLocation());const float Scale=MeshWorld.GetScale3D().GetAbsMax();
   // Component-space lift is independent of Actor Z. Moving the person is not a kick.
   const float Reference=I==0?LeftReferenceHeight:RightReferenceHeight;
   const float Lift=FMath::Max(0.f,Foot.GetLocation().Z-Reference);
   const float Contact=bLockPlantedFeet?FMath::Clamp(O.Curve.Get(I==0?LeftPlantCurve:RightPlantCurve),0.f,1.f):1.f;
   const float Plant=(1.f-FMath::SmoothStep(MaxPlantHeight*.5f,MaxPlantHeight,Lift))*(1.f-DragSuppression[I])*Contact*RuntimeGroundAlpha;LastPlant[I]=Plant;
   const FVector Normal=MeshWorld.InverseTransformVectorNoScale(Ground[I].Normal).GetSafeNormal();
   const FVector Up=MeshWorld.InverseTransformVectorNoScale(FVector::UpVector).GetSafeNormal();
   const TArray<FVector>& Points=I==0?LeftSolePoints:RightSolePoints;
   const bool Auto=bAutoSoleContact&&!Points.IsEmpty();
   const FVector SourceUp=Auto?Foot.GetRotation().RotateVector(I==0?LeftSoleNormal:RightSoleNormal):Up;
   Rotation=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(SourceUp,Normal),Plant);
   float Height=SoleHeight*Scale;
   if(Auto){
    float MinSupport=BIG_NUMBER;const FQuat TargetRot=Rotation*Foot.GetRotation();
    for(const auto& P:Points){const FVector Relative=MeshWorld.TransformVector(TargetRot.RotateVector(P*Foot.GetScale3D()));MinSupport=FMath::Min(MinSupport,float(FVector::DotProduct(Ground[I].Normal,Relative)));}
    Height=(-MinSupport+.15f*Scale)/FMath::Max(.2f,Ground[I].Normal.Z);
   }
   FVector Target=WorldFoot;Target.Z=Ground[I].Point.Z+Height;
   Offset=(MeshWorld.InverseTransformPosition(Target)-Foot.GetLocation())*Plant;
   if(Plant>.001f){RequestedPelvis=Any?FMath::Min(RequestedPelvis,Offset.Z):Offset.Z;Any=true;}
  }
  if(bWorldValid&&(bLockPlantedFeet||bFootLocked[I]||bLockReleasing[I])){
   const float Lock=FMath::Clamp(O.Curve.Get(I==0?LeftPlantCurve:RightPlantCurve),0.f,1.f)*(1.f-DragSuppression[I])*RuntimeGroundAlpha;
   const FVector Horizontal=UpdateFootLock(I,MeshWorld.TransformPosition(Foot.GetLocation()),Lock,bEnabled&&Ground[I].bHit&&bLockPlantedFeet);
   Offset.X=Horizontal.X;Offset.Y=Horizontal.Y;
  }
  SmoothedOffset[I]=FMath::Lerp(SmoothedOffset[I],Offset,A);
  // Plant curves already ease in/out; filtering XY again lets a locked foot drift.
  if(bLockPlantedFeet){SmoothedOffset[I].X=Offset.X;SmoothedOffset[I].Y=Offset.Y;}
  SmoothedRotation[I]=FQuat::Slerp(SmoothedRotation[I],Rotation,A).GetNormalized();
 }
 PelvisOffset=FMath::Lerp(PelvisOffset,FMath::Clamp(RequestedPelvis,-MaxPelvisOffset,MaxPelvisOffset),A);bSnap=false;
 const FVector Shift(0,0,PelvisOffset);
 FTransform P=O.Pose.GetComponentSpaceTransform(PelvisIndex);P.AddToTranslation(Shift);P.NormalizeRotation();Out.Emplace(PelvisIndex,P);
 for(int I=0;I<2;I++){
  const auto F=Feet[I]->GetCompactPoseIndex(Bones);
  if(!O.Pose.GetPose().IsValidIndex(F))continue;
  const auto K=Bones.GetParentBoneIndex(F);
  if(!O.Pose.GetPose().IsValidIndex(K))continue;
  const auto H=Bones.GetParentBoneIndex(K);
  if(!O.Pose.GetPose().IsValidIndex(H))continue;
  FTransform Hip=O.Pose.GetComponentSpaceTransform(H),Knee=O.Pose.GetComponentSpaceTransform(K),Foot=O.Pose.GetComponentSpaceTransform(F);
  const FVector Target=Foot.GetLocation()+SmoothedOffset[I];const FQuat FootRot=(SmoothedRotation[I]*Foot.GetRotation()).GetNormalized();
  FVector Line=(Foot.GetLocation()-Hip.GetLocation()).GetSafeNormal();FVector Bend=Knee.GetLocation()-Hip.GetLocation();Bend-=Line*FVector::DotProduct(Bend,Line);
  // Reference-pose straight legs need a stable forward knee pole.
  if(!Bend.Normalize()){Bend=FVector::ForwardVector-Line*FVector::DotProduct(FVector::ForwardVector,Line);Bend.Normalize();}
  Hip.AddToTranslation(Shift);Knee.AddToTranslation(Shift);Foot.AddToTranslation(Shift);
  const FVector Pole=Knee.GetLocation()+Bend*50.f;
  AnimationCore::SolveTwoBoneIK(Hip,Knee,Foot,Pole,Target,false,1.,1.);
  Foot.SetRotation(FootRot);
  Hip.NormalizeRotation();Knee.NormalizeRotation();Foot.NormalizeRotation();
  Out.Emplace(H,Hip);Out.Emplace(K,Knee);Out.Emplace(F,Foot);
 }
 Out.Sort(FCompareBoneTransformIndex());
}
