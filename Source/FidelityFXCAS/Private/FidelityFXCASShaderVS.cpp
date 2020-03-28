#include "FidelityFXCASShaderVS.h"
#include "CommonRenderResources.h"
#include "Containers/DynamicRHIResourceArray.h"

void FFidelityFXCASVertexBuffer::InitRHI()
{
	//GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.0f, FColor::Silver, FString::Printf(TEXT("Initializing FidelityFX CAS Vertex Buffer...")));

	TResourceArray<FFilterVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
	Vertices.SetNumUninitialized(6);

	Vertices[0].Position = FVector4(-1,  1, 0, 1);
	Vertices[1].Position = FVector4( 1,  1, 0, 1);
	Vertices[2].Position = FVector4(-1, -1, 0, 1);
	Vertices[3].Position = FVector4( 1, -1, 0, 1);

	Vertices[0].UV = FVector2D(0, 0);
	Vertices[1].UV = FVector2D(1, 0);
	Vertices[2].UV = FVector2D(0, 1);
	Vertices[3].UV = FVector2D(1, 1);

	// Create vertex buffer. Fill buffer with initial data upon creation
	FRHIResourceCreateInfo CreateInfo(&Vertices);
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
}

IMPLEMENT_GLOBAL_SHADER(FFidelityFXCASShaderVS, "/Plugin/FidelityFXCAS/Private/TestShader.usf", "MainVertexShader", SF_Vertex);

bool FFidelityFXCASShaderVS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return true;
}
