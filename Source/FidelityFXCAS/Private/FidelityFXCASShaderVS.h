#pragma once

#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
//#include "Containers/DynamicRHIResourceArray.h"

//-------------------------------------------------------------------------------------------------
// Vertex buffer resource
//-------------------------------------------------------------------------------------------------

class FFidelityFXCASVertexBuffer : public FVertexBuffer
{
public:
	void InitRHI()
	{
		TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(4);

		Vertices[0].Position = FVector4(-1, 1, 0, 1);
		Vertices[1].Position = FVector4(1, 1, 0, 1);
		Vertices[2].Position = FVector4(-1, -1, 0, 1);
		Vertices[3].Position = FVector4(1, -1, 0, 1);

		Vertices[0].UV = FVector2D(0, 0);
		Vertices[1].UV = FVector2D(1, 0);
		Vertices[2].UV = FVector2D(0, 1);
		Vertices[3].UV = FVector2D(1, 1);

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};
TGlobalResource<FFidelityFXCASVertexBuffer> GFidelityFXCASVertexBuffer;

//-------------------------------------------------------------------------------------------------
// Vertex shader (one version for RHI and RDG)
//-------------------------------------------------------------------------------------------------

class FFidelityFXCASShaderVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FFidelityFXCASShaderVS);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return true; }

	FFidelityFXCASShaderVS() = default;
	FFidelityFXCASShaderVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) { }
};

IMPLEMENT_GLOBAL_SHADER(FFidelityFXCASShaderVS, "/Plugin/FidelityFXCAS/Private/CAS_ShaderVS.usf", "mainVS", SF_Vertex);
