#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

//-------------------------------------------------------------------------------------------------
// RHI Version
//-------------------------------------------------------------------------------------------------

class FFidelityFXCASShaderCS_RHI : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FFidelityFXCASShaderCS_RHI, Global, FIDELITYFXCAS_API);
public:
	SHADER_USE_PARAMETER_STRUCT(FFidelityFXCASShaderCS_RHI, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER(FUintVector4, const0)
	SHADER_PARAMETER(FUintVector4, const1)
	SHADER_PARAMETER_TEXTURE(Texture2D<float4>, InputTexture)
	SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

template<bool FP16, bool SHARPEN_ONLY>
class TFidelityFXCASShaderCS_RHI : public FFidelityFXCASShaderCS_RHI
{
	DECLARE_EXPORTED_SHADER_TYPE(TFidelityFXCASShaderCS_RHI, Global, FIDELITYFXCAS_API);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	TFidelityFXCASShaderCS_RHI() = default;
	explicit TFidelityFXCASShaderCS_RHI(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFidelityFXCASShaderCS_RHI(Initializer) { }
};

typedef TFidelityFXCASShaderCS_RHI<0, 0> TFidelityFXCASShaderCS_RHI_FP32_Upscale;
typedef TFidelityFXCASShaderCS_RHI<0, 1> TFidelityFXCASShaderCS_RHI_FP32_SharpenOnly;
typedef TFidelityFXCASShaderCS_RHI<1, 0> TFidelityFXCASShaderCS_RHI_FP16_Upscale;
typedef TFidelityFXCASShaderCS_RHI<1, 1> TFidelityFXCASShaderCS_RHI_FP16_SharpenOnly;

//-------------------------------------------------------------------------------------------------
// RDG Version
//-------------------------------------------------------------------------------------------------

class FFidelityFXCASShaderCS_RDG : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FFidelityFXCASShaderCS_RDG, Global, FIDELITYFXCAS_API);
public:
	SHADER_USE_PARAMETER_STRUCT(FFidelityFXCASShaderCS_RDG, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER(FUintVector4, const0)
	SHADER_PARAMETER(FUintVector4, const1)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float4>, InputTexture)
	SHADER_PARAMETER_UAV(RWTexture2D<float4>, OutputTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

template<bool FP16, bool SHARPEN_ONLY>
class TFidelityFXCASShaderCS_RDG : public FFidelityFXCASShaderCS_RDG
{
	DECLARE_EXPORTED_SHADER_TYPE(TFidelityFXCASShaderCS_RDG, Global, FIDELITYFXCAS_API);
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	TFidelityFXCASShaderCS_RDG() = default;
	explicit TFidelityFXCASShaderCS_RDG(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FFidelityFXCASShaderCS_RDG(Initializer) { }
};

typedef TFidelityFXCASShaderCS_RDG<0, 0> TFidelityFXCASShaderCS_RDG_FP32_Upscale;
typedef TFidelityFXCASShaderCS_RDG<0, 1> TFidelityFXCASShaderCS_RDG_FP32_SharpenOnly;
typedef TFidelityFXCASShaderCS_RDG<1, 0> TFidelityFXCASShaderCS_RDG_FP16_Upscale;
typedef TFidelityFXCASShaderCS_RDG<1, 1> TFidelityFXCASShaderCS_RDG_FP16_SharpenOnly;