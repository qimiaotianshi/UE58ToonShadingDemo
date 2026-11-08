#include "WardrobeFootstepSurfaceRegion.h"

#include "Components/BoxComponent.h"

AWardrobeFootstepSurfaceRegion::AWardrobeFootstepSurfaceRegion(){
 PrimaryActorTick.bCanEverTick=false;
 Bounds=CreateDefaultSubobject<UBoxComponent>(TEXT("SurfaceBounds"));SetRootComponent(Bounds);
 Bounds->InitBoxExtent(FVector(300,300,30));
 Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Bounds->SetGenerateOverlapEvents(false);Bounds->SetHiddenInGame(true);
}
bool AWardrobeFootstepSurfaceRegion::ContainsContact(FVector WorldLocation) const {
 if(!Bounds||WorldLocation.ContainsNaN())return false;
 const FVector Local=Bounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
 const FVector Extent=Bounds->GetUnscaledBoxExtent();
 // A small surface tolerance handles feet touching the waterline, but a bridge above it
 // cannot match. A contact below the bottom is too deep for this shallow-water region.
 return FMath::Abs(Local.X)<=Extent.X&&FMath::Abs(Local.Y)<=Extent.Y&&Local.Z>=-Extent.Z&&Local.Z<=Extent.Z+8.f;
}
