#include "FidelityFXCASShaderCS.h"

//IMPLEMENT_GLOBAL_SHADER(FFidelityFXCASShaderCS, "/Plugin/FidelityFXCAS/Private/CAS_Shader.usf", "mainCS", SF_Compute);
IMPLEMENT_SHADER_TYPE(FIDELITYFXCAS_API, FFidelityFXCASShaderCS, TEXT("/Plugin/FidelityFXCAS/Private/CAS_Shader.usf"), TEXT("mainCS"), SF_Compute);

typedef TFidelityFXCASShaderCS<0, 0> TFidelityFXCASShaderCS_0_0;
typedef TFidelityFXCASShaderCS<0, 1> TFidelityFXCASShaderCS_0_1;
typedef TFidelityFXCASShaderCS<1, 0> TFidelityFXCASShaderCS_1_0;
typedef TFidelityFXCASShaderCS<1, 1> TFidelityFXCASShaderCS_1_1;

//IMPLEMENT_GLOBAL_SHADER(TFidelityFXCASShaderCS_0_0, "/Plugin/FidelityFXCAS/Private/CAS_Shader.usf", "mainCS", SF_Compute);
//IMPLEMENT_GLOBAL_SHADER(TFidelityFXCASShaderCS_0_1, "/Plugin/FidelityFXCAS/Private/CAS_Shader.usf", "mainCS", SF_Compute);
//IMPLEMENT_GLOBAL_SHADER(TFidelityFXCASShaderCS_1_0, "/Plugin/FidelityFXCAS/Private/CAS_Shader.usf", "mainCS", SF_Compute);
//IMPLEMENT_GLOBAL_SHADER(TFidelityFXCASShaderCS_1_1, "/Plugin/FidelityFXCAS/Private/CAS_Shader.usf", "mainCS", SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_0_0, TEXT("/Plugin/FidelityFXCAS/Private/CAS_Shader.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_0_1, TEXT("/Plugin/FidelityFXCAS/Private/CAS_Shader.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_1_0, TEXT("/Plugin/FidelityFXCAS/Private/CAS_Shader.usf"), TEXT("mainCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<> FIDELITYFXCAS_API, TFidelityFXCASShaderCS_1_1, TEXT("/Plugin/FidelityFXCAS/Private/CAS_Shader.usf"), TEXT("mainCS"), SF_Compute);

//FFidelityFXCASShaderCS::FFidelityFXCASShaderCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
//	: FGlobalShader(Initializer)
//{
//}

bool FFidelityFXCASShaderCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FFidelityFXCASShaderCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	//OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_FP16"), 0);
	//OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_SHARPEN_ONLY"), 1);

	OutEnvironment.SetDefine(TEXT("WIDTH"), 64);
	OutEnvironment.SetDefine(TEXT("HEIGHT"), 1);
	OutEnvironment.SetDefine(TEXT("DEPTH"),  1);
}

template<bool FP16, bool SHARPEN_ONLY>
bool TFidelityFXCASShaderCS<FP16, SHARPEN_ONLY>::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return FFidelityFXCASShaderCS::ShouldCompilePermutation(Parameters);
}

template<bool FP16, bool SHARPEN_ONLY>
void TFidelityFXCASShaderCS<FP16, SHARPEN_ONLY>::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FFidelityFXCASShaderCS::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_FP16"), FP16 ? 1 : 0);
	OutEnvironment.SetDefine(TEXT("CAS_SAMPLE_SHARPEN_ONLY"), SHARPEN_ONLY ? 1 : 0);
}
