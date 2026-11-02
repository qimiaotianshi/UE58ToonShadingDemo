#include "SunLensFlareActor.h"
#include "SunLensFlareViewExtension.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneViewExtension.h"
#include "UObject/ConstructorHelpers.h"

ASunLensFlareActor::ASunLensFlareActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    auto MakeComponent = [this](const TCHAR* Name)
    {
        UStaticMeshComponent* Component = CreateDefaultSubobject<UStaticMeshComponent>(Name);
        Component->SetupAttachment(RootComponent);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->CastShadow = false;
        Component->bReceivesDecals = false;
        Component->bVisibleInRayTracing = false;
        Component->bAffectDistanceFieldLighting = false;
        Component->bAffectDynamicIndirectLighting = false;
        // The original geometry is moved into camera space by WPO.
        Component->BoundsScale = 10000.f;
        return Component;
    };
    GhostComponent = MakeComponent(TEXT("StaticMesh_LensFlareGhost"));
    HaloComponent = MakeComponent(TEXT("StaticMesh_LensFlareHalo"));
    GlareComponent = MakeComponent(TEXT("StaticMesh_LensFlareGlare"));
    static ConstructorHelpers::FObjectFinder<USunLensFlareConfig> DefaultConfig(TEXT("/Game/Render/SunLensFlare/DA_MainMapLensFlare.DA_MainMapLensFlare"));
    if (DefaultConfig.Succeeded()) Config = DefaultConfig.Object;
}

void ASunLensFlareActor::RefreshMaterials()
{
    ActiveConfig = Config;
    DynamicMaterials.Reset();
    UStaticMeshComponent* Components[] = { GhostComponent, HaloComponent, GlareComponent };
    UStaticMesh* Meshes[] = {Config ? Config->StaticMeshGhost.Get() : nullptr, Config ? Config->StaticMeshHalo.Get() : nullptr, Config ? Config->StaticMeshGlare.Get() : nullptr};
    UMaterialInterface* Materials[] = {Config ? Config->MaterialGhost.Get() : nullptr, Config ? Config->MaterialHalo.Get() : nullptr, Config ? Config->MaterialGlare.Get() : nullptr};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        Components[Index]->SetStaticMesh(Meshes[Index]);
        Components[Index]->SetVisibility(bEnabled && Meshes[Index] && Materials[Index]);
        UMaterialInstanceDynamic* MID = Meshes[Index] && Materials[Index] ? UMaterialInstanceDynamic::Create(Materials[Index], this) : nullptr;
        Components[Index]->SetMaterial(0, MID);
        DynamicMaterials.Add(MID);
    }
}

void ASunLensFlareActor::EnsureResources()
{
    if (!SunSample)
    {
        SunSample = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
        SunSample->ClearColor = FLinearColor::Transparent;
        SunSample->InitCustomFormat(1, 1, PF_FloatRGBA, true);
        SunSample->UpdateResourceImmediate(true);
    }
    if (!ViewExtension && GetWorld() && GetWorld()->Scene)
        ViewExtension = FSceneViewExtensions::NewExtension<FSunLensFlareViewExtension>(GetWorld());
}

void ASunLensFlareActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshMaterials();
}

void ASunLensFlareActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld() || IsTemplate()) return;
    if (DynamicMaterials.Num() != 3) RefreshMaterials();
    // Fade the old preset out before replacing its geometry and materials.
    if (ActiveConfig != Config && CurrentColor.GetMax() < .05f) RefreshMaterials();
    if (!bEnabled || !ActiveConfig)
    {
        GhostComponent->SetVisibility(false); HaloComponent->SetVisibility(false); GlareComponent->SetVisibility(false);
        if (ViewExtension) ViewExtension->SetEnabled(false);
        CurrentColor = FLinearColor::Transparent;
        return;
    }
    EnsureResources();
    if (!IsValid(SunLight) && bAutoFindSunLight)
    {
        ADirectionalLight* Fallback = nullptr;
        for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
        {
            if (!Fallback) Fallback = *It;
            if (It->GetComponent()->bAtmosphereSunLight && It->GetComponent()->AtmosphereSunLightIndex == 0) { Fallback = *It; break; }
        }
        SunLight = Fallback;
    }
    const FVector SunDirection = IsValid(SunLight) ? SunLight->GetActorForwardVector() : ManualSunRotation.Vector();
    const FLinearColor TargetColor = ActiveConfig == Config ? ColorTint : FLinearColor::Transparent;
    for (int32 Index = 0; Index < 4; ++Index)
        CurrentColor.Component(Index) = FMath::FInterpConstantTo(CurrentColor.Component(Index), TargetColor.Component(Index), DeltaSeconds, ColorFadeRate);
    for (int32 Index = 0; Index < DynamicMaterials.Num(); ++Index)
    {
        if (UMaterialInstanceDynamic* MID = DynamicMaterials[Index])
        {
            MID->SetScalarParameterValue(TEXT("IsSunLensflare"), 1.f);
            MID->SetVectorParameterValue(TEXT("SunDirectionWS"), FLinearColor(SunDirection));
            // Fade the authored tint, then apply brightness. Physical exposure may
            // require a large intensity multiplier without a minutes-long fade.
            MID->SetVectorParameterValue(TEXT("ColorTint"), CurrentColor * Intensity);
            MID->SetScalarParameterValue(TEXT("SceneSampleLumThreshold"), ColorThreshold);
            MID->SetScalarParameterValue(TEXT("SceneSampleLumThresholdRange"), FMath::Max(ColorThresholdRange, .001f));
            MID->SetScalarParameterValue(TEXT("CullCosTheta"), CullCosTheta);
            MID->SetScalarParameterValue(TEXT("CullSmooth"), FMath::Max(CullSmooth, .001f));
            MID->SetScalarParameterValue(TEXT("UseSceneSample"), bSampleScene ? 1.f : 0.f);
            MID->SetTextureParameterValue(TEXT("SunSceneSample"), SunSample);
            if (Index == 0) MID->SetScalarParameterValue(TEXT("RampSample"), RampIndex + .5f);
        }
    }
    UStaticMeshComponent* Components[] = { GhostComponent, HaloComponent, GlareComponent };
    for (int32 Index = 0; Index < 3; ++Index) Components[Index]->SetVisibility(DynamicMaterials[Index] != nullptr);
    if (ViewExtension)
    {
        ViewExtension->Update(SunSample->GameThread_GetRenderTargetResource(), SunDirection, SunSampleRadiusUV, SampleResponse, SkyDepth);
        ViewExtension->SetEnabled(bEnabled && bSampleScene);
    }
}

void ASunLensFlareActor::ReleaseExtension()
{
    if (ViewExtension) ViewExtension->SetEnabled(false);
    ViewExtension.Reset();
}
void ASunLensFlareActor::EndPlay(const EEndPlayReason::Type Reason) { ReleaseExtension(); Super::EndPlay(Reason); }
void ASunLensFlareActor::BeginDestroy() { ReleaseExtension(); Super::BeginDestroy(); }
