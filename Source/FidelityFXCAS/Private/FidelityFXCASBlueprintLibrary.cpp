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








class FMyTestVS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FMyTestVS, Global,);
	//DECLARE_EXPORTED_SHADER_TYPE(FMyTestVS, Global, MYMODULE_API);

	FMyTestVS() { }
	FMyTestVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	//static bool ShouldCache(EShaderPlatform Platform)
	//{
	//	return true;
	//}
};

class FMyTestPS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FMyTestPS, Global,);
	//DECLARE_EXPORTED_SHADER_TYPE(FMyTestPS, Global, MYMODULE_API);

	FShaderParameter MyColorParameter;

	FMyTestPS() { }
	FMyTestPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MyColorParameter.Bind(Initializer.ParameterMap, TEXT("MyColor"), SPF_Mandatory);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		// Add your own defines for the shader code
		OutEnvironment.SetDefine(TEXT("MY_DEFINE"), 1);


		//OutEnvironment.SetDefine(TEXT("GRID_SUBDIVISION_X"), kSubdivisionX);
		//OutEnvironment.SetDefine(TEXT("GRID_SUBDIVISION_Y"), kSubdivisionY);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	//static bool ShouldCache(EShaderPlatform Platform)
	//{
	//	// Could skip compiling for Platform == SP_METAL for example
	//	return true;
	//}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MyColorParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetColor(FRHICommandList& RHICmdList, const FLinearColor& Color)
	{
		SetShaderValue(RHICmdList, GetPixelShader(), MyColorParameter, Color);
	}
};

class FMyTestCS : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FMyTestCS, Global, );
	//DECLARE_EXPORTED_SHADER_TYPE(FMyTestCS, Global, SHADERSPLUS_API);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
    {
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	FMyTestCS() = default;
	explicit FMyTestCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
	{
		Input.Bind(Initializer.ParameterMap, TEXT("Input"));
		Output.Bind(Initializer.ParameterMap, TEXT("Output"));
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("INPUT_COMPONENTS"), 4);
		OutEnvironment.SetDefine(TEXT("OUTPUT_COMPONENTS"), 4);
	}

	void SetInput(FRHICommandList& RHICmdList, FShaderResourceViewRHIRef InInput)
	{
		if (Input.IsBound())
			RHICmdList.SetShaderResourceViewParameter(GetComputeShader(), Input.GetBaseIndex(), InInput);
	}

	void SetOutput(FRHICommandList& RHICmdList, FUnorderedAccessViewRHIRef InOutput)
	{
		if (Output.IsBound())
			RHICmdList.SetUAVParameter(GetComputeShader(), Output.GetBaseIndex(), InOutput);
	}

	void Unbind(FRHICommandList& RHICmdList)
	{
		const auto ComputeShaderRHI = GetComputeShader();

		if (Input.IsBound())
			RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, Input.GetBaseIndex(), FShaderResourceViewRHIRef());

		if (Output.IsBound())
			RHICmdList.SetUAVParameter(ComputeShaderRHI, Output.GetBaseIndex(), FUnorderedAccessViewRHIRef());
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		const auto bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Input << Output;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter Input;
	FShaderResourceParameter Output;
};











IMPLEMENT_SHADER_TYPE(, FMyTestVS, TEXT("/Plugin/FidelityFXCAS/Private/TestShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FMyTestPS, TEXT("/Plugin/FidelityFXCAS/Private/TestShader.usf"), TEXT("MainPS"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(, FMyTestCS, TEXT("/Plugin/FidelityFXCAS/Private/TestShader.usf"), TEXT("MainCS"), SF_Compute);











/*



class FShaderInstance
{
public:
	FShaderInstance(const ERHIFeatureLevel::Type InFeatureLevel)
	{
		FeatureLevel = InFeatureLevel;
		bIsExecuting = false;
		bIsUnloading = false;
	}
	virtual ~FShaderInstance()
	{
		bIsUnloading = true;
	}

	virtual bool CanExecute()
	{
		check(IsInGameThread());
		return !bIsUnloading && !bIsExecuting;
	}

	FORCEINLINE void SetDebugLabel(const FName Label) { DebugLabel = Label; }

protected:
	TOptional<FName> DebugLabel;

	FThreadSafeBool bIsExecuting;
	FThreadSafeBool bIsUnloading;
	ERHIFeatureLevel::Type FeatureLevel;
};



class FComputeShaderInstance : public FShaderInstance
{
public:
	FComputeShaderInstance(const ERHIFeatureLevel::Type FeatureLevel) : FShaderInstance(FeatureLevel)
	{
		bHasBeenRun = false;
	}
	virtual ~FComputeShaderInstance() { }

	void Dispatch()
	{
		if (!CanExecute())
			return;

		bIsExecuting = true;
		OnDispatch();
		bIsExecuting = false;
	}

	virtual void OnDispatch() { }

	void DispatchOnce()
	{
		if (!CanExecute() || bHasBeenRun)
			return;

		bIsExecuting = true;
		OnDispatchOnce();
		bIsExecuting = false;

		bHasBeenRun = true;
	}
	virtual void OnDispatchOnce() { }

protected:
	FThreadSafeBool bHasBeenRun;
};












template <typename TParameters>
class TComputeShaderInstance : public FShaderInstance
{
public:
	TComputeShaderInstance(const ERHIFeatureLevel::Type FeatureLevel) : FShaderInstance(FeatureLevel) { }
	virtual ~TComputeShaderInstance() { }

	void Dispatch(TParameters& Parameters)
	{
		if (!CanExecute())
			return;

		bIsExecuting = true;
		OnDispatch(Parameters);
		bIsExecuting = false;
	}

	virtual void OnDispatch(TParameters& Parameters) { }

	void DispatchOnce(TParameters& Parameters)
	{
		if (!CanExecute() || bHasBeenRun)
			return;

		bIsExecuting = true;
		OnDispatchOnce(Parameters);
		bIsExecuting = false;

		bHasBeenRun = true;
	}

	virtual void OnDispatchOnce(TParameters& Parameters) { }

protected:
	FThreadSafeBool bHasBeenRun;
};







struct FConvertParameters
{
public:
	FIntPoint Size;

	// Inputs
	FShaderResourceViewRHIRef Input_SRV;

	// Outputs
	FUnorderedAccessViewRHIRef Output_UAV;
};


class TConvertInstance : public TComputeShaderInstance<FConvertParameters>
{
public:
	TConvertInstance(const int32 SizeX, const int32 SizeY, const ERHIFeatureLevel::Type FeatureLevel);
	virtual ~TConvertInstance() { }

	void OnDispatch(FConvertParameters& Parameters) override;

	FTexture2DRHIRef GetOutput() const { return Output; }
	FUnorderedAccessViewRHIRef GetOutput_UAV() const { return Output_UAV; }
	FShaderResourceViewRHIRef GetOutput_SRV() const { return Output_SRV; }

private:
	FTexture2DRHIRef Output;
	FUnorderedAccessViewRHIRef Output_UAV;
	FShaderResourceViewRHIRef Output_SRV;

	constexpr int32 GetComponentCount(EPixelFormat Format);
};

TConvertInstance::TConvertInstance(const int32 SizeX, const int32 SizeY, const ERHIFeatureLevel::Type FeatureLevel)
	: TComputeShaderInstance<FConvertParameters>(FeatureLevel)
{
	FRHIResourceCreateInfo CreateInfo;
	Output = RHICreateTexture2D(SizeX, SizeY, PF_FloatRGBA, 1, 1, TexCreate_ShaderResource | TexCreate_UAV, CreateInfo);
	Output_UAV = RHICreateUnorderedAccessView(Output);
	Output_SRV = RHICreateShaderResourceView(Output, 0);
}

void TConvertInstance::OnDispatch(FConvertParameters& Parameters)
{
	check(Parameters.Input_SRV.IsValid());

	Parameters.Size = Output->GetSizeXY();
	Parameters.Output_UAV = Output_UAV;

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		Convert,
		FConvertParameters, Parameters, Parameters,
		{
			SCOPED_DRAW_EVENT(RHICmdList, Convert);

			TShaderMapRef<FMyTestCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());

			ComputeShader->SetInput(RHICmdList, Parameters.Input_SRV);
			ComputeShader->SetOutput(RHICmdList, Parameters.Output_UAV);

			DispatchComputeShader(RHICmdList, *ComputeShader, 32, 32, 1);
			ComputeShader->Unbind(RHICmdList);
		}
	)
}

static TConvertInstance ConvertCS(256, 266, ERHIFeatureLevel::SM5);




*/






















static TAutoConsoleVariable<int32> CVarMyTest(
	TEXT("r.MyTest"),
	0,
	TEXT("Test My Global Shader, set it to 0 to disable, or to 1, 2 or 3 for fun!"),
	ECVF_RenderThreadSafe
);













struct FTextureVertex
{
	FVector4 Position;
	FVector2D UV;
};


class FTextureVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		const auto Stride = sizeof(FTextureVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, Position), VET_Float4, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FTextureVertex, UV), VET_Float2, 1, Stride));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

//extern SHADERSPLUS_API TGlobalResource<FTextureVertexDeclaration> GTextureVertexDeclaration;

TGlobalResource<FTextureVertexDeclaration> GTextureVertexDeclaration;

/*
RENDERCORE_API FVertexDeclarationRHIRef& GetVertexDeclarationFTexture()
{
	return GTextureVertexDeclaration.VertexDeclarationRHI;
}
*/










static void DrawCASToRenderTarget_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	const FName& TextureRenderTargetName,
	FTextureRenderTargetResource* OutTextureRenderTargetResource,
	ERHIFeatureLevel::Type FeatureLevel,
	FLinearColor ColorParam)
{
	check(IsInRenderingThread());

#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	TextureRenderTargetName.ToString(EventName);
	SCOPED_DRAW_EVENTF(RHICmdList, SceneCapture, TEXT("DrawCASToRenderTarget_RenderThread %s"), *EventName);
#else
	SCOPED_DRAW_EVENT(RHICmdList, DrawCASToRenderTarget_RenderThread);
#endif

	FRHITexture2D* RenderTargetTexture = OutTextureRenderTargetResource->GetRenderTargetTexture();

	RHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, RenderTargetTexture);

	FRHIRenderPassInfo RPInfo(RenderTargetTexture, ERenderTargetActions::DontLoad_Store, OutTextureRenderTargetResource->TextureRHI);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("DrawUVDisplacement"));
	{
		//FIntPoint DisplacementMapResolution(OutTextureRenderTargetResource->GetSizeX(), OutTextureRenderTargetResource->GetSizeY());

		// Update viewport.
		//RHICmdList.SetViewport(0, 0, 0.f, DisplacementMapResolution.X, DisplacementMapResolution.Y, 1.f);

		// Get shaders.
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef< FMyTestVS > VertexShader(GlobalShaderMap);
		TShaderMapRef< FMyTestPS > PixelShader(GlobalShaderMap);

		// Set the graphic pipeline state.
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		//GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		//GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTextureVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		// Update viewport.
		//RHICmdList.SetViewport(0, 0, 0.f, OutTextureRenderTargetResource->GetSizeX(), OutTextureRenderTargetResource->GetSizeY(), 1.f);

		// Update shader uniform parameters.
		PixelShader->SetColor(RHICmdList, ColorParam);
		//VertexShader->SetParameters(RHICmdList, VertexShader->GetVertexShader(), CompiledCameraModel, DisplacementMapResolution);
		//PixelShader->SetParameters(RHICmdList, PixelShader->GetPixelShader(), CompiledCameraModel, DisplacementMapResolution);

		// Draw grid.
		//uint32 PrimitiveCount = kSubdivisionX * kSubdivisionY * 2;
		//RHICmdList.DrawPrimitive(0, PrimitiveCount, 1);





		FRHIResourceCreateInfo CreateInfo;
		FVertexBufferRHIRef VertexBufferRHI = RHICreateVertexBuffer(sizeof(FTextureVertex) * 4, BUF_Volatile, CreateInfo);
		void* VoidPtr = RHILockVertexBuffer(VertexBufferRHI, 0, sizeof(FTextureVertex) * 4, RLM_WriteOnly);

		FTextureVertex* Vertices = (FTextureVertex*)VoidPtr;
		Vertices[0].Position = FVector4(-1.0f, 1.0f, 0, 1.0f);
		Vertices[1].Position = FVector4(1.0f, 1.0f, 0, 1.0f);
		Vertices[2].Position = FVector4(-1.0f, -1.0f, 0, 1.0f);
		Vertices[3].Position = FVector4(1.0f, -1.0f, 0, 1.0f);
		Vertices[0].UV = FVector2D(0, 0);
		Vertices[1].UV = FVector2D(1, 0);
		Vertices[2].UV = FVector2D(0, 1);
		Vertices[3].UV = FVector2D(1, 1);
		RHIUnlockVertexBuffer(VertexBufferRHI);

		RHICmdList.SetStreamSource(0, VertexBufferRHI, 0);
		//RHICmdList.DrawPrimitive(PT_TriangleStrip, 0, 2, 1);
		RHICmdList.DrawPrimitive(0, 2, 1);
	}
	RHICmdList.EndRenderPass();
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


	

	FFidelityFXCASShaderCS::FParameters PassParameters;
	PassParameters.InputTexture = InInputTexture;
	PassParameters.OutputTexture = ComputeShaderOutputUAV;

	CasSetup(reinterpret_cast<AU1*>(&PassParameters.const0), reinterpret_cast<AU1*>(&PassParameters.const1), SharpnessVal, static_cast<AF1>(RenderWidth),
		static_cast<AF1>(RenderHeight), OutWidth, OutHeight);


	bool FP16 = false;
	bool SharpenOnly = (RenderWidth == OutWidth && RenderHeight == OutHeight);

	if (SharpenOnly)
	{
		TShaderMapRef<TFidelityFXCASShaderCS<false, true>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
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
		TShaderMapRef<TFidelityFXCASShaderCS<false, false>> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
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

void UFidelityFXCASBlueprintLibrary::TEST_SetRT(class UTextureRenderTarget2D* OutputRenderTarget)
{
	FFidelityFXCASModule::Get().TEST_SetRT(OutputRenderTarget);
}

void UFidelityFXCASBlueprintLibrary::TEST_SetTexture(class UTexture2D* InInputTexture)
{
	FFidelityFXCASModule::Get().TEST_SetTexture(InInputTexture);
}
