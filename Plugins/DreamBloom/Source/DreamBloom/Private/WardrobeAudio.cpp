#include "WardrobeAudio.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

const FWardrobeSoundEntry& UWardrobeAudioData::GetEntry(EWardrobeUISound Event) const {
 switch(Event){
 #define ENTRY(Name) case EWardrobeUISound::Name:return Name;
 ENTRY(ActionSwitch) ENTRY(MapSelect)   ENTRY(StartGame) ENTRY(Login)
 ENTRY(Back) ENTRY(Confirm) ENTRY(Cancel) ENTRY(Unavailable)    ENTRY(ResetCamera)
 #undef ENTRY
 default: {static const FWardrobeSoundEntry Empty;return Empty;}
 }
}
void UWardrobeAudioSubsystem::Configure(UWardrobeAudioData* Data){
 if(Settings==Data)return;
 Settings=Data;LoadedSounds.Reset();LastPlayed.Reset();
 if(!Settings)return;
 for(int32 I=0;I<=int32(EWardrobeUISound::ResetCamera);I++){
  if(USoundBase* Sound=Settings->GetEntry(EWardrobeUISound(I)).Sound.LoadSynchronous())LoadedSounds.AddUnique(Sound);
 }
}
bool UWardrobeAudioSubsystem::PlayEvent(EWardrobeUISound Event){
 return Settings?PlayEntry(Event,Settings->GetEntry(Event)):false;
}
bool UWardrobeAudioSubsystem::PlayEntry(EWardrobeUISound Event,const FWardrobeSoundEntry& Entry){
 if(!GetWorld()||(Settings&&(!Settings->bEnabled||Settings->MasterVolume<=0)))return false;
 if(Entry.Sound.IsNull()||Entry.Volume<=0)return false;
 const double Now=FPlatformTime::Seconds();
 if(const double* Last=LastPlayed.Find(Event))if(Now-*Last<FMath::Max(0.f,Entry.MinInterval))return false;
 USoundBase* Sound=Entry.Sound.LoadSynchronous();if(!Sound)return false;
 LoadedSounds.AddUnique(Sound);
 ActiveSounds.RemoveAll([](const TObjectPtr<UAudioComponent>& C){return !IsValid(C.Get())||!C->IsPlaying();});
 const int32 Limit=FMath::Clamp(Settings?Settings->MaxSimultaneousSounds:4,1,16);
 while(ActiveSounds.Num()>=Limit){if(IsValid(ActiveSounds[0].Get()))ActiveSounds[0]->Stop();ActiveSounds.RemoveAt(0);}
 UAudioComponent* Component=UGameplayStatics::SpawnSound2D(GetGameInstance(),Sound,
  FMath::Clamp(Settings?Settings->MasterVolume:.7f,0.f,1.f)*FMath::Clamp(Entry.Volume,0.f,2.f),FMath::Clamp(Entry.Pitch,.5f,2.f),0,nullptr,true,true);
 if(!Component)return false;
 ActiveSounds.Add(Component);LastPlayed.Add(Event,Now);++PlayCount;
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_UI_SOUND event=%s asset=%s persist=%d count=%d"),*StaticEnum<EWardrobeUISound>()->GetNameStringByValue(int64(Event)),*Sound->GetPathName(),Component->bIgnoreForFlushing,PlayCount);
 return true;
}
void UWardrobeAudioSubsystem::Deinitialize(){
 for(auto C:ActiveSounds)if(IsValid(C.Get()))C->Stop();
 ActiveSounds.Reset();LoadedSounds.Reset();Settings=nullptr;Super::Deinitialize();
}
void UWardrobeAudioSubsystem::AuditTravelAudio(){
 for(auto C:ActiveSounds)if(IsValid(C.Get())&&C->IsPlaying())UE_LOG(LogTemp,Display,TEXT("WARDROBE_AUDIO_ALIVE_AFTER_TRAVEL sound=%s"),*GetNameSafe(C->Sound));
}
