#include "WardrobeLocomotion.h"

#include "WardrobeMovementAudio.h"
#include "WardrobeFootstepSurfaceRegion.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

TEnumAsByte<EPhysicalSurface> AWardrobeExplorer::DetectFootstepSurface(bool LeftFoot,FVector& ContactLocation) const {
 const FVector Feet=GetActorLocation()-FVector(0,0,GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
 FVector Foot=Feet;
 if(GetMesh()&&Settings&&Settings->MovementAudio){
  const FName Bone=LeftFoot?Settings->MovementAudio->LeftFootBone:Settings->MovementAudio->RightFootBone;
  if(GetMesh()->GetBoneIndex(Bone)!=INDEX_NONE)Foot=GetMesh()->GetSocketLocation(Bone);
 }
 ContactLocation=FVector(Foot.X,Foot.Y,Feet.Z);
 if(!GetWorld()||Foot.ContainsNaN())return SurfaceType_Default;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(WardrobeFootstepSurface),false,this);Params.bReturnPhysicalMaterial=true;Params.bReturnFaceIndex=true;
 const FVector Start(Foot.X,Foot.Y,FMath::Max(Foot.Z,Feet.Z)+40);
 const FVector End(Foot.X,Foot.Y,FMath::Min(Foot.Z,Feet.Z)-100);
 FHitResult Hit;
 EPhysicalSurface Surface=SurfaceType_Default;
 auto SurfaceForHit=[](const FHitResult& Result){
  const UPhysicalMaterial* Material=Result.PhysMaterial.Get();
  auto* Component=Result.GetComponent();
  EPhysicalSurface Type=UPhysicalMaterial::DetermineSurfaceType(Material);
  if(Type!=SurfaceType_Default)return Type;
  if(Component)if(auto* Body=Component->GetBodyInstance(Result.BoneName))Type=UPhysicalMaterial::DetermineSurfaceType(Body->GetSimplePhysicalMaterial());
  if(Type!=SurfaceType_Default)return Type;
  // Imported meshes may retain a default body material despite a configured material
  // instance override. Prefer the actual collision face; one-slot meshes are unambiguous.
  UMaterialInterface* FaceMaterial=nullptr;int32 Section=INDEX_NONE;
  if(Component&&Result.FaceIndex!=INDEX_NONE)FaceMaterial=Component->GetMaterialFromCollisionFaceIndex(Result.FaceIndex,Section);
  if(!FaceMaterial)if(auto* Mesh=Cast<UMeshComponent>(Component))if(Mesh->GetNumMaterials()==1)FaceMaterial=Mesh->GetMaterial(0);
  return FaceMaterial?UPhysicalMaterial::DetermineSurfaceType(FaceMaterial->GetPhysicalMaterial()):SurfaceType_Default;
 };
 if(GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Params)){
  ContactLocation=Hit.ImpactPoint;
  Surface=SurfaceForHit(Hit);
  // Some imported road meshes expose their material only through complex collision.
  // Never replace a known simple surface with a different hit farther below it.
  if(Surface==SurfaceType_Default){
   Params.bTraceComplex=true;FHitResult Complex;
   if(GetWorld()->LineTraceSingleByChannel(Complex,Start,End,ECC_Visibility,Params)&&FVector::DistSquared(Complex.ImpactPoint,Hit.ImpactPoint)<25.f)
    Surface=SurfaceForHit(Complex);
  }
 }
 // Check the ground contact, not the capsule center: stacked floors/bridges stay dry.
 const AWardrobeFootstepSurfaceRegion* Best=nullptr;double BestVolume=TNumericLimits<double>::Max();
 for(TActorIterator<AWardrobeFootstepSurfaceRegion> It(GetWorld());It;++It){
  if(!IsValid(*It)||!It->ContainsContact(ContactLocation))continue;
  const FVector Extent=It->Bounds->GetScaledBoxExtent();const double Volume=Extent.X*Extent.Y*Extent.Z;
  if(!Best||It->Priority>Best->Priority||(It->Priority==Best->Priority&&Volume<BestVolume)){Best=*It;BestVolume=Volume;}
 }
 return Best?Best->Surface.GetValue():Surface;
}
