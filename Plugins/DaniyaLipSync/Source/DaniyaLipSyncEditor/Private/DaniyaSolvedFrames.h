#pragma once
#include "CoreMinimal.h"
#include "FrameAnimationData.h"

namespace DaniyaSolvedFrames
{
// Audio feature/resampling counts can leave up to two empty slots at the end of a full solve.
// Do not accept interior gaps, partial maps, or a missing tail longer than two output frames.
inline bool RepairAudioTail(TArray<FFrameAnimationData>& Frames, double FPS, double Duration, FString& Error)
{
    if (Frames.IsEmpty() || !FMath::IsFinite(FPS) || FPS<=0 || !FMath::IsFinite(Duration) || Duration<=0) return false;
    int32 End=Frames.Num();
    while (End>0 && Frames[End-1].AnimationData.IsEmpty()) --End;
    if (End==Frames.Num()) return true;
    const int32 Missing=Frames.Num()-End;
    if (End==0 || Missing>2 || Duration-End/FPS>2./FPS+.0001)
    { Error=FString::Printf(TEXT("Audio solve has %d empty trailing frames. Reprocess the full audio; the missing range is too long to repair."),Missing); return false; }
    for (int32 I=End;I<Frames.Num();++I) Frames[I].AnimationData=Frames[End-1].AnimationData;
    return true;
}
}
