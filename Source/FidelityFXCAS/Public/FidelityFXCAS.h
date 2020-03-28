// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FFidelityFXCASModule : public IModuleInterface
{
public:
	// Static accessors
	static FORCEINLINE FFidelityFXCASModule& Get()	{ return FModuleManager::LoadModuleChecked<FFidelityFXCASModule>("FidelityFXCAS"); }
	static FORCEINLINE bool IsAvailable()			{ return FModuleManager::Get().IsModuleLoaded("FidelityFXCAS"); }

	// IModuleInterface implementation
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Screen space post process CAS
	bool GetIsSSCASEnabled() const;
	void SetIsSSCASEnabled(bool Enabled);
	FORCEINLINE void EnableSSCAS() { SetIsSSCASEnabled(true); }
	FORCEINLINE void DisableSSCAS() { SetIsSSCASEnabled(false); }
protected:
	FDelegateHandle OnResolvedSceneColorHandle;
	TRefCountPtr<IPooledRenderTarget> ComputeShaderOutput;
	void OnResolvedSceneColor_RenderThread(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext);
	// Screen space shader params
	float SSCASSharpness = 0.0f;
public:
	FORCEINLINE float GetSSCASSharpness() const { return SSCASSharpness; }
	FORCEINLINE void SetSSCASSharpness(float Sharpness) { SSCASSharpness = Sharpness; }

	// TEST
public:
	void TEST_SetRT(class UTextureRenderTarget2D* OutputRenderTarget) { TEST_RT = OutputRenderTarget; }
	void TEST_SetTexture(class UTexture2D* InInputTexture) { TEST_InputTexture = InInputTexture; }
	class UTextureRenderTarget2D* TEST_RT;
	class UTexture2D* TEST_InputTexture;
};
