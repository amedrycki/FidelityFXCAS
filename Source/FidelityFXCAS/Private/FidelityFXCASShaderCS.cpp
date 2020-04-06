#include "FidelityFXCASShaderCS.h"

//-------------------------------------------------------------------------------------------------
// RHI Version
//-------------------------------------------------------------------------------------------------

IMPLEMENT_SHADER_TYPE(FIDELITYFXCAS_API, FFidelityFXCASShaderCS_RHI, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);

IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP32_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP32_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP16_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP16_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);

bool FFidelityFXCASShaderCS_RHI::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FFidelityFXCASShaderCS_RHI::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("WIDTH"), 64);
	OutEnvironment.SetDefine(TEXT("HEIGHT"), 1);
	OutEnvironment.SetDefine(TEXT("DEPTH"),  1);
}

template<bool FP16, bool SHARPEN_ONLY>
bool TFidelityFXCASShaderCS_RHI<FP16, SHARPEN_ONLY>::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FFidelityFXCASShaderCS_RHI::ShouldCompilePermutation(Parameters);
}

template<bool FP16, bool SHARPEN_ONLY>
void TFidelityFXCASShaderCS_RHI<FP16, SHARPEN_ONLY>::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FFidelityFXCASShaderCS_RHI::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_FP16"), FP16 ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_SHARPEN_ONLY"), SHARPEN_ONLY ? 1 : 0);
}

#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
//-------------------------------------------------------------------------------------------------
// RDG Version
//-------------------------------------------------------------------------------------------------

IMPLEMENT_SHADER_TYPE(FIDELITYFXCAS_API, FFidelityFXCASShaderCS_RDG, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);

IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP32_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP32_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP16_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP16_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);

bool FFidelityFXCASShaderCS_RDG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FFidelityFXCASShaderCS_RDG::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("WIDTH"), 64);
	OutEnvironment.SetDefine(TEXT("HEIGHT"), 1);
	OutEnvironment.SetDefine(TEXT("DEPTH"), 1);
}

template<bool FP16, bool SHARPEN_ONLY>
bool TFidelityFXCASShaderCS_RDG<FP16, SHARPEN_ONLY>::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FFidelityFXCASShaderCS_RDG::ShouldCompilePermutation(Parameters);
}

template<bool FP16, bool SHARPEN_ONLY>
void TFidelityFXCASShaderCS_RDG<FP16, SHARPEN_ONLY>::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FFidelityFXCASShaderCS_RDG::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_FP16"), FP16 ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_SHARPEN_ONLY"), SHARPEN_ONLY ? 1 : 0);
}

#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
