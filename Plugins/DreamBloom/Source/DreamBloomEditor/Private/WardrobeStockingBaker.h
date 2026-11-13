#pragma once

#include "CoreMinimal.h"

class USkeletalMesh;
class UWardrobeStockingSurface;
class SWindow;

/** Native editor entry points; the bake itself is shared with Blueprint and Python. */
namespace WardrobeStockingBaker {
void Startup();
void Shutdown();
void RegisterMenus();
TSharedPtr<SWindow> OpenWindow(USkeletalMesh* InitialStocking=nullptr);
bool Validate(USkeletalMesh* Stocking,USkeletalMesh* Body,const FString& PackagePath,bool AllowOverwrite,FText& Error);
UWardrobeStockingSurface* Bake(USkeletalMesh* Stocking,USkeletalMesh* Body,const FString& PackagePath,bool AllowOverwrite,FText& Error);
}
