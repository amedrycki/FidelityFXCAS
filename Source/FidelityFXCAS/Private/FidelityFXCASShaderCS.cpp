
#if FX_CAS_PLUGIN_ENABLED

#include "FidelityFXCASShaderCS.h"
#include "FidelityFXCASShaderCompilationRules.h"
#include "Misc/EngineVersionComparison.h"

//-------------------------------------------------------------------------------------------------
// RHI Version
//-------------------------------------------------------------------------------------------------

#if !UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.25
IMPLEMENT_TYPE_LAYOUT(FFidelityFXCASShaderCS_RHI);
#endif	// UE v4.25

IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP32_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP32_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
#if FX_CAS_FP16_ENABLED
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP16_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RHI_FP16_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
#endif // FX_CAS_FP16_ENABLED

bool FFidelityFXCASShaderCS_RHI::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FFidelityFXCASShaderCompilationRules::ShouldCompilePermutation(Parameters);
}

void FFidelityFXCASShaderCS_RHI::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("PLATFORM_PS4"), Parameters.Platform == EShaderPlatform::SP_PS4 ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("WIDTH"), 64);
	OutEnvironment.SetDefine(TEXT("HEIGHT"), 1);
	OutEnvironment.SetDefine(TEXT("DEPTH"),  1);
}

template<bool FP16, bool SHARPEN_ONLY>
bool TFidelityFXCASShaderCS_RHI<FP16, SHARPEN_ONLY>::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FFidelityFXCASShaderCompilationRules::ShouldCompilePermutationCS<FP16>(Parameters);
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

#if !UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.25
IMPLEMENT_TYPE_LAYOUT(FFidelityFXCASShaderCS_RDG);
#endif	// UE v4.25

IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP32_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP32_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
#if FX_CAS_FP16_ENABLED
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP16_Upscale,     TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_RDG_FP16_SharpenOnly, TEXT("/Plugin/FidelityFXCAS/Private/CAS_ShaderCS.usf"), TEXT("mainCS"), SF_Compute);
#endif // FX_CAS_FP16_ENABLED

bool FFidelityFXCASShaderCS_RDG::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FFidelityFXCASShaderCompilationRules::ShouldCompilePermutation(Parameters);
}

void FFidelityFXCASShaderCS_RDG::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("PLATFORM_PS4"), Parameters.Platform == EShaderPlatform::SP_PS4 ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("WIDTH"), 64);
	OutEnvironment.SetDefine(TEXT("HEIGHT"), 1);
	OutEnvironment.SetDefine(TEXT("DEPTH"), 1);
}

template<bool FP16, bool SHARPEN_ONLY>
bool TFidelityFXCASShaderCS_RDG<FP16, SHARPEN_ONLY>::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FFidelityFXCASShaderCompilationRules::ShouldCompilePermutationCS<FP16>(Parameters);
}

template<bool FP16, bool SHARPEN_ONLY>
void TFidelityFXCASShaderCS_RDG<FP16, SHARPEN_ONLY>::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FFidelityFXCASShaderCS_RDG::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_FP16"), FP16 ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_SHARPEN_ONLY"), SHARPEN_ONLY ? 1 : 0);
}

#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
#endif // FX_CAS_PLUGIN_ENABLED
