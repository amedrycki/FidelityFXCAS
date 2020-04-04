#pragma once

#include "RHIResources.h"
#include "RendererInterface.h"

class FFidelityFXCASPassParams
{
	static FTextureRHIRef EMPTY_TextureRHIRef;
	static FSceneRenderTargetItem EMPTY_SceneRenderTargetItem;
	static FUnorderedAccessViewRHIRef EMPTY_UnorderedAccessViewRHIRef;

protected:
	FIntPoint InputSize = FIntPoint::ZeroValue;
	FIntPoint OutputSize = FIntPoint::ZeroValue;

public:
	TRefCountPtr<IPooledRenderTarget> CSOutput;

	FORCEINLINE const FIntPoint& GetInputSize() const  { return InputSize; }
	FORCEINLINE const FIntPoint& GetOutputSize() const { return OutputSize; }

	FORCEINLINE const FSceneRenderTargetItem& GetCSOutputRTItem() const    { return CSOutput.IsValid() ? CSOutput->GetRenderTargetItem() : EMPTY_SceneRenderTargetItem; }
	FORCEINLINE const FUnorderedAccessViewRHIRef& GetUAV() const           { return GetCSOutputRTItem().IsValid() ? GetCSOutputRTItem().UAV : EMPTY_UnorderedAccessViewRHIRef; }
	FORCEINLINE const FTextureRHIRef& GetCSOutputTargetableTexture() const { return GetCSOutputRTItem().IsValid() ? GetCSOutputRTItem().TargetableTexture : EMPTY_TextureRHIRef; }
};

FTextureRHIRef FFidelityFXCASPassParams::EMPTY_TextureRHIRef;
FSceneRenderTargetItem FFidelityFXCASPassParams::EMPTY_SceneRenderTargetItem;
FUnorderedAccessViewRHIRef FFidelityFXCASPassParams::EMPTY_UnorderedAccessViewRHIRef;
