#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FidelityFXCASBlueprintLibrary.generated.h"


UCLASS(MinimalAPI, meta = (ScriptName = "FidelityFXCASLibrary"))
class UFidelityFXCASBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS", meta = (WorldContext = "WorldContextObject"))
	static void DrawToRenderTarget(const UObject* WorldContextObject, class UTextureRenderTarget2D* OutputRenderTarget, class UTexture2D* InInputTexture);

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static bool GetIsFidelityFXSSCASEnabled();

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void SetIsFidelityFXSSCASEnabled(bool Enabled);

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void EnableFidelityFXSSCAS() { SetIsFidelityFXSSCASEnabled(true); }

	UFUNCTION(BlueprintCallable, Category = "FidelityFX | CAS")
	static void DisableFidelityFXSSCAS() { SetIsFidelityFXSSCASEnabled(false); }
};
