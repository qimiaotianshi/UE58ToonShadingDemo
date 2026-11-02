#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "SunLensFlareActor.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTextureRenderTarget2D;
class ADirectionalLight;
class FSunLensFlareViewExtension;

UCLASS(BlueprintType)
class SUNLENSFLARE_API USunLensFlareConfig : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<UStaticMesh> StaticMeshGhost;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<UMaterialInterface> MaterialGhost;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<UStaticMesh> StaticMeshHalo;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<UMaterialInterface> MaterialHalo;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<UStaticMesh> StaticMeshGlare;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<UMaterialInterface> MaterialGlare;
};

UCLASS(Blueprintable, meta=(DisplayName="Sun Lens Flare Actor"))
class SUNLENSFLARE_API ASunLensFlareActor : public AActor
{
    GENERATED_BODY()
public:
    ASunLensFlareActor();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<USunLensFlareConfig> Config;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") TObjectPtr<ADirectionalLight> SunLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") bool bAutoFindSunLight = true;
    // Direction in which sunlight travels, matching DirectionalLight's forward vector.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") FRotator ManualSunRotation = FRotator(-25, 0, 0);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare") FLinearColor ColorTint = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare", meta=(ClampMin="0")) float Intensity = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare", meta=(ClampMin="0")) float ColorThreshold = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare", meta=(ClampMin="0.001")) float ColorThresholdRange = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare", meta=(ClampMin="0")) float RampIndex = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare|Visibility", meta=(ClampMin="-1", ClampMax="1")) float CullCosTheta = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare|Visibility", meta=(ClampMin="0.001")) float CullSmooth = .1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare|Visibility", meta=(ClampMin="0.001")) float ColorFadeRate = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare|Visibility") bool bSampleScene = true;
    // Original Kuro PC sampling offsets, measured in scene-buffer UV space.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare|Visibility", meta=(ClampMin="0", ClampMax="0.1")) float SunSampleRadiusUV = .008f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare|Visibility", meta=(ClampMin="0.001")) float SampleResponse = 12.f;
    // Zero tests clear sky only; a positive value also accepts geometry beyond this distance.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens Flare|Visibility", meta=(ClampMin="0")) float SkyDepth = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lens Flare") TObjectPtr<UStaticMeshComponent> GhostComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lens Flare") TObjectPtr<UStaticMeshComponent> HaloComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Lens Flare") TObjectPtr<UStaticMeshComponent> GlareComponent;
    UFUNCTION(BlueprintCallable, Category="Lens Flare") void RefreshMaterials();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void BeginDestroy() override;
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }
private:
    UPROPERTY(Transient) TObjectPtr<USunLensFlareConfig> ActiveConfig;
    UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;
    UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> SunSample;
    FLinearColor CurrentColor = FLinearColor::Transparent;
    TSharedPtr<FSunLensFlareViewExtension, ESPMode::ThreadSafe> ViewExtension;
    void EnsureResources();
    void ReleaseExtension();
};
