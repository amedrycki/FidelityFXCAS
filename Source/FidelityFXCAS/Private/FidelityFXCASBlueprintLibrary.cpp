#include "FidelityFXCASBlueprintLibrary.h"

#include "Engine/Texture2D.h"
#include "Logging/MessageLog.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"

#include "FidelityFXCAS.h"
#include "FidelityFXCASPassParams.h"

//-------------------------------------------------------------------------------------------------
// Local compute shader output pool
//-------------------------------------------------------------------------------------------------
#if FX_CAS_PLUGIN_ENABLED
TMap<UTextureRenderTarget2D*, TRefCountPtr<IPooledRenderTarget>> GFXCASCSOutputs;
FORCEINLINE static TRefCountPtr<IPooledRenderTarget>& GFXCASGetCSOutput(UTextureRenderTarget2D* InOutputRenderTarget)
{
	return GFXCASCSOutputs.FindOrAdd(InOutputRenderTarget);
}
#endif // FX_CAS_PLUGIN_ENABLED

//-------------------------------------------------------------------------------------------------
// UFidelityFXCASBlueprintLibrary class
//-------------------------------------------------------------------------------------------------

UFidelityFXCASBlueprintLibrary::UFidelityFXCASBlueprintLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UFidelityFXCASBlueprintLibrary::GetIsSSCASEnabled()
{
	return FFidelityFXCASModule::Get().GetIsSSCASEnabled();
}

void UFidelityFXCASBlueprintLibrary::SetIsSSCASEnabled(bool bEnabled)
{
	FFidelityFXCASModule::Get().SetIsSSCASEnabled(bEnabled);
}

float UFidelityFXCASBlueprintLibrary::GetSSCASSharpness()
{
	return FFidelityFXCASModule::Get().GetSSCASSharpness();
}

void UFidelityFXCASBlueprintLibrary::SetSSCASSharpness(float Sharpness)
{
	FFidelityFXCASModule::Get().SetSSCASSharpness(Sharpness);
}

bool UFidelityFXCASBlueprintLibrary::GetUseFP16()
{
	return FFidelityFXCASModule::Get().GetUseFP16();
}

void UFidelityFXCASBlueprintLibrary::SetUseFP16(bool UseFP16)
{
	FFidelityFXCASModule::Get().SetUseFP16(UseFP16);
}

void UFidelityFXCASBlueprintLibrary::InitSSCASCSOutputs(const FIntPoint& Size)
{
	FFidelityFXCASModule::Get().InitSSCASCSOutputs(Size);
}

void UFidelityFXCASBlueprintLibrary::InitCSOutput(class UTextureRenderTarget2D* InOutputRenderTarget)
{
#if FX_CAS_PLUGIN_ENABLED
	// Check output
	if (!InOutputRenderTarget)
	{
		FMessageLog("Blueprint").Warning(FText::FromString(TEXT("FidelityFXCAS InitCSOutput: OutputRenderTarget is required.")));
		return;
	}

	ENQUEUE_RENDER_COMMAND(FidelityFXCASBP_InitCSOutput)(
		[InOutputRenderTarget](FRHICommandListImmediate& RHICmdList)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASBP_InitCSOutput); // Used to gather CPU profiling data for the UE4 session frontend
			SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASBP_InitCSOutput);  // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

			FIntPoint Size(InOutputRenderTarget->SizeX, InOutputRenderTarget->SizeY);
			FFidelityFXCASModule::Get().PrepareComputeShaderOutput_RenderThread(GRHICommandList.GetImmediateCommandList(), Size, GFXCASGetCSOutput(InOutputRenderTarget));
		}
	);
#endif // FX_CAS_PLUGIN_ENABLED
}

void UFidelityFXCASBlueprintLibrary::DrawToRenderTarget(class UTextureRenderTarget2D* InOutputRenderTarget, class UTexture2D* InInputTexture, float InSharpness, bool InUseFP16)
{
#if FX_CAS_PLUGIN_ENABLED
	// Check input texture
	if (!InInputTexture)
	{
		FMessageLog("Blueprint").Warning(FText::FromString(TEXT("FidelityFXCAS DrawToRenderTarget: InInputTexture is required.")));
		return;
	}
	FTextureResource* InputTextureResource = InInputTexture->Resource;
	if (!InputTextureResource)
	{
		FMessageLog("Blueprint").Warning(FText::FromString(TEXT("FidelityFXCAS DrawToRenderTarget: Input texture's resource is NULL.")));
		return;
	}

	// Check output
	if (!InOutputRenderTarget)
	{
		FMessageLog("Blueprint").Warning(FText::FromString(TEXT("FidelityFXCAS DrawToRenderTarget: OutputRenderTarget is required.")));
		return;
	}

	//FRHICommandListImmediate& RHICmdList = GRHICommandList.GetImmediateCommandList();

	FTextureRHIRef InputTexture = InInputTexture->Resource->TextureRHI;

	ENQUEUE_RENDER_COMMAND(FidelityFXCASBP_DrawToRenderTarget)(
		[InOutputRenderTarget, InputTexture, InSharpness, InUseFP16](FRHICommandListImmediate& RHICmdList)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASBP_DrawToRenderTarget); // Used to gather CPU profiling data for the UE4 session frontend
			SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASBP_DrawToRenderTarget);  // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

			FTextureRenderTargetResource* RTResource = InOutputRenderTarget->GetRenderTargetResource();
			if (!RTResource)
				return;

			// Prepare pass parameters
			FFidelityFXCASPassParams_RHI CASPassParams(InputTexture, RTResource->TextureRHI, GFXCASGetCSOutput(InOutputRenderTarget));
			CASPassParams.Sharpness = FMath::Clamp(InSharpness, 0.0f, 1.0f);
			CASPassParams.bUseFP16 = InUseFP16;

			// Make sure the computer shader output is ready and has the correct size
			FFidelityFXCASModule::Get().PrepareComputeShaderOutput_RenderThread(RHICmdList, CASPassParams.GetOutputSize(), CASPassParams.CSOutput);

			// Call shaders
			FFidelityFXCASModule::Get().RunComputeShader_RHI_RenderThread(RHICmdList, CASPassParams);
			FFidelityFXCASModule::Get().DrawToRenderTarget_RHI_RenderThread(RHICmdList, CASPassParams);
		}
	);
#endif // FX_CAS_PLUGIN_ENABLED
}
