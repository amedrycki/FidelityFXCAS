// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FidelityFXCAS.h"
#include "FidelityFXCASPassParams.h"
#include "FidelityFXCASShaderCS.h"
#include "FidelityFXCASShaderPS.h"
#include "FidelityFXCASShaderVS.h"
#include "FidelityFXCASIncludes.h"

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
#include "Misc/EngineVersionComparison.h"

#define LOCTEXT_NAMESPACE "FFidelityFXCASModule"

//-------------------------------------------------------------------------------------------------
// Console variables
//-------------------------------------------------------------------------------------------------
#if FX_CAS_PLUGIN_ENABLED
static TAutoConsoleVariable<int32> CVarFidelityFXCAS_DisplayInfo(
	TEXT("r.fxcas.DisplayInfo"),
	0,
	TEXT("Enables onscreen inormation display for AMD FidelityFX CAS plugin.\n")
	TEXT("<=0: OFF (default)\n")
	TEXT(" >0: ON"),
	ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarFidelityFXCAS_SSCAS(
	TEXT("r.fxcas.SSCAS"),
	0,
	TEXT("Enables screen space contrast adaptive sharpening.\n")
	TEXT("0: OFF (default)\n")
	TEXT("1: ON"),
	ECVF_Cheat);

static TAutoConsoleVariable<float> CVarFidelityFXCAS_SSCASSharpness(
	TEXT("r.fxcas.SSCASSharpness"),
	0.5f,
	TEXT("Sets the screen space CAS sharpness value (default: 0.5).\n")
	TEXT("0: minimum (lower ringing)\n")
	TEXT("1: maximum (higher ringing)"),
	ECVF_Cheat);

#if FX_CAS_FP16_ENABLED
static TAutoConsoleVariable<bool> CVarFidelityFXCAS_SSCASFP16(
	TEXT("r.fxcas.SSCASFP16"),
	0,
	TEXT("Should the screen space CAS use half precision floats.\n")
	TEXT("0: OFF (normal precision)\n")
	TEXT("1: ON (half precision)"),
	ECVF_Cheat);
#endif // FX_CAS_FP16_ENABLED

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
				FFidelityFXCASModule& Module = FFidelityFXCASModule::Get();
#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
				static const FString SharpenOnly(TEXT(""));
#else
				static const FString SharpenOnly(TEXT(" (no upsampling)"));
#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
				// Screen space CAS ON / OFF
				FString Message = FString::Printf(TEXT("FidelityFX SS CAS%s: %s"), *SharpenOnly, Module.GetIsSSCASEnabled() ? TEXT("ON") : TEXT("OFF"));
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
	bool bNewSSCAS = CVarFidelityFXCAS_SSCAS.GetValueOnGameThread();
	if (bNewSSCAS != GSSCAS)
	{
		FFidelityFXCASModule::Get().SetIsSSCASEnabled(bNewSSCAS);
		GSSCAS = bNewSSCAS;
	}

	// SS CAS sharpness
	static float GSSCASSharpness = 0.5f;
	float NewSSCASSharpness = FMath::Clamp(CVarFidelityFXCAS_SSCASSharpness.GetValueOnGameThread(), 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(NewSSCASSharpness, GSSCASSharpness))
	{
		FFidelityFXCASModule::Get().SetSSCASSharpness(NewSSCASSharpness);
		GSSCASSharpness = NewSSCASSharpness;
	}

#if FX_CAS_FP16_ENABLED
	// SS CAS half precision
	static bool GSSCASFP16 = false;
	bool bNewSSCASFP16 = CVarFidelityFXCAS_SSCASFP16.GetValueOnGameThread();
	if (bNewSSCASFP16 != GSSCASFP16)
	{
		FFidelityFXCASModule::Get().SetUseFP16(bNewSSCASFP16);
		GSSCASFP16 = bNewSSCASFP16;
	}
#endif // FX_CAS_FP16_ENABLED

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
#endif // FX_CAS_PLUGIN_ENABLED

//-------------------------------------------------------------------------------------------------
// FFidelityFXCASModule class implementation
//-------------------------------------------------------------------------------------------------

FFidelityFXCASModule& FFidelityFXCASModule::Get()
{
	static FFidelityFXCASModule* CachedModule = NULL;
	if (!CachedModule)
		CachedModule = &FModuleManager::LoadModuleChecked<FFidelityFXCASModule>("FidelityFXCAS");
	return *CachedModule;
}

bool FFidelityFXCASModule::IsEnabledOnCurrentPlatform()
{
#if FX_CAS_PLUGIN_ENABLED
	return true;
#else // FX_CAS_PLUGIN_ENABLED
	return false;
#endif // FX_CAS_PLUGIN_ENABLED
}

void FFidelityFXCASModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Add shaders path mapping
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("FidelityFXCAS"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/FidelityFXCAS"), PluginShaderDir);

	// Reset variables
	bIsSSCASEnabled = false;
	SSCASSharpness = 0.5f;
#if FX_CAS_PLUGIN_ENABLED
	OnResolvedSceneColorHandle.Reset();
#endif // FX_CAS_PLUGIN_ENABLED
}

void FFidelityFXCASModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.
	// For modules that support dynamic reloading, we call this function before unloading the module.

	SetIsSSCASEnabled(false);	// Turn off screen space CAS
}

void FFidelityFXCASModule::SetIsSSCASEnabled(bool Enabled)
{
#if FX_CAS_PLUGIN_ENABLED
	if (Enabled != GetIsSSCASEnabled())
	{
		bIsSSCASEnabled = Enabled;
		UpdateSSCASEnabled();
		CVarFidelityFXCAS_SSCAS->Set(Enabled);
	}
#endif // FX_CAS_PLUGIN_ENABLED
}

void FFidelityFXCASModule::UpdateSSCASEnabled()
{
#if FX_CAS_PLUGIN_ENABLED
	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	if (bIsSSCASEnabled)
	{
#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
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
#else // FX_CAS_CUSTOM_UPSCALE_CALLBACK
		BindResolvedSceneColorCallback(RendererModule);
#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
	}
	else
	{
		UnbindResolvedSceneColorCallback(RendererModule);
#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
		UnbindCustomUpscaleCallback(RendererModule);
#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
	}
#endif // FX_CAS_PLUGIN_ENABLED
}

void FFidelityFXCASModule::SetSSCASSharpness(float Sharpness)
{
#if FX_CAS_PLUGIN_ENABLED
	if (!FMath::IsNearlyEqual(Sharpness, SSCASSharpness))
	{
		SSCASSharpness = FMath::Clamp(Sharpness, 0.0f, 1.0f);
		CVarFidelityFXCAS_SSCASSharpness->Set(Sharpness);
	}
#endif // FX_CAS_PLUGIN_ENABLED
}

void FFidelityFXCASModule::SetUseFP16(bool UseFP16)
{
#if FX_CAS_PLUGIN_ENABLED && FX_CAS_FP16_ENABLED
	if (UseFP16 != bUseFP16)
	{
		bUseFP16 = UseFP16;
		CVarFidelityFXCAS_SSCASFP16->Set(UseFP16);
	}
#endif // FX_CAS_PLUGIN_ENABLED && FX_CAS_FP16_ENABLED
}

#if FX_CAS_PLUGIN_ENABLED
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

#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
void FFidelityFXCASModule::BindCustomUpscaleCallback(IRendererModule* RendererModule)
{
	if (!RendererModule->GetCustomUpscalePassCallback().IsBound())
		RendererModule->GetCustomUpscalePassCallback().BindRaw(this, &FFidelityFXCASModule::OnAddUpscalePass_RenderThread);
}

void FFidelityFXCASModule::UnbindCustomUpscaleCallback(IRendererModule* RendererModule)
{
	RendererModule->GetCustomUpscalePassCallback().Unbind();
}
#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK

void FFidelityFXCASModule::PrepareComputeShaderOutput_RenderThread(FRHICommandListImmediate& RHICmdList, const FIntPoint& OutputSize, TRefCountPtr<IPooledRenderTarget>& CSOutput, const TCHAR* InDebugName)
{
	check(IsInRenderingThread());

	static const TCHAR* GFXCASCSOutputDebugName = TEXT("FidelityFXCASModule_CSOutput");
	const TCHAR* DebugName = InDebugName ? InDebugName : GFXCASCSOutputDebugName;

	bool bNeedsRecreate = false;
	if (CSOutput.IsValid())
	{
		// If the render target already exists, check if the size hasn't changed
		FRHITexture2D* RHITexture2D = CSOutput->GetRenderTargetItem().TargetableTexture->GetTexture2D();
		bNeedsRecreate = (!RHITexture2D || (RHITexture2D->GetSizeXY() != OutputSize));
	}

	if (bNeedsRecreate)
	{
		// Release the shader output
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.f, FColor::Silver, FString::Printf(TEXT("Releasing compute shader output...")));
		GRenderTargetPool.FreeUnusedResource(CSOutput);
	}

	// Create the shader output
	if (!CSOutput.IsValid() || bNeedsRecreate)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2.f, FColor::Silver, FString::Printf(TEXT("Creating compute shader output [%dx%d]..."), OutputSize.X, OutputSize.Y));
		FPooledRenderTargetDesc CSOutputDesc(FPooledRenderTargetDesc::Create2DDesc(OutputSize, PF_FloatRGBA, FClearValueBinding::None,
			TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		CSOutputDesc.DebugName = DebugName;
		GRenderTargetPool.FindFreeElement(RHICmdList, CSOutputDesc, CSOutput, DebugName);
	}
}
#endif // FX_CAS_PLUGIN_ENABLED

void FFidelityFXCASModule::InitSSCASCSOutputs(const FIntPoint& Size)
{
#if FX_CAS_PLUGIN_ENABLED
	ENQUEUE_RENDER_COMMAND(FidelityFXCAS_InitSSCASCSOutputs)(
		[this, Size](FRHICommandListImmediate& RHICmdList)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASBP_InitSSCASCSOutputs); // Used to gather CPU profiling data for the UE4 session frontend
			SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASBP_InitSSCASCSOutputs);  // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

			PrepareComputeShaderOutput_RenderThread(GRHICommandList.GetImmediateCommandList(), Size, ComputeShaderOutput_RHI);
#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
			PrepareComputeShaderOutput_RenderThread(GRHICommandList.GetImmediateCommandList(), Size, ComputeShaderOutput_RDG);
#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
		}
	);
#endif // FX_CAS_PLUGIN_ENABLED
}

#if FX_CAS_PLUGIN_ENABLED
void FFidelityFXCASModule::OnResolvedSceneColor_RenderThread(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_OnResolvedSceneColor); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_OnResolvedSceneColor);  // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	const FTextureRHIRef& SceneColorTexture = SceneContext.GetSceneColorTexture();
	if (!SceneColorTexture.IsValid())
		return;
	const FRHITexture2D* SceneColorTexture2D = SceneColorTexture->GetTexture2D();
	if (!SceneColorTexture2D)
		return;

	// Prepare pass parameters
	FFidelityFXCASPassParams_RHI CASPassParams(SceneColorTexture, SceneColorTexture, ComputeShaderOutput_RHI);
	CASPassParams.Sharpness = FMath::Clamp(SSCASSharpness, 0.0f, 1.0f);
	CASPassParams.bUseFP16 = bUseFP16;

	// Update resolution info
	SetSSCASResolutionInfo(CASPassParams.GetInputSize(), CASPassParams.GetOutputSize());

	// Make sure the computer shader output is ready and has the correct size
	PrepareComputeShaderOutput_RenderThread(RHICmdList, CASPassParams.GetOutputSize(), CASPassParams.CSOutput);

	// Call shaders
	RunComputeShader_RHI_RenderThread(RHICmdList, CASPassParams);
	DrawToRenderTarget_RHI_RenderThread(RHICmdList, CASPassParams);
}

#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
void FFidelityFXCASModule::OnAddUpscalePass_RenderThread(class FRDGBuilder& GraphBuilder, const FIntRect& InInputViewRect, class FRDGTexture* SceneColor, const FRenderTargetBinding& RTBinding)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_OnAddUpscalePass);             // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_OnAddUpscalePass); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	// Prepare pass parameters
	FFidelityFXCASPassParams_RDG CASPassParams(InInputViewRect, SceneColor, RTBinding, ComputeShaderOutput_RDG);
	CASPassParams.Sharpness = FMath::Clamp(SSCASSharpness, 0.0f, 1.0f);
	CASPassParams.bUseFP16 = bUseFP16;

	// Update resolution info
	SetSSCASResolutionInfo(CASPassParams.GetInputSize(), CASPassParams.GetOutputSize());

	// Make sure the computer shader output is ready and has the correct size
	PrepareComputeShaderOutput_RenderThread(GraphBuilder.RHICmdList, CASPassParams.GetOutputSize(), CASPassParams.CSOutput);

	// Call shaders
	RunComputeShader_RDG_RenderThread(GraphBuilder, CASPassParams);
	DrawToRenderTarget_RDG_RenderThread(GraphBuilder, CASPassParams);
}
#endif	// FX_CAS_CUSTOM_UPSCALE_CALLBACK

void FFidelityFXCASModule::RunComputeShader_RHI_RenderThread(FRHICommandListImmediate& RHICmdList, const class FFidelityFXCASPassParams_RHI& CASPassParams)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_RunComputeShader_RHI); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_RunComputeShader_RHI);  // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UnbindRenderTargets(RHICmdList);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, CASPassParams.GetUAV());

	// Setup shader parameters
	FFidelityFXCASShaderCS_RHI::FParameters PassParameters;
	PassParameters.InputTexture = CASPassParams.GetInputTexture();
	PassParameters.OutputTexture = CASPassParams.GetUAV();
	CasSetup(reinterpret_cast<AU1*>(&PassParameters.const0), reinterpret_cast<AU1*>(&PassParameters.const1),
		CASPassParams.Sharpness,
		static_cast<AF1>(CASPassParams.GetInputSize().X), static_cast<AF1>(CASPassParams.GetInputSize().Y),
		CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y);

	// Choose shader version and dispatch
	bool SharpenOnly = (CASPassParams.GetInputSize() == CASPassParams.GetOutputSize());
#if FX_CAS_FP16_ENABLED
	if (CASPassParams.bUseFP16 && SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP16_SharpenOnly> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#else
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#endif	// UE v4.24
	}
	else
#endif // FX_CAS_FP16_ENABLED
	if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP32_SharpenOnly> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#else
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#endif	// UE v4.24
	}
#if FX_CAS_FP16_ENABLED
	else if (CASPassParams.bUseFP16)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP16_Upscale> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#else
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#endif	// UE v4.24
	}
#endif // FX_CAS_FP16_ENABLED
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RHI_FP32_Upscale> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#else
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
#endif	// UE v4.24
	}
}

#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
void FFidelityFXCASModule::RunComputeShader_RDG_RenderThread(FRDGBuilder& GraphBuilder, const class FFidelityFXCASPassParams_RDG& CASPassParams)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_RunComputeShader_RDG);             // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_RunComputeShader_RDG); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	// Setup shader parameters
	FFidelityFXCASShaderCS_RDG::FParameters* PassParameters = GraphBuilder.AllocParameters<FFidelityFXCASShaderCS_RDG::FParameters>();
	PassParameters->InputTexture = CASPassParams.GetInputTexture();
	PassParameters->OutputTexture = CASPassParams.GetUAV();
	CasSetup(reinterpret_cast<AU1*>(&PassParameters->const0), reinterpret_cast<AU1*>(&PassParameters->const1),
		CASPassParams.Sharpness,
		static_cast<AF1>(CASPassParams.GetInputSize().X), static_cast<AF1>(CASPassParams.GetInputSize().Y),
		CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y);

	// Choose shader version and dispatch
	bool SharpenOnly = (CASPassParams.GetInputSize() == CASPassParams.GetOutputSize());
#if FX_CAS_FP16_ENABLED
	if (CASPassParams.bUseFP16 && SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<true, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
	else
#endif // FX_CAS_FP16_ENABLED
	if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<false, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
#if FX_CAS_FP16_ENABLED
	else if (CASPassParams.bUseFP16)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<true, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
#endif // FX_CAS_FP16_ENABLED
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<false, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", CASPassParams.GetInputSize().X, CASPassParams.GetInputSize().Y, CASPassParams.GetOutputSize().X, CASPassParams.GetOutputSize().Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(CASPassParams.GetOutputSize()));
	}
}
#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK

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
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_DrawToRenderTarget_RHI); // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_DrawToRenderTarget_RHI);  // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

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
#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
#else
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
#endif	// UE v4.24
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
	SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), PassParameters);
#else
	SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters);
#endif	// UE v4.24

	// Draw
	RHICmdList.SetStreamSource(0, GFidelityFXCASVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawPrimitive(0, 2, 1);

	// Resolve render target
	//RHICmdList.CopyToResolveTarget(CASPassParams.GetRTTexture(), CASPassParams.GetRTTexture(), FResolveParams());

	RHICmdList.EndRenderPass();
}

#if FX_CAS_CUSTOM_UPSCALE_CALLBACK
void FFidelityFXCASModule::DrawToRenderTarget_RDG_RenderThread(FRDGBuilder& GraphBuilder, const class FFidelityFXCASPassParams_RDG& CASPassParams)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_DrawToRenderTarget_RDG);             // Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_DrawToRenderTarget_RDG); // Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

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
#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
#else
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
#endif	// UE v4.24
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

#if UE_VERSION_OLDER_THAN(4, 25, 0)	// UE v4.24
		SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), *PassParameters);
#else
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);
#endif	// UE v4.24

		// Draw
		RHICmdList.SetStreamSource(0, GFidelityFXCASVertexBuffer.VertexBufferRHI, 0);
		RHICmdList.DrawPrimitive(0, 2, 1);
	});
}
#endif // FX_CAS_CUSTOM_UPSCALE_CALLBACK
#endif // FX_CAS_PLUGIN_ENABLED

void FFidelityFXCASModule::GetSSCASResolutionInfo(FIntPoint& OutInputResolution, FIntPoint& OutOutputResolution) const
{
	FScopeLock Lock(&ResolutionInfoCS);
	OutInputResolution = InputResolution;
	OutOutputResolution = OutputResolution;
}

#if FX_CAS_PLUGIN_ENABLED
void FFidelityFXCASModule::SetSSCASResolutionInfo(const FIntPoint& InInputResolution, const FIntPoint& InOutputResolution)
{
	FScopeLock Lock(&ResolutionInfoCS);
	InputResolution = InInputResolution;
	OutputResolution = InOutputResolution;
}
#endif // FX_CAS_PLUGIN_ENABLED

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FFidelityFXCASModule, FidelityFXCAS)
