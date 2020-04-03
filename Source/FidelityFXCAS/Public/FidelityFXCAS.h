// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FFidelityFXCASModule : public IModuleInterface
{
public:
	// Static accessors
	static FORCEINLINE FFidelityFXCASModule& Get() { return FModuleManager::LoadModuleChecked<FFidelityFXCASModule>("FidelityFXCAS"); }
	static FORCEINLINE bool IsAvailable()          { return FModuleManager::Get().IsModuleLoaded("FidelityFXCAS"); }

	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Screen space post process CAS enable / disable
	bool GetIsSSCASEnabled() const;
	void SetIsSSCASEnabled(bool Enabled);
	FORCEINLINE void EnableSSCAS()  { SetIsSSCASEnabled(true); }
	FORCEINLINE void DisableSSCAS() { SetIsSSCASEnabled(false); }

	// Screen space shader params
protected:
	float SSCASSharpness = 0.0f;
public:
	FORCEINLINE float GetSSCASSharpness() const         { return SSCASSharpness; }
	FORCEINLINE void SetSSCASSharpness(float Sharpness) { SSCASSharpness = Sharpness; }

protected:
	// SSCAS (no upscale) using Renderer's ResolvedSceneColor callback
	TRefCountPtr<IPooledRenderTarget> ComputeShaderOutput;
	FDelegateHandle OnResolvedSceneColorHandle;	// Post process render pipeline hook and handle
	void OnResolvedSceneColor_RenderThread(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext);

	// SSCAS (with upscale) using custom Renderer modification callback
	bool bIsSSCASEnabled;
	void OnAddUpscalePass_RenderThread(class FRDGBuilder& GraphBuilder, class FRDGTexture* SceneColor, const FRenderTargetBinding& RTBinding);

	// Resolution info
	mutable FCriticalSection ResolutionInfoCS;
	FIntPoint InputResolution;
	FIntPoint OutputResolution;
public:
	void GetSSCASResolutionInfo(FIntPoint& OutInputResolution, FIntPoint& OutOutputResolution) const;
	FORCEINLINE FIntPoint GetSSCASInputResolution() const  { FScopeLock Lock(&ResolutionInfoCS); return InputResolution; }
	FORCEINLINE FIntPoint GetSSCASOutputResolution() const { FScopeLock Lock(&ResolutionInfoCS); return OutputResolution; }

	// TEST
	void TEST_SetRT(class UTextureRenderTarget2D* OutputRenderTarget) { TEST_RT = OutputRenderTarget; }
	void TEST_SetTexture(class UTexture2D* InInputTexture)            { TEST_InputTexture = InInputTexture; }
	class UTextureRenderTarget2D* TEST_RT;
	class UTexture2D* TEST_InputTexture;
};
