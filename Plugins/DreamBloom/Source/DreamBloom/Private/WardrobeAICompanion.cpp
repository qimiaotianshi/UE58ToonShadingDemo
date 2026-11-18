#include "WardrobeAICompanion.h"

namespace WardrobeAICompanion {
void BoundMemory(FWardrobeCompanionMemory& M,int32 MaxTurns,int32 MaxNotes){
 MaxTurns=FMath::Clamp(MaxTurns,4,32);MaxNotes=FMath::Clamp(MaxNotes,8,64);
 for(auto& T:M.Turns)T.Text=T.Text.TrimStartAndEnd().Left(T.bPlayer?500:120);
 M.Turns.RemoveAll([](const auto& T){return T.Text.IsEmpty();});
 if(M.Turns.Num()>MaxTurns)M.Turns.RemoveAt(0,M.Turns.Num()-MaxTurns);
 for(auto& N:M.Notes)N=N.TrimStartAndEnd().Left(120);
 M.Notes.RemoveAll([](const FString& N){return N.IsEmpty();});
 if(M.Notes.Num()>MaxNotes)M.Notes.RemoveAt(0,M.Notes.Num()-MaxNotes);
}
FString NormalizeLine(const FString& Line){
 FString Out;for(TCHAR C:Line)if(!FChar::IsWhitespace(C)&&!FChar::IsPunct(C))Out.AppendChar(FChar::ToLower(C));return Out;
}
TArray<FString> SelectNotes(const FWardrobeCompanionMemory& M,const FString& Query){
 struct FRank{int32 Index;int32 Score;};TArray<FRank> Ranked;
 const FString Q=NormalizeLine(Query).Left(600);
 // Bounded lexical lookup, not a vector database. Recency breaks ties.
 for(int32 I=FMath::Max(0,M.Notes.Num()-64);I<M.Notes.Num();++I){
  const FString N=NormalizeLine(M.Notes[I]);int32 Score=0;
  for(int32 C=0;C+1<Q.Len();++C)if(N.Contains(Q.Mid(C,2)))++Score;
  Ranked.Add({I,Score});
 }
 Ranked.Sort([](const auto& A,const auto& B){return A.Score==B.Score?A.Index>B.Index:A.Score>B.Score;});
 TArray<FString> Result;for(int32 I=0;I<FMath::Min(8,Ranked.Num());++I)Result.Add(M.Notes[Ranked[I].Index]);return Result;
}
FString DescribeRecent(const FWardrobeCompanionMemory& M){
 FString Out;for(int32 I=FMath::Max(0,M.Turns.Num()-12);I<M.Turns.Num();++I){
  const auto& T=M.Turns[I];Out+=(T.bPlayer?TEXT("玩家："):TEXT("角色："))+T.Text.Left(T.bPlayer?500:120)+TEXT("\n");
 }return Out;
}
}
