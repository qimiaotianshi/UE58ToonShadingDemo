#include "WardrobeStory.h"
#include "WardrobeLoading.h"
#include "WardrobeMenu.h"
#include "WardrobeShowcase.h"
#include "WardrobeLocomotion.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceDirector.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Misc/PackageName.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Fonts/CompositeFont.h"
#if WITH_EDITOR
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#endif

namespace {
TSoftObjectPtr<UWorld> StoryWorldAsset(UWorld* World){
 if(!World)return {};
 const FString Package=UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
 return TSoftObjectPtr<UWorld>(FSoftObjectPath(Package+TEXT(".")+FPackageName::GetLongPackageAssetName(Package)));
}
bool StoryWorldMatches(UWorld* World,const TSoftObjectPtr<UWorld>& Level){
 return World&&StoryWorldAsset(World).ToSoftObjectPath().GetLongPackageName()==Level.ToSoftObjectPath().GetLongPackageName();
}
FText StoryText(const TCHAR* Text){return FText::FromString(Text);}
}

TArray<FString> UWardrobeStoryFlow::ValidateConfiguration() const {
 TArray<FString> Errors;TSet<FName> Ids;
 if(Steps.IsEmpty())Errors.Add(TEXT("演出列表为空"));
 if(SaveNamespace.IsEmpty()||SaveNamespace.Len()>64||SaveNamespace.Contains(TEXT("/"))||SaveNamespace.Contains(TEXT("\\"))||SaveNamespace.Contains(TEXT(".."))||SaveNamespace.Contains(TEXT(":"))||SaveNamespace.Contains(TEXT("*"))||SaveNamespace.Contains(TEXT("?"))||SaveNamespace.Contains(TEXT("\""))||SaveNamespace.Contains(TEXT("<"))||SaveNamespace.Contains(TEXT(">"))||SaveNamespace.Contains(TEXT("|")))Errors.Add(TEXT("Save Namespace 需为稳定的简单名称，不能包含路径"));
 for(int32 I=0;I<Steps.Num();++I){const auto& Step=Steps[I];
  const FString Prefix=FString::Printf(TEXT("演出 %d："),I+1);
  if(Step.StoryId.IsNone()||Ids.Contains(Step.StoryId))Errors.Add(Prefix+TEXT("Story ID 为空或重复"));
  Ids.Add(Step.StoryId);
  if(Step.TriggerId.IsNone())Errors.Add(Prefix+TEXT("Trigger ID 为空"));
  if(Step.Level.IsNull()||!FPackageName::DoesPackageExist(Step.Level.ToSoftObjectPath().GetLongPackageName()))Errors.Add(Prefix+TEXT("关卡不存在"));
  if(Step.Sequence.IsNull()||!FPackageName::DoesPackageExist(Step.Sequence.ToSoftObjectPath().GetLongPackageName()))Errors.Add(Prefix+TEXT("Sequence 不存在"));
 }
 return Errors;
}

AWardrobeStoryTrigger::AWardrobeStoryTrigger(){
 Box=CreateDefaultSubobject<UBoxComponent>(TEXT("StoryArea"));SetRootComponent(Box);
 Box->SetBoxExtent(FVector(150,150,150));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Box->SetCollisionResponseToAllChannels(ECR_Ignore);Box->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
 Box->SetGenerateOverlapEvents(true);Box->SetHiddenInGame(true);
}
bool AWardrobeStoryTrigger::ContainsPlayer(APawn* Pawn) const{
 if(!IsValid(Pawn)||!Box||!Box->IsQueryCollisionEnabled())return false;
 // Query actual shapes rather than the event overlap cache: saved PlayerStarts and streamed boxes
 // must also work before the first overlap notification, and do not require pawn overlap events.
 if(auto* Character=Cast<ACharacter>(Pawn)){
  const auto* Capsule=Character->GetCapsuleComponent();
  return Box->OverlapComponent(Capsule->GetComponentLocation(),Capsule->GetComponentQuat(),FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()));
 }
 return Box->IsOverlappingActor(Pawn);
}
void AWardrobeStoryTrigger::BeginPlay(){
 Super::BeginPlay();Box->OnComponentBeginOverlap.AddDynamic(this,&AWardrobeStoryTrigger::OnEntered);
 if(GetGameInstance())if(auto* Story=GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>())if(!Story->Flow&&!StoryFlow.IsNull())Story->Configure(StoryFlow.LoadSynchronous());
}
void AWardrobeStoryTrigger::OnEntered(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&){
 if(Other!=UGameplayStatics::GetPlayerPawn(this,0))return;
 if(GetGameInstance())if(auto* Story=GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>())Story->TryTrigger(this);
}

class SWardrobeStoryOverlay : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SWardrobeStoryOverlay){} SLATE_ARGUMENT(UWardrobeStorySubsystem*,Story) SLATE_END_ARGS()
 void Construct(const FArguments& Args){
  Story=Args._Story;
  Font=MakeShared<FCompositeFont>(NAME_None,IPluginManager::Get().FindPlugin(TEXT("DreamBloom"))->GetBaseDir()/TEXT("Resources/NotoSansSC-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  ChildSlot[SNew(SOverlay)+SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,60)
   [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))).BorderBackgroundColor(FLinearColor(.07f,.025f,.05f,.7f)).Padding(FMargin(28,14))
    .Visibility_Lambda([this](){return Story.IsValid()&&Story->IsWaitingForClick()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})
    [SNew(STextBlock).Font(FSlateFontInfo(Font,20)).ColorAndOpacity(FLinearColor(1,.85f,.92f,1))
     .Text_Lambda([this](){return Story.IsValid()&&Story->Flow?Story->Flow->ContinuePrompt:StoryText(TEXT("点击屏幕继续"));})]]];
 }
 virtual bool SupportsKeyboardFocus() const override{return true;}
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent& Event) override{
  if(Event.GetEffectingButton()==EKeys::LeftMouseButton&&Story.IsValid())Story->ClickContinue();
  return FReply::Handled();
 }
 virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override {if(Story.IsValid())Story->ArmContinueClick();return FReply::Handled();}
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent&) override{return FReply::Handled();}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& Event) override{
  if(Story.IsValid()&&!Event.IsRepeat()&&(Event.GetKey()==EKeys::SpaceBar||Event.GetKey()==EKeys::Enter))Story->ClickContinue();
  return FReply::Handled();
 }
 virtual FReply OnKeyUp(const FGeometry&,const FKeyEvent&) override {if(Story.IsValid())Story->ArmContinueClick();return FReply::Handled();}
 virtual FReply OnKeyChar(const FGeometry&,const FCharacterEvent&) override{return FReply::Handled();}
private:
 TWeakObjectPtr<UWardrobeStorySubsystem> Story;TSharedPtr<FCompositeFont> Font;
};

void UWardrobeStorySubsystem::Initialize(FSubsystemCollectionBase& Collection){
 Super::Initialize(Collection);Collection.InitializeDependency<UWardrobeLoadingSubsystem>();
 TravelHandle=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>()->OnTravelCompleted.AddWeakLambda(this,[this](bool Success){
  if(!Success&&bRestorePending){bRestorePending=false;LastError=StoryText(TEXT("地图加载失败，请重新选择存档"));}
 });
 if(!IsRunningCommandlet()&&!IsRunningDedicatedServer()){
  TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UWardrobeStorySubsystem::Tick));
  CleanupHandle=FWorldDelegates::OnWorldCleanup.AddUObject(this,&UWardrobeStorySubsystem::OnWorldCleanup);
 }
}
void UWardrobeStorySubsystem::Deinitialize(){
 SaveCheckpoint();CancelCinematic();FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
 GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>()->OnTravelCompleted.Remove(TravelHandle);Save=nullptr;Flow=nullptr;Super::Deinitialize();
}
bool UWardrobeStorySubsystem::Configure(UWardrobeStoryFlow* InFlow){
 if(!InFlow||bActive)return false;
 const auto Errors=InFlow->ValidateConfiguration();if(!Errors.IsEmpty()){LastError=FText::FromString(FString::Join(Errors,TEXT("\n")));UE_LOG(LogTemp,Warning,TEXT("WARDROBE_STORY_CONFIG %s"),*LastError.ToString());return false;}
 if(Flow!=InFlow){Save=nullptr;ActiveSlot=INDEX_NONE;bRestorePending=false;}Flow=InFlow;LastError=FText::GetEmpty();return true;
}
FString UWardrobeStorySubsystem::SlotName(int32 Index) const{return Flow?FString::Printf(TEXT("%s_%02d"),*Flow->SaveNamespace,Index):FString();}
int32 UWardrobeStorySubsystem::CurrentStepIndex() const {
 if(!Flow||!Save||Save->bStoryComplete)return INDEX_NONE;
 return Flow->Steps.IndexOfByPredicate([this](const FWardrobeStoryStep& Step){return Step.StoryId==Save->NextStoryId;});
}
FName UWardrobeStorySubsystem::GetCurrentStoryId() const {return Save&&!Save->bStoryComplete?Save->NextStoryId:NAME_None;}
bool UWardrobeStorySubsystem::IsSaveUsable(const UWardrobeStorySave* Candidate) const{
 if(!Candidate||!Flow||Candidate->FormatVersion!=1||Candidate->FlowPath!=Flow->GetPathName()||Candidate->Level.IsNull()||!FPackageName::DoesPackageExist(Candidate->Level.ToSoftObjectPath().GetLongPackageName()))return false;
 if(Candidate->bHasPlayerTransform&&(Candidate->PlayerTransform.ContainsNaN()||!Candidate->PlayerTransform.GetRotation().IsNormalized()||Candidate->ControlRotation.ContainsNaN()))return false;
 return Candidate->bStoryComplete||Flow->Steps.ContainsByPredicate([Candidate](const FWardrobeStoryStep& Step){return Step.StoryId==Candidate->NextStoryId;});
}
TArray<FWardrobeStorySlot> UWardrobeStorySubsystem::GetSaveSlots() const{
 TArray<FWardrobeStorySlot> Result;if(!Flow)return Result;
 for(int32 I=0;I<FMath::Clamp(Flow->SaveSlotCount,1,16);++I){FWardrobeStorySlot Info;Info.Index=I;Info.bExists=UGameplayStatics::DoesSaveGameExist(SlotName(I),0);
  if(Info.bExists){const auto* Stored=Cast<UWardrobeStorySave>(UGameplayStatics::LoadGameFromSlot(SlotName(I),0));Info.bValid=IsSaveUsable(Stored);
   if(Stored){Info.PlayerName=Stored->PlayerName;Info.SavedAt=Stored->SavedAt;
    const auto* Step=Flow->Steps.FindByPredicate([Stored](const FWardrobeStoryStep& Item){return Item.StoryId==Stored->NextStoryId;});
    Info.Progress=Stored->bStoryComplete?StoryText(TEXT("剧情已完成")):Step?(Step->Title.IsEmpty()?FText::FromName(Step->StoryId):Step->Title):StoryText(TEXT("剧情配置已变化"));
   }else Info.Progress=StoryText(TEXT("存档无法读取"));
  }else Info.Progress=StoryText(TEXT("空存档"));Result.Add(Info);
 }return Result;
}
bool UWardrobeStorySubsystem::TravelToSave(UWardrobeStorySave* Candidate,int32 Slot,UWardrobeMenuData* Menu){
 if(!IsSaveUsable(Candidate)||bActive||!GetGameInstance())return false;
 auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();if(!Loading||Loading->IsLoading())return false;
 if(!Loading->TravelToLevel(Candidate->Level,Menu,{},StoryText(TEXT("继续旅程")),TEXT("game=/Game/BluePrint/ALS/Game/BP_ALSGameMode.BP_ALSGameMode_C"))){LastError=StoryText(TEXT("无法加载存档地图，请检查地图与 GameMode 是否加入打包"));return false;}
 Save=Candidate;ActiveSlot=Slot;bRestorePending=true;ObservedWorld.Reset();
 if(auto* Session=GetGameInstance()->GetSubsystem<UWardrobeSession>()){Session->Name=Save->PlayerName;Session->LoggedIn=true;Session->ExplorationMenuData=Menu;Session->ExplorationReturnLevel=Save->ReturnLevel;}
 return true;
}
bool UWardrobeStorySubsystem::StartNewGame(int32 Slot,const FString& Name,UWardrobeMenuData* Menu,bool bOverwrite){
 if(!Flow||!Flow->Steps.IsValidIndex(0)||Slot<0||Slot>=FMath::Clamp(Flow->SaveSlotCount,1,16)||bActive||bRestorePending)return false;
 auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();if(Loading&&Loading->IsLoading())return false;
 if(UGameplayStatics::DoesSaveGameExist(SlotName(Slot),0)&&!bOverwrite){LastError=StoryText(TEXT("此存档已存在，请确认覆盖或选择空槽"));return false;}
 auto* Session=GetGameInstance()->GetSubsystem<UWardrobeSession>();int32 Initial=Session?Session->SelectedMap:0;
 auto MapReady=[Menu](int32 I){return Menu&&Menu->Levels.IsValidIndex(I)&&!Menu->Levels[I].Level.IsNull()&&FPackageName::DoesPackageExist(Menu->Levels[I].Level.ToSoftObjectPath().GetLongPackageName());};
 if(!MapReady(Initial)){Initial=INDEX_NONE;for(int32 I=0;Menu&&I<Menu->Levels.Num();++I)if(MapReady(I)){Initial=I;break;}}
 if(Initial==INDEX_NONE){LastError=StoryText(TEXT("请先在菜单 Levels 中配置至少一张有效地图"));return false;}
 auto* Candidate=NewObject<UWardrobeStorySave>(this);Candidate->FlowPath=Flow->GetPathName();Candidate->PlayerName=Name.TrimStartAndEnd().Left(16);if(Candidate->PlayerName.IsEmpty())Candidate->PlayerName=TEXT("旅人");
 Candidate->NextStoryId=Flow->Steps[0].StoryId;Candidate->Level=Menu->Levels[Initial].Level;Candidate->MenuData=Menu;Candidate->ReturnLevel=StoryWorldAsset(GetWorld());
 Candidate->SavedAt=FDateTime::UtcNow();if(!UGameplayStatics::SaveGameToSlot(Candidate,SlotName(Slot),0)){LastError=StoryText(TEXT("新存档写入失败，请检查存档目录"));UE_LOG(LogTemp,Warning,TEXT("WARDROBE_STORY_SAVE_FAILED"));return false;}
 Save=Candidate;ActiveSlot=Slot;ObservedWorld.Reset();LastError=FText::GetEmpty();
 if(Session){Session->Name=Candidate->PlayerName;Session->LoggedIn=true;Session->SelectedMap=Initial;Session->ExplorationMenuData=Menu;Session->ExplorationReturnLevel=Candidate->ReturnLevel;}
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_STORY_NEW_SLOT slot=%d next=%s defaultMap=%s (destination selection pending)"),Slot,*Candidate->NextStoryId.ToString(),*Candidate->Level.ToString());
 return true;
}
bool UWardrobeStorySubsystem::ContinueGame(int32 Slot,UWardrobeMenuData* Menu){
 if(!Flow||Slot<0||Slot>=FMath::Clamp(Flow->SaveSlotCount,1,16))return false;
 auto* Candidate=Cast<UWardrobeStorySave>(UGameplayStatics::LoadGameFromSlot(SlotName(Slot),0));
 if(!IsSaveUsable(Candidate)){LastError=StoryText(TEXT("存档无效或剧情编号、地图已被移除"));return false;}
 if(!Menu)Menu=Candidate->MenuData.LoadSynchronous();return TravelToSave(Candidate,Slot,Menu);
}
bool UWardrobeStorySubsystem::ContinueLatest(UWardrobeMenuData* Menu){
 int32 Best=INDEX_NONE;FDateTime Latest=FDateTime::MinValue();for(const auto& Slot:GetSaveSlots())if(Slot.bValid&&Slot.SavedAt>=Latest){Latest=Slot.SavedAt;Best=Slot.Index;}
 if(Best==INDEX_NONE){LastError=StoryText(TEXT("还没有可继续的存档"));return false;}return ContinueGame(Best,Menu);
}
bool UWardrobeStorySubsystem::SaveCheckpoint(){
 if(!Save||!Flow||ActiveSlot==INDEX_NONE||bRestorePending||!GetWorld())return false;
 if(!bActive)if(auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0)){
  Save->Level=StoryWorldAsset(GetWorld());Save->PlayerTransform=Pawn->GetActorTransform();Save->bHasPlayerTransform=true;
  if(auto* PC=UGameplayStatics::GetPlayerController(this,0))Save->ControlRotation=PC->GetControlRotation();
 }
 Save->SavedAt=FDateTime::UtcNow();const bool Ok=UGameplayStatics::SaveGameToSlot(Save,SlotName(ActiveSlot),0);
 if(!Ok){LastError=StoryText(TEXT("存档写入失败，请检查存档目录"));UE_LOG(LogTemp,Warning,TEXT("WARDROBE_STORY_SAVE_FAILED"));}
 else UE_LOG(LogTemp,Display,TEXT("WARDROBE_STORY_SAVED slot=%d next=%s"),ActiveSlot,*Save->NextStoryId.ToString());return Ok;
}
bool UWardrobeStorySubsystem::TryTrigger(AWardrobeStoryTrigger* Trigger){
 if(bActive||bRestorePending||Trigger==BlockedTrigger.Get()||!IsValid(Trigger)||!Trigger->Box||!GetGameInstance())return false;
 auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();if(Loading&&Loading->IsLoading())return false;
 auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0);auto* PC=UGameplayStatics::GetPlayerController(this,0);
 if(!Pawn||!PC||!Pawn->IsLocallyControlled()||!Trigger->ContainsPlayer(Pawn))return false;
 if(!Flow&&!Trigger->StoryFlow.IsNull())if(!Configure(Trigger->StoryFlow.LoadSynchronous()))return false;
 // Standalone map preview has an in-memory campaign; menu-started games own a slot.
 if(!Save&&Flow&&!Flow->Steps.IsEmpty()){Save=NewObject<UWardrobeStorySave>(this);Save->FlowPath=Flow->GetPathName();Save->NextStoryId=Flow->Steps[0].StoryId;Save->PlayerName=TEXT("旅人");}
 const int32 Index=CurrentStepIndex();if(Index==INDEX_NONE)return false;
 const auto& Step=Flow->Steps[Index];if(Step.TriggerId!=Trigger->TriggerId||!StoryWorldMatches(GetWorld(),Step.Level))return false;
 int32 Matches=0;for(TActorIterator<AWardrobeStoryTrigger> It(GetWorld());It;++It)if(It->TriggerId==Step.TriggerId)++Matches;
 if(Matches!=1){LastError=StoryText(TEXT("当前关卡 Trigger ID 重复，无法确定演出范围"));return false;}
 SaveCheckpoint();PlayingStoryId=Step.StoryId;PendingTrigger=Trigger;Controller=PC;HiddenPawn=Pawn;PreviousView=PC->GetViewTarget();
 BeforeTransform=Pawn->GetActorTransform();BeforeControl=PC->GetControlRotation();
 bSavedHidden=Pawn->IsHidden();bSavedCollision=Pawn->GetActorEnableCollision();bSavedCursor=PC->bShowMouseCursor;
 if(auto* Character=Cast<ACharacter>(Pawn)){auto* Movement=Character->GetCharacterMovement();BeforeMovementMode=Movement->MovementMode;BeforeCustomMovementMode=Movement->CustomMovementMode;Movement->StopMovementImmediately();Movement->DisableMovement();}
 Pawn->SetActorHiddenInGame(true);Pawn->SetActorEnableCollision(false);PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);bInputGuarded=true;
 if(auto* Explorer=Cast<AWardrobeExplorer>(Pawn))Explorer->StopSprint();
 bActive=true;bWaiting=false;bClickArmed=false;PC->bShowMouseCursor=true;ShowOverlay();
 TWeakObjectPtr<UWardrobeStorySubsystem> Weak(this);
 LoadHandle=UAssetManager::GetStreamableManager().RequestAsyncLoad(Step.Sequence.ToSoftObjectPath(),FStreamableDelegate::CreateLambda([Weak](){if(Weak.IsValid()&&Weak->bActive)Weak->StartLoadedSequence();}));
 if(!LoadHandle.IsValid()){LastError=StoryText(TEXT("演出资源加载请求失败"));CancelCinematic();return false;}
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_STORY_TRIGGER story=%s"),*PlayingStoryId.ToString());return true;
}
void UWardrobeStorySubsystem::StartLoadedSequence(){
 const int32 Index=CurrentStepIndex();if(!bActive||Index==INDEX_NONE||!HiddenPawn.IsValid()||!Controller.IsValid()){CancelCinematic();return;}
 auto* Sequence=Flow->Steps[Index].Sequence.Get();if(!Sequence||!Sequence->GetMovieScene()||Sequence->GetMovieScene()->GetPlaybackRange().IsEmpty()){LastError=StoryText(TEXT("演出 Sequence 为空或没有播放范围"));CancelCinematic();return;}
 FMovieSceneSequencePlaybackSettings Settings;Settings.FinishCompletionStateOverride=EMovieSceneCompletionModeOverride::ForceRestoreState;
 ALevelSequenceActor* Created=nullptr;Player=ULevelSequencePlayer::CreateLevelSequencePlayer(GetWorld(),Sequence,Settings,Created);SequenceActor=Created;
 if(!Player){LastError=StoryText(TEXT("无法创建演出播放器"));CancelCinematic();return;}
 Player->OnFinished.AddDynamic(this,&UWardrobeStorySubsystem::OnSequenceFinished);Player->OnStop.AddDynamic(this,&UWardrobeStorySubsystem::OnSequenceStopped);
 Player->Play();
}
void UWardrobeStorySubsystem::ShowOverlay(){
 if(!FSlateApplication::IsInitialized())return;
 PreviousFocus=FSlateApplication::Get().GetKeyboardFocusedWidget();Overlay=SNew(SWardrobeStoryOverlay).Story(this);
 if(auto* Viewport=GetGameInstance()->GetGameViewportClient())Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(),200);
 if(Controller.IsValid()){FInputModeUIOnly Mode;Mode.SetWidgetToFocus(Overlay);Controller->SetInputMode(Mode);}
 FSlateApplication::Get().SetAllUserFocus(Overlay,EFocusCause::SetDirectly);
}
void UWardrobeStorySubsystem::CloseOverlay(){
 if(Overlay&&GetGameInstance())if(auto* Viewport=GetGameInstance()->GetGameViewportClient())Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());Overlay.Reset();
}
void UWardrobeStorySubsystem::WaitForClick(ULevelSequencePlayer* ExpectedPlayer){
 if(!bActive||!Player||Player!=ExpectedPlayer||bWaiting)return;
 bWaiting=true;bClickArmed=false;Player->Pause();
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_STORY_WAIT story=%s time=%.3f"),*PlayingStoryId.ToString(),Player->GetCurrentTime().AsSeconds());
}
bool UWardrobeStorySubsystem::ClickContinue(){if(!bWaiting||!bClickArmed)return false;ContinueSequence();return true;}
void UWardrobeStorySubsystem::ContinueSequence(){
 if(!bActive||!bWaiting||!Player)return;bWaiting=false;bClickArmed=false;Player->Play();
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_STORY_CONTINUE story=%s"),*PlayingStoryId.ToString());
}
void UWardrobeStorySubsystem::OnSequenceFinished(){bDeferredFinish=true;}
void UWardrobeStorySubsystem::OnSequenceStopped(){if(!bFinishing)bDeferredStop=true;}
void UWardrobeStorySubsystem::RestorePlayer(){
 if(HiddenPawn.IsValid()){
  if(!BeforeTransform.ContainsNaN()&&BeforeTransform.GetRotation().IsNormalized())HiddenPawn->SetActorTransform(BeforeTransform,false,nullptr,ETeleportType::TeleportPhysics);
  HiddenPawn->SetActorHiddenInGame(bSavedHidden);HiddenPawn->SetActorEnableCollision(bSavedCollision);
  if(auto* Character=Cast<ACharacter>(HiddenPawn.Get()))Character->GetCharacterMovement()->SetMovementMode(static_cast<EMovementMode>(BeforeMovementMode),BeforeCustomMovementMode);
 }
 if(Controller.IsValid()){
  if(AActor* View=PreviousView.IsValid()?PreviousView.Get():HiddenPawn.Get())Controller->SetViewTarget(View);Controller->SetControlRotation(BeforeControl);
  if(bInputGuarded){Controller->SetIgnoreMoveInput(false);Controller->SetIgnoreLookInput(false);}
  Controller->FlushPressedKeys();Controller->bShowMouseCursor=bSavedCursor;
  if(auto* Explorer=Cast<AWardrobeExplorerController>(Controller.Get()))Explorer->SetExplorerInput();
  else {FInputModeGameOnly Mode;Controller->SetInputMode(Mode);}
 }
 bInputGuarded=false;
}
void UWardrobeStorySubsystem::CancelCinematic(){
 if(LoadHandle.IsValid())LoadHandle->CancelHandle();LoadHandle.Reset();
 bFinishing=true;if(Player){Player->OnFinished.RemoveAll(this);Player->OnStop.RemoveAll(this);Player->Stop();}bFinishing=false;
 Player=nullptr;if(IsValid(SequenceActor))SequenceActor->Destroy();SequenceActor=nullptr;
 if(bActive){BlockedTrigger=PendingTrigger;RestorePlayer();}CloseOverlay();bActive=bWaiting=bClickArmed=bDeferredFinish=bDeferredStop=false;
 Controller.Reset();HiddenPawn.Reset();PreviousView.Reset();PendingTrigger.Reset();PlayingStoryId=NAME_None;
}
void UWardrobeStorySubsystem::OnWorldCleanup(UWorld* World,bool,bool){
 if(World==ObservedWorld.Get()||(HiddenPawn.IsValid()&&HiddenPawn->GetWorld()==World)){CancelCinematic();ObservedWorld.Reset();}
}
bool UWardrobeStorySubsystem::Tick(float DeltaSeconds){AdvanceRuntime(DeltaSeconds);
#if WITH_EDITOR
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeStoryQA")))RunStoryQA();
#endif
 return true;}
#if WITH_EDITOR
void UWardrobeStorySubsystem::RunStoryQA(){
 if(!Flow||Flow->GetPathName()!=TEXT("/Game/BluePrint/Story/DA_StoryFlow.DA_StoryFlow")||IsRunningCommandlet())return;
 const double Now=FPlatformTime::Seconds();if(QAStarted==0)QAStarted=Now;
 auto Finish=[this](bool Success,const FString& Detail){
  const FString Out=FPaths::ProjectSavedDir()/TEXT("Story77QA");IFileManager::Get().MakeDirectory(*Out,true);
  FFileHelper::SaveStringToFile(Success?TEXT("Passed\n")+Detail:TEXT("Failed\n")+Detail,*(Out/TEXT("result.txt")));
  UE_LOG(LogTemp,Display,TEXT("STORY77_GAME_QA %s %s"),Success?TEXT("PASSED"):TEXT("FAILED"),*Detail);
  if(Success)UGameplayStatics::DeleteGameInSlot(SlotName(2),0);ActiveSlot=INDEX_NONE;FPlatformMisc::RequestExit(false);QAStage=99;
 };
 if(QAStage==99)return;if(Now-QAStarted>150){Finish(false,FString::Printf(TEXT("watchdog stage=%d error=%s"),QAStage,*LastError.ToString()));return;}
 auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();if(L->IsLoading()||bRestorePending)return;
 auto* Session=GetGameInstance()->GetSubsystem<UWardrobeSession>();auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0);auto* PC=UGameplayStatics::GetPlayerController(this,0);
 auto Capture=[](const FString& Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Story77QA")/(Name+TEXT(".png")),true,false);};
 auto EnterArea=[this,Pawn](FName Id){if(!Pawn)return;for(TActorIterator<AWardrobeStoryTrigger> It(GetWorld());It;++It)if(It->TriggerId==Id){Pawn->SetActorLocation(It->GetActorLocation(),false,nullptr,ETeleportType::TeleportPhysics);return;}};
 if(QAStage==0&&Now-QAStarted>3){
  if(QAHold==0){Capture(TEXT("01-menu"));QAHold=Now;return;}
  if(Now-QAHold<.7)return;QAHold=0;auto* Menu=LoadObject<UWardrobeMenuData>(nullptr,TEXT("/Game/BluePrint/Story/DA_StoryMenu.DA_StoryMenu"));
  if(!StartNewGame(2,TEXT("演出测试旅人"),Menu,true)){Finish(false,LastError.ToString());return;}
  if(!L->TravelToLevel(Menu->Levels[0].Level,Menu,{},Menu->Levels[0].Label,TEXT("game=/Game/BluePrint/ALS/Game/BP_ALSGameMode.BP_ALSGameMode_C"))){Finish(false,TEXT("selected destination travel"));return;}
  QAStage=1;return;
 }
 if(QAStage==1&&Pawn){EnterArea(TEXT("Area01"));QAStage=2;QAWaits=0;return;}
 if(QAStage==2||QAStage==4||QAStage==8){
  if(bActive&&bWaiting){
   if(QAHold==0){QAHold=Now;Capture(FString::Printf(TEXT("%02d-wait-%d"),QAStage,QAWaits));++QAWaits;
    for(TActorIterator<AActor> It(GetWorld());It;++It)if(It->IsA<ACameraActor>()||It->IsA<ASkeletalMeshActor>())UE_LOG(LogTemp,Display,TEXT("STORY77_QA_ACTOR %s %s rotation=%s hidden=%d"),*It->GetName(),*It->GetActorLocation().ToString(),*It->GetActorRotation().ToString(),It->IsHidden());
    for(TActorIterator<ACameraActor> It(GetWorld());It;++It)UE_LOG(LogTemp,Display,TEXT("STORY77_CAMERA component=%s forward=%s"),*It->GetCameraComponent()->GetComponentLocation().ToString(),*It->GetCameraComponent()->GetForwardVector().ToString());
    for(TActorIterator<ASkeletalMeshActor> It(GetWorld());It;++It)UE_LOG(LogTemp,Display,TEXT("STORY77_MESH component=%s bounds=%s hidden=%d visible=%d"),*It->GetSkeletalMeshComponent()->GetComponentLocation().ToString(),*It->GetSkeletalMeshComponent()->Bounds.Origin.ToString(),It->GetSkeletalMeshComponent()->bHiddenInGame,It->GetSkeletalMeshComponent()->IsVisible());
    UE_LOG(LogTemp,Display,TEXT("STORY77_QA_WAIT stage=%d count=%d hidden=%d collision=%d move=%d look=%d paused=%d time=%.3f"),QAStage,QAWaits,Pawn&&Pawn->IsHidden(),Pawn&&Pawn->GetActorEnableCollision(),PC&&PC->IsMoveInputIgnored(),PC&&PC->IsLookInputIgnored(),Player&&Player->IsPaused(),Player?Player->GetCurrentTime().AsSeconds():-1);
    if(!Pawn||!Pawn->IsHidden()||Pawn->GetActorEnableCollision()||!PC->IsMoveInputIgnored()||!PC->IsLookInputIgnored()){Finish(false,TEXT("player guard"));return;}
   }
   if(Now-QAHold>1){ContinueSequence();QAHold=0;}
  }
  if(!bActive&&Save&&((QAStage==2&&Save->NextStoryId==TEXT("Story02"))||(QAStage==4&&Save->NextStoryId==TEXT("Story03"))||(QAStage==8&&Save->bStoryComplete))){
   if(QAWaits!=2){Finish(false,FString::Printf(TEXT("expected two authored waits stage=%d count=%d"),QAStage,QAWaits));return;}
   if(!Pawn||Pawn->IsHidden()||!Pawn->GetActorEnableCollision()||PC->IsMoveInputIgnored()||PC->IsLookInputIgnored()){Finish(false,TEXT("player restoration"));return;}
   Capture(FString::Printf(TEXT("%02d-restored"),QAStage));++QAStage;QAHold=0;
  }return;
 }
 if(QAStage==3){EnterArea(TEXT("Area02"));QAStage=4;QAWaits=0;return;}
 if(QAStage==5){if(auto* Explorer=Cast<AWardrobeExplorerController>(PC)){Explorer->ReturnToShowcase();QAStage=6;}return;}
 if(QAStage==6){
  if(UGameplayStatics::GetCurrentLevelName(this,true)!=TEXT("L_StoryMenu"))return;
  Capture(TEXT("06-continue-menu"));if(!ContinueLatest(Session->ExplorationMenuData)){Finish(false,LastError.ToString());return;}QAStage=7;return;
 }
 if(QAStage==7&&Pawn){
  if(Save->NextStoryId!=TEXT("Story03")||Save->PlayerName!=TEXT("演出测试旅人")){Finish(false,TEXT("continue progress/name"));return;}
  if(StoryWorldMatches(GetWorld(),Flow->Steps[0].Level)){
   if(!L->TravelToLevel(Flow->Steps[2].Level,Session->ExplorationMenuData,{},StoryText(TEXT("示例 B")),TEXT("game=/Game/BluePrint/ALS/Game/BP_ALSGameMode.BP_ALSGameMode_C"))){Finish(false,TEXT("cross-map travel"));return;}return;
  }
  if(StoryWorldMatches(GetWorld(),Flow->Steps[2].Level)){EnterArea(TEXT("Area03"));QAStage=8;QAWaits=0;}return;
 }
 if(QAStage==9){
  const auto Slots=GetSaveSlots();if(!Slots.IsValidIndex(2)||!Slots[2].bValid||!Save->bStoryComplete){Finish(false,TEXT("final persisted progress"));return;}
  Finish(true,TEXT("3 performances / 6 authored event pauses / new game and bounded slot / hidden player and restored input / menu return and saved name, location, progress / cross-map travel"));
 }
}
#endif
void UWardrobeStorySubsystem::AdvanceRuntime(float){
 UWorld* World=GetWorld();if(!World||!World->IsGameWorld())return;
 auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();if(Loading&&Loading->IsLoading())return;
 auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0);auto* PC=UGameplayStatics::GetPlayerController(this,0);
 if(bRestorePending){
  if(!Save||!StoryWorldMatches(World,Save->Level)||!Pawn||!PC)return;
  if(Save->bHasPlayerTransform){Pawn->SetActorTransform(Save->PlayerTransform,false,nullptr,ETeleportType::TeleportPhysics);PC->SetControlRotation(Save->ControlRotation);}
  bRestorePending=false;SaveCheckpoint();
 }
 if(World!=ObservedWorld.Get()){
  ObservedWorld=World;const int32 Index=CurrentStepIndex();if(Index!=INDEX_NONE&&StoryWorldMatches(World,Flow->Steps[Index].Level)){
   int32 Count=0;for(TActorIterator<AWardrobeStoryTrigger> It(World);It;++It)if(It->TriggerId==Flow->Steps[Index].TriggerId)++Count;
   if(Count!=1)UE_LOG(LogTemp,Warning,TEXT("WARDROBE_STORY_AREA story=%s trigger=%s matches=%d (may be streamed later)"),*Save->NextStoryId.ToString(),*Flow->Steps[Index].TriggerId.ToString(),Count);
  }
  // A newly chosen destination is a checkpoint even before the first story trigger.
  // Menu return never replaces the saved exploration map/position.
  if(Save&&ActiveSlot!=INDEX_NONE&&Cast<AWardrobeExplorerController>(PC)&&Pawn)SaveCheckpoint();
 }
 if(bActive){
  if(!HiddenPawn.IsValid()||!Controller.IsValid()){CancelCinematic();return;}
  if(bDeferredFinish){
   const FName Completed=PlayingStoryId;const int32 Index=CurrentStepIndex();
   CancelCinematic();if(Save&&Index!=INDEX_NONE){Save->CompletedStories.AddUnique(Completed);Save->bStoryComplete=!Flow->Steps.IsValidIndex(Index+1);Save->NextStoryId=Save->bStoryComplete?NAME_None:Flow->Steps[Index+1].StoryId;SaveCheckpoint();OnStoryCompleted.Broadcast(Completed);}
   return;
  }
  if(bDeferredStop){CancelCinematic();return;}
  if(bWaiting&&Player&&Player->IsPlaying())Player->Pause();
  if(bWaiting&&FSlateApplication::IsInitialized()&&!FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton)&&!Controller->IsInputKeyDown(EKeys::SpaceBar)&&!Controller->IsInputKeyDown(EKeys::Enter))bClickArmed=true;
  return;
 }
 if(BlockedTrigger.IsValid()&&Pawn&&!BlockedTrigger->ContainsPlayer(Pawn))BlockedTrigger.Reset();
 if(World->IsPaused()||!Flow||!Pawn||!PC||!Pawn->IsLocallyControlled()||CurrentStepIndex()==INDEX_NONE)return;
 // Checking overlap also handles spawning/continuing inside a box and streamed-in regions.
 for(TActorIterator<AWardrobeStoryTrigger> It(World);It;++It)if(TryTrigger(*It))break;
}
void UWardrobeStoryLibrary::WaitForStoryClick(const UObject* Context){
 if(!Context||!Context->GetWorld()||!Context->GetWorld()->IsGameWorld())return;
 if(auto* GI=Context->GetWorld()->GetGameInstance())if(auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>()){
  auto* Active=Story->GetStoryPlayer();if(!Active)return;
  if(const auto* Director=Cast<ULevelSequenceDirector>(Context)){Story->WaitForClick(Director->Player);return;}
  // Verify the helper is bound to this root player. Events in unrelated preview sequences cannot pause the story.
  if(!Cast<AWardrobeStoryCue>(Context))return;
  if(const auto* Scene=Active->GetSequence()->GetMovieScene())for(const auto& Binding:Scene->GetBindings())
   if(Active->GetBoundObjects(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(Binding.GetObjectGuid()))).Contains(const_cast<UObject*>(Context))){Story->WaitForClick(Active);return;}
 }
}
void AWardrobeStoryCue::WaitForClick(){UWardrobeStoryLibrary::WaitForStoryClick(this);}
