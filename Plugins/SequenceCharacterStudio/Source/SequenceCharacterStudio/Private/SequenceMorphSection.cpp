#include "SequenceMorphSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneChannelEditorData.h"
EMovieSceneChannelProxyType USequenceMorphSection::CacheChannelProxy(){
 const auto Type=Super::CacheChannelProxy();
#if WITH_EDITOR
 if(ChannelProxy.IsValid())for(const auto& Entry:ChannelProxy->GetAllEntries())for(const auto& ConstMeta:Entry.GetMetaData()){
  // Metadata belongs to this newly built proxy; Sequencer reads it to construct numeric editors.
  auto& Meta=const_cast<FMovieSceneChannelMetaData&>(ConstMeta);
  Meta.PropertyMetaData.Add(TEXT("Delta"),TEXT("0"));
  Meta.PropertyMetaData.Remove(TEXT("LinearDeltaSensitivity"));
  Meta.PropertyMetaData.Remove(TEXT("UIMin"));Meta.PropertyMetaData.Remove(TEXT("UIMax"));
  Meta.PropertyMetaData.Add(TEXT("ClampMin"),TEXT("-1"));Meta.PropertyMetaData.Add(TEXT("ClampMax"),TEXT("1"));
 }
#endif
 return Type;
}
