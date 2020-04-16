#pragma once

#if FX_CAS_PLUGIN_ENABLED

#include "FidelityFXCASShaderCompilationRules.h"

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

//-------------------------------------------------------------------------------------------------
// RHI Version
//-------------------------------------------------------------------------------------------------

class FFidelityFXCASShaderPS_RHI : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFidelityFXCASShaderPS_RHI);
	SHADER_USE_PARAMETER_STRUCT(FFidelityFXCASShaderPS_RHI, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, UpscaledTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, samLinearClamp)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FFidelityFXCASShaderCompilationRules::ShouldCompilePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFidelityFXCASShaderPS_RHI, "/Plugin/FidelityFXCAS/Private/CAS_ShaderPS.usf", "mainPS", SF_Pixel);

#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
//-------------------------------------------------------------------------------------------------
// RDG Version
//-------------------------------------------------------------------------------------------------

class FFidelityFXCASShaderPS_RDG : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFidelityFXCASShaderPS_RDG);
	SHADER_USE_PARAMETER_STRUCT(FFidelityFXCASShaderPS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, UpscaledTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, samLinearClamp)
	RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FFidelityFXCASShaderCompilationRules::ShouldCompilePermutation(Parameters);
	}
};

IMPLEMENT_GLOBAL_SHADER(FFidelityFXCASShaderPS_RDG, "/Plugin/FidelityFXCAS/Private/CAS_ShaderPS.usf", "mainPS", SF_Pixel);

#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
#endif // FX_CAS_PLUGIN_ENABLED
