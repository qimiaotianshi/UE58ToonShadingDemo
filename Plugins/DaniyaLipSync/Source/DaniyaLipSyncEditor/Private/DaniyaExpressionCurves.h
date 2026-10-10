#pragma once
#include "CoreMinimal.h"
class UMetaHumanPerformance;
class USkeletalMesh;
class UAnimSequence;
namespace DaniyaExpressions
{
    TMap<FName,float> MapFrame(const TMap<FString,float>& Controls, float Gain);
    TArray<FName> SupportedMorphs(USkeletalMesh* Mesh);
    bool Bake(UMetaHumanPerformance* Performance, USkeletalMesh* Mesh, UAnimSequence* Animation, float Gain, bool bSmooth, TArray<FName>& Written, FString& Error);
}
