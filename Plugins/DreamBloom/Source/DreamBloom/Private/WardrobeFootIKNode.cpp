#include "WardrobeFootIKNode.h"
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
 if(!Positions.GetVertexData()||!W.GetDataVertexBuffer()->GetWeightData())return false;
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
 FAnimNode_SkeletalControlBase::Initialize_AnyThread(C);PelvisOffset=0;bSnap=true;
 for(int I=0;I<2;I++){SmoothedOffset[I]=FVector::ZeroVector;SmoothedRotation[I]=FQuat::Identity;Ground[I]=FGroundSample();}
}
void FAnimNode_WardrobeFootIK::UpdateInternal(const FAnimationUpdateContext& C){DeltaTime=C.GetDeltaTime();}
void FAnimNode_WardrobeFootIK::InitializeBoneReferences(const FBoneContainer& B){
 Pelvis.Initialize(B);LeftFoot.Initialize(B);RightFoot.Initialize(B);LeftToe.Initialize(B);RightToe.Initialize(B);
}
bool FAnimNode_WardrobeFootIK::IsValidToEvaluate(const USkeleton*,const FBoneContainer& B){
 if(!Pelvis.IsValidToEvaluate(B)||!LeftFoot.IsValidToEvaluate(B)||!RightFoot.IsValidToEvaluate(B))return false;
 for(const auto* Foot:{&LeftFoot,&RightFoot}){
  auto Knee=B.GetParentBoneIndex(Foot->GetCompactPoseIndex(B));if(Knee==INDEX_NONE)return false;
  auto Hip=B.GetParentBoneIndex(Knee);if(Hip==INDEX_NONE)return false;
  auto Ancestor=Hip;bool Found=false;
  while(Ancestor!=INDEX_NONE){if(Ancestor==Pelvis.GetCompactPoseIndex(B)){Found=true;break;}Ancestor=B.GetParentBoneIndex(Ancestor);}
  if(!Found)return false;
 }
 return true;
}
void FAnimNode_WardrobeFootIK::PreUpdate(const UAnimInstance* Instance){
 DragSuppression[0]=DragSuppression[1]=0;
 if(Instance)if(auto* A=Cast<AWardrobeShowcase>(Instance->GetOwningActor())){DragSuppression[0]=A->GetLimbGroundSuppression(LeftFoot.BoneName);DragSuppression[1]=A->GetLimbGroundSuppression(RightFoot.BoneName);}
 for(auto& G:Ground)G.bHit=false;
 auto* Mesh=Instance?Instance->GetSkelMeshComponent():nullptr;
 if(!Mesh||!Mesh->GetWorld()||!bEnabled)return;
 MeshWorld=Mesh->GetComponentTransform();
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
 const float A=bSnap||BlendTime<=0?1.f:1.f-FMath::Exp(-DeltaTime*4.6f/BlendTime);
 float RequestedPelvis=0;bool Any=false;
 for(int I=0;I<2;I++){
  const auto F=Feet[I]->GetCompactPoseIndex(Bones);const FTransform& Foot=O.Pose.GetComponentSpaceTransform(F);
  FVector Offset=FVector::ZeroVector;FQuat Rotation=FQuat::Identity;
  if(bEnabled&&Ground[I].bHit){
   const FVector WorldFoot=MeshWorld.TransformPosition(Foot.GetLocation());const float Scale=MeshWorld.GetScale3D().GetAbsMax();
   // Component-space lift is independent of Actor Z. Moving the person is not a kick.
   const float Reference=I==0?LeftReferenceHeight:RightReferenceHeight;
   const float Lift=FMath::Max(0.f,Foot.GetLocation().Z-Reference);
   const float Plant=(1.f-FMath::SmoothStep(MaxPlantHeight*.5f,MaxPlantHeight,Lift))*(1.f-DragSuppression[I]);LastPlant[I]=Plant;
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
  SmoothedOffset[I]=FMath::Lerp(SmoothedOffset[I],Offset,A);
  SmoothedRotation[I]=FQuat::Slerp(SmoothedRotation[I],Rotation,A).GetNormalized();
 }
 PelvisOffset=FMath::Lerp(PelvisOffset,FMath::Clamp(RequestedPelvis,-MaxPelvisOffset,MaxPelvisOffset),A);bSnap=false;
 const FVector Shift(0,0,PelvisOffset);
 FTransform P=O.Pose.GetComponentSpaceTransform(Pelvis.GetCompactPoseIndex(Bones));P.AddToTranslation(Shift);Out.Emplace(Pelvis.GetCompactPoseIndex(Bones),P);
 for(int I=0;I<2;I++){
  const auto F=Feet[I]->GetCompactPoseIndex(Bones),K=Bones.GetParentBoneIndex(F),H=Bones.GetParentBoneIndex(K);
  FTransform Hip=O.Pose.GetComponentSpaceTransform(H),Knee=O.Pose.GetComponentSpaceTransform(K),Foot=O.Pose.GetComponentSpaceTransform(F);
  const FVector Target=Foot.GetLocation()+SmoothedOffset[I];const FQuat FootRot=(SmoothedRotation[I]*Foot.GetRotation()).GetNormalized();
  FVector Line=(Foot.GetLocation()-Hip.GetLocation()).GetSafeNormal();FVector Bend=Knee.GetLocation()-Hip.GetLocation();Bend-=Line*FVector::DotProduct(Bend,Line);
  // Reference-pose straight legs need a stable forward knee pole.
  if(!Bend.Normalize()){Bend=FVector::ForwardVector-Line*FVector::DotProduct(FVector::ForwardVector,Line);Bend.Normalize();}
  Hip.AddToTranslation(Shift);Knee.AddToTranslation(Shift);Foot.AddToTranslation(Shift);
  const FVector Pole=Knee.GetLocation()+Bend*50.f;
  AnimationCore::SolveTwoBoneIK(Hip,Knee,Foot,Pole,Target,false,1.,1.);
  Foot.SetRotation(FootRot);
  Out.Emplace(H,Hip);Out.Emplace(K,Knee);Out.Emplace(F,Foot);
 }
 Out.Sort(FCompareBoneTransformIndex());
}
