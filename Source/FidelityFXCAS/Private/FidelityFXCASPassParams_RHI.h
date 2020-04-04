#pragma once

#include "FidelityFXCASPassParams.h"
#include "Engine/TextureRenderTarget2D.h"

class FFidelityFXCASPassParams_RHI : public FFidelityFXCASPassParams
{
protected:
	FTextureRHIRef InputTexture;
	FTextureRHIRef RTTexture;

public:
	FFidelityFXCASPassParams_RHI(const FTextureRHIRef& InInputTexture, const FTextureRHIRef& InRTTexture)
		: InputTexture(InInputTexture)
		, RTTexture(InRTTexture)
	{
		InputSize = InputTexture.IsValid() ? FIntPoint(InputTexture->GetSizeXYZ().X, InputTexture->GetSizeXYZ().Y) : FIntPoint::ZeroValue;
		OutputSize = RTTexture.IsValid() ? FIntPoint(RTTexture->GetSizeXYZ().X, RTTexture->GetSizeXYZ().Y) : FIntPoint::ZeroValue;
	}

	FORCEINLINE const FTextureRHIRef& GetInputTexture() const  { return InputTexture; }
	FORCEINLINE const FTextureRHIRef& GetRTTexture() const     { return RTTexture; }
};

