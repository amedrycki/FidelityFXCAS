#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FFidelityFXCASShaderPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFidelityFXCASShaderPS);
	SHADER_USE_PARAMETER_STRUCT(FFidelityFXCASShaderPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, UpscaledTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, samLinearClamp)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

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
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};
