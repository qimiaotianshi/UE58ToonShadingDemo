#include "WardrobeAISpeechTypes.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeEmotionTextTest,"Wardrobe.AISpeech.EmotionTagsAndVoicePayload",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeEmotionTextTest::RunTest(const FString&){
 const auto* Data=GetDefault<UWardrobeAISpeechData>();FString Text,Error;FName Emotion;
 TestEqual(TEXT("Thirty-six usable default moods"),Data->Emotions.Num(),36);
 TestTrue(TEXT("Tagged response accepted after excluding markup from length"),WardrobeAISpeech::CleanDialogue(TEXT("【开心】12345"),5,Data->Emotions,Text,Emotion,Error));
 TestEqual(TEXT("Known tag selects its preset"),Emotion,FName(TEXT("开心")));TestEqual(TEXT("Spoken text contains no mood"),Text,FString(TEXT("12345")));
 TestTrue(TEXT("Any additional metadata is removed"),WardrobeAISpeech::CleanDialogue(TEXT("【难过】你好【叹气】。"),10,Data->Emotions,Text,Emotion,Error));
 TestEqual(TEXT("Only the leading tag selects emotion"),Emotion,FName(TEXT("难过")));TestEqual(TEXT("Embedded markup never reaches TTS"),Text,FString(TEXT("你好。")));
 TestTrue(TEXT("Unknown mood preserves dialogue"),WardrobeAISpeech::CleanDialogue(TEXT("【不存在】你好。"),10,Data->Emotions,Text,Emotion,Error));TestEqual(TEXT("Unknown mood falls back"),Emotion,FName(TEXT("平静")));
 TestTrue(TEXT("Untagged legacy reply remains valid"),WardrobeAISpeech::CleanDialogue(TEXT("你好。"),10,Data->Emotions,Text,Emotion,Error));TestEqual(TEXT("Missing mood falls back"),Emotion,FName(TEXT("平静")));
 TestFalse(TEXT("Metadata-only response is rejected"),WardrobeAISpeech::CleanDialogue(TEXT("【开心】"),10,Data->Emotions,Text,Emotion,Error));
 TestFalse(TEXT("Unclosed tag cannot be read aloud"),WardrobeAISpeech::CleanDialogue(TEXT("【开心"),10,Data->Emotions,Text,Emotion,Error));
 TestFalse(TEXT("Length limit applies to the spoken line"),WardrobeAISpeech::CleanDialogue(TEXT("【开心】123456"),5,Data->Emotions,Text,Emotion,Error));
 FWardrobeSpeechJob Job;Job.Text=TEXT("【开心】你好【动作】。");Job.Emotions=Data->Emotions;
 const auto Voice=WardrobeAISpeech::BuildVoiceRequest(Job);TestEqual(TEXT("Actual TTS payload is stripped"),Voice->GetStringField(TEXT("text")),FString(TEXT("你好。")));
 const auto Chat=WardrobeAISpeech::BuildChatRequest(Job);const FString Prompt=Chat->GetArrayField(TEXT("messages"))[0]->AsObject()->GetStringField(TEXT("content"));
 for(const auto& P:Data->Emotions){
  TestTrue(TEXT("Configured names are advertised to the model"),Prompt.Contains(P.Emotion.ToString()));
  TestTrue(TEXT("Every expanded tag can be selected"),WardrobeAISpeech::CleanDialogue(TEXT("【")+P.Emotion.ToString()+TEXT("】你好。"),10,Data->Emotions,Text,Emotion,Error));
  TestEqual(TEXT("Expanded tag resolves to the exact preset"),Emotion,P.Emotion);
 }
 Job.PreviousEmotion=TEXT("开心微笑");const FString VariedPrompt=WardrobeAISpeech::BuildChatRequest(Job)->GetArrayField(TEXT("messages"))[0]->AsObject()->GetStringField(TEXT("content"));
 TestTrue(TEXT("Previous successful emotion informs variant selection"),VariedPrompt.Contains(TEXT("上次已播放的心情：开心微笑")));
 const FString JSON=TEXT("{\"choices\":[{\"message\":{\"content\":\"【疑惑】你好。\"}}]}");
 TestTrue(TEXT("LLM response and tagged direct text share sanitizer"),WardrobeAISpeech::ParseDialogue(JSON,10,Text,Error,&Data->Emotions,&Emotion));TestEqual(TEXT("LLM mood propagated"),Emotion,FName(TEXT("疑惑")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeEmotionLibraryTest,"Wardrobe.AISpeech.ExpressionLibraryUpgrade",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeEmotionLibraryTest::RunTest(const FString&){
 auto* Data=NewObject<UWardrobeAISpeechData>();TSet<FName> Names;TSet<FString> Recipes;int32 Happy=0;
 for(const auto& E:Data->Emotions){
  TestFalse(TEXT("Preset names are unique"),Names.Contains(E.Emotion));Names.Add(E.Emotion);
  TestTrue(TEXT("Each preset has a usable facial recipe"),!E.Morphs.IsEmpty());FString Recipe;
  for(const auto& M:E.Morphs){TestTrue(TEXT("Preset weights are finite and bounded"),FMath::IsFinite(M.Weight)&&M.Weight>=0&&M.Weight<=1);Recipe+=M.Morph.ToString()+FString::Printf(TEXT(":%.4f;"),M.Weight);}
  TestFalse(TEXT("Preset variants have different recipes"),Recipes.Contains(Recipe));Recipes.Add(Recipe);
  if(E.Emotion.ToString().StartsWith(TEXT("开心")))++Happy;
 }
 TestEqual(TEXT("Four happy expressions are provided"),Happy,4);
 Data->Emotions.SetNum(7);Data->Emotions[0].Intensity=.27f;Data->Emotions[0].Morphs[0].Weight=.11f;
 FWardrobeSpeechEmotion Custom;Custom.Emotion=TEXT("自定义心情");Data->Emotions.Add(Custom);
 Data->AddMissingDefaultEmotions();TestEqual(TEXT("Existing DA is supplemented, including custom entry"),Data->Emotions.Num(),37);
 TestEqual(TEXT("Existing custom intensity is preserved"),Data->Emotions[0].Intensity,.27f);
 TestEqual(TEXT("Existing morph tuning is preserved"),Data->Emotions[0].Morphs[0].Weight,.11f);
 Data->AddMissingDefaultEmotions();TestEqual(TEXT("Supplementing twice does not duplicate presets"),Data->Emotions.Num(),37);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeEmotionLayerTest,"Wardrobe.AISpeech.EmotionLayerProtectsVowels",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeEmotionLayerTest::RunTest(const FString&){
 FWardrobeSpeechJob Job;Job.Emotion=TEXT("开心");Job.ExpressionIntensity=1;
 FWardrobeSpeechMorph Vowel;Vowel.Morph=TEXT("あ");Vowel.Control=TEXT("Daniya.A");Job.Morphs.Add(Vowel);
 FWardrobeSpeechEmotion E;E.Emotion=Job.Emotion;
 auto Add=[&](FName Name,float Value){FWardrobeSpeechEmotionMorph M;M.Morph=Name;M.Weight=Value;E.Morphs.Add(M);};
 Add(TEXT("あ"),1);Add(TEXT("にこり"),.5);Add(TEXT("blink"),.2);Add(TEXT("bad"),std::numeric_limits<float>::quiet_NaN());Job.Emotions.Add(E);
 FWardrobeSpeechClip Clip;Clip.Names={TEXT("あ"),TEXT("blink")};Clip.Frames=2;Clip.Values={.2f,.9f,.7f,.0f};
 WardrobeAISpeech::ApplyEmotion(Clip,Job);const int32 A=Clip.Names.IndexOfByKey(TEXT("あ")),B=Clip.Names.IndexOfByKey(TEXT("blink")),Smile=Clip.Names.IndexOfByKey(TEXT("にこり"));
 TestEqual(TEXT("First vowel frame unchanged"),Clip.Values[A],.2f);TestEqual(TEXT("Second vowel frame unchanged"),Clip.Values[Clip.Names.Num()+A],.7f);
 TestEqual(TEXT("Speech blink is never reduced by preset"),Clip.Values[B],.9f);
 TestTrue(TEXT("Expression is present on a separate curve"),Clip.Values[Smile]>.4f&&Clip.Values[Smile]<.6f);
 for(float V:Clip.Values)TestTrue(TEXT("All generated curves finite and bounded"),FMath::IsFinite(V)&&V>=0&&V<=1);
 TestEqual(TEXT("Heap frames remain correctly sized"),Clip.Values.Num(),Clip.Frames*Clip.Names.Num());
 return true;
}
#endif
