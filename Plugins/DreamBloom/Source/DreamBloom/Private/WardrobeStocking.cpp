#include "WardrobeStocking.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"

namespace WardrobeStocking {
bool RayTriangle(const FVector& O,const FVector& D,const FVector& A,const FVector& B,const FVector& C,double& T){
 const FVector AB=B-A,AC=C-A,P=FVector::CrossProduct(D,AC);const double Det=FVector::DotProduct(AB,P);
 if(FMath::Abs(Det)<1e-10)return false;
 const FVector S=O-A;const double U=FVector::DotProduct(S,P)/Det;if(U<0||U>1)return false;
 const FVector Q=FVector::CrossProduct(S,AB);const double V=FVector::DotProduct(D,Q)/Det;if(V<0||U+V>1)return false;
 T=FVector::DotProduct(AC,Q)/Det;return T>0;
}
float PinchWeight(float D,float R,float Tip,float Power){
 R=FMath::Max(.1f,R);Tip=FMath::Clamp(Tip,.01f,R*.25f);
 const float Extent=FMath::Sqrt(R*R+Tip*Tip)-Tip;
 return FMath::Pow(FMath::Clamp(1-(FMath::Sqrt(D*D+Tip*Tip)-Tip)/Extent,0.f,1.f),FMath::Clamp(Power,1.f,3.f));
}
void SpringStep(FVector& P,FVector& V,const FVector& T,float F,float Z,float Dt){
 if(!FMath::IsFinite(Dt)||Dt<=0)return;
 const int32 Steps=FMath::Clamp(FMath::CeilToInt(FMath::Min(Dt,.1f)*240),1,24);const float H=FMath::Min(Dt,.1f)/Steps;
 const float W=2*PI*FMath::Clamp(F,1.f,20.f);
 for(int32 I=0;I<Steps;I++){V+=(W*W*(T-P)-2*FMath::Clamp(Z,.2f,2.f)*W*V)*H;P+=V*H;}
}
}

UWardrobeStockingComponent::UWardrobeStockingComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;}
void UWardrobeStockingComponent::BeginPlay(){Super::BeginPlay();RefreshSurface();}
void UWardrobeStockingComponent::EndPlay(const EEndPlayReason::Type R){ResetPull();if(RenderMesh)RenderMesh->DestroyComponent();RenderMesh=nullptr;Super::EndPlay(R);}

bool UWardrobeStockingComponent::ValidateData() const{
 if(!Data||!Body||Data->BodyMesh!=Body->GetSkeletalMeshAsset()||Data->Vertices.IsEmpty()||Data->Triangles.IsEmpty()||Data->Triangles.Num()%3)return false;
 const int32 Bones=Data->BodyMesh->GetRefSkeleton().GetNum();
 auto ValidVertices=[Bones](const TArray<FWardrobeStockingVertex>& V){for(const auto& P:V){if(P.Bones.IsEmpty()||P.Bones.Num()!=P.Weights.Num()||P.Position.ContainsNaN())return false;float Total=0;for(int32 I=0;I<P.Bones.Num();I++){if(P.Bones[I]<0||P.Bones[I]>=Bones||!FMath::IsFinite(P.Weights[I])||P.Weights[I]<0)return false;Total+=P.Weights[I];}if(FMath::Abs(Total-1)>.01f)return false;}return true;};
 if(!ValidVertices(Data->Vertices)||!ValidVertices(Data->OccluderVertices)||Data->OccluderTriangles.Num()%3)return false;
 for(int32 I:Data->Triangles)if(!Data->Vertices.IsValidIndex(I))return false;
 for(int32 I:Data->OccluderTriangles)if(!Data->OccluderVertices.IsValidIndex(I))return false;
 return true;
}
bool UWardrobeStockingComponent::RefreshSurface(){
 auto* Owner=Cast<AWardrobeShowcase>(GetOwner());const auto* P=Owner?Owner->GetInteractionProfile():nullptr;
 auto* Desired=P&&P->Stockings.bEnabled?P->Stockings.Surface.Get():nullptr;
 Body=Owner?Owner->Character.Get():nullptr;
 if(!Desired||!Body){ResetPull();Data=nullptr;if(RenderMesh)RenderMesh->SetVisibility(false);return false;}
 const bool Changed=Data!=Desired||!RenderMesh;Settings=P->Stockings;
 if(Data!=Desired){ResetPull();Data=Desired;}
 if(!ValidateData()){
  if(RenderMesh)RenderMesh->SetVisibility(false);
  UE_LOG(LogTemp,Warning,TEXT("STOCKING_INVALID_SURFACE actor=%s data=%s"),*GetNameSafe(Owner),*GetNameSafe(Data));Data=nullptr;return false;
 }
 if(!RenderMesh){
  RenderMesh=NewObject<UProceduralMeshComponent>(Owner,TEXT("StockingSurface"));Owner->AddInstanceComponent(RenderMesh);
  RenderMesh->SetupAttachment(Body);RenderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);RenderMesh->SetCastShadow(false);
  RenderMesh->SetTranslucentSortPriority(1);RenderMesh->RegisterComponent();AddTickPrerequisiteComponent(Body);
 }
 RenderMesh->SetVisibility(true);
 RenderMesh->SetLightingChannels(Body->LightingChannels.bChannel0,Body->LightingChannels.bChannel1,Body->LightingChannels.bChannel2);
 if(Changed){
  RenderMesh->ClearAllMeshSections();bCreatedSection=false;BuildNeighbors();
  UV.Reset();for(const auto& V:Data->Vertices)UV.Add(V.UV);
  Influence.Init(0,Data->Vertices.Num());Stretch.Init(0,Data->Vertices.Num());Colors.Init(FLinearColor::White,Data->Vertices.Num());
  UpdateSkin();UpdateGeometry();
  UE_LOG(LogTemp,Display,TEXT("STOCKING_SURFACE_READY actor=%s vertices=%d triangles=%d"),*GetNameSafe(Owner),Data->Vertices.Num(),Data->Triangles.Num()/3);
 }
 UpdateMaterial();return true;
}
void UWardrobeStockingComponent::BuildNeighbors(){
 Neighbors.SetNum(Data->Vertices.Num());for(auto& N:Neighbors)N.Reset();NormalWeld.SetNum(Data->Vertices.Num());
 FaceNormalSigns.SetNum(Data->Triangles.Num()/3);
 for(int32 T=0;T<Data->Triangles.Num();T+=3){
  const auto& A=Data->Vertices[Data->Triangles[T]];const auto& B=Data->Vertices[Data->Triangles[T+1]];const auto& C=Data->Vertices[Data->Triangles[T+2]];
  const FVector FaceNormal=FVector::CrossProduct(B.Position-A.Position,C.Position-A.Position);
  FaceNormalSigns[T/3]=FVector::DotProduct(FaceNormal,A.Normal+B.Normal+C.Normal)<0?-1:1;
 }
 for(int32 I=0;I<Data->Triangles.Num();I+=3)for(int32 J=0;J<3;J++){
  const int32 A=Data->Triangles[I+J],B=Data->Triangles[I+(J+1)%3];Neighbors[A].AddUnique(B);Neighbors[B].AddUnique(A);
 }
 TMap<FIntVector,int32> Weld;
 for(int32 I=0;I<Data->Vertices.Num();I++){
  const FVector P=Data->Vertices[I].Position*1000;const FIntVector Key(FMath::RoundToInt(P.X),FMath::RoundToInt(P.Y),FMath::RoundToInt(P.Z));
  NormalWeld[I]=I;
  if(const int32* Other=Weld.Find(Key)){Neighbors[I].AddUnique(*Other);Neighbors[*Other].AddUnique(I);NormalWeld[I]=*Other;}else Weld.Add(Key,I);
 }
}
void UWardrobeStockingComponent::SkinVertices(const TArray<FWardrobeStockingVertex>& Source,TArray<FVector>& Result){
 Result.SetNumUninitialized(Source.Num());
 for(int32 I=0;I<Source.Num();I++){const auto& V=Source[I];FVector P=FVector::ZeroVector;for(int32 J=0;J<V.Bones.Num();J++)P+=FVector(BoneMatrices[V.Bones[J]].TransformPosition(V.Position))*V.Weights[J];Result[I]=P;}
}
void UWardrobeStockingComponent::UpdateSkin(){
 if(!Data||!Body)return;
 Body->HandleExistingParallelEvaluationTask(true,true);
 const auto& Pose=Body->GetComponentSpaceTransforms();const auto& Inverse=Data->BodyMesh->GetRefBasesInvMatrix();
 BoneMatrices.SetNum(Inverse.Num());
 for(int32 I=0;I<Inverse.Num();I++)BoneMatrices[I]=FMatrix(Inverse[I])*(Pose.IsValidIndex(I)?Pose[I].ToMatrixWithScale():FMatrix::Identity);
 SkinVertices(Data->Vertices,SkinnedRest);RestNormals.SetNumUninitialized(Data->Vertices.Num());
 for(int32 I=0;I<Data->Vertices.Num();I++){const auto& V=Data->Vertices[I];FVector N=FVector::ZeroVector;for(int32 J=0;J<V.Bones.Num();J++)N+=BoneMatrices[V.Bones[J]].TransformVector(V.Normal)*V.Weights[J];RestNormals[I]=N.GetSafeNormal();SkinnedRest[I]+=RestNormals[I]*FMath::Clamp(Settings.SurfaceClearance,0.f,2.f);}
}
void UWardrobeStockingComponent::UpdateMaterial(){
 // Bind the authored material unchanged. Deformation inputs are vertex color R (strain) and G (pull).
 if(RenderMesh&&RenderMesh->GetMaterial(0)!=Settings.Material)RenderMesh->SetMaterial(0,Settings.Material);
}
bool UWardrobeStockingComponent::BeginPull(const FVector& O,const FVector& D){
 auto* Owner=Cast<AWardrobeShowcase>(GetOwner());
 if(!Owner||!Owner->IsInteractionEnabled()||Owner->IsInteracting()||Owner->IsSequenceSwitching()||Owner->IsPinching()||Owner->HasLimbPose()||Owner->IsReleaseAnimationPlaying()||D.IsNearlyZero()||HasPull())return false;
 if(!RefreshSurface())return false;UpdateSkin();UpdateGeometry();
 const FTransform X=Body->GetComponentTransform();const FVector Origin=X.InverseTransformPosition(O),Ray=X.InverseTransformVector(D).GetSafeNormal();
 double Best=1e8;int32 Face=INDEX_NONE;FCollisionQueryParams Query(SCENE_QUERY_STAT(StockingPull),false);Query.AddIgnoredActor(Owner);FHitResult Block;
 if(GetWorld()->LineTraceSingleByChannel(Block,O,O+D.GetSafeNormal()*100000,ECC_Visibility,Query))Best=(X.InverseTransformPosition(Block.ImpactPoint)-Origin).Size();
 for(int32 T=0;T<Data->Triangles.Num();T+=3){double Distance;const int32 A=Data->Triangles[T],B=Data->Triangles[T+1],C=Data->Triangles[T+2];if(WardrobeStocking::RayTriangle(Origin,Ray,Positions[A],Positions[B],Positions[C],Distance)&&Distance<Best){Best=Distance;Face=T;}}
 if(Face==INDEX_NONE)return false;
 SkinVertices(Data->OccluderVertices,OccluderPositions);
 for(int32 T=0;T<Data->OccluderTriangles.Num();T+=3){double Distance;if(WardrobeStocking::RayTriangle(Origin,Ray,OccluderPositions[Data->OccluderTriangles[T]],OccluderPositions[Data->OccluderTriangles[T+1]],OccluderPositions[Data->OccluderTriangles[T+2]],Distance)&&Distance<Best-.05)return false;}
 GrabLocal=Origin+Ray*Best;Anchor=Data->Triangles[Face];
 for(int32 J=1;J<3;J++){int32 V=Data->Triangles[Face+J];if(FVector::DistSquared(Positions[V],GrabLocal)<FVector::DistSquared(Positions[Anchor],GrabLocal))Anchor=V;}
 NormalLocal=FVector::CrossProduct(Positions[Data->Triangles[Face+1]]-Positions[Data->Triangles[Face]],Positions[Data->Triangles[Face+2]]-Positions[Data->Triangles[Face]]).GetSafeNormal();
 if(FVector::DotProduct(NormalLocal,Ray)>0)NormalLocal=-NormalLocal;
 // This surface is authored in character centimetres. Feet narrow smoothly into the leg settings.
 RegionBlend=FMath::SmoothStep(14.f,22.f,float(Data->Vertices[Anchor].Position.Z));
 PullLimit=FMath::Max(.1f,Settings.MaximumPull)*FMath::Lerp(FMath::Clamp(Settings.FootPullRatio,.1f,1.f),1.f,RegionBlend);
 PinchRadius=FMath::Lerp(FMath::Min(Settings.InfluenceRadius,PullLimit*1.25f),Settings.InfluenceRadius,RegionBlend);
 Distances.Init(TNumericLimits<float>::Max(),Data->Vertices.Num());Influence.Init(0,Data->Vertices.Num());Active.Reset();
 struct FEntry{float Distance;int32 Vertex;};TArray<FEntry> Heap;auto Predicate=[](const FEntry& A,const FEntry& B){return A.Distance<B.Distance;};
 for(int32 J=0;J<3;J++){int32 V=Data->Triangles[Face+J];Distances[V]=FVector::Distance(GrabLocal,SkinnedRest[V]);Heap.HeapPush(FEntry{Distances[V],V},Predicate);}
 while(!Heap.IsEmpty()){
  FEntry E;Heap.HeapPop(E,Predicate,EAllowShrinking::No);if(E.Distance!=Distances[E.Vertex])continue;if(E.Distance>PinchRadius)break;
  for(int32 V:Neighbors[E.Vertex]){const float Next=E.Distance+FVector::Distance(SkinnedRest[E.Vertex],SkinnedRest[V]);if(Next<Distances[V]&&Next<PinchRadius){Distances[V]=Next;Heap.HeapPush(FEntry{Next,V},Predicate);}}
 }
 for(int32 V=0;V<Distances.Num();V++)if(Distances[V]<PinchRadius){const float Distance=FMath::Lerp(float(FVector::Distance(GrabLocal,SkinnedRest[V])),Distances[V],RegionBlend);Influence[V]=WardrobeStocking::PinchWeight(Distance,PinchRadius,Settings.PinchTipRadius,Settings.PinchSharpness);if(Influence[V]>0)Active.Add(V);}
 if(Active.IsEmpty()){ResetPull();return false;}
 Offset=Velocity=Target=FVector::ZeroVector;bDragging=true;PlaneWorldPoint=X.TransformPosition(GrabLocal);PlaneWorldNormal=D.GetSafeNormal();
 Owner->StopPhysicsTouch();UE_LOG(LogTemp,Display,TEXT("STOCKING_GRAB actor=%s vertex=%d patch=%d"),*GetNameSafe(Owner),Anchor,Active.Num());return true;
}
void UWardrobeStockingComponent::UpdatePull(const FVector& O,const FVector& D){
 if(!bDragging||!Body||D.IsNearlyZero())return;
 const double Dot=FVector::DotProduct(D,PlaneWorldNormal);if(FMath::Abs(Dot)<1e-8)return;
 const double T=FVector::DotProduct(PlaneWorldPoint-O,PlaneWorldNormal)/Dot;if(T<=0)return;
 FVector Delta=Body->GetComponentTransform().InverseTransformVector((O+D*T-PlaneWorldPoint)*FMath::Clamp(Settings.DragSensitivity,.1f,2.f));
 const double Inward=FVector::DotProduct(Delta,NormalLocal);if(Inward<0)Delta-=NormalLocal*Inward;
 const FVector Tangent=FVector::VectorPlaneProject(Delta,NormalLocal);Delta+=NormalLocal*FMath::Min(Tangent.Size()*.35,double(PullLimit*.4f));Target=Delta.GetClampedToMaxSize(FMath::Min(PullLimit,PinchRadius*1.25f));
}
void UWardrobeStockingComponent::ReleasePull(){if(bDragging){bDragging=false;Target=FVector::ZeroVector;UE_LOG(LogTemp,Display,TEXT("STOCKING_RELEASE actor=%s"),*GetNameSafe(GetOwner()));}}
void UWardrobeStockingComponent::ResetPull(){Anchor=INDEX_NONE;bDragging=false;Offset=Velocity=Target=FVector::ZeroVector;Active.Reset();for(float& V:Influence)V=0;}
void UWardrobeStockingComponent::Simulate(float Dt){
 if(!Data||!Body||!RenderMesh||!FMath::IsFinite(Dt)||Dt<0)return;
 auto* Owner=Cast<AWardrobeShowcase>(GetOwner());if(HasPull()&&(!Owner||!Owner->IsInteractionEnabled()||Owner->IsInteracting()||Owner->IsSequenceSwitching()))ResetPull();
 if(HasPull()){
  WardrobeStocking::SpringStep(Offset,Velocity,Target,bDragging?Settings.FollowFrequency:Settings.ReleaseFrequency,bDragging?Settings.FollowDamping:Settings.ReleaseDamping,Dt);
  const double Inward=FVector::DotProduct(Offset,NormalLocal);if(Inward<0){Offset-=NormalLocal*Inward;const double V=FVector::DotProduct(Velocity,NormalLocal);if(V<0)Velocity-=NormalLocal*V;}
  Offset=Offset.GetClampedToMaxSize(PullLimit);
  if(!bDragging&&Offset.SizeSquared()<1e-6&&Velocity.SizeSquared()<1e-4)ResetPull();
 }
 UpdateSkin();UpdateGeometry();
 if(HasPull()&&Settings.bDrawGrab)DrawDebugLine(GetWorld(),Body->GetComponentTransform().TransformPosition(SkinnedRest[Anchor]),Body->GetComponentTransform().TransformPosition(SkinnedRest[Anchor]+Offset),FColor::Magenta,false,0,0,1);
}
void UWardrobeStockingComponent::UpdateGeometry(){
 if(!Data||!RenderMesh||SkinnedRest.Num()!=Data->Vertices.Num())return;
 Positions=SkinnedRest;const FVector Direction=Offset.GetSafeNormal();
 const float Gather=Settings.FootGather*(1-RegionBlend)*FMath::Clamp(float(Offset.Size())/FMath::Max(.1f,PinchRadius*.5f),0.f,1.f);
 for(int32 V:Active){
  FVector Delta=Offset*Influence[V];const FVector N=RestNormals[V];const double Inward=FVector::DotProduct(Delta,N);const FVector Sliding=Inward<0?Delta-N*Inward:Delta;
  float Contact=FMath::Clamp(float(FVector::DotProduct(Direction,N))/.25f,0.f,1.f);Contact=Contact*Contact*Contact*(Contact*(Contact*6-15)+10);
  Delta=FMath::Lerp(Delta*Contact,Sliding,RegionBlend);
  if(Gather>0&&Contact>0){FVector G=-FVector::VectorPlaneProject(SkinnedRest[V]-SkinnedRest[Anchor],Direction)*(Gather*FMath::Pow(Influence[V],3)*Contact);const double Into=FVector::DotProduct(G,N);if(Into<0)G*=FMath::Min(1.0,FMath::Max(0.0,FVector::DotProduct(Delta,N))/-Into);Delta+=G;}
  Positions[V]+=Delta;
 }
 Normals.Init(FVector::ZeroVector,Positions.Num());
 // Imported winding can oppose the authored normal. Keep that orientation when rebuilding a pulled surface.
 for(int32 T=0;T<Data->Triangles.Num();T+=3){const int32 A=Data->Triangles[T],B=Data->Triangles[T+1],C=Data->Triangles[T+2];const FVector N=FVector::CrossProduct(Positions[B]-Positions[A],Positions[C]-Positions[A])*FaceNormalSigns[T/3];Normals[NormalWeld[A]]+=N;Normals[NormalWeld[B]]+=N;Normals[NormalWeld[C]]+=N;}
 TArray<FVector> Welded=Normals;for(int32 V=0;V<Normals.Num();V++){Normals[V]=Welded[NormalWeld[V]].GetSafeNormal();if(Normals[V].IsNearlyZero())Normals[V]=RestNormals[V];}
 Stretch.Init(0,Positions.Num());
 for(int32 V:Active){float Sum=0;int32 Count=0;for(int32 N:Neighbors[V]){const float Rest=FVector::Distance(SkinnedRest[V],SkinnedRest[N]);if(Rest<.0001f)continue;Sum+=FMath::Max(0.f,float(FVector::Distance(Positions[V],Positions[N]))/Rest-1);Count++;}Stretch[V]=FMath::Clamp(FMath::Pow(Sum*6/FMath::Max(1,Count),1.26f)*.75f,0.f,1.f);}
 for(int32 V=0;V<Colors.Num();V++)Colors[V]=FLinearColor(Stretch[V],Influence[V]*FMath::Clamp(float(Offset.Size())/2.f,0.f,1.f),0,1);
 const TArray<FProcMeshTangent> Tangents;
 if(!bCreatedSection){RenderMesh->CreateMeshSection_LinearColor(0,Positions,Data->Triangles,Normals,UV,Colors,Tangents,false,false);bCreatedSection=true;}
 else RenderMesh->UpdateMeshSection_LinearColor(0,Positions,Normals,UV,Colors,Tangents,false);
}
float UWardrobeStockingComponent::GetMaximumStretch() const{float Max=0;for(float V:Stretch)Max=FMath::Max(Max,V);return Max;}
void UWardrobeStockingComponent::TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* F){
 Super::TickComponent(Dt,T,F);auto* O=Cast<AWardrobeShowcase>(GetOwner());const auto* P=O?O->GetInteractionProfile():nullptr;
 if(!P||!P->Stockings.bEnabled||!Data||Data!=P->Stockings.Surface||!Body||Data->BodyMesh!=Body->GetSkeletalMeshAsset()){RefreshSurface();if(!Data)return;}
 Settings=P->Stockings;UpdateMaterial();Simulate(Dt);
}

bool AWardrobeShowcase::BeginStockingPull(const FVector& O,const FVector& D){return Stockings&&Stockings->BeginPull(O,D);}
bool AWardrobeShowcase::IsStockingDragging() const{return Stockings&&Stockings->IsDragging();}


