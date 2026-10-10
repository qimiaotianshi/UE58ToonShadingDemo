#include "WardrobeStations.h"
#include "WardrobeShowcase.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

void AWardrobeViewCamera::ApplyView(const FMinimalViewInfo& View){
 AuthoredView=View;SetActorLocationAndRotation(View.Location,View.Rotation);
 auto* C=GetCameraComponent();
 C->SetFieldOfView(View.FOV);C->SetProjectionMode(View.ProjectionMode);C->SetAspectRatio(View.AspectRatio);
 C->SetConstraintAspectRatio(View.bConstrainAspectRatio);C->SetOrthoWidth(View.OrthoWidth);
 C->PostProcessSettings=View.PostProcessSettings;C->PostProcessBlendWeight=View.PostProcessBlendWeight;
}
void AWardrobeViewCamera::CalcCamera(float DeltaTime,FMinimalViewInfo& OutResult){
 GetCameraComponent()->GetCameraView(DeltaTime,OutResult);
}
UWorld* UWardrobeStationController::GetWorld() const{return IsValid(Player)?Player->GetWorld():nullptr;}
bool UWardrobeStationController::ResolveStation(int32 Index){
 if(!CharacterData||!CharacterData->Characters.IsValidIndex(Index)||!Stations.IsValidIndex(Index)||!GetWorld()){
  LastError=FText::FromString(TEXT("请在 DA_WardrobeCharacters 中配置人物条目"));return false;
 }
 const auto& Option=CharacterData->Characters[Index];
 auto Fail=[&](const FString& Reason){LastError=FText::FromString(FString::Printf(TEXT("人物条目 %d：%s"),Index,*Reason));UE_LOG(LogTemp,Warning,TEXT("WARDROBE_TAG_BINDING %s"),*LastError.ToString());return false;};
 if(Option.CameraTag.IsNone()||Option.CharacterTag.IsNone())return Fail(TEXT("Camera Tag 和 Character Tag 不能为空"));
 ACameraActor* Camera=nullptr;AWardrobeShowcase* Character=nullptr;
 int32 Cameras=0,Characters=0;
 // Count all matching actor tags, including wrong actor types: never pick an arbitrary match.
 for(TActorIterator<AActor> It(GetWorld());It;++It){
  if(It->ActorHasTag(Option.CameraTag)){++Cameras;Camera=Cast<ACameraActor>(*It);}
  if(It->ActorHasTag(Option.CharacterTag)){++Characters;Character=Cast<AWardrobeShowcase>(*It);}
 }
 if(Cameras!=1||!Camera)return Fail(FString::Printf(TEXT("Camera Tag '%s' 匹配 %d 个 Actor，需要唯一 CameraActor/CineCameraActor"),*Option.CameraTag.ToString(),Cameras));
 if(Characters!=1||!Character)return Fail(FString::Printf(TEXT("Character Tag '%s' 匹配 %d 个 Actor，需要唯一 WardrobeShowcase"),*Option.CharacterTag.ToString(),Characters));
 for(int32 I=0;I<CharacterData->Characters.Num();++I)if(I!=Index){
  const auto& Other=CharacterData->Characters[I];
  if(Other.CharacterTag==Option.CharacterTag||Character->ActorHasTag(Other.CharacterTag))return Fail(TEXT("同一人物被多个 DA 条目使用，请为每个人物配置独立条目"));
 }
 auto& S=Stations[Index];S.Camera=Camera;S.Character=Character;S.Settings=Option;
 return ValidateStation(Index);
}
float UWardrobeStationController::EvaluateProgress(const UCurveFloat* Curve,float Time){
 const float T=FMath::Clamp(Time,0.f,1.f);
 if(T<=0)return 0;if(T>=1)return 1;
 if(Curve){
  const float Start=Curve->GetFloatValue(0),End=Curve->GetFloatValue(1),Value=Curve->GetFloatValue(T);
  if(FMath::IsFinite(Value)&&FMath::IsFinite(Start)&&FMath::IsFinite(End)&&!FMath::IsNearlyEqual(Start,End))
   return FMath::Clamp((Value-Start)/(End-Start),0.f,1.f);
 }
 return T*T*(3.f-2.f*T);
}
FVector UWardrobeStationController::EvaluatePath(const FVector& A,const FVector& B,const FVector& C,const FVector& D,float T){
 const float S=1.f-T;return S*S*S*A+3.f*S*S*T*B+3.f*S*T*T*C+T*T*T*D;
}
bool UWardrobeStationController::ValidateStation(int32 Index){
 if(!Stations.IsValidIndex(Index)){LastError=FText::FromString(TEXT("请在 DA_WardrobeCharacters 中配置人物条目"));return false;}
 const auto& S=Stations[Index];
 if(!IsValid(S.Camera)||!IsValid(S.Character)||S.Camera->GetWorld()!=GetWorld()||S.Character->GetWorld()!=GetWorld()){
  LastError=FText::FromString(TEXT("此组合的相机或人物未配置，或所在关卡尚未加载"));return false;
 }
 if(!S.Character->Character->GetSkeletalMeshAsset()){
  LastError=FText::FromString(TEXT("此组合的人物尚未在 Character 组件上配置模型"));return false;
 }
 return true;
}
bool UWardrobeStationController::Initialize(APlayerController* Controller,UWardrobeCharacterData* Data,int32 PreferredStation){
 if(!IsValid(Controller))return false;
 if(IsValid(ViewCamera))return true;
 Player=Controller;PreviousViewTarget=Player->GetViewTarget();CharacterData=Data;
 if(!CharacterData||CharacterData->Characters.IsEmpty()){LastError=FText::FromString(TEXT("DA_WardrobeCharacters 没有配置人物条目"));return false;}
 Stations.SetNum(CharacterData->Characters.Num());
 for(TActorIterator<AWardrobeShowcase> It(GetWorld());It;++It)It->SetInteractionEnabled(false);
 FActorSpawnParameters Params;Params.ObjectFlags|=RF_Transient;
 ViewCamera=GetWorld()->SpawnActor<AWardrobeViewCamera>(Params);
 if(!ViewCamera)return false;
 const int32 Initial=Stations.IsValidIndex(PreferredStation)?PreferredStation:CharacterData->InitialCharacter;
 if(SelectStation(Initial,true))return true;
 for(int32 I=0;I<Stations.Num();I++)if(I!=Initial&&SelectStation(I,true))return true;
 ViewCamera->Destroy();ViewCamera=nullptr;return false;
}
bool UWardrobeStationController::SelectStation(int32 Index,bool bInstant){
 LastError=FText::GetEmpty();
 if(bTransitioning||!IsValid(Player)||!IsValid(ViewCamera))return false;
 if(!ResolveStation(Index))return false;
 if(CurrentStation==Index)return true;
 // Restore the old interaction before sampling the current preview view.
 for(auto C:ManagedCharacters)if(IsValid(C))C->SetInteractionEnabled(false);
 TargetStation=Index;
 auto& S=Stations[Index];
 ManagedCharacters.AddUnique(S.Character);
 InitialRotations.FindOrAdd(S.Character,S.Character->GetActorRotation());
 S.Character->SetInteractionEnabled(false);
 S.Character->InteractionData=CharacterData;S.Character->InteractionProfileIndex=Index;S.Character->BuildInteractionRegions();
 S.Camera->GetCameraComponent()->GetCameraView(0,EndView);
 ViewCamera->CalcCamera(0,StartView);
 Control1=FMath::Lerp(StartView.Location,EndView.Location,1.f/3.f)+StartView.Rotation.RotateVector(S.Settings.DepartureOffset);
 Control2=FMath::Lerp(StartView.Location,EndView.Location,2.f/3.f)+EndView.Rotation.RotateVector(S.Settings.ArrivalOffset);
 Player->SetViewTarget(ViewCamera);Elapsed=0;bTransitioning=true;
 if(bInstant||CurrentStation==INDEX_NONE||S.Settings.Duration<=0)FinishTransition();
 return true;
}
void UWardrobeStationController::FinishTransition(){
 if(!ValidateStation(TargetStation)){AbortTransition();return;}
 ViewCamera->ApplyView(EndView);CurrentStation=TargetStation;TargetStation=INDEX_NONE;bTransitioning=false;
 GetActiveCharacter()->SetInteractionEnabled(bPreviewEnabled);
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_STATION_ARRIVED index=%d character=%s camera=%s"),CurrentStation,*GetNameSafe(GetActiveCharacter()),*GetNameSafe(Stations[CurrentStation].Camera));
 OnStationArrived.Broadcast(CurrentStation);
}
void UWardrobeStationController::AbortTransition(){
 bTransitioning=false;TargetStation=INDEX_NONE;
 if(IsValid(ViewCamera))ViewCamera->ApplyView(StartView);
 if(auto* C=GetActiveCharacter())C->SetInteractionEnabled(bPreviewEnabled);
}
void UWardrobeStationController::Tick(float Dt){

 if(!bTransitioning){
  // Cameras with Auto Activate can begin play after the menu. Reassert preview
  // ownership, but never take camera cuts away from an active interaction.
  if(bPreviewEnabled&&IsValid(Player)&&IsValid(ViewCamera)){
   auto* Active=GetActiveCharacter();
   if(Active&&!Active->IsInteracting()){
    if(Stations.IsValidIndex(CurrentStation)&&IsValid(Stations[CurrentStation].Camera)){
     FMinimalViewInfo Live;Stations[CurrentStation].Camera->GetCameraComponent()->GetCameraView(Dt,Live);
     auto* C=ViewCamera->GetCameraComponent();C->PostProcessSettings=Live.PostProcessSettings;C->PostProcessBlendWeight=Live.PostProcessBlendWeight;
    }
    if(Player->GetViewTarget()!=ViewCamera)Player->SetViewTarget(ViewCamera);
   }
  }
  return;
 }
 if(!IsValid(Player)||!IsValid(ViewCamera)||!ValidateStation(TargetStation)){AbortTransition();return;}
 const auto& S=Stations[TargetStation];
 FMinimalViewInfo Live;S.Camera->GetCameraComponent()->GetCameraView(Dt,Live);EndView.PostProcessSettings=Live.PostProcessSettings;EndView.PostProcessBlendWeight=Live.PostProcessBlendWeight;
 Elapsed+=FMath::Max(0.f,Dt);
 const float T=S.Settings.Duration>0?Elapsed/S.Settings.Duration:1.f;
 const float Alpha=EvaluateProgress(S.Settings.ProgressCurve,T);
 FMinimalViewInfo View=StartView;View.BlendViewInfo(EndView,Alpha);
 View.PostProcessSettings=EndView.PostProcessSettings;
 View.PostProcessBlendWeight=FMath::Lerp(StartView.PostProcessBlendWeight,EndView.PostProcessBlendWeight,Alpha);
 auto& PP=View.PostProcessSettings;
 PP.DepthOfFieldFocalDistance=FMath::Lerp(StartView.PostProcessSettings.DepthOfFieldFocalDistance,EndView.PostProcessSettings.DepthOfFieldFocalDistance,Alpha);
 PP.DepthOfFieldFstop=FMath::Lerp(StartView.PostProcessSettings.DepthOfFieldFstop,EndView.PostProcessSettings.DepthOfFieldFstop,Alpha);
 PP.DepthOfFieldSensorWidth=FMath::Lerp(StartView.PostProcessSettings.DepthOfFieldSensorWidth,EndView.PostProcessSettings.DepthOfFieldSensorWidth,Alpha);
 View.Location=EvaluatePath(StartView.Location,Control1,Control2,EndView.Location,Alpha);
 View.Rotation=FQuat::Slerp(StartView.Rotation.Quaternion(),EndView.Rotation.Quaternion(),Alpha).Rotator();
 ViewCamera->ApplyView(View);
 if(CharacterData&&CharacterData->bDrawTransitionPath){
  FVector Previous=StartView.Location;
  for(int32 I=1;I<=32;I++){const FVector Next=EvaluatePath(StartView.Location,Control1,Control2,EndView.Location,I/32.f);DrawDebugLine(GetWorld(),Previous,Next,FColor::Cyan,false,0,0,2);Previous=Next;}
 }
 if(T>=1)FinishTransition();
}
AWardrobeShowcase* UWardrobeStationController::GetActiveCharacter() const{
 if(bTransitioning||!Stations.IsValidIndex(CurrentStation))return nullptr;
 auto* C=Stations[CurrentStation].Character.Get();return IsValid(C)?C:nullptr;
}
ACameraActor* UWardrobeStationController::GetPreviewCamera() const{return IsValid(ViewCamera)?ViewCamera.Get():nullptr;}
void UWardrobeStationController::SetPreviewEnabled(bool Enabled){
 bPreviewEnabled=Enabled;
 auto* Active=GetActiveCharacter();
 for(auto C:ManagedCharacters)if(IsValid(C))C->SetInteractionEnabled(Enabled&&C==Active);
}
bool UWardrobeStationController::CanManipulate() const{return bPreviewEnabled&&!bTransitioning&&GetActiveCharacter()&&!GetActiveCharacter()->IsInteracting();}
bool UWardrobeStationController::CanRotateCharacter() const{return CanManipulate()&&Stations[CurrentStation].Settings.bAllowCharacterRotation;}
void UWardrobeStationController::ResetPreview(){
 if(!CanManipulate()||!ValidateStation(CurrentStation))return;
 Stations[CurrentStation].Camera->GetCameraComponent()->GetCameraView(0,EndView);ViewCamera->ApplyView(EndView);
 auto* C=GetActiveCharacter();if(const auto* Rotation=InitialRotations.Find(C))C->SetActorRotation(*Rotation);
}
void UWardrobeStationController::Shutdown(){
 SetPreviewEnabled(false);
 if(IsValid(Player)&&Player->GetViewTarget()==ViewCamera&&IsValid(PreviousViewTarget))Player->SetViewTarget(PreviousViewTarget);
 if(IsValid(ViewCamera))ViewCamera->Destroy();
 ViewCamera=nullptr;Player=nullptr;ManagedCharacters.Reset();Stations.Reset();CurrentStation=TargetStation=INDEX_NONE;bTransitioning=false;
}
