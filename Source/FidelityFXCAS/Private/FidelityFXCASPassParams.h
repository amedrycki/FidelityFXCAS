#pragma once

#include "RHIResources.h"
#include "RendererInterface.h"

//-------------------------------------------------------------------------------------------------
// Base class
//-------------------------------------------------------------------------------------------------

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

//-------------------------------------------------------------------------------------------------
// RHI Version
//-------------------------------------------------------------------------------------------------

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

	FORCEINLINE const FTextureRHIRef& GetInputTexture() const { return InputTexture; }
	FORCEINLINE const FTextureRHIRef& GetRTTexture() const    { return RTTexture; }
};

//-------------------------------------------------------------------------------------------------
// RDG Version
//-------------------------------------------------------------------------------------------------

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
