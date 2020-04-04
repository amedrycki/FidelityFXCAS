#pragma once

#include "FidelityFXCASPassParams.h"
#include "RenderGraphResources.h"

class FFidelityFXCASPassParams_RDG : public FFidelityFXCASPassParams
{
protected:
	FRDGTextureRef InputTexture;
	FRenderTargetBinding RTBinding;

public:
	FFidelityFXCASPassParams_RDG(const FRDGTextureRef& InInputTexture, const FRenderTargetBinding& InRTBinding)
		: InputTexture(InInputTexture)
		, RTBinding(InRTBinding)
	{
		InputSize = InputTexture != nullptr ? InputTexture->Desc.Extent : FIntPoint::ZeroValue;
		OutputSize = RTBinding.GetTexture() != nullptr ? RTBinding.GetTexture()->Desc.Extent : FIntPoint::ZeroValue;
	}

	FORCEINLINE const FRDGTextureRef& GetInputTexture() const    { return InputTexture; }
	FORCEINLINE const FRenderTargetBinding& GetRTBinding() const { return RTBinding; }
};
