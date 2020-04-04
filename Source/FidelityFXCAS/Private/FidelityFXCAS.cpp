// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FidelityFXCAS.h"
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

struct FFidelityFXDrawParams
{
	FIntPoint InputSize = FIntPoint::ZeroValue;
	FIntPoint OutputSize = FIntPoint::ZeroValue;

	FTextureRHIRef InputTexture;
	//UTextureRenderTarget2D* RenderTarget;

	FTextureRenderTargetResource* RenderTargetResource;
	FTexture2DRHIRef RenderTargetTexture2DRHI;
	FTextureRHIRef RenderTargetTextureRHI;

	TRefCountPtr<IPooledRenderTarget> ComputeShaderOutput;

	FFidelityFXDrawParams() { }
	FFidelityFXDrawParams(UTextureRenderTarget2D* InRenderTarget)
	{
		SetRenderTarget(InRenderTarget);
	}
	FFidelityFXDrawParams(FTextureRHIRef InInputTexture, UTextureRenderTarget2D* InRenderTarget)
		: InputTexture(InInputTexture)
	{
		SetRenderTarget(InRenderTarget);
		InputSize = GetInputTextureSize();
	}
	FFidelityFXDrawParams(FTextureRHIRef InInputTexture, FTextureRenderTargetResource* InRenderTarget)
		: InputTexture(InInputTexture)
	{
		SetRenderTarget(InRenderTarget);
		InputSize = GetInputTextureSize();
	}
	FFidelityFXDrawParams(FTextureRHIRef InInputTexture, FTextureRHIRef InRenderTarget)
		: InputTexture(InInputTexture)
	{
		SetRenderTarget(InRenderTarget);
		InputSize = GetInputTextureSize();
	}

	void SetRenderTarget(UTextureRenderTarget2D* InRenderTarget)
	{
		SetRenderTarget(InRenderTarget ? InRenderTarget->GetRenderTargetResource() : nullptr);
	}

	void SetRenderTarget(FTextureRenderTargetResource* InRenderTarget)
	{
		RenderTargetResource = InRenderTarget;
		RenderTargetTexture2DRHI = RenderTargetResource ? RenderTargetResource->GetRenderTargetTexture() : nullptr;
		RenderTargetTextureRHI = RenderTargetResource ? RenderTargetResource->TextureRHI : nullptr;
		OutputSize = RenderTargetTexture2DRHI.IsValid() ? FIntPoint(RenderTargetTextureRHI->GetSizeXYZ().X, RenderTargetTextureRHI->GetSizeXYZ().Y) : FIntPoint::ZeroValue;
	}
	void SetRenderTarget(FTextureRHIRef InRenderTarget)
	{
		RenderTargetTextureRHI = InRenderTarget;
		RenderTargetTexture2DRHI = (RenderTargetTextureRHI.IsValid() && RenderTargetTextureRHI->GetTexture2D()) ? RenderTargetTextureRHI->GetTexture2D() : nullptr;
		OutputSize = RenderTargetTexture2DRHI.IsValid() ? FIntPoint(RenderTargetTextureRHI->GetSizeXYZ().X, RenderTargetTextureRHI->GetSizeXYZ().Y) : FIntPoint::ZeroValue;
	}

private:
	const FRHITexture2D* GetInputTexture2D() { return InputTexture.IsValid() ? InputTexture->GetTexture2D() : nullptr; }
public:
	const FIntPoint GetInputTextureSize() { return GetInputTexture2D() ? GetInputTexture2D()->GetSizeXY() : FIntPoint::ZeroValue; }

private:
	static FTextureRHIRef EMPTY_TextureRHIRef;
	static FSceneRenderTargetItem EMPTY_SceneRenderTargetItem;
	static FUnorderedAccessViewRHIRef EMPTY_UnorderedAccessViewRHIRef;

public:
	//FTextureRenderTargetResource* GetRenderTargetResource() const { return RenderTargetResource; }
	const FTexture2DRHIRef& GetRenderTargetTexture() const { return RenderTargetTexture2DRHI; }
	const FTextureRHIRef& GetRenterTargetTextureRHI() const { return RenderTargetTextureRHI; }
	//const FIntPoint GetRenderTargetSize() { return OutputSize; }

private:
	const FSceneRenderTargetItem& GetComputeShaderOutputRTItem() const { return ComputeShaderOutput.IsValid() ? ComputeShaderOutput->GetRenderTargetItem() : EMPTY_SceneRenderTargetItem; }
public:
	const FUnorderedAccessViewRHIRef& GetUAV() const { return GetComputeShaderOutputRTItem().IsValid() ? GetComputeShaderOutputRTItem().UAV : EMPTY_UnorderedAccessViewRHIRef; }
	const FTextureRHIRef& GetComputeShaderOutputTargetableTexture() const { return GetComputeShaderOutputRTItem().IsValid() ? GetComputeShaderOutputRTItem().TargetableTexture : EMPTY_TextureRHIRef; }
};

FTextureRHIRef FFidelityFXDrawParams::EMPTY_TextureRHIRef;
FSceneRenderTargetItem FFidelityFXDrawParams::EMPTY_SceneRenderTargetItem;
FUnorderedAccessViewRHIRef FFidelityFXDrawParams::EMPTY_UnorderedAccessViewRHIRef;




struct FFidelityFXDrawParams_RDG
{
	FIntPoint InputSize = FIntPoint::ZeroValue;
	FIntPoint OutputSize = FIntPoint::ZeroValue;

	FRDGTextureRef InputTexture;
	FRDGTextureRef RenderTarget;

	FRenderTargetBinding RTBinding;

	FTextureRenderTargetResource* RenderTargetResource;
	FTexture2DRHIRef RenderTargetTexture2DRHI;
	FTextureRHIRef RenderTargetTextureRHI;

	TRefCountPtr<IPooledRenderTarget> ComputeShaderOutput;

	FFidelityFXDrawParams_RDG() { }
	FFidelityFXDrawParams_RDG(FRDGTextureRef InInputTexture, const FRenderTargetBinding& InRTBinding)
		: InputTexture(InInputTexture)
		, RTBinding(InRTBinding)
	{
		InputSize = GetInputTextureSize();
		OutputSize = RTBinding.GetTexture() != nullptr ? RTBinding.GetTexture()->Desc.Extent : FIntPoint::ZeroValue;
	}

	const FIntPoint GetInputTextureSize() { return InputTexture != nullptr ? InputTexture->Desc.Extent : FIntPoint::ZeroValue; }

private:
	static FTextureRHIRef EMPTY_TextureRHIRef;
	static FSceneRenderTargetItem EMPTY_SceneRenderTargetItem;
	static FUnorderedAccessViewRHIRef EMPTY_UnorderedAccessViewRHIRef;

private:
	const FSceneRenderTargetItem& GetComputeShaderOutputRTItem() const { return ComputeShaderOutput.IsValid() ? ComputeShaderOutput->GetRenderTargetItem() : EMPTY_SceneRenderTargetItem; }
public:
	const FUnorderedAccessViewRHIRef& GetUAV() const { return GetComputeShaderOutputRTItem().IsValid() ? GetComputeShaderOutputRTItem().UAV : EMPTY_UnorderedAccessViewRHIRef; }
	const FTextureRHIRef& GetComputeShaderOutputTargetableTexture() const { return GetComputeShaderOutputRTItem().IsValid() ? GetComputeShaderOutputRTItem().TargetableTexture : EMPTY_TextureRHIRef; }
};

FTextureRHIRef FFidelityFXDrawParams_RDG::EMPTY_TextureRHIRef;
FSceneRenderTargetItem FFidelityFXDrawParams_RDG::EMPTY_SceneRenderTargetItem;
FUnorderedAccessViewRHIRef FFidelityFXDrawParams_RDG::EMPTY_UnorderedAccessViewRHIRef;

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

	FFidelityFXDrawParams DrawParams(SceneColorTexture, SceneColorTexture);

	// Update resolution info
	SetSSCASResolutionInfo(DrawParams.InputSize, DrawParams.OutputSize);

	// Make sure the computer shader output is ready and with correct size
	PrepareComputeShaderOutput(RHICmdList, DrawParams.OutputSize, ComputeShaderOutput_RHI);
	DrawParams.ComputeShaderOutput = ComputeShaderOutput_RHI;

	// Call shaders
	RunComputeShader_RHI_RenderThread(RHICmdList, DrawParams);
	DrawToRenderTarget_RHI_RenderThread(RHICmdList, DrawParams);
}

void FFidelityFXCASModule::OnAddUpscalePass_RenderThread(FRDGBuilder& GraphBuilder, FRDGTexture* SceneColor, const FRenderTargetBinding& RTBinding)
{
	check(IsInRenderingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_Render);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_Render);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FFidelityFXDrawParams_RDG DrawParams(SceneColor, RTBinding);

	// Update resolution info
	SetSSCASResolutionInfo(DrawParams.InputSize, DrawParams.OutputSize);

	// Make sure the computer shader output is ready and with correct size
	PrepareComputeShaderOutput(GraphBuilder.RHICmdList, DrawParams.OutputSize, ComputeShaderOutput_RDG);
	DrawParams.ComputeShaderOutput = ComputeShaderOutput_RDG;

	// Call shaders
	RunComputeShader_RDG_RenderThread(GraphBuilder, DrawParams);
	DrawToRenderTarget_RDG_RenderThread(GraphBuilder, DrawParams);
}

void FFidelityFXCASModule::RunComputeShader_RHI_RenderThread(FRHICommandListImmediate& RHICmdList, const struct FFidelityFXDrawParams& DrawParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_ComputeShader_RHI);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_ComputeShader_RHI);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DrawParams.GetUAV());

	// Setup shader parameters
	FFidelityFXCASShaderCS::FParameters PassParameters;
	PassParameters.InputTexture = DrawParams.InputTexture;
	PassParameters.OutputTexture = DrawParams.GetUAV();
	CasSetup(reinterpret_cast<AU1*>(&PassParameters.const0), reinterpret_cast<AU1*>(&PassParameters.const1),
		SSCASSharpness,
		static_cast<AF1>(DrawParams.InputSize.X), static_cast<AF1>(DrawParams.InputSize.Y),
		DrawParams.OutputSize.X, DrawParams.OutputSize.Y);

	// Choose shader version and dispatch
	bool SharpenOnly = (DrawParams.InputSize == DrawParams.OutputSize);
	if (CVarFidelityFXCAS_SSCASNoUpscale.GetValueOnGameThread() > 0)
		SharpenOnly = true;
	if (bUseFP16 && SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS<true, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
	}
	else if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS<false, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
	}
	else if (bUseFP16)
	{
		TShaderMapRef<TFidelityFXCASShaderCS<true, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
	}
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS<false, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
	}
}

void FFidelityFXCASModule::RunComputeShader_RDG_RenderThread(FRDGBuilder& GraphBuilder, const struct FFidelityFXDrawParams_RDG& DrawParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_ComputeShader_RDG);				// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_ComputeShader_RDG);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	// Setup shader parameters
	FFidelityFXCASShaderCS_RDG::FParameters* PassParameters = GraphBuilder.AllocParameters<FFidelityFXCASShaderCS_RDG::FParameters>();
	PassParameters->InputTexture = DrawParams.InputTexture;
	PassParameters->OutputTexture = DrawParams.GetUAV();
	CasSetup(reinterpret_cast<AU1*>(&PassParameters->const0), reinterpret_cast<AU1*>(&PassParameters->const1),
		SSCASSharpness,
		static_cast<AF1>(DrawParams.InputSize.X), static_cast<AF1>(DrawParams.InputSize.Y),
		DrawParams.OutputSize.X, DrawParams.OutputSize.Y);

	// Choose shader version and dispatch
	bool SharpenOnly = (DrawParams.InputSize == DrawParams.OutputSize);
	if (CVarFidelityFXCAS_SSCASNoUpscale.GetValueOnGameThread() > 0)
		SharpenOnly = true;
	if (bUseFP16 && SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<true, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", DrawParams.InputSize.X, DrawParams.InputSize.Y, DrawParams.OutputSize.X, DrawParams.OutputSize.Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
	}
	else if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<false, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", DrawParams.InputSize.X, DrawParams.InputSize.Y, DrawParams.OutputSize.X, DrawParams.OutputSize.Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
	}
	else if (bUseFP16)
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<true, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", DrawParams.InputSize.X, DrawParams.InputSize.Y, DrawParams.OutputSize.X, DrawParams.OutputSize.Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
	}
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS_RDG<false, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::AddPass(GraphBuilder,
			RDG_EVENT_NAME("Upscale CS %dx%d -> %dx%d", DrawParams.InputSize.X, DrawParams.InputSize.Y, DrawParams.OutputSize.X, DrawParams.OutputSize.Y),
			*ComputeShader, PassParameters, GetDispatchGroupCount(DrawParams.OutputSize));
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

void FFidelityFXCASModule::DrawToRenderTarget_RHI_RenderThread(FRHICommandListImmediate& RHICmdList, const struct FFidelityFXDrawParams& DrawParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_PixelShader);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_PixelShader);		// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FRHIRenderPassInfo RenderPassInfo(DrawParams.GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("FidelityFXCASModule_DrawToRenderTarget_RHI_RenderThread"));

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFidelityFXCASShaderVS> VertexShader(ShaderMap);
	TShaderMapRef<FFidelityFXCASShaderPS> PixelShader(ShaderMap);

	// Setup the pixel shader
	FFidelityFXCASShaderPS::FParameters PassParameters;
	PassParameters.UpscaledTexture = DrawParams.GetComputeShaderOutputTargetableTexture();
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
	RHICmdList.CopyToResolveTarget(DrawParams.GetRenderTargetTexture(), DrawParams.GetRenterTargetTextureRHI(), FResolveParams());

	RHICmdList.EndRenderPass();
}

void FFidelityFXCASModule::DrawToRenderTarget_RDG_RenderThread(FRDGBuilder& GraphBuilder, const struct FFidelityFXDrawParams_RDG& DrawParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_PixelShader);				// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, FidelityFXCASModule_PixelShader);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFidelityFXCASShaderVS> VertexShader(ShaderMap);
	TShaderMapRef<FFidelityFXCASShaderPS_RDG> PixelShader(ShaderMap);

	// Setup the pixel shader
	FFidelityFXCASShaderPS_RDG::FParameters* PassParameters = GraphBuilder.AllocParameters<FFidelityFXCASShaderPS_RDG::FParameters>();
	PassParameters->UpscaledTexture = DrawParams.GetComputeShaderOutputTargetableTexture();
	PassParameters->samLinearClamp = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	PassParameters->RenderTargets[0] = DrawParams.RTBinding;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Upscale PS %dx%d -> %dx%d", DrawParams.InputSize.X, DrawParams.InputSize.Y, DrawParams.OutputSize.X, DrawParams.OutputSize.Y),
		PassParameters,
		ERDGPassFlags::Raster,
		[VertexShader, PixelShader, PassParameters, DrawParams](FRHICommandList& RHICmdList)
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
