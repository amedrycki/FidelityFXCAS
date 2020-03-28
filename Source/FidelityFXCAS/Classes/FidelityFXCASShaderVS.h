#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

class FFidelityFXCASVertexBuffer : public FVertexBuffer
{
public:
	void InitRHI();
};
TGlobalResource<FFidelityFXCASVertexBuffer> GFidelityFXCASVertexBuffer;

class FFidelityFXCASShaderVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFidelityFXCASShaderVS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);

	FFidelityFXCASShaderVS() = default;
	FFidelityFXCASShaderVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) { }
};
