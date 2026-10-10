#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

AWardrobeShowcase::AWardrobeShowcase(){
 PrimaryActorTick.bCanEverTick=true;
 Character=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Daniya"));SetRootComponent(Character);
 Character->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Character->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}
void AWardrobeShowcase::BeginPlay(){
 Super::BeginPlay();InitialAnimClass=Character->GetAnimClass();
 if(!MenuData)MenuData=LoadObject<UWardrobeMenuData>(nullptr,TEXT("/Game/Wardrobe/DA_WardrobeMenu.DA_WardrobeMenu"));
 BuildInteractionRegions();
}
void AWardrobeShowcase::EndPlay(const EEndPlayReason::Type Reason){bEndingShowcase=true;StopDefaultSequence();ClearPinch();StopPhysicsTouch();CancelInteraction(false);Super::EndPlay(Reason);}
void AWardrobeShowcase::Tick(float Dt){Super::Tick(Dt);TickSequencePhysicsBlend(Dt);TickInteraction(Dt);TickPhysicsTouch(Dt);TickPinch(Dt);TickCursorLook(Dt);}
AWardrobePreviewGameMode::AWardrobePreviewGameMode(){DefaultPawnClass=nullptr;}
