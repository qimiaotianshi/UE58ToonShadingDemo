#pragma once
#include "CoreMinimal.h"

namespace WardrobeAISpeech {
 /** Relative paths are based on the UE project, never the process working directory.
  * An AI folder may contain the portable bundle itself or one/two wrapper folders.
  * Ambiguous bundles must be selected explicitly. */
 FString ResolveTTSRoot(const FString& ConfiguredRoot,const FString& ProjectDirectory,FString& Error);
}
