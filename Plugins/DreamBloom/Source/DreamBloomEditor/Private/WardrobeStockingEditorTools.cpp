#include "WardrobeStockingEditorTools.h"
#include "WardrobeStocking.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/World.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"

bool UWardrobeStockingEditorTools::CapturePreview(USceneCaptureComponent2D* Capture){
 if(!Capture||!Capture->GetWorld()||!Capture->TextureTarget)return false;
 FAssetCompilingManager::Get().FinishAllCompilation();
 if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
 Capture->GetWorld()->SendAllEndOfFrameUpdates();
 FlushRenderingCommands();
 Capture->CaptureScene();
 FlushRenderingCommands();
 return true;
}

namespace {
TArray<FTransform> ComponentReference(const USkeletalMesh* Mesh){
 const auto& Ref=Mesh->GetRefSkeleton();TArray<FTransform> Pose=Ref.GetRefBonePose();
 for(int32 I=0;I<Pose.Num();I++){const int32 Parent=Ref.GetParentIndex(I);if(Parent!=INDEX_NONE)Pose[I]*=Pose[Parent];}return Pose;
}
FName MatchingName(FName Name){FString S=Name.ToString();S.ReplaceInline(TEXT("."),TEXT("_"));return FName(*S);}
bool MakeLegFrame(const USkeletalMesh* Mesh,const TArray<FTransform>& Pose,FMatrix& Frame,float& Width){
 const auto& R=Mesh->GetRefSkeleton();int32 L=R.FindBoneIndex(TEXT("足D_L")),Right=R.FindBoneIndex(TEXT("足D_R")),KL=R.FindBoneIndex(TEXT("ひざD_L")),KR=R.FindBoneIndex(TEXT("ひざD_R"));
 if(L==INDEX_NONE||Right==INDEX_NONE||KL==INDEX_NONE||KR==INDEX_NONE)return false;
 const FVector O=(Pose[L].GetLocation()+Pose[Right].GetLocation())*.5,K=(Pose[KL].GetLocation()+Pose[KR].GetLocation())*.5;
 FVector X=Pose[L].GetLocation()-Pose[Right].GetLocation();Width=X.Size();if(Width<.0001||!X.Normalize())return false;
 FVector Z=O-K;Z=FVector::VectorPlaneProject(Z,X).GetSafeNormal();const FVector Y=FVector::CrossProduct(Z,X).GetSafeNormal();
 if(Z.IsNearlyZero()||Y.IsNearlyZero())return false;Frame=FMatrix(FPlane(X,0),FPlane(Y,0),FPlane(Z,0),FPlane(O,1));return true;
}
bool AppendSection(const FSkeletalMeshLODModel& L,const TArray<FSoftSkinVertex>& Skin,const FSkelMeshSection& S,const TArray<int32>& BoneMap,const FMatrix& Align,TArray<FWardrobeStockingVertex>& Vertices,TArray<int32>& Triangles){
 TMap<int32,int32> Remap;
 for(uint32 I=0;I<S.NumTriangles*3;I++){
  const int32 Source=L.IndexBuffer[S.BaseIndex+I];if(!Skin.IsValidIndex(Source)){UE_LOG(LogTemp,Error,TEXT("STOCKING_BAKE bad vertex=%d count=%d"),Source,Skin.Num());return false;}int32* Existing=Remap.Find(Source);
  if(Existing){Triangles.Add(*Existing);continue;}
  const auto& Soft=Skin[Source];FWardrobeStockingVertex V;V.Position=Align.TransformPosition(FVector(Soft.Position));
  V.Normal=Align.TransformVector(FVector(Soft.TangentZ.X,Soft.TangentZ.Y,Soft.TangentZ.Z)).GetSafeNormal();
  V.UV=FVector2D(Soft.UVs[0]);
  for(uint32 J=0;J<MAX_TOTAL_INFLUENCES;J++){
   const uint16 W=Soft.InfluenceWeights[J];if(!W)continue;const uint32 Bone=Soft.InfluenceBones[J];
   if(!S.BoneMap.IsValidIndex(Bone)||!BoneMap.IsValidIndex(S.BoneMap[Bone])||BoneMap[S.BoneMap[Bone]]==INDEX_NONE){UE_LOG(LogTemp,Error,TEXT("STOCKING_BAKE bone map failure vertex=%d localBone=%u sectionBones=%d refBone=%d weight=%u"),Source,Bone,S.BoneMap.Num(),S.BoneMap.IsValidIndex(Bone)?int32(S.BoneMap[Bone]):-1,W);return false;}
   V.Bones.Add(BoneMap[S.BoneMap[Bone]]);V.Weights.Add(float(W));
  }
  float Sum=0;for(float W:V.Weights)Sum+=W;if(Sum<=0){UE_LOG(LogTemp,Error,TEXT("STOCKING_BAKE unweighted vertex=%d"),Source);return false;}for(float& W:V.Weights)W/=Sum;
  const int32 Destination=Vertices.Add(MoveTemp(V));Remap.Add(Source,Destination);Triangles.Add(Destination);
 }
 return true;
}
}

FString UWardrobeStockingEditorTools::DescribeReference(USkeletalMesh* Mesh){
 if(!Mesh)return TEXT("missing");const auto P=ComponentReference(Mesh);FString Result=Mesh->GetPathName()+TEXT("\n");
 for(int32 I=0;I<P.Num();I++){const FString Name=Mesh->GetRefSkeleton().GetBoneName(I).ToString();if(Name.Contains(TEXT("足"))||Name.Contains(TEXT("ひざ"))||I==0)Result+=FString::Printf(TEXT("%s\t%s\n"),*Name,*P[I].GetLocation().ToString());}return Result;
}
UWardrobeStockingSurface* UWardrobeStockingEditorTools::BakeSurface(USkeletalMesh* Stocking,USkeletalMesh* Body,const FString& Path){
 if(!Stocking||!Body||!FPackageName::IsValidLongPackageName(Path))return nullptr;
 // FBX commandlet imports may leave secondary packages unsaved. Keep the source mesh independently editable.
 auto* Skeleton=Stocking->GetSkeleton();
 if(!Skeleton){
  const FString SkeletonPath=Stocking->GetOutermost()->GetName()+TEXT("_Skeleton");
  UPackage* SkeletonPackage=CreatePackage(*SkeletonPath);SkeletonPackage->FullyLoad();
  Skeleton=FindObject<USkeleton>(SkeletonPackage,*FPackageName::GetLongPackageAssetName(SkeletonPath));
  if(!Skeleton){Skeleton=NewObject<USkeleton>(SkeletonPackage,*FPackageName::GetLongPackageAssetName(SkeletonPath),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(Skeleton);}
  if(!Skeleton->MergeAllBonesToBoneTree(Stocking,false))return nullptr;
  Stocking->Modify();Stocking->SetSkeleton(Skeleton);
 }
 FSavePackageArgs SourceSave;SourceSave.TopLevelFlags=RF_Public|RF_Standalone;SourceSave.SaveFlags=SAVE_NoError;
 if(!UPackage::SavePackage(Skeleton->GetOutermost(),Skeleton,*FPackageName::LongPackageNameToFilename(Skeleton->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),SourceSave))return nullptr;
 // Textile picking is done against its actual triangles and does not need a separate rigid-body physics asset.
 Stocking->SetPhysicsAsset(nullptr);
 if(!UPackage::SavePackage(Stocking->GetOutermost(),Stocking,*FPackageName::LongPackageNameToFilename(Stocking->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),SourceSave))return nullptr;
 const auto* SourceData=Stocking->GetImportedModel();const auto* BodyData=Body->GetImportedModel();
 if(!SourceData||SourceData->LODModels.IsEmpty()||!BodyData||BodyData->LODModels.IsEmpty())return nullptr;
 const auto SourcePose=ComponentReference(Stocking),BodyPose=ComponentReference(Body);FMatrix SourceFrame,TargetFrame;float SourceWidth,TargetWidth;
 if(!MakeLegFrame(Stocking,SourcePose,SourceFrame,SourceWidth)||!MakeLegFrame(Body,BodyPose,TargetFrame,TargetWidth))return nullptr;
 const float Scale=TargetWidth/SourceWidth;FMatrix ScaleMatrix=FMatrix::Identity;ScaleMatrix.M[0][0]=ScaleMatrix.M[1][1]=ScaleMatrix.M[2][2]=Scale;
 const FMatrix Align=SourceFrame.Inverse()*ScaleMatrix*TargetFrame;
 TArray<int32> Map;Map.SetNum(Stocking->GetRefSkeleton().GetNum());
 float Worst=0;
 for(int32 I=0;I<Map.Num();I++){
  Map[I]=Body->GetRefSkeleton().FindBoneIndex(MatchingName(Stocking->GetRefSkeleton().GetBoneName(I)));
  const FString N=Stocking->GetRefSkeleton().GetBoneName(I).ToString();
  if(Map[I]!=INDEX_NONE&&(N.StartsWith(TEXT("足D_"))||N.StartsWith(TEXT("ひざD_"))||N.StartsWith(TEXT("足首D_"))))Worst=FMath::Max(Worst,float(FVector::Dist(Align.TransformPosition(SourcePose[I].GetLocation()),BodyPose[Map[I]].GetLocation())));
 }
 UE_LOG(LogTemp,Display,TEXT("STOCKING_IMPORT_ALIGNMENT scale=%.5f maxLegBoneError=%.5f cm"),Scale,Worst);
 if(Worst>1){UE_LOG(LogTemp,Error,TEXT("Stocking reference pose does not match the body; refusing to bake."));return nullptr;}
 TArray<FWardrobeStockingVertex> Vertices,Occluders;TArray<int32> Triangles,OccluderTriangles;
 const auto& SourceLOD=SourceData->LODModels[0];TArray<FSoftSkinVertex> SourceVertices;SourceLOD.GetVertices(SourceVertices);
 for(const auto& S:SourceLOD.Sections)if(!AppendSection(SourceLOD,SourceVertices,S,Map,Align,Vertices,Triangles))return nullptr;
 TArray<int32> Identity;for(int32 I=0;I<BodyPose.Num();I++)Identity.Add(I);
 const auto& BodyLOD=BodyData->LODModels[0];const auto& Materials=Body->GetMaterials();TArray<FSoftSkinVertex> BodyVertices;BodyLOD.GetVertices(BodyVertices);
 // The supplied blend and UE mesh can be different revisions even with identical leg bones.
 // Fit the textile to the actual displayed skin and transfer barycentric skin weights.
 TArray<FWardrobeStockingVertex> Skin;TArray<int32> SkinTriangles;
 for(const auto& S:BodyLOD.Sections)if(Materials.IsValidIndex(S.MaterialIndex)&&Materials[S.MaterialIndex].MaterialSlotName==TEXT("肌肤")){
  if(!AppendSection(BodyLOD,BodyVertices,S,Identity,FMatrix::Identity,Skin,SkinTriangles))return nullptr;
 }
 if(SkinTriangles.IsEmpty())return nullptr;
 double WorstFit=0,TotalFit=0;int32 Inside=0;
 for(auto& V:Vertices){
  double Best=TNumericLimits<double>::Max();int32 Face=INDEX_NONE;FVector Point;
  for(int32 T=0;T<SkinTriangles.Num();T+=3){
   const FVector P=FMath::ClosestPointOnTriangleToPoint(V.Position,Skin[SkinTriangles[T]].Position,Skin[SkinTriangles[T+1]].Position,Skin[SkinTriangles[T+2]].Position);
   const double D=FVector::DistSquared(P,V.Position);if(D<Best){Best=D;Face=T;Point=P;}
  }
  if(Face==INDEX_NONE)return nullptr;
  const auto& A=Skin[SkinTriangles[Face]];const auto& B=Skin[SkinTriangles[Face+1]];const auto& C=Skin[SkinTriangles[Face+2]];
  const FVector Bary=FMath::GetBaryCentric2D(Point,A.Position,B.Position,C.Position);
  FVector N=FVector::CrossProduct(B.Position-A.Position,C.Position-A.Position).GetSafeNormal();
  const FVector Smooth=(A.Normal*Bary.X+B.Normal*Bary.Y+C.Normal*Bary.Z).GetSafeNormal();if(FVector::DotProduct(N,Smooth)<0)N=-N;
  const double Error=FMath::Sqrt(Best);WorstFit=FMath::Max(WorstFit,Error);TotalFit+=Error;if(FVector::DotProduct(V.Position-Point,N)<0)Inside++;
  V.Position=Point+N*.12;V.Normal=Smooth.IsNearlyZero()?N:Smooth;
  TMap<int32,float> Weights;
  const FWardrobeStockingVertex* Corners[]={&A,&B,&C};const double Factors[]={Bary.X,Bary.Y,Bary.Z};
  for(int32 J=0;J<3;J++)for(int32 K=0;K<Corners[J]->Bones.Num();K++)Weights.FindOrAdd(Corners[J]->Bones[K])+=Corners[J]->Weights[K]*float(FMath::Max(0.0,Factors[J]));
  V.Bones.Reset();V.Weights.Reset();float Sum=0;for(const auto& W:Weights)if(W.Value>.00001f){V.Bones.Add(W.Key);V.Weights.Add(W.Value);Sum+=W.Value;}
  if(Sum<=0)return nullptr;for(float& W:V.Weights)W/=Sum;
 }
 UE_LOG(LogTemp,Display,TEXT("STOCKING_SKIN_FIT originalInside=%d/%d averageDistance=%.5f maxDistance=%.5f cm finalShellOffset=0.12 cm"),Inside,Vertices.Num(),TotalFit/Vertices.Num(),WorstFit);
 for(const auto& S:BodyLOD.Sections){
  if(!Materials.IsValidIndex(S.MaterialIndex))continue;const FString Name=Materials[S.MaterialIndex].MaterialSlotName.ToString();
  if(Name==TEXT("披风")||Name==TEXT("连衣裙通用")||Name==TEXT("裙内")||Name==TEXT("鞋子")||Name==TEXT("圈"))if(!AppendSection(BodyLOD,BodyVertices,S,Identity,FMatrix::Identity,Occluders,OccluderTriangles))return nullptr;
 }
 UPackage* Package=CreatePackage(*Path);Package->FullyLoad();const FName Name(*FPackageName::GetLongPackageAssetName(Path));
 auto* Surface=FindObject<UWardrobeStockingSurface>(Package,*Name.ToString());const bool New=!Surface;
 if(!Surface)Surface=NewObject<UWardrobeStockingSurface>(Package,Name,RF_Public|RF_Standalone);
 Surface->Modify();Surface->StockingMesh=Stocking;Surface->BodyMesh=Body;Surface->Vertices=MoveTemp(Vertices);Surface->Triangles=MoveTemp(Triangles);Surface->OccluderVertices=MoveTemp(Occluders);Surface->OccluderTriangles=MoveTemp(OccluderTriangles);
 if(New)FAssetRegistryModule::AssetCreated(Surface);Package->MarkPackageDirty();
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 if(!UPackage::SavePackage(Package,Surface,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args)){UE_LOG(LogTemp,Error,TEXT("STOCKING_BAKE could not save %s"),*Path);return nullptr;}
 UE_LOG(LogTemp,Display,TEXT("STOCKING_BAKED vertices=%d triangles=%d occluders=%d"),Surface->Vertices.Num(),Surface->Triangles.Num()/3,Surface->OccluderTriangles.Num()/3);return Surface;
}
