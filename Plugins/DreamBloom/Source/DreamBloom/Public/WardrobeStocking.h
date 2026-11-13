#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "WardrobeStocking.generated.h"

class USkeletalMesh;
class UMaterialInterface;
class UProceduralMeshComponent;
class USkeletalMeshComponent;

/** Offline skin data. Cooked gameplay never reads discarded skeletal-mesh CPU buffers. */
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeStockingVertex {
 GENERATED_BODY()
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector Position=FVector::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector Normal=FVector::UpVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FVector2D UV=FVector2D::ZeroVector;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<int32> Bones;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TArray<float> Weights;
};

UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeStockingSurface : public UDataAsset {
 GENERATED_BODY()
public:
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Source") TObjectPtr<USkeletalMesh> StockingMesh;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Source") TObjectPtr<USkeletalMesh> BodyMesh;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Geometry") TArray<FWardrobeStockingVertex> Vertices;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Geometry") TArray<int32> Triangles;
 /** Opaque skirt, cape and shoes prevent grabbing fabric through the costume. */
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Geometry") TArray<FWardrobeStockingVertex> OccluderVertices;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Geometry") TArray<int32> OccluderTriangles;
};

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeStockingSettings {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stockings") bool bEnabled=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stockings",meta=(EditCondition="bEnabled")) TObjectPtr<UWardrobeStockingSurface> Surface;
 /** Appearance is authored in this material or material instance; gameplay only supplies vertex strain/pull. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stockings",meta=(EditCondition="bEnabled")) TObjectPtr<UMaterialInterface> Material;
 /** Optional fitting clearance, applied along the skinned normal. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Stockings",meta=(ClampMin="0",ClampMax="2",Units="cm")) float SurfaceClearance=.05f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pull",meta=(ClampMin="1",ClampMax="40",Units="cm")) float InfluenceRadius=16;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pull",meta=(ClampMin=".1",ClampMax="30",Units="cm")) float MaximumPull=16;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pull",meta=(ClampMin=".1",ClampMax="2")) float DragSensitivity=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pull",meta=(ClampMin=".1",ClampMax="1")) float FootPullRatio=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pull",meta=(ClampMin=".1",ClampMax="2",Units="cm")) float PinchTipRadius=.4f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pull",meta=(ClampMin="1",ClampMax="3")) float PinchSharpness=2;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pull",meta=(ClampMin="0",ClampMax="1")) float FootGather=.85f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spring",meta=(ClampMin="1",ClampMax="20")) float FollowFrequency=5;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spring",meta=(ClampMin=".2",ClampMax="2")) float FollowDamping=.65f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spring",meta=(ClampMin="1",ClampMax="20")) float ReleaseFrequency=11;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Spring",meta=(ClampMin=".2",ClampMax="2")) float ReleaseDamping=.85f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Debug") bool bDrawGrab=false;
};

namespace WardrobeStocking {
 DREAMBLOOM_API bool RayTriangle(const FVector& Origin,const FVector& Ray,const FVector& A,const FVector& B,const FVector& C,double& Distance);
 DREAMBLOOM_API float PinchWeight(float Distance,float Radius,float Tip,float Sharpness);
 DREAMBLOOM_API void SpringStep(FVector& Position,FVector& Velocity,const FVector& Target,float Frequency,float Damping,float Dt);
}

/** A separately rendered textile surface driven by the character's final animation pose. */
UCLASS(ClassGroup=(Wardrobe),meta=(BlueprintSpawnableComponent))
class DREAMBLOOM_API UWardrobeStockingComponent : public UActorComponent {
 GENERATED_BODY()
public:
 UWardrobeStockingComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
 UFUNCTION(BlueprintCallable,Category="Stockings") bool RefreshSurface();
 UFUNCTION(BlueprintCallable,Category="Stockings") bool BeginPull(const FVector& WorldOrigin,const FVector& WorldRay);
 UFUNCTION(BlueprintCallable,Category="Stockings") void UpdatePull(const FVector& WorldOrigin,const FVector& WorldRay);
 UFUNCTION(BlueprintCallable,Category="Stockings") void ReleasePull();
 UFUNCTION(BlueprintCallable,Category="Stockings") void ResetPull();
 UFUNCTION(BlueprintPure,Category="Stockings") bool IsDragging() const{return bDragging;}
 UFUNCTION(BlueprintPure,Category="Stockings") bool HasPull() const{return Anchor!=INDEX_NONE;}
 UFUNCTION(BlueprintPure,Category="Stockings") float GetPullDistance() const{return Offset.Size();}
 UFUNCTION(BlueprintPure,Category="Stockings") float GetMaximumStretch() const;
 const TArray<FVector>& GetPositions() const{return Positions;}
 const TArray<FVector>& GetRestPositions() const{return SkinnedRest;}
 const TArray<float>& GetInfluences() const{return Influence;}
 /** Also used by deterministic editor automation against the production simulation. */
 UFUNCTION(BlueprintCallable,Category="Stockings") void Simulate(float Dt);
private:
 UPROPERTY(Transient) TObjectPtr<UWardrobeStockingSurface> Data;
 UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
 UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> RenderMesh;
 FWardrobeStockingSettings Settings;
 TArray<TArray<int32>> Neighbors;
 TArray<int32> NormalWeld;
 /** Preserve each imported face's orientation relative to its authored outward normals. */
 TArray<int8> FaceNormalSigns;
 TArray<FMatrix> BoneMatrices;
 TArray<FVector> SkinnedRest,Positions,Normals,RestNormals,OccluderPositions;
 TArray<FVector2D> UV;
 TArray<FLinearColor> Colors;
 TArray<float> Influence,Distances,Stretch;
 TArray<int32> Active;
 FVector Offset=FVector::ZeroVector,Velocity=FVector::ZeroVector,Target=FVector::ZeroVector;
 FVector GrabLocal=FVector::ZeroVector,NormalLocal=FVector::UpVector;
 FVector PlaneWorldPoint=FVector::ZeroVector,PlaneWorldNormal=FVector::ForwardVector;
 int32 Anchor=INDEX_NONE;
 float RegionBlend=1,PullLimit=16,PinchRadius=16;
 bool bDragging=false,bCreatedSection=false;
 bool ValidateData() const;
 void BuildNeighbors();
 void SkinVertices(const TArray<FWardrobeStockingVertex>& Source,TArray<FVector>& Result);
 void UpdateSkin();
 void UpdateGeometry();
 void UpdateMaterial();
};

