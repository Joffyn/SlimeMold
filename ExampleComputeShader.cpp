#include "ExampleComputeShader.h"
#include "ComputeShader/Public/ExampleComputeShader/ExampleComputeShader.h"
#include "PixelShaderUtils.h"
#include "RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MaterialShader.h"

DECLARE_STATS_GROUP(TEXT("ExampleComputeShader"), STATGROUP_ExampleComputeShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ExampleComputeShader Execute"), STAT_ExampleComputeShader_Execute, STATGROUP_ExampleComputeShader);

// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
class COMPUTESHADER_API FExampleComputeShader : public FGlobalShader
{
public:
	
	DECLARE_GLOBAL_SHADER(FExampleComputeShader);
	SHADER_USE_PARAMETER_STRUCT(FExampleComputeShader, FGlobalShader);
	
	
	class FExampleComputeShader_Perm_TEST : SHADER_PERMUTATION_INT("TEST", 1);
	using FPermutationDomain = TShaderPermutationDomain<
		FExampleComputeShader_Perm_TEST
	>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		/*
		* Here's where you define one or more of the input parameters for your shader.
		* Some examples:
		*/
		// SHADER_PARAMETER(uint32, MyUint32) // On the shader side: uint32 MyUint32;
		// SHADER_PARAMETER(FVector3f, MyVector) // On the shader side: float3 MyVector;

		// SHADER_PARAMETER_TEXTURE(Texture2D, MyTexture) // On the shader side: Texture2D<float4> MyTexture; (float4 should be whatever you expect each pixel in the texture to be, in this case float4(R,G,B,A) for 4 channels)
		// SHADER_PARAMETER_SAMPLER(SamplerState, MyTextureSampler) // On the shader side: SamplerState MySampler; // CPP side: TStaticSamplerState<ESamplerFilter::SF_Bilinear>::GetRHI();

		// SHADER_PARAMETER_ARRAY(float, MyFloatArray, [3]) // On the shader side: float MyFloatArray[3];

		// SHADER_PARAMETER_UAV(RWTexture2D<FVector4f>, MyTextureUAV) // On the shader side: RWTexture2D<float4> MyTextureUAV;
		// SHADER_PARAMETER_UAV(RWStructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWStructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_UAV(RWBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: RWBuffer<FMyCustomStruct> MyCustomStructs;

		// SHADER_PARAMETER_SRV(StructuredBuffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: StructuredBuffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Buffer<FMyCustomStruct>, MyCustomStructs) // On the shader side: Buffer<FMyCustomStruct> MyCustomStructs;
		// SHADER_PARAMETER_SRV(Texture2D<FVector4f>, MyReadOnlyTexture) // On the shader side: Texture2D<float4> MyReadOnlyTexture;

		// SHADER_PARAMETER_STRUCT_REF(FMyCustomStruct, MyCustomStruct)

		
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FSlime>, SlimeBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<FVector4f>, RenderImport) // On the shader side: Texture2D<float4> MyReadOnlyTexture;
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<FVector4f>, RenderTarget)
		SHADER_PARAMETER(float, DeltaTime) // On the shader side: uint32 MyUint32;
		SHADER_PARAMETER(float, TotalTime) // On the shader side: uint32 MyUint32;
		SHADER_PARAMETER(float, MoveSpeed) // On the shader side: uint32 MyUint32;
		SHADER_PARAMETER(float, TurnSpeed) // On the shader side: uint32 MyUint32;
		SHADER_PARAMETER(int, SlimeAmount) // On the shader side: uint32 MyUint32;
		SHADER_PARAMETER(float, SensorDistance) // On the shader side: uint32 MyUint32;
		SHADER_PARAMETER(int, SensorSize) // On the shader side: uint32 MyUint32;
		

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		
		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		/*
		* Here you define constants that can be used statically in the shader code.
		* Example:
		*/
		// OutEnvironment.SetDefine(TEXT("MY_CUSTOM_CONST"), TEXT("1"));

		/*
		* These defines are used in the thread count section of our shader
		*/
		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_ExampleComputeShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_ExampleComputeShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_ExampleComputeShader_Z);

		// This shader must support typed UAV load and we are testing if it is supported at runtime using RHIIsTypedUAVLoadSupported
		//OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

		// FForwardLightingParameters::ModifyCompilationEnvironment(Parameters.Platform, OutEnvironment);
	}
private:
};

// This will tell the engine to create the shader and where the shader entry point is.
//                            ShaderType                            ShaderPath                     Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FExampleComputeShader, "/ComputeShaderShaders/ExampleComputeShader/ExampleComputeShader.usf", "ExampleComputeShader", SF_Compute);

void FExampleComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FExampleComputeShaderDispatchParams Params, TFunction<void(const TArray<FSlime>& OutputVal)> AsyncCallback) {
	FRDGBuilder GraphBuilder(RHICmdList);

	{
		SCOPE_CYCLE_COUNTER(STAT_ExampleComputeShader_Execute);
		DECLARE_GPU_STAT(ExampleComputeShader)
		RDG_EVENT_SCOPE(GraphBuilder, "ExampleComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, ExampleComputeShader);
		
		typename FExampleComputeShader::FPermutationDomain PermutationVector;
		
		// Add any static permutation options here
		// PermutationVector.Set<FExampleComputeShader::FMyPermutationName>(12345);

		TShaderMapRef<FExampleComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		
		//PF_B8G8R8A8
		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid) {



			FExampleComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FExampleComputeShader::FParameters>();

			FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Params.RenderTarget->GetSizeXY(), PF_B8G8R8A8, FClearValueBinding::None, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
			FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc, TEXT("ExampleComputeShader"));


			FSceneRenderTargetItem RenderTargetItem;
			RenderTargetItem.TargetableTexture = Params.RenderTarget->GetRenderTargetTexture();
			RenderTargetItem.ShaderResourceTexture = Params.RenderTarget->GetRenderTargetTexture();
			FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(Params.RenderTarget->GetSizeXY(),
				Params.RenderTarget->GetRenderTargetTexture()->GetFormat(), FClearValueBinding::Black, TexCreate_None, TexCreate_RenderTargetable |
				TexCreate_ShaderResource | TexCreate_UAV, false);
			TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
			GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, PooledRenderTarget, RenderTargetItem);

			FRDGTextureRef DisplacementRenderTargetRDG = GraphBuilder.RegisterExternalTexture(PooledRenderTarget, TEXT("RenderTarget"));
			PassParameters->RenderImport = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(DisplacementRenderTargetRDG));
			//FRDGTextureUAVRef TmpTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TargetTexture));
			//TmpTexture->GetParent()->Desc = Desc;
			
			//ExtractedTexture->
			
			//GraphBuilder.QueueTextureExtraction(TmpTexture, &ExtractedTexture);
			FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, Params.RenderTarget->GetRenderTargetTexture(), TEXT("ExampleComputeShader"));
			PassParameters->RenderTarget = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(TmpTexture));
			
			//PassParameters->RenderTarget = GraphBuilder.CreateTexture(FRenderTargetBinding(TmpTexture, ERenderTargetLoadAction::ELoad));
			
			const void* RawData = (void*)Params.slimes.GetData();
			int NumInputs = Params.slimes.Num();
			int InputSize = sizeof(FSlime);
			//FRDGBufferRef InputBuffer = CreateUploadBuffer(GraphBuilder, TEXT("InputBuffer"), InputSize, NumInputs, RawData, InputSize * NumInputs);
			
			FRDGBufferRef OutPutBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("Slimes"), InputSize, NumInputs, RawData, InputSize * NumInputs);
			PassParameters->SlimeBuffer = GraphBuilder.CreateUAV(OutPutBuffer);
			PassParameters->DeltaTime = Params.DeltaTime;
			PassParameters->TotalTime = Params.TotalTime;
			PassParameters->SlimeAmount = NumInputs;
			PassParameters->MoveSpeed = Params.MoveSpeed;
			PassParameters->TurnSpeed = Params.TurnSpeed;
			PassParameters->SensorDistance = Params.SensorDistance;
			PassParameters->SensorSize = Params.SensorSize;
			//PassParameters->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(InputBuffer, PF_R32_SINT));

			//FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(
			//	FRDGBufferDesc::CreateBufferDesc(sizeof(int32), 1),
			//	TEXT("OutputBuffer"));
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);


			//PassParameters->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_SINT));
			//GraphBuilder.Execute();

			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
			{
				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
			});

			
			FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteExampleComputeShaderOutput"));
			AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutPutBuffer, 0u);

			auto RunnerFunc = [GPUBufferReadback, AsyncCallback, NumInputs](auto&& RunnerFunc) -> void {
				if (GPUBufferReadback->IsReady()) {
					
						TArray<FSlime> output((FSlime*)GPUBufferReadback->Lock(1), NumInputs);
						GPUBufferReadback->Unlock();

						AsyncTask(ENamedThreads::GameThread, [AsyncCallback, output]() {
							AsyncCallback(output);
						});

						delete GPUBufferReadback;
					
				} 
				else 
				{
					AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
						RunnerFunc(RunnerFunc);
					});
				}
			};

			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
			});

			// The copy will fail if we don't have matching formats, let's check and make sure we do.
			if (TargetTexture->Desc.Format == PF_B8G8R8A8) {
				AddCopyTexturePass(GraphBuilder, TmpTexture, TargetTexture, FRHICopyTextureInfo());
			}
			else {
			#if WITH_EDITOR
				GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The provided render target has an incompatible format (Please change the RT format to: RGBA8).")));
			#endif
			}
			
		} else {
			#if WITH_EDITOR
				GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader has a problem.")));
			#endif

			// We exit here as we don't want to crash the game if the shader is not found or has an error.
			
		}
	}

	GraphBuilder.Execute();
}

//		AddReadbackTexturePass(GraphBuilder, RDG_EVENT_NAME("ExecuteExampleComputeShader"), TmpTexture,
//[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
//{
//	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
//});