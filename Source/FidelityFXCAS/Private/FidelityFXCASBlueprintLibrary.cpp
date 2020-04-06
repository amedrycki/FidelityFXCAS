#include "FidelityFXCASBlueprintLibrary.h"

#include "Engine/Classes/Engine/CanvasRenderTarget2D.h"
#include "Engine/Classes/Engine/TextureRenderTarget2D.h"
#include "Engine/Classes/Engine/World.h"
#include "Runtime/Core/Public/HAL/ThreadSafeBool.h"
#include "Runtime/RenderCore/Public/RenderTargetPool.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "SceneUtils.h"
#include "SceneInterface.h"
#include "ShaderParameterUtils.h"
#include "Logging/MessageLog.h"



static const uint32 kSubdivisionX = 32;
static const uint32 kSubdivisionY = 16;


UFidelityFXCASBlueprintLibrary::UFidelityFXCASBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}




//------------------------------------------------------------------------------------------------------------------------------
// INTEGRATION SUMMARY FOR CPU
// ===========================
// // Make sure <stdint.h> has already been included.
// // Setup pre-portability-header defines.
// #define A_CPU 1
// // Include the portability header (requires version 1.20190530 or later which is backwards compatible).
// #include "ffx_a.h"
// // Include the CAS header.
// #include "ffx_cas.h"
// ...
// // Call the setup function to build out the constants for the shader, pass these to the shader.
// // The 'varAU4(const0);' expands into 'uint32_t const0[4];' on the CPU.
// varAU4(const0);
// varAU4(const1);
// CasSetup(const0,const1,
//  0.0f,             // Sharpness tuning knob (0.0 to 1.0).
//  1920.0f,1080.0f,  // Example input size.
//  2560.0f,1440.0f); // Example output size.
// ...
// // Later dispatch the shader based on the amount of semi-persistent loop unrolling.
// // Here is an example for running with the 16x16 (4-way unroll for 32-bit or 2-way unroll for 16-bit)
// vkCmdDispatch(cmdBuf,(widthInPixels+15)>>4,(heightInPixels+15)>>4,1);
//------------------------------------------------------------------------------------------------------------------------------

#include "FidelityFXCASIncludes.h"











#include "RenderGraphUtils.h"


// This struct contains all the data we need to pass from the game thread to draw our effect.
struct FShaderUsageExampleParameters
{
	UTextureRenderTarget2D* RenderTarget;

	FIntPoint GetRenderTargetSize() const
	{
		return CachedRenderTargetSize;
	}

	FShaderUsageExampleParameters() { }
	FShaderUsageExampleParameters(UTextureRenderTarget2D* InRenderTarget)
		: RenderTarget(InRenderTarget)
	{
		CachedRenderTargetSize = RenderTarget ? FIntPoint(RenderTarget->SizeX, RenderTarget->SizeY) : FIntPoint::ZeroValue;
	}

private:
	FIntPoint CachedRenderTargetSize;
};



#include "FidelityFXCASShaderCS.h"


static void RunComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, FUnorderedAccessViewRHIRef ComputeShaderOutputUAV,
	FTextureRHIRef InInputTexture)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_ComputeShader); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_Compute); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, ComputeShaderOutputUAV);

	float SharpnessVal = 0.5f;
	float RenderWidth = 956.0f;
	float RenderHeight = 764.0f;
	float OutWidth = DrawParameters.GetRenderTargetSize().X;	// 956.0f;
	float OutHeight = DrawParameters.GetRenderTargetSize().Y;	// 764.0f;

	/*
	FComputeShaderExampleCS::FParameters PassParameters;
	PassParameters.InputTexture = InInputTexture;
	PassParameters.OutputTexture = ComputeShaderOutputUAV;
	PassParameters.TextureSize = FVector2D(DrawParameters.GetRenderTargetSize().X, DrawParameters.GetRenderTargetSize().Y);
	PassParameters.SimulationState = DrawParameters.SimulationState;

	CasSetup(reinterpret_cast<AU1*>(&PassParameters.const0), reinterpret_cast<AU1*>(&PassParameters.const1), SharpnessVal, static_cast<AF1>(RenderWidth),
		static_cast<AF1>(RenderHeight), OutWidth, OutHeight);

	TShaderMapRef<FComputeShaderExampleCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	*/


	

	FFidelityFXCASShaderCS_RHI::FParameters PassParameters;
	PassParameters.InputTexture = InInputTexture;
	PassParameters.OutputTexture = ComputeShaderOutputUAV;

	CasSetup(reinterpret_cast<AU1*>(&PassParameters.const0), reinterpret_cast<AU1*>(&PassParameters.const1), SharpnessVal, static_cast<AF1>(RenderWidth),
		static_cast<AF1>(RenderHeight), OutWidth, OutHeight);


	bool FP16 = false;
	bool SharpenOnly = (RenderWidth == OutWidth && RenderHeight == OutHeight);

	if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI<false, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		// Uniform parameters
		//FVector2D Dummy;
		//ComputeShader->SetUniformParameters(RHICmdList, Dummy);

		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(DrawParameters.GetRenderTargetSize().X, DrawParameters.GetRenderTargetSize().Y, 1));
		//FIntVector(FMath::DivideAndRoundUp(DrawParameters.GetRenderTargetSize().X, NUM_THREADS_PER_GROUP_DIMENSION),
		//	FMath::DivideAndRoundUp(DrawParameters.GetRenderTargetSize().Y, NUM_THREADS_PER_GROUP_DIMENSION), 1));

		//ComputeShader->Unbind(RHICmdList);
	}
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI<false, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(DrawParameters.GetRenderTargetSize().X, DrawParameters.GetRenderTargetSize().Y, 1));
	}
}


TRefCountPtr<IPooledRenderTarget> ComputeShaderOutput;






























#include "ShaderParameterUtils.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterMacros.h"
#include "ShaderParameterStruct.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Runtime/RenderCore/Public/PixelShaderUtils.h"


#include "FidelityFXCASShaderVS.h"
#include "FidelityFXCASShaderPS.h"

void DrawToRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, FTextureRHIRef InComputeShaderOutput)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_PixelShader); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_Pixel); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FRHIRenderPassInfo RenderPassInfo(DrawParameters.RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("ShaderPlugin_OutputToRenderTarget"));

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFidelityFXCASShaderVS> VertexShader(ShaderMap);
	TShaderMapRef<FFidelityFXCASShaderPS_RHI> PixelShader(ShaderMap);

	// Set the graphic pipeline state.
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

	// Setup the pixel shader
	FFidelityFXCASShaderPS_RHI::FParameters PassParameters;
	PassParameters.UpscaledTexture = InComputeShaderOutput;
	PassParameters.samLinearClamp = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), PassParameters);

	// Draw
	RHICmdList.SetStreamSource(0, GFidelityFXCASVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawPrimitive(0, 2, 1);

	// Resolve render target
	RHICmdList.CopyToResolveTarget(DrawParameters.RenderTarget->GetRenderTargetResource()->GetRenderTargetTexture(), DrawParameters.RenderTarget->GetRenderTargetResource()->TextureRHI, FResolveParams());

	RHICmdList.EndRenderPass();
}



//#include <EngineGlobals.h>
#include <Runtime/Engine/Classes/Engine/Engine.h>





static void Draw_RenderThread2(FRHICommandListImmediate& RHICmdList, const FShaderUsageExampleParameters& DrawParameters, FTextureRHIRef InInputTexture)
{
	check(IsInRenderingThread());

	if (!DrawParameters.RenderTarget)
	{
		return;
	}

	//FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();

	QUICK_SCOPE_CYCLE_COUNTER(STAT_ShaderPlugin_Render);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, ShaderPlugin_Render);		// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	// Release the shader output if the size changed
	bool NeedsRecreate = false;
	if (ComputeShaderOutput.IsValid())
	{
		FRHITexture2D* RHITexture2D = ComputeShaderOutput->GetRenderTargetItem().TargetableTexture->GetTexture2D();
		FIntPoint CurrentSize(RHITexture2D->GetSizeX(), RHITexture2D->GetSizeY());
		if (CurrentSize != DrawParameters.GetRenderTargetSize())
		{
			//GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Red, FString::Printf(TEXT("Releasing ComputeShaderOutput [%dx%d]..."), CurrentSize.X, CurrentSize.Y));
			//GRenderTargetPool.FreeUnusedResource(ComputeShaderOutput);
			NeedsRecreate = true;
		}
	}

	// Create the shader output
	if (!ComputeShaderOutput.IsValid() || NeedsRecreate)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Green, FString::Printf(TEXT("Creating ComputeShaderOutput [%dx%d]..."), DrawParameters.GetRenderTargetSize().X, DrawParameters.GetRenderTargetSize().Y));
		//FPooledRenderTargetDesc ComputeShaderOutputDesc(FPooledRenderTargetDesc::Create2DDesc(DrawParameters.GetRenderTargetSize(), PF_R32_UINT, FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		FPooledRenderTargetDesc ComputeShaderOutputDesc(FPooledRenderTargetDesc::Create2DDesc(DrawParameters.GetRenderTargetSize(), PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		ComputeShaderOutputDesc.DebugName = TEXT("ShaderPlugin_ComputeShaderOutput");
		GRenderTargetPool.FindFreeElement(RHICmdList, ComputeShaderOutputDesc, ComputeShaderOutput, TEXT("ShaderPlugin_ComputeShaderOutput"));
	}

	RunComputeShader_RenderThread(RHICmdList, DrawParameters, ComputeShaderOutput->GetRenderTargetItem().UAV, InInputTexture);
	DrawToRenderTarget_RenderThread(RHICmdList, DrawParameters, ComputeShaderOutput->GetRenderTargetItem().TargetableTexture);
}








#include "Runtime/Engine/Classes/Engine/Texture2D.h"




// static
void UFidelityFXCASBlueprintLibrary::DrawToRenderTarget(const UObject* WorldContextObject, class UTextureRenderTarget2D* OutputRenderTarget, class UTexture2D* InInputTexture)
{
	//UWorld* World = WorldContextObject->GetWorld();

	check(IsInGameThread());

	if (!OutputRenderTarget)
	{
		FMessageLog("Blueprint").Warning(FText::FromString(TEXT("DrawToRenderTarget: Output render target is required.")));
		return;
	}

	// Output.
	//const FName TextureRenderTargetName = OutputRenderTarget->GetFName();
	//FTextureRenderTargetResource* TextureRenderTargetResource = OutputRenderTarget->GameThread_GetRenderTargetResource();

	//ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();

	FTextureRHIRef InputTexture = InInputTexture->Resource->TextureRHI;
	FShaderUsageExampleParameters Params(OutputRenderTarget);
	ENQUEUE_RENDER_COMMAND(Compute)(
		[Params, InputTexture](FRHICommandListImmediate& RHICmdList)
		{
			Draw_RenderThread2(RHICmdList, Params, InputTexture);
		}
	);
}





#include "FidelityFXCAS.h"



bool UFidelityFXCASBlueprintLibrary::GetIsFidelityFXSSCASEnabled()
{
	return FFidelityFXCASModule::Get().GetIsSSCASEnabled();
}

void UFidelityFXCASBlueprintLibrary::SetIsFidelityFXSSCASEnabled(bool Enabled)
{
	FFidelityFXCASModule::Get().SetIsSSCASEnabled(Enabled);
}
