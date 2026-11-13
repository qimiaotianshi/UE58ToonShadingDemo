#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeStocking.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStockingNumericsTest,"DreamBloom.Stockings.PickingAndSpring",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStockingNumericsTest::RunTest(const FString&){
 double T=0;TestTrue(TEXT("Front triangle hit"),WardrobeStocking::RayTriangle(FVector(.2,.2,4),FVector(0,0,-1),FVector::ZeroVector,FVector(1,0,0),FVector(0,1,0),T));
 TestEqual(TEXT("Hit depth"),T,4.0);TestFalse(TEXT("Ray outside triangle misses"),WardrobeStocking::RayTriangle(FVector(2,2,4),FVector(0,0,-1),FVector::ZeroVector,FVector(1,0,0),FVector(0,1,0),T));
 TestFalse(TEXT("Parallel ray misses"),WardrobeStocking::RayTriangle(FVector(.2,.2,4),FVector(1,0,0),FVector::ZeroVector,FVector(1,0,0),FVector(0,1,0),T));
 FVector P=FVector::ZeroVector,V=FVector::ZeroVector;for(int32 I=0;I<120;I++)WardrobeStocking::SpringStep(P,V,FVector(10,0,0),5,.65f,1.f/60);
 TestTrue(TEXT("Held spring follows target"),P.Equals(FVector(10,0,0),.001));
 WardrobeStocking::SpringStep(P,V,FVector::ZeroVector,11,.85f,1.f/60);TestTrue(TEXT("Release is continuous"),P.X>0&&P.X<10);
 for(int32 I=0;I<120;I++)WardrobeStocking::SpringStep(P,V,FVector::ZeroVector,11,.85f,1.f/60);
 TestTrue(TEXT("Release recovers neutral"),P.IsNearlyZero(.001));
 WardrobeStocking::SpringStep(P,V,FVector(1,0,0),5,.65f,std::numeric_limits<float>::quiet_NaN());TestFalse(TEXT("Invalid delta time cannot corrupt spring"),P.ContainsNaN());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStockingAssetTest,"DreamBloom.Stockings.DaniyaSkinPullRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStockingAssetTest::RunTest(const FString&){
 auto* Surface=LoadObject<UWardrobeStockingSurface>(nullptr,TEXT("/Game/Wardrobe/Stockings/DA_DaniyaStockingSurface.DA_DaniyaStockingSurface"));
 auto* Profiles=LoadObject<UWardrobeCharacterData>(nullptr,TEXT("/Game/Wardrobe/DA_WardrobeCharacters.DA_WardrobeCharacters"));
 if(!TestNotNull(TEXT("Baked independent textile"),Surface)||!TestNotNull(TEXT("Configured showcase DA"),Profiles))return false;
 TestTrue(TEXT("Both Daniya showcase profiles enabled"),Profiles->Characters.Num()==2&&Profiles->Characters[0].Stockings.bEnabled&&Profiles->Characters[1].Stockings.bEnabled);
 TestTrue(TEXT("Baked skin has dense geometry"),Surface->Vertices.Num()>4000&&Surface->Triangles.Num()>12000);
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeShowcase>();A->Character->SetSkeletalMeshAsset(Surface->BodyMesh);A->Character->SetLightingChannels(false,false,true);A->Character->RefreshBoneTransforms();
 auto* TestProfiles=NewObject<UWardrobeCharacterData>();TestProfiles->Characters.SetNum(1);TestProfiles->Characters[0].Stockings=Profiles->Characters[0].Stockings;
 A->InteractionData=TestProfiles;A->InteractionProfileIndex=0;A->SetInteractionEnabled(true);
 auto* C=A->Stockings.Get();const bool Ready=C->RefreshSurface();TestTrue(TEXT("Surface binds to real character bones"),Ready);
 if(!Ready){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 auto* Textile=A->FindComponentByClass<UProceduralMeshComponent>();
 TestTrue(TEXT("Textile preserves character lighting channel 2"),Textile&&!Textile->LightingChannels.bChannel0&&!Textile->LightingChannels.bChannel1&&Textile->LightingChannels.bChannel2);
 if(Textile){
  const auto* Section=Textile->GetProcMeshSection(0);
  if(TestNotNull(TEXT("Rendered textile section exists"),Section)){
   int32 Outward=0;bool CheckedFresnel=false;
   for(int32 V=0;V<Section->ProcVertexBuffer.Num();++V){
    const FVector N=Section->ProcVertexBuffer[V].Normal.GetSafeNormal(),Authored=Surface->Vertices[V].Normal.GetSafeNormal();
    const float Facing=FVector::DotProduct(N,Authored);Outward+=Facing>0;
    if(!CheckedFresnel&&Surface->Vertices[V].Position.Z>30&&Surface->Vertices[V].Position.Z<45&&Facing>.9f){
     auto Fresnel=[N](const FVector& View){return .04f+.96f*FMath::Pow(1.f-FMath::Max(0.f,float(FVector::DotProduct(N,View))),5.f);};
     TestTrue(TEXT("Front-facing stocking Fresnel stays near its base reflectance"),Fresnel(Authored)<.1f);
     TestTrue(TEXT("Grazing stocking Fresnel rises instead of remaining constant"),Fresnel(FVector::CrossProduct(N,FVector::UpVector).GetSafeNormal())>.9f);
     CheckedFresnel=true;
    }
   }
   TestTrue(TEXT("Reconstructed textile normals point along the authored skin normals"),Outward>Section->ProcVertexBuffer.Num()*.95f);
   TestTrue(TEXT("A calf vertex supplies a valid Fresnel normal"),CheckedFresnel);
  }
 }
 if(Textile){
  auto* Authored=TestProfiles->Characters[0].Stockings.Material.Get();
  TestNotNull(TEXT("Profile has an authored stocking material"),Authored);
  TestTrue(TEXT("Textile uses the authored material without runtime overrides"),Textile->GetMaterial(0)==Authored);
  auto* Alternate=UMaterialInstanceDynamic::Create(Authored,TestProfiles);
  Alternate->SetScalarParameterValue(TEXT("BaseOpacity"),.42f);
  TestProfiles->Characters[0].Stockings.Material=Alternate;C->RefreshSurface();
  TestTrue(TEXT("Changing the profile material updates the textile"),Textile->GetMaterial(0)==Alternate);
  C->TickComponent(1.f/60,ELevelTick::LEVELTICK_All,nullptr);
  TestTrue(TEXT("Gameplay preserves custom material opacity"),FMath::IsNearlyEqual(Alternate->K2_GetScalarParameterValue(TEXT("BaseOpacity")),.42f));
  TestProfiles->Characters[0].Stockings.Material=nullptr;C->RefreshSurface();
  TestFalse(TEXT("Clearing the profile material removes the previous override"),Textile->GetMaterial(0)==Alternate);
  TestProfiles->Characters[0].Stockings.Material=Authored;C->RefreshSurface();
 }
 TestFalse(TEXT("Off-character ray misses"),C->BeginPull(FVector(1000,1000,1000),FVector::ForwardVector));
 FVector Origin,Ray,Point;int32 Face=INDEX_NONE;
 const auto& Rest=C->GetRestPositions();
 for(int32 I=0;I<Surface->Triangles.Num();I+=3){
  const FVector P=(Rest[Surface->Triangles[I]]+Rest[Surface->Triangles[I+1]]+Rest[Surface->Triangles[I+2]])/3;
  if(P.Z<30||P.Z>45)continue;
  FVector N=FVector::CrossProduct(Rest[Surface->Triangles[I+1]]-Rest[Surface->Triangles[I]],Rest[Surface->Triangles[I+2]]-Rest[Surface->Triangles[I]]).GetSafeNormal();
  if(N.IsNearlyZero())continue;
  Origin=P+N*40;Ray=-N;if(C->BeginPull(Origin,Ray)){Face=I;Point=P;break;}
 }
 TestTrue(TEXT("Visible calf can be picked"),Face!=INDEX_NONE);
 if(Face!=INDEX_NONE){
  FVector Tangent=FVector::CrossProduct(Ray,FVector::UpVector).GetSafeNormal();if(Tangent.IsNearlyZero())Tangent=FVector::RightVector;
  C->UpdatePull(Origin+Tangent*12,Ray);for(int32 I=0;I<90;I++)C->Simulate(1.f/60);
  TestTrue(TEXT("Textile pulls away from the skin"),C->GetPullDistance()>2);TestTrue(TEXT("Pull remains limited"),C->GetPullDistance()<=TestProfiles->Characters[0].Stockings.MaximumPull+.01f);
  TestTrue(TEXT("Real edge elongation drives transparency"),C->GetMaximumStretch()>.05f);
  int32 Unaffected=0;for(int32 V=0;V<Rest.Num();V++)if(C->GetInfluences()[V]==0){Unaffected++;TestTrue(TEXT("Outside patch keeps skinned position"),C->GetPositions()[V].Equals(C->GetRestPositions()[V],.0001));if(Unaffected>20)break;}
  TestTrue(TEXT("Only a connected local patch moves"),Unaffected>20);
  C->ReleasePull();TestFalse(TEXT("Release ends pointer gesture"),C->IsDragging());TestTrue(TEXT("Release retains recovering surface"),C->HasPull());
  for(int32 I=0;I<240;I++)C->Simulate(1.f/60);
  TestFalse(TEXT("Recovery completes"),C->HasPull());float Error=0;for(int32 V=0;V<Rest.Num();V++)Error=FMath::Max(Error,float(FVector::Dist(C->GetPositions()[V],C->GetRestPositions()[V])));
  TestTrue(TEXT("No permanent holes or deformation"),Error<.001f);TestEqual(TEXT("Stretch clears after release"),C->GetMaximumStretch(),0.f);
  TestTrue(TEXT("Can regrab after recovery"),C->BeginPull(Origin,Ray));A->SetInteractionEnabled(false);C->Simulate(1.f/60);TestFalse(TEXT("Changing station cancels grab"),C->HasPull());
  A->SetInteractionEnabled(true);TestProfiles->Characters[0].Stockings.bEnabled=false;TestFalse(TEXT("DA opt-out disables rendering and picking"),C->RefreshSurface());
  TestTrue(TEXT("Disabled textile is hidden"),Textile&&!Textile->IsVisible());
 }
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
