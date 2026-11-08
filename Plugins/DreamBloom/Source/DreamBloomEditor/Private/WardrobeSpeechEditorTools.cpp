#include "WardrobeSpeechEditorTools.h"
#include "WardrobeSpeechGraphNode.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
bool UWardrobeSpeechEditorTools::AddSpeechNode(UAnimBlueprint* BP){
 if(!BP)return false;
 for(const auto& Graph:BP->FunctionGraphs)for(const auto& N:Graph->Nodes)if(Cast<UAnimGraphNode_WardrobeSpeech>(N.Get()))return true;
 for(const auto& Graph:BP->FunctionGraphs)for(const auto& N:Graph->Nodes)if(auto* Root=Cast<UAnimGraphNode_Root>(N.Get())){
  UEdGraphPin* Input=Root->FindPin(TEXT("Result"));if(!Input||Input->LinkedTo.Num()!=1)return false;
  auto* Old=Input->LinkedTo[0];auto* Added=NewObject<UAnimGraphNode_WardrobeSpeech>(Graph.Get());Graph->Modify();BP->Modify();
  Graph->AddNode(Added,false,false);Added->CreateNewGuid();Added->PostPlacedNewNode();Added->AllocateDefaultPins();Added->NodePosX=Root->NodePosX-180;Added->NodePosY=Root->NodePosY;
  UEdGraphPin *In=Added->FindPin(TEXT("Source")),*Out=nullptr;for(auto* P:Added->Pins)if(P->Direction==EGPD_Output)Out=P;
  if(!In||!Out){Graph->RemoveNode(Added);return false;}
  Old->BreakLinkTo(Input);const auto* Schema=Graph->GetSchema();
  if(!Schema->TryCreateConnection(Old,In)||!Schema->TryCreateConnection(Out,Input)){
   In->BreakAllPinLinks();Out->BreakAllPinLinks();Schema->TryCreateConnection(Old,Input);Graph->RemoveNode(Added);return false;
  }
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
 }return false;
}
