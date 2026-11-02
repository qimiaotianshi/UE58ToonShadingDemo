#include "SunLensFlareViewExtension.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphUtils.h"
#include "TextureResource.h"
#include "SceneRenderTargetParameters.h"
#include "PostProcess/PostProcessInputs.h"
#include "SystemTextures.h"
#include "SceneRendering.h"

class FSunLensFlareSampleCS : public FGlobalShader
{
    DECLARE_GLOBAL_SHADER(FSunLensFlareSampleCS);
    SHADER_USE_PARAMETER_STRUCT(FSunLensFlareSampleCS, FGlobalShader);
public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
        SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneColorTexture)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SceneDepthTexture)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HistoryTexture)
        SHADER_PARAMETER_SAMPLER(SamplerState, SampleSampler)
        SHADER_PARAMETER_SAMPLER(SamplerState, DepthSampler)
        SHADER_PARAMETER(FVector2f, SunUV)
        SHADER_PARAMETER(FVector2f, RectMin)
        SHADER_PARAMETER(FVector2f, RectSize)
        SHADER_PARAMETER(FVector2f, InvBufferSize)
        SHADER_PARAMETER(float, RadiusUV)
        SHADER_PARAMETER(float, MixAlpha)
        SHADER_PARAMETER(float, SkyDepth)
        SHADER_PARAMETER(float, SunInView)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputTexture)
    END_SHADER_PARAMETER_STRUCT()
    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    { return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5); }
};
IMPLEMENT_GLOBAL_SHADER(FSunLensFlareSampleCS, "/Plugin/SunLensFlare/SunLensFlareSample.usf", "MainCS", SF_Compute);

void FSunLensFlareViewExtension::Update(FTextureRenderTargetResource* Target, FVector Direction, float Radius, float Response, float Depth)
{
    TSharedRef<FSunLensFlareViewExtension, ESPMode::ThreadSafe> Self = StaticCastSharedRef<FSunLensFlareViewExtension>(AsShared());
    ENQUEUE_RENDER_COMMAND(UpdateSunLensFlare)([Self, Target, Direction, Radius, Response, Depth](FRHICommandListImmediate&)
    {
        Self->OutputTexture = Target ? Target->GetRenderTargetTexture() : nullptr;
        Self->SunDirection = Direction;
        Self->RadiusUV = FMath::Max(0.f, Radius);
        Self->ResponseSpeed = FMath::Max(.001f, Response);
        Self->SkyDepth = Depth;
    });
}

void FSunLensFlareViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs)
{
    if (!bEnabled.Load() || !OutputTexture || !Inputs.SceneTextures || InView.bIsSceneCapture || InView.bIsReflectionCapture) return;
    // One history per actor. Ignore additional views so unrelated editor panels cannot overwrite it.
    if (InView.Family->Views.Num() && InView.Family->Views[0] != &InView) return;
    FRDGTextureRef Color = Inputs.SceneTextures->GetParameters()->SceneColorTexture;
    FRDGTextureRef Depth = Inputs.SceneTextures->GetParameters()->SceneDepthTexture;
    if (!Color || !Depth) return;
    const FVector4 Clip = InView.ViewMatrices.GetWorldToClip().TransformFVector4(FVector4(-SunDirection, 0));
    FVector2f UV(-1.f, -1.f);
    if (Clip.W > UE_SMALL_NUMBER) UV = FVector2f(Clip.X / Clip.W * .5 + .5, .5 - Clip.Y / Clip.W * .5);
    const bool bOnScreen = Clip.W > UE_SMALL_NUMBER && UV.X >= 0.f && UV.X <= 1.f && UV.Y >= 0.f && UV.Y <= 1.f;
    FRDGTextureRef Previous = History && !InView.bCameraCut ? GraphBuilder.RegisterExternalTexture(History) : GSystemTextures.GetBlackDummy(GraphBuilder);
    FRDGTextureRef Sample = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(1,1), PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV), TEXT("SunLensFlare.Sample"));
    auto* Parameters = GraphBuilder.AllocParameters<FSunLensFlareSampleCS::FParameters>();
    Parameters->View = InView.ViewUniformBuffer;
    Parameters->SceneColorTexture = Color;
    Parameters->SceneDepthTexture = Depth;
    Parameters->HistoryTexture = Previous;
    Parameters->SampleSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->DepthSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    Parameters->SunUV = UV;
    // ViewRect is the actual buffer rectangle when screen percentage is enabled.
    const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(InView);
    Parameters->RectMin = FVector2f(ViewInfo.ViewRect.Min);
    Parameters->RectSize = FVector2f(ViewInfo.ViewRect.Size());
    Parameters->InvBufferSize = FVector2f(1.f / Color->Desc.Extent.X, 1.f / Color->Desc.Extent.Y);
    Parameters->RadiusUV = RadiusUV;
    const float Delta = InView.Family->Time.GetDeltaWorldTimeSeconds();
    Parameters->MixAlpha = History && !InView.bCameraCut ? 1.f - FMath::Exp(-ResponseSpeed * FMath::Max(Delta, 1.f/240.f)) : 1.f;
    Parameters->SkyDepth = SkyDepth;
    Parameters->SunInView = bOnScreen ? 1.f : 0.f;
    Parameters->OutputTexture = GraphBuilder.CreateUAV(Sample);
    TShaderMapRef<FSunLensFlareSampleCS> Shader(GetGlobalShaderMap(InView.GetFeatureLevel()));
    FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("SunLensFlare.SampleSun"), Shader, Parameters, FIntVector(1,1,1));
    FRDGTextureRef Output = GraphBuilder.RegisterExternalTexture(CreateRenderTarget(OutputTexture, TEXT("SunLensFlare.MaterialSample")));
    AddCopyTexturePass(GraphBuilder, Sample, Output);
    GraphBuilder.QueueTextureExtraction(Sample, &History);
}
