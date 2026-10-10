#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SequenceCharacterTools.generated.h"
class ULevelSequence;class USkeletalMeshComponent;class UMovieSceneControlRigParameterTrack;class UAnimSequence;
USTRUCT(BlueprintType)
struct SEQUENCECHARACTERSTUDIOEDITOR_API FSequenceMorphImportResult {
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly,Category="Import") bool bSuccess=false;
 UPROPERTY(BlueprintReadOnly,Category="Import") int32 ImportedCurves=0;
 UPROPERTY(BlueprintReadOnly,Category="Import") int32 WrittenKeys=0;
 UPROPERTY(BlueprintReadOnly,Category="Import") TArray<FName> SkippedCurves;
 UPROPERTY(BlueprintReadOnly,Category="Import") FString Message;
};
UCLASS()
class SEQUENCECHARACTERSTUDIOEDITOR_API USequenceCharacterTools:public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Sequence Character Studio|Editor")
 static UMovieSceneControlRigParameterTrack* AddMorphTrack(ULevelSequence* Sequence,USkeletalMeshComponent* Component);
 UFUNCTION(BlueprintCallable,Category="Sequence Character Studio|Editor")
 static bool KeyMorph(ULevelSequence* Sequence,UMovieSceneControlRigParameterTrack* Track,FName Morph,int32 DisplayFrame,float Value);
 /** Bake animation float curves that match this rig's mesh morphs into editable Sequencer controls.
  * Samples at Sequence display rate, includes the exact source end time. Only matching morph channels
  * in the imported interval are replaced; bone keys and source animation are never modified. Undoable. */
 UFUNCTION(BlueprintCallable,Category="Sequence Character Studio|Editor",meta=(AdvancedDisplay="Gain,bReplaceRange"))
 static FSequenceMorphImportResult ImportAnimationMorphCurves(ULevelSequence* Sequence,UMovieSceneControlRigParameterTrack* Track,UAnimSequence* Animation,int32 StartDisplayFrame=0,float Gain=1.f,bool bReplaceRange=true);
};
