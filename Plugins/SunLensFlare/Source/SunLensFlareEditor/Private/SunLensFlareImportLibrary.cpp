#include "SunLensFlareImportLibrary.h"
#include "Editor.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "StaticParameterSet.h"
#include "EditorViewportClient.h"
#include "ShaderCompiler.h"
#include "Modules/ModuleManager.h"

bool USunLensFlareImportLibrary::ImportProperties(UObject* Object, const FString& Text)
{
    if (!Object) return false;
    FImportObjectParams Params;
    Params.DestData = reinterpret_cast<uint8*>(Object);
    Params.SourceText = *Text;
    Params.ObjectStruct = Object->GetClass();
    Params.SubobjectRoot = Object;
    Params.SubobjectOuter = Object;
    Params.Warn = GWarn;
    Params.bShouldCallEditChange = false;
    return ImportObjectProperties(Params) != nullptr;
}
bool USunLensFlareImportLibrary::ImportMaterialProperties(UMaterial* Material, const FString& Text)
{
    return Material && ImportProperties(Material->GetEditorOnlyData(), Text);
}
FString USunLensFlareImportLibrary::GetMaterialErrors(UMaterial* Material)
{
    if (!Material) return TEXT("Null material");
    FMaterialResource* Resource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
    if (!Resource) return TEXT("No material resource");
    Resource->FinishCompilation();
    FString Result = FString::Join(Resource->GetCompileErrors(), TEXT("\n"));
    for (UMaterialExpression* Expression : Resource->GetErrorExpressions())
    {
        if (!Expression) continue;
        Result += TEXT("\nExpression: ") + Expression->GetPathName();
        for (FExpressionInput* Input : Expression->GetInputsView())
            if (Input && Input->Expression) Result += FString::Printf(TEXT("\n  <- %s [%d]"), *Input->Expression->GetPathName(), Input->OutputIndex);
    }
    return Result;
}
bool USunLensFlareImportLibrary::SetStaticSwitch(UMaterialInstanceConstant* Instance, FName Name, bool Value)
{
    if (!Instance) return false;
    TArray<FMaterialParameterInfo> Infos;
    TArray<FGuid> Guids;
    Instance->GetAllStaticSwitchParameterInfo(Infos, Guids);
    FStaticParameterSet Parameters = Instance->GetStaticParameters();
    for (int32 Index = 0; Index < Infos.Num(); ++Index)
    {
        if (Infos[Index].Name != Name) continue;
        FStaticSwitchParameter* Parameter = Parameters.StaticSwitchParameters.FindByPredicate([&](const FStaticSwitchParameter& P) { return P.ParameterInfo == Infos[Index]; });
        if (!Parameter) Parameter = &Parameters.StaticSwitchParameters.AddDefaulted_GetRef();
        Parameter->ParameterInfo = Infos[Index];
        Parameter->ExpressionGUID = Guids[Index];
        Parameter->bOverride = true;
        Parameter->Value = Value;
        Instance->UpdateStaticPermutation(Parameters);
        Instance->MarkPackageDirty();
        return true;
    }
    return false;
}
void USunLensFlareImportLibrary::EnablePreviewViewport()
{
    if (GEditor && GEditor->GetActiveViewport())
    {
        FEditorViewportClient* Client = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
        if (Client) Client->SetRealtime(true);
    }
}
void USunLensFlareImportLibrary::ClearExpressions(UObject* Asset)
{
    // Detach old expressions without invalidating the array while iterating over it.
    // Existing editor/preview references keep the old objects alive until normal GC.
    if (UMaterial* Material = Cast<UMaterial>(Asset))
    {
        Material->GetExpressionCollection().Empty();
        Material->GetEditorOnlyData()->FrontMaterial.Expression = nullptr;
        Material->GetEditorOnlyData()->MaterialAttributes.Expression = nullptr;
    }
    else if (UMaterialFunction* Function = Cast<UMaterialFunction>(Asset))
    {
        Function->GetExpressionCollection().Empty();
    }
}
void USunLensFlareImportLibrary::LinkFunctionCalls(UObject* Asset)
{
    UMaterial* Material = Cast<UMaterial>(Asset);
    UMaterialFunction* Function = Cast<UMaterialFunction>(Asset);
    if (!Material && !Function) return;
    TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions = Material ? Material->GetExpressions() : Function->GetExpressions();
    for (UMaterialExpression* Expression : Expressions)
    {
        Expression->Material = Material;
        Expression->Function = Function;
    }
    for (UMaterialExpression* Expression : Expressions)
    {
        if (UMaterialExpressionMaterialFunctionCall* Call = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
            Call->UpdateFromFunctionResource(false);
    }
}
IMPLEMENT_MODULE(FDefaultModuleImpl, SunLensFlareEditor)
