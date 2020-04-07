#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FidelityFXCASBlueprintLibrary.generated.h"


UCLASS(MinimalAPI, meta = (ScriptName = "FidelityFXCASLibrary"))
class UFidelityFXCASBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	//-------------------------------------------------------------------------------------------------
	// Screen space CAS
	//-------------------------------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static bool GetIsSSCASEnabled();

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void SetIsSSCASEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void ToggleIsSSCASEnabled() { SetIsSSCASEnabled(!GetIsSSCASEnabled()); }

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void EnableSSCAS() { SetIsSSCASEnabled(true); }

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void DisableSSCAS() { SetIsSSCASEnabled(false); }

	// Allows early initialization of compute shader outputs for screen space CAS (i.e. during loading)
	// If not called the outputs will be lazy-loaded during the first render
	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void InitSSCASCSOutputs(const FIntPoint& Size);

	//-------------------------------------------------------------------------------------------------
	// Render to texture CAS
	//-------------------------------------------------------------------------------------------------

	// Allows early initialization of compute shader output for a particular rendertarget (i.e. during loading)
	// If not called the outputs will be lazy-loaded during the first render
	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void InitCSOutput(class UTextureRenderTarget2D* InOutputRenderTarget);

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS", meta = (WorldContext = "WorldContextObject"))
	static void DrawToRenderTarget(/*const UObject* WorldContextObject, */class UTextureRenderTarget2D* InOutputRenderTarget, class UTexture2D* InInputTexture);
};
