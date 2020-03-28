#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FFidelityFXCASShaderCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FFidelityFXCASShaderCS, Global, FIDELITYFXCAS_API);
public:
	//DECLARE_GLOBAL_SHADER(FFidelityFXCASShaderCS);
	SHADER_USE_PARAMETER_STRUCT(FFidelityFXCASShaderCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER(FUintVector4, const0)
	SHADER_PARAMETER(FUintVector4, const1)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, InputTexture)
	SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	//FFidelityFXCASShaderCS() = default;
	//explicit FFidelityFXCASShaderCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
};

template<bool FP16, bool SHARPEN_ONLY>
class TFidelityFXCASShaderCS : public FFidelityFXCASShaderCS
{
	DECLARE_EXPORTED_SHADER_TYPE(TFidelityFXCASShaderCS, Global, FIDELITYFXCAS_API);
public:
	//DECLARE_GLOBAL_SHADER(TFidelityFXCASShaderCS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	TFidelityFXCASShaderCS() = default;
	explicit TFidelityFXCASShaderCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFidelityFXCASShaderCS(Initializer) { }
};
