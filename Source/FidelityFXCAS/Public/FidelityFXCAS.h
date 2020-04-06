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
	void UpdateSSCASEnabled();
protected:
	bool bIsSSCASEnabled;

	// SSCAS callbacks management
	void BindResolvedSceneColorCallback(IRendererModule* RendererModule);   // No upscale
	void UnbindResolvedSceneColorCallback(IRendererModule* RendererModule);
	void BindCustomUpscaleCallback(IRendererModule* RendererModule);        // Upscale
	void UnbindCustomUpscaleCallback(IRendererModule* RendererModule);

	// Screen space shader params
public:
	FORCEINLINE float GetSSCASSharpness() const         { return SSCASSharpness; }
	FORCEINLINE void SetSSCASSharpness(float Sharpness) { SSCASSharpness = Sharpness; }
	FORCEINLINE bool GetUseFP16() const                 { return bUseFP16; }
	FORCEINLINE void SetUseFP16(bool UseFP16)           { bUseFP16 = UseFP16; }
protected:
	float SSCASSharpness = 0.0f;
	bool bUseFP16 = false;

	// Compute shader output
	TRefCountPtr<IPooledRenderTarget> ComputeShaderOutput_RHI;
	TRefCountPtr<IPooledRenderTarget> ComputeShaderOutput_RDG;
	void PrepareComputeShaderOutput(FRHICommandListImmediate& RHICmdList, const FIntPoint& OutputSize, TRefCountPtr<IPooledRenderTarget>& CSOutput);

	// SSCAS (no upscale) using Renderer's ResolvedSceneColor callback (RHI)
	FDelegateHandle OnResolvedSceneColorHandle;	// Post process render pipeline hook and handle
	void OnResolvedSceneColor_RenderThread(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext);

	// SSCAS (with upscale) using custom Renderer callback (RDG)
	void OnAddUpscalePass_RenderThread(class FRDGBuilder& GraphBuilder, class FRDGTexture* SceneColor, const FRenderTargetBinding& RTBinding);

	// Compute shader call
	void RunComputeShader_RHI_RenderThread(FRHICommandListImmediate& RHICmdList, const class FFidelityFXCASPassParams_RHI& CASPassParams);
	void RunComputeShader_RDG_RenderThread(FRDGBuilder& GraphBuilder, const class FFidelityFXCASPassParams_RDG& CASPassParams);
	static FIntVector GetDispatchGroupCount(FIntPoint OutputSize);

	// Pixel shader draw
	void DrawToRenderTarget_RHI_RenderThread(FRHICommandListImmediate& RHICmdList, const class FFidelityFXCASPassParams_RHI& CASPassParams);
	void DrawToRenderTarget_RDG_RenderThread(FRDGBuilder& GraphBuilder, const class FFidelityFXCASPassParams_RDG& CASPassParams);

	// Resolution info
	mutable FCriticalSection ResolutionInfoCS;
	FIntPoint InputResolution;
	FIntPoint OutputResolution;
public:
	void GetSSCASResolutionInfo(FIntPoint& OutInputResolution, FIntPoint& OutOutputResolution) const;
	FORCEINLINE FIntPoint GetSSCASInputResolution() const  { FScopeLock Lock(&ResolutionInfoCS); return InputResolution; }
	FORCEINLINE FIntPoint GetSSCASOutputResolution() const { FScopeLock Lock(&ResolutionInfoCS); return OutputResolution; }
protected:
	void SetSSCASResolutionInfo(const FIntPoint& InInputResolution, const FIntPoint& InOutputResolution);
};
