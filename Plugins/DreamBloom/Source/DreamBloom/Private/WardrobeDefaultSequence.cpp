#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"

bool AWardrobeShowcase::IsDefaultSequencePlaying() const{return IsValid(DefaultSequencePlayer)&&DefaultSequencePlayer->IsPlaying();}
void AWardrobeShowcase::StopDefaultSequence(){
 // Restore even if a Sequence section uses Keep State, then release all spawnables/audio.
 if(IsValid(DefaultSequencePlayer)){DefaultSequencePlayer->Stop();DefaultSequencePlayer->RestoreState();}
 if(IsValid(DefaultSequenceActor))DefaultSequenceActor->Destroy();
 DefaultSequencePlayer=nullptr;DefaultSequenceActor=nullptr;PlayingDefaultSequence=nullptr;
}
bool AWardrobeShowcase::StartDefaultSequence(){
 if(bEndingShowcase||!bInteractionEnabled||IsInteracting()||!GetWorld())return false;
 const auto* Profile=GetInteractionProfile();if(!Profile)return false;
 if(Profile->DefaultSequence.IsNull()){StopDefaultSequence();return false;}
 auto* Sequence=Profile->DefaultSequence.LoadSynchronous();
 if(!Sequence||!Sequence->GetMovieScene())return false;
 const auto Range=Sequence->GetMovieScene()->GetPlaybackRange();
 if(!Range.HasLowerBound()||!Range.HasUpperBound()||Range.IsEmpty()||Range.GetUpperBoundValue()<=Range.GetLowerBoundValue()){
  CharacterMessage=FText::FromString(TEXT("默认 Sequence 需要有效的播放起止范围"));return false;
 }
 if(IsDefaultSequencePlaying()&&PlayingDefaultSequence==Sequence)return true;
 FMovieSceneSequencePlaybackSettings Settings;Settings.LoopCount.Value=-1;
 Settings.FinishCompletionStateOverride=EMovieSceneCompletionModeOverride::ForceRestoreState;
 // Station camera owns the preview framing. Idle sequences may animate scene cameras,
 // but camera-cut tracks must not steal the view from QE navigation.
 Settings.bDisableCameraCuts=true;
 ALevelSequenceActor* Actor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(this,Sequence,Settings,Actor);
 if(!Player||!Actor){if(Actor)Actor->Destroy();return false;}
 StopDefaultSequence();DefaultSequencePlayer=Player;DefaultSequenceActor=Actor;PlayingDefaultSequence=Sequence;
 Player->Play();BeginSequencePhysicsFadeIn();UE_LOG(LogTemp,Display,TEXT("WARDROBE_DEFAULT_SEQUENCE actor=%s sequence=%s"),*GetName(),*Sequence->GetPathName());return true;
}
