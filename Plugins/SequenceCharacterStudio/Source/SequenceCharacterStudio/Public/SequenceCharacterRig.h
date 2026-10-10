#pragma once
#include "CoreMinimal.h"
#include "Rigs/FKControlRig.h"
#include "SequenceCharacterRig.generated.h"
/** Morph-only control rig. Body animation is authored using Unreal's own Control Rig tracks. */
UCLASS(Blueprintable, meta=(DisplayName="BlendShape Controls"))
class SEQUENCECHARACTERSTUDIO_API USequenceCharacterRig : public UFKControlRig {
 GENERATED_BODY()
public:
 virtual void Initialize(bool bInitRigUnits=true) override;
 // Use a final animation layer. Execute still replaces morph values, so its delta
 // relative to input curves yields the authored absolute expression at full weight.
 virtual bool IsAdditive() const override {return true;}
};
