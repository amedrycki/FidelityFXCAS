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

//-------------------------------------------------------------------------------------------------
// RDG Version
//-------------------------------------------------------------------------------------------------

class FFidelityFXCASShaderCS_RDG : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FFidelityFXCASShaderCS_RDG, Global, FIDELITYFXCAS_API);
public:
	//DECLARE_GLOBAL_SHADER(FFidelityFXCASShaderCS_RDG);
	SHADER_USE_PARAMETER_STRUCT(FFidelityFXCASShaderCS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER(FUintVector4, const0)
	SHADER_PARAMETER(FUintVector4, const1)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputTexture)
	SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	//FFidelityFXCASShaderCS_RDG() = default;
	//explicit FFidelityFXCASShaderCS_RDG(const ShaderMetaType::CompiledShaderInitializerType& Initializer);
};

template<bool FP16, bool SHARPEN_ONLY>
class TFidelityFXCASShaderCS_RDG : public FFidelityFXCASShaderCS_RDG
{
	DECLARE_EXPORTED_SHADER_TYPE(TFidelityFXCASShaderCS_RDG, Global, FIDELITYFXCAS_API);
public:
	//DECLARE_GLOBAL_SHADER(TFidelityFXCASShaderCS_RDG);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	TFidelityFXCASShaderCS_RDG() = default;
	explicit TFidelityFXCASShaderCS_RDG(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFidelityFXCASShaderCS_RDG(Initializer) { }
};
