#pragma once
#include "CoreMinimal.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "SequenceMorphSection.generated.h"
/** Standard Control Rig section with ordinary float-property drag behavior. */
UCLASS()
class SEQUENCECHARACTERSTUDIO_API USequenceMorphSection : public UMovieSceneControlRigParameterSection {
 GENERATED_BODY()
public:
 virtual EMovieSceneChannelProxyType CacheChannelProxy() override;
};
