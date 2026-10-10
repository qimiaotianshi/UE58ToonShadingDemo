#include "DaniyaMouthLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, DaniyaLipSync)

namespace
{
float Unit(float V) { return FMath::IsFinite(V) ? FMath::Clamp(V, 0.f, 1.f) : 0.f; }
float Read(const TMap<FString,float>& C, const TCHAR* Suffix)
{
    const float* V = C.Find(FString(TEXT("CTRL_expressions_")) + Suffix);
    return V ? Unit(*V) : 0.f;
}
float Avg(const TMap<FString,float>& C, std::initializer_list<const TCHAR*> Names)
{
    float V=0; for (const TCHAR* N : Names) V+=Read(C,N); return V/Names.size();
}
}

const TArray<FName>& UDaniyaMouthLibrary::MorphNames()
{
    static const TArray<FName> Names={FName(TEXT("\u3042")),FName(TEXT("\u3044")),FName(TEXT("\u3046")),FName(TEXT("\u3048")),FName(TEXT("\u304a"))};
    return Names;
}

FDaniyaMouthWeights UDaniyaMouthLibrary::Sanitize(FDaniyaMouthWeights W)
{
    W.A=Unit(W.A); W.I=Unit(W.I); W.U=Unit(W.U); W.E=Unit(W.E); W.O=Unit(W.O);
    const float D=FMath::Max(1.f,W.A+W.I+W.U+W.E+W.O);
    W.A/=D; W.I/=D; W.U/=D; W.E/=D; W.O/=D;
    return W;
}

FDaniyaMouthWeights UDaniyaMouthLibrary::BuildDaniyaMouthWeights(const TMap<FString,float>& C, float Gain)
{
    const float J=Unit(Read(C,TEXT("jawOpen"))*(FMath::IsFinite(Gain)?FMath::Max(0.f,Gain):0.f));
    const float F=Unit(Avg(C,{TEXT("mouthFunnelDL"),TEXT("mouthFunnelDR"),TEXT("mouthFunnelUL"),TEXT("mouthFunnelUR")})*2.8f);
    const float P=Unit(Avg(C,{TEXT("mouthLipsPurseDL"),TEXT("mouthLipsPurseDR"),TEXT("mouthLipsPurseUL"),TEXT("mouthLipsPurseUR")})*3.f);
    const float W=Unit((Avg(C,{TEXT("mouthStretchL"),TEXT("mouthStretchR")})+Avg(C,{TEXT("mouthCornerPullL"),TEXT("mouthCornerPullR")}))*2.2f);
    const float Closed=Unit(Avg(C,{TEXT("mouthLipsTogetherDL"),TEXT("mouthLipsTogetherDR"),TEXT("mouthLipsTogetherUL"),TEXT("mouthLipsTogetherUR")})*2.f);
    const float R=FMath::Max(F,P), Q=J*(1.f-Closed*.8f);
    FDaniyaMouthWeights V;
    V.A=Q*(1-R)*(1-.65f*W);
    V.I=W*(1-R)*(1-.55f*J)*FMath::Min(1.f,3*J);
    V.U=P*(1-.7f*J)*FMath::Min(1.f,4*J);
    V.E=Q*W*(1-R)*.7f;
    V.O=Q*R*1.4f;
    // Match the original demo: normalize BEFORE clamping individual weights.
    const float D=FMath::Max(1.f,V.A+V.I+V.U+V.E+V.O);
    V.A=Unit(V.A/D); V.I=Unit(V.I/D); V.U=Unit(V.U/D); V.E=Unit(V.E/D); V.O=Unit(V.O/D);
    return V;
}

TArray<FString> UDaniyaMouthLibrary::RequiredControlNames()
{
    TArray<FString> Out;
    const TCHAR* Names[]={TEXT("jawOpen"),TEXT("mouthFunnelDL"),TEXT("mouthFunnelDR"),TEXT("mouthFunnelUL"),TEXT("mouthFunnelUR"),TEXT("mouthLipsPurseDL"),TEXT("mouthLipsPurseDR"),TEXT("mouthLipsPurseUL"),TEXT("mouthLipsPurseUR"),TEXT("mouthStretchL"),TEXT("mouthStretchR"),TEXT("mouthCornerPullL"),TEXT("mouthCornerPullR"),TEXT("mouthLipsTogetherDL"),TEXT("mouthLipsTogetherDR"),TEXT("mouthLipsTogetherUL"),TEXT("mouthLipsTogetherUR")};
    for (const TCHAR* N:Names) Out.Add(FString(TEXT("CTRL_expressions_"))+N);
    return Out;
}

bool UDaniyaMouthLibrary::ValidateDaniyaControls(const TMap<FString,float>& C, TArray<FString>& Problems)
{
    Problems.Reset();
    for (const FString& N:RequiredControlNames())
    {
        const float* V=C.Find(N);
        if (!V) Problems.Add(TEXT("Missing: ")+N);
        else if (!FMath::IsFinite(*V)) Problems.Add(TEXT("Non-finite: ")+N);
    }
    return Problems.IsEmpty();
}

bool UDaniyaMouthLibrary::ApplyDaniyaMouthWeights(USkeletalMeshComponent* Mesh, FDaniyaMouthWeights W, FString& Error)
{
    Error.Reset();
    if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeletalMeshAsset())) { Error=TEXT("A SkeletalMeshComponent with a mesh is required."); return false; }
    for (FName N:MorphNames()) if (!Mesh->GetSkeletalMeshAsset()->FindMorphTarget(N)) { Error=TEXT("Missing morph: ")+N.ToString(); return false; }
    W=Sanitize(W); const float Values[]={W.A,W.I,W.U,W.E,W.O};
    for (int32 I=0; I<5; ++I) Mesh->SetMorphTarget(MorphNames()[I],Values[I],true);
    return true;
}
