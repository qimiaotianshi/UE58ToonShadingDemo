#include "SequenceCharacterRig.h"
#include "ControlRigObjectBinding.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
void USequenceCharacterRig::Initialize(bool bInitRigUnits){
 FRigElementInitializationOptions Options;Options.bGenerateBoneControls=false;Options.bImportCurves=false;
 if(GetObjectBinding().IsValid())if(auto* Component=Cast<USkeletalMeshComponent>(GetObjectBinding()->GetBoundObject()))if(auto* Mesh=Component->GetSkeletalMeshAsset()){
  for(const auto& Morph:Mesh->GetMorphTargets())if(Morph)Options.CurveNames.AddUnique(Morph->GetFName());
 }
 SetInitializationOptions(Options);Super::Initialize(bInitRigUnits);
 // Reference bones only pass through the incoming pose; no transform controls are generated.
}
