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
	0,
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

// Sink to track console variables value changes
static void FidelityFXCASDisplayInfoSink()
{
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
				FString Message = FString::Printf(TEXT("FidelityFX SS CAS: %s"), Module.GetIsSSCASEnabled() ? TEXT("ON") : TEXT("OFF"));
				if (Module.GetIsSSCASEnabled())
					Message += FString::Printf(TEXT(" (Sharpness: %.2f)"), Module.GetSSCASSharpness());
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Info, FText::AsCultureInvariant(Message));
			});
		}
		else
		{
			FCoreDelegates::OnGetOnScreenMessages.Remove(Handle);
		}
		GDisplayInfo = bNewDisplayInfo;
	}

	// SS CAS
	static bool GSSCAS = false;
	bool bNewSSCAS = CVarFidelityFXCAS_SSCAS.GetValueOnGameThread() > 0;
	if (bNewSSCAS != GSSCAS)
	{
		FFidelityFXCASModule::Get().SetIsSSCASEnabled(bNewSSCAS);
		GSSCAS = bNewSSCAS;
	}

	// SS CAS Sharpness
	static float GSSCASSharpness = 0.0f;
	float NewSSCASSharpness = FMath::Clamp(CVarFidelityFXCAS_SSCASSharpness.GetValueOnGameThread(), 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(NewSSCASSharpness, GSSCASSharpness))
	{
		FFidelityFXCASModule::Get().SetSSCASSharpness(NewSSCASSharpness);
		GSSCASSharpness = NewSSCASSharpness;
	}
}
FAutoConsoleVariableSink CFidelityFXCASCVarSink(FConsoleCommandDelegate::CreateStatic(&FidelityFXCASDisplayInfoSink));

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
	OnResolvedSceneColorHandle.Reset();
	SSCASSharpness = 0.0f;
}

void FFidelityFXCASModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

bool FFidelityFXCASModule::GetIsSSCASEnabled() const
{
	return OnResolvedSceneColorHandle.IsValid();
}

void FFidelityFXCASModule::SetIsSSCASEnabled(bool Enabled)
{
	if (Enabled == GetIsSSCASEnabled())
		return;	// Nothing to do

	const FName RendererModuleName("Renderer");
	IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);

	if (Enabled)
	{
		if (RendererModule)
			OnResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FFidelityFXCASModule::OnResolvedSceneColor_RenderThread);
	}
	else
	{
		if (RendererModule)
			RendererModule->GetResolvedSceneColorCallbacks().Remove(OnResolvedSceneColorHandle);
		OnResolvedSceneColorHandle.Reset();
	}
}

struct FFidelityFXDrawParams
{
	float SharpnessVal = 0.5f;
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

	const FRHITexture2D* GetInputTexture2D() { return InputTexture.IsValid() ? InputTexture->GetTexture2D() : nullptr; }
	const FIntPoint GetInputTextureSize() { return GetInputTexture2D() ? GetInputTexture2D()->GetSizeXY() : FIntPoint::ZeroValue; }

	static FTexture2DRHIRef EMPTY_Texture2DRHIRef;
	static FTextureRHIRef EMPTY_TextureRHIRef;
	static FSceneRenderTargetItem EMPTY_SceneRenderTargetItem;
	static FUnorderedAccessViewRHIRef EMPTY_UnorderedAccessViewRHIRef;

	FTextureRenderTargetResource* GetRenderTargetResource() const { return RenderTargetResource; }
	const FTexture2DRHIRef& GetRenderTargetTexture() const { return RenderTargetTexture2DRHI; }
	const FTextureRHIRef& GetRenterTargetTextureRHI() const { return RenderTargetTextureRHI; }
	const FIntPoint GetRenderTargetSize() { return OutputSize; }

	const FSceneRenderTargetItem& GetComputeShaderOutputRTItem() const { return ComputeShaderOutput.IsValid() ? ComputeShaderOutput->GetRenderTargetItem() : EMPTY_SceneRenderTargetItem; }
	const FUnorderedAccessViewRHIRef& GetUAV() const { return GetComputeShaderOutputRTItem().IsValid() ? GetComputeShaderOutputRTItem().UAV : EMPTY_UnorderedAccessViewRHIRef; }
	const FTextureRHIRef& GeetComputeShaderOutputTargetableTexture() const { return GetComputeShaderOutputRTItem().IsValid() ? GetComputeShaderOutputRTItem().TargetableTexture : EMPTY_TextureRHIRef; }
};

FTexture2DRHIRef FFidelityFXDrawParams::EMPTY_Texture2DRHIRef;
FTextureRHIRef FFidelityFXDrawParams::EMPTY_TextureRHIRef;
FSceneRenderTargetItem FFidelityFXDrawParams::EMPTY_SceneRenderTargetItem;
FUnorderedAccessViewRHIRef FFidelityFXDrawParams::EMPTY_UnorderedAccessViewRHIRef;


static void RunComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList, const FFidelityFXDrawParams& DrawParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_ComputeShader);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_ComputeShader);	// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	UnbindRenderTargets(RHICmdList);
	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DrawParams.GetUAV());

	FFidelityFXCASShaderCS::FParameters PassParameters;
	PassParameters.InputTexture = DrawParams.InputTexture;
	PassParameters.OutputTexture = DrawParams.GetUAV();

	CasSetup(reinterpret_cast<AU1*>(&PassParameters.const0), reinterpret_cast<AU1*>(&PassParameters.const1), DrawParams.SharpnessVal, static_cast<AF1>(DrawParams.InputSize.X),
		static_cast<AF1>(DrawParams.InputSize.Y), DrawParams.OutputSize.X, DrawParams.OutputSize.Y);

	bool FP16 = false;
	bool SharpenOnly = (DrawParams.InputSize == DrawParams.OutputSize);

	if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS<false, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(DrawParams.OutputSize.X, DrawParams.OutputSize.Y, 1));
	}
	else
	{
		TShaderMapRef<TFidelityFXCASShaderCS<false, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
		FComputeShaderUtils::Dispatch(RHICmdList, *ComputeShader, PassParameters, FIntVector(DrawParams.OutputSize.X, DrawParams.OutputSize.Y, 1));
	}
}

void DrawToRenderTarget_RenderThread(FRHICommandListImmediate& RHICmdList, const FFidelityFXDrawParams& DrawParams)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FidelityFXCASModule_PixelShader);	// Used to gather CPU profiling data for the UE4 session frontend
	SCOPED_DRAW_EVENT(RHICmdList, FidelityFXCASModule_PixelShader);		// Used to profile GPU activity and add metadata to be consumed by for example RenderDoc

	FRHIRenderPassInfo RenderPassInfo(DrawParams.GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RenderPassInfo, TEXT("FidelityFXCASModule_OutputToRenderTarget"));

	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FFidelityFXCASShaderVS> VertexShader(ShaderMap);
	TShaderMapRef<FFidelityFXCASShaderPS> PixelShader(ShaderMap);

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
	FFidelityFXCASShaderPS::FParameters PassParameters;
	PassParameters.UpscaledTexture = DrawParams.GeetComputeShaderOutputTargetableTexture();
	PassParameters.samLinearClamp = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	SetShaderParameters(RHICmdList, *PixelShader, PixelShader->GetPixelShader(), PassParameters);

	// Draw
	RHICmdList.SetStreamSource(0, GFidelityFXCASVertexBuffer.VertexBufferRHI, 0);
	RHICmdList.DrawPrimitive(0, 2, 1);

	// Resolve render target
	RHICmdList.CopyToResolveTarget(DrawParams.GetRenderTargetTexture(), DrawParams.GetRenterTargetTextureRHI(), FResolveParams());

	RHICmdList.EndRenderPass();
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

	//FFidelityFXDrawParams DrawParams(TEST_InputTexture->Resource->TextureRHI, TEST_RT);
	FFidelityFXDrawParams DrawParams(SceneColorTexture, SceneColorTexture);
	//DrawParams.InputSize = SceneColorTexture2D->GetSizeXY();
	//DrawParams.OutputSize = SceneContext.GetBufferSizeXY();

	//DrawParams.OutputSize = DrawParams.GetRenderTargetSize();

	// Release the shader output if the size changed
	bool NeedsRecreate = false;
	if (ComputeShaderOutput.IsValid())
	{
		FRHITexture2D* RHITexture2D = ComputeShaderOutput->GetRenderTargetItem().TargetableTexture->GetTexture2D();
		FIntPoint CurrentSize = RHITexture2D->GetSizeXY();
		if (CurrentSize != DrawParams.OutputSize)
		{
			//GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Red, FString::Printf(TEXT("Releasing ComputeShaderOutput [%dx%d]..."), CurrentSize.X, CurrentSize.Y));
			//GRenderTargetPool.FreeUnusedResource(ComputeShaderOutput);
			NeedsRecreate = true;
		}
	}

	// Create the shader output
	if (!ComputeShaderOutput.IsValid() || NeedsRecreate)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5.f, FColor::Green, FString::Printf(TEXT("Creating ComputeShaderOutput [%dx%d]..."), DrawParams.OutputSize.X, DrawParams.OutputSize.Y));
		FPooledRenderTargetDesc ComputeShaderOutputDesc(FPooledRenderTargetDesc::Create2DDesc(DrawParams.OutputSize, PF_FloatRGBA, FClearValueBinding::None, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		ComputeShaderOutputDesc.DebugName = TEXT("FidelityFXCASModule_ComputeShaderOutput");
		GRenderTargetPool.FindFreeElement(RHICmdList, ComputeShaderOutputDesc, ComputeShaderOutput, TEXT("FidelityFXCASModule_ComputeShaderOutput"));
	}

	DrawParams.ComputeShaderOutput = ComputeShaderOutput;
	DrawParams.SharpnessVal = SSCASSharpness;

	//auto& SceneColorRT = SceneContext.GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture;

	FTextureRHIRef REF = SceneContext.GetSceneColor()->GetRenderTargetItem().ShaderResourceTexture;

	//RunComputeShader_RenderThread(RHICmdList, DrawParams, SceneContext.GetSceneColorTextureUAV(), REF);
	//RunComputeShader_RenderThread(RHICmdList, DrawParams, SceneContext.GetSceneColor()->GetRenderTargetItem().UAV, SceneContext.GetSceneColor()->GetRenderTargetItem().TargetableTexture);

	//RunComputeShader_RenderThread(RHICmdList, DrawParams, ComputeShaderOutput->GetRenderTargetItem().UAV, SceneContext.GetSceneColorTexture());
	//DrawToRenderTarget_RenderThread(RHICmdList DrawParams, ComputeShaderOutput->GetRenderTargetItem().TargetableTexture);

	RunComputeShader_RenderThread(RHICmdList, DrawParams);
	DrawToRenderTarget_RenderThread(RHICmdList, DrawParams);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FFidelityFXCASModule, FidelityFXCAS)
