// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FidelityFXCAS.h"
#include "FidelityFXCASPassParams.h"
#include "FidelityFXCASShaderCS.h"
#include "FidelityFXCASShaderPS.h"
#include "FidelityFXCASShaderVS.h"
#include "FidelityFXCASIncludes.h"

#include "Engine.h"
#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RendererInterface.h"
#include "RenderGraphUtils.h"
#include "ShaderCore.h"
#include "ShaderParameterStruct.h"
#include "Runtime/Renderer/Private/PostProcess/SceneRenderTargets.h"

#define LOCTEXT_NAMESPACE "FFidelityFXCASModule"

//-------------------------------------------------------------------------------------------------
// Console variables
//-------------------------------------------------------------------------------------------------
static TAutoConsoleVariable<int32> CVarFidelityFXCAS_DisplayInfo(
	TEXT("r.fxcas.DisplayInfo"),
	1,
	TEXT("Enables onscreen inormation display for AMD FidelityFX CAS plugin.\n")
	TEXT("<=0: OFF (default)\n")
	TEXT(" >0: ON"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarFidelityFXCAS_SSCAS(
	TEXT("r.fxcas.SSCAS"),
	0,
	TEXT("Enables screen space contrast adaptive sharpening.\n")
	TEXT("<=0: OFF (default)\n")
	TEXT(" >0: ON"),
	ECVF_Cheat);

static TAutoConsoleVariable<float> CVarFidelityFXCAS_SSCASSharpness(
	TEXT("r.fxcas.SSCASSharpness"),
	0,
	TEXT("Sets the screen space CAS sharpness value.\n")
	TEXT("0: minimum (lower ringing) (default)\n")
	TEXT("1: maximum (higher ringing)"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarFidelityFXCAS_SSCASFP16(
	TEXT("r.fxcas.SSCASFP16"),
	0,
	TEXT("Should the screen space CAS use half precision floats.\n")
	TEXT("<=0: OFF (normal precision)\n")
	TEXT(">0: ON (half precision)"),
	ECVF_Cheat);

static TAutoConsoleVariable<int32> CVarFidelityFXCAS_SSCASNoUpscale(
	TEXT("r.fxcas.SSCASNoUpscale"),
	0,
	TEXT("Test variable to disable upscaling.\n")
	TEXT("<=0: OFF (with upscaling)\n")
	TEXT(">0: ON (no upscaling)"),
	ECVF_Cheat);

// Sink to track console variables value changes
static void FidelityFXCASCVarSink()
{
#if !UE_BUILD_SHIPPING
	// DisplayInfo
	static bool GDisplayInfo = false;	// Default value
	bool bNewDisplayInfo = CVarFidelityFXCAS_DisplayInfo.GetValueOnGameThread() > 0;
	if (bNewDisplayInfo != GDisplayInfo)
	{
		static FDelegateHandle Handle;
		if (bNewDisplayInfo)
		{
			Handle = FCoreDelegates::OnGetOnScreenMessages.AddLambda([](FCoreDelegates::FSeverityMessageMap& OutMessages) {
				// Screen space CAS
				FFidelityFXCASModule& Module = FFidelityFXCASModule::Get();
				// ON / OFF
				FString Message = FString::Printf(TEXT("FidelityFX SS CAS: %s"), Module.GetIsSSCASEnabled() ? TEXT("ON") : TEXT("OFF"));
				if (Module.GetIsSSCASEnabled())
				{
					// Resolution
					FIntPoint InputRes, OutputRes;
					Module.GetSSCASResolutionInfo(InputRes, OutputRes);
					if (InputRes == OutputRes)
						Message += FString::Printf(TEXT(" | Resolution: %dx%d"), OutputRes.X, OutputRes.Y);
					else
						Message += FString::Printf(TEXT(" | Resolution: %dx%d -> %dx%d"), InputRes.X, InputRes.Y, OutputRes.X, OutputRes.Y);
					// Sharpness
					Message += FString::Printf(TEXT(" | Sharpness: %.2f"), Module.GetSSCASSharpness());
				}
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::AsCultureInvariant(Message));
			});
		}
		else
		{
			FCoreDelegates::OnGetOnScreenMessages.Remove(Handle);
		}
		GDisplayInfo = bNewDisplayInfo;
	}
#endif // !UE_BUILD_SHIPPING

	// SS CAS
	static bool GSSCAS = false;
	bool bNewSSCAS = CVarFidelityFXCAS_SSCAS.GetValueOnGameThread() > 0;
	if (bNewSSCAS != GSSCAS)
	{
		FFidelityFXCASModule::Get().SetIsSSCASEnabled(bNewSSCAS);
		GSSCAS = bNewSSCAS;
	}

	// SS CAS sharpness
	static float GSSCASSharpness = 0.0f;
	float NewSSCASSharpness = FMath::Clamp(CVarFidelityFXCAS_SSCASSharpness.GetValueOnGameThread(), 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(NewSSCASSharpness, GSSCASSharpness))
	{
		FFidelityFXCASModule::Get().SetSSCASSharpness(NewSSCASSharpness);
		GSSCASSharpness = NewSSCASSharpness;
	}

	// SS CAS half precision
	static bool GSSCASFP16 = false;
	bool bNewSSCASFP16 = CVarFidelityFXCAS_SSCASFP16.GetValueOnGameThread() > 0;
	if (bNewSSCASFP16 != GSSCASFP16)
	{
		FFidelityFXCASModule::Get().SetUseFP16(bNewSSCASFP16);
		GSSCASFP16 = bNewSSCASFP16;
	}

	// Screen percentage
	static const TConsoleVariableData<float>* CVarScreenPercentage = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
	if (CVarScreenPercentage)
	{
		static float GScreenPercentage = CVarScreenPercentage->GetValueOnGameThread();
		float NewScreenPercentage = CVarScreenPercentage->GetValueOnAnyThread();
		if (NewScreenPercentage != GScreenPercentage)
		{
			FFidelityFXCASModule::Get().UpdateSSCASEnabled();
			GScreenPercentage = NewScreenPercentage;
		}
	}
}
FAutoConsoleVariableSink CFidelityFXCASCVarSink(FConsoleCommandDelegate::CreateStatic(&FidelityFXCASCVarSink));

//-------------------------------------------------------------------------------------------------
// FFidelityFXCASModule class implementation
//-------------------------------------------------------------------------------------------------

void FFidelityFXCASModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Add shaders path mapping
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("FidelityFXCAS"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/FidelityFXCAS"), PluginShaderDir);

	// Reset variables
	bIsSSCASEnabled = false;
	SSCASSharpness = 0.0f;
	OnResolvedSceneColorHandle.Reset();
}

void FFidelityFXCASModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.

	SetIsSSCASEnabled(false);	// Turn off screen space CAS
}

bool FFidelityFXCASModule::GetIsSSCASEnabled() const
{
	return bIsSSCASEnabled;
}

void FFidelityFXCASModule::SetIsSSCASEnabled(bool Enabled)
{
	if (Enabled != GetIsSSCASEnabled())
	{
		bIsSSCASEnabled = Enabled;
		UpdateSSCASEnabled();
	}
}

void FFidelityFXCASModule::UpdateSSCASEnabled()
{
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	if (bIsSSCASEnabled)
	{
		bool bUpscale = false;
		static const TConsoleVariableData<float>* CVarScreenPercentage = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.ScreenPercentage"));
		if (CVarScreenPercentage)
			bUpscale = CVarScreenPercentage->GetValueOnAnyThread() < 100.0f;
		if (bUpscale)
		{
			UnbindResolvedSceneColorCallback(RendererModule);
			BindCustomUpscaleCallback(RendererModule);
		}
		else
		{
			UnbindCustomUpscaleCallback(RendererModule);
			BindResolvedSceneColorCallback(RendererModule);
		}
	}
	else
	{
		UnbindResolvedSceneColorCallback(RendererModule);
		UnbindCustomUpscaleCallback(RendererModule);
	}
}

void FFidelityFXCASModule::BindResolvedSceneColorCallback(IRendererModule* RendererModule)
{
	if (!OnResolvedSceneColorHandle.IsValid())
		OnResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FFidelityFXCASModule::OnResolvedSceneColor_RenderThread);
}

void FFidelityFXCASModule::UnbindResolvedSceneColorCallback(IRendererModule* RendererModule)
{
	RendererModule->GetResolvedSceneColorCallbacks().Remove(OnResolvedSceneColorHandle);
	OnResolvedSceneColorHandle.Reset();
}

void FFidelityFXCASModule::BindCustomUpscaleCallback(IRendererModule* RendererModule)
{
	if (!RendererModule->GetCustomUpscalePassCallback().IsBound())
		RendererModule->GetCustomUpscalePassCallback().BindRaw(this, &FFidelityFXCASModule::OnAddUpscalePass_RenderThread);
}

void FFidelityFXCASModule::UnbindCustomUpscaleCallback(IRendererModule* RendererModule)
{
	RendererModule->GetCustomUpscalePassCallback().Unbind();
}

void FFidelityFXCASModule::PrepareComputeShaderOutput(FRHICommandListImmediate& RHICmdList, const FIntPoint& OutputSize, TRefCountPtr<IPooledRenderTarget>& CSOutput)
{
	bool NeedsRecreate = false;
	if (CSOutput.IsValid())
	{
		// Check size if already exists
		FRHITexture2D* RHITexture2D = CSOutput->GetRenderTargetItem().TargetableTexture->GetTexture2D();
		FIntPoint CurrentSize = RHITexture2D->GetSizeXY();
		if (CurrentSize != OutputSize)
		{
			// Release the shader output if the size changed
			//GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Red, FString::Printf(TEXT("Releasing compute shader output [%dx%d]..."), CurrentSize.X, CurrentSize.Y));
			//GRenderTargetPool.FreeUnusedResource(CSOutput);
			NeedsRecreate = true;
		}
	}

	// Create the shader output
	if (!CSOutput.IsValid() || NeedsRecreate)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Green, FString::Printf(TEXT("Creating compute shader output [%dx%d]..."), OutputSize.X, OutputSize.Y));
		FPooledRenderTargetDesc CSOutputDesc(FPooledRenderTargetDesc::Create2DDesc(OutputSize, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		CSOutputDesc.DebugName = TEXT("FidelityFXCASModule_ComputeShaderOutput");
		GRenderTargetPool.FindFreeElement(RHICmdList, CSOutputDesc, CSOutput, TEXT("FidelityFXCASModule_ComputeShaderOutput"));
	}
}

void FFidelityFXCASModule::OnResolvedSceneColor_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_Render);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_Render);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	const FTextureRHIRef& SceneColorTexture = SceneContext.GetSceneColorTexture();
	if (!SceneColorTexture.IsValid())
		return;
	const FRHITexture2D* SceneColorTexture2D = SceneColorTexture->GetTexture2D();
	if (!SceneColorTexture2D)
		return;

	FFidelityFXCASPassParams_RHI CASPassParams(SceneColorTexture, SceneColorTexture);

	// Update resolution info
	SetSSCASResolutionInfo(CASPassParams.GetInputSize(), CASPassParams.GetOutputSize());

	// Make sure the computer shader output is ready and with correct size
	PrepareComputeShaderOutput(RHICmdList, CASPassParams.GetOutputSize(), ComputeShaderOutput_RHI);
	CASPassParams.CSOutput = ComputeShaderOutput_RHI;

	// Call shaders
	RunComputeShader_RHI_RenderThread(RHICmdList, CASPassParams);
	DrawToRenderTarget_RHI_RenderThread(RHICmdList, CASPassParams);
}

void FFidelityFXCASModule::OnAddUpscalePass_RenderThread(FRDGBuilder& GraphBuilder, FRDGTexture* SceneColor, const FRenderTargetBinding& RTBinding)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_Render);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_Render);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FFidelityFXCASPassParams_RDG CASPassParams(SceneColor, RTBinding);

	// Update resolution info
	SetSSCASResolutionInfo(CASPassParams.GetInputSize(), CASPassParams.GetOutputSize());

	// Make sure the computer shader output is ready and with correct size
	PrepareComputeShaderOutput(GraphBuilder.RHICmdList, CASPassParams.GetOutputSize(), ComputeShaderOutput_RDG);
	CASPassParams.CSOutput = ComputeShaderOutput_RDG;

	// Call shaders
	RunComputeShader_RDG_RenderThread(GraphBuilder, CASPassParams);
	DrawToRenderTarget_RDG_RenderThread(GraphBuilder, CASPassParams);
}

void FFidelityFXCASModule::RunComputeShader_RHI_RenderThread(FRHICommandListImmediate& RHICmdList, const class FFidelityFXCASPassParams_RHI& CASPassParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_ComputeShader_RHI);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_ComputeShader_RHI);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, CASPassParams.GetUAV());

	// Setup shader parameters
	FFidelityFXCASShaderCS_RHI::FParameters PassParameters;
	PassParameters.InputTexture = CASPassParams.GetInputTexture();
	PassParameters.OutputTexture = CASPassParams.GetUAV();
	CasSetup(reinterpret_cast<AU1*>(&PassParameters.const0), reinterpret_cast<AU1*>(&PassParameters.const1),
		SSCASSharpness,
		static_cast<AF1>(CASPassParams.GetInputSize().X), static_cast<AF1>(CASPassParams.GetInputSize().Y),
		CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y);

	// Choose shader version and dispatch
	bool SharpenOnly = (CASPassParams.GetInputSize() == CASPassParams.GetOutputSize());
	if (CVarFidelityFXCAS_SSCASNoUpscale.GetValueOnGameThread() > 0)
		SharpenOnly = true;
	if (bUseFP16 && SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP16_SharpenOnly> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
	else if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP32_SharpenOnly> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
	else if (bUseFP16)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP16_Upscale> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP32_Upscale> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
}

void FFidelityFXCASModule::RunComputeShader_RDG_RenderThread(FRDGBuilder& GraphBuilder, const class FFidelityFXCASPassParams_RDG& CASPassParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_ComputeShader_RDG);				// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_ComputeShader_RDG);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	// Setup shader parameters
	FFidelityFXCASShaderCS_RDG::FParameters* PassParameters = GraphBuilder.AllocParameters<FFidelityFXCASShaderCS_RDG::FParameters>();
	PassParameters->InputTexture = CASPassParams.GetInputTexture();
	PassParameters->OutputTexture = CASPassParams.GetUAV();
	CasSetup(reinterpret_cast<AU1*>(&PassParameters->const0), reinterpret_cast<AU1*>(&PassParameters->const1),
		SSCASSharpness,
		static_cast<AF1>(CASPassParams.GetInputSize().X), static_cast<AF1>(CASPassParams.GetInputSize().Y),
		CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y);

	// Choose shader version and dispatch
	bool SharpenOnly = (CASPassParams.GetInputSize() == CASPassParams.GetOutputSize());
	if (CVarFidelityFXCAS_SSCASNoUpscale.GetValueOnGameThread() > 0)
		SharpenOnly = true;
	if (bUseFP16 && SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<true, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
	else if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<false, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
	else if (bUseFP16)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<true, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<false, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
}

FIntVector FFidelityFXCASModule::GetDispatchGroupCount(FIntPoint OutputSize)
{
	// This value is the image region dim that each thread group of the CAS shader operates on
	static const int32 ThreadGroupWorkRegionDim = 16;
	FIntVector DispatchGroupCount(0, 0, 1);
	DispatchGroupCount.X = (OutputSize.X + (ThreadGroupWorkRegionDim - 1)) / ThreadGroupWorkRegionDim;
	DispatchGroupCount.Y = (OutputSize.Y + (ThreadGroupWorkRegionDim - 1)) / ThreadGroupWorkRegionDim;
	return DispatchGroupCount;
}

void FFidelityFXCASModule::DrawToRenderTarget_RHI_RenderThread(FRHICommandListImmediate& RHICmdList, const class FFidelityFXCASPassParams_RHI& CASPassParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_PixelShader);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_PixelShader);		// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FRHIRenderPassInfo RenderPassInfo(CASPassParams.GetRTTexture(), ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("FidelityFXCASModule_DrawToRenderTarget_RHI_RenderThread"));

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFidelityFXCASShaderVS> VertexShader(ShaderMap);
	TShaderMapRef<FFidelityFXCASShaderPS_RHI> PixelShader(ShaderMap);

	// Setup the pixel shader
	FFidelityFXCASShaderPS_RHI::FParameters PassParameters;
	PassParameters.UpscaledTexture = CASPassParams.GetCSOutputTargetableTexture();
	PassParameters.samLinearClamp = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

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

	SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), PassParameters);

	// Draw
	RHICmdList.SetStreamSource(0, GFidelityFXCASVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawPrimitive(0, 2, 1);

	// Resolve render target
	//RHICmdList.CopyToResolveTarget(CASPassParams.GetRTTexture(), CASPassParams.GetRTTexture(), FResolveParams());

	RHICmdList.EndRenderPass();
}

void FFidelityFXCASModule::DrawToRenderTarget_RDG_RenderThread(FRDGBuilder& GraphBuilder, const class FFidelityFXCASPassParams_RDG& CASPassParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_PixelShader);				// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_PixelShader);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFidelityFXCASShaderVS> VertexShader(ShaderMap);
	TShaderMapRef<FFidelityFXCASShaderPS_RDG> PixelShader(ShaderMap);

	// Setup the pixel shader
	FFidelityFXCASShaderPS_RDG::FParameters* PassParameters = GraphBuilder.AllocParameters<FFidelityFXCASShaderPS_RDG::FParameters>();
	PassParameters->UpscaledTexture = CASPassParams.GetCSOutputTargetableTexture();
	PassParameters->samLinearClamp = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->RenderTargets[0] = CASPassParams.GetRTBinding();

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Upscale PS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
		PassParameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, PassParameters](FRHICommandList& RHICmdList)
	{
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

		SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), *PassParameters);

		// Draw
		RHICmdList.SetStreamSource(0, GFidelityFXCASVertexBuffer.VertexBufferRHI, 0);
		RHICmdList.DrawPrimitive(0, 2, 1);
	});
}

void FFidelityFXCASModule::GetSSCASResolutionInfo(FIntPoint& OutInputResolution, FIntPoint& OutOutputResolution) const
{
	FScopeLock Lock(&ResolutionInfoCS);
	OutInputResolution = InputResolution;
	OutOutputResolution = OutputResolution;
}

void FFidelityFXCASModule::SetSSCASResolutionInfo(const FIntPoint& InInputResolution, const FIntPoint& InOutputResolution)
{
	FScopeLock Lock(&ResolutionInfoCS);
	InputResolution = InInputResolution;
	OutputResolution = InOutputResolution;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FFidelityFXCASModule, FidelityFXCAS)
