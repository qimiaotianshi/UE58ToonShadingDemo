#pragma once
#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "RenderTargetPool.h"
#include "RHIResources.h"
class FTextureRenderTargetResource;

class FSunLensFlareViewExtension : public FWorldSceneViewExtension
{
public:
    FSunLensFlareViewExtension(const FAutoRegister& AutoRegister, UWorld* World) : FWorldSceneViewExtension(AutoRegister, World) {}
    virtual void SetupViewFamily(FSceneViewFamily&) override {}
    virtual void SetupView(FSceneViewFamily&, FSceneView&) override {}
    virtual void BeginRenderViewFamily(FSceneViewFamily&) override {}
    virtual void PrePostProcessPass_RenderThread(FRDGBuilder&, const FSceneView&, const FPostProcessingInputs&) override;
    void Update(FTextureRenderTargetResource* Target, FVector Direction, float Radius, float Response, float Depth);
    void SetEnabled(bool bInEnabled) { bEnabled.Store(bInEnabled); }
protected:
    virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
    { return bEnabled.Load() && FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context); }
private:
    TAtomic<bool> bEnabled{true};
    FTextureRHIRef OutputTexture;
    FVector SunDirection = FVector(0, 0, -1);
    float RadiusUV = .008f;
    float ResponseSpeed = 12.f;
    float SkyDepth = 0.f;
    TRefCountPtr<IPooledRenderTarget> History;
};
