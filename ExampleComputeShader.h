#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"

#include "ExampleComputeShader.generated.h"

USTRUCT(BlueprintType)
struct FSlime
{
	GENERATED_BODY()
		UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Var")
		float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Var")
		float Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Var")
		float Angle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slime Var")
		int Type;
};

struct COMPUTESHADER_API FExampleComputeShaderDispatchParams
{
	int X;
	int Y;
	int Z;


	float DeltaTime;
	float TotalTime;
	float MoveSpeed;
	float SensorDistance;
	float TurnSpeed;
	int SensorSize;
	TArray<FSlime> slimes;
	FRenderTarget* RenderTarget;


	FExampleComputeShaderDispatchParams(int x, int y, int z)
		: X(x)
		, Y(y)
		, Z(z)
	{
	}
};

// This is a public interface that we define so outside code can invoke our compute shader.
class COMPUTESHADER_API FExampleComputeShaderInterface {
public:
	// Executes this shader on the render thread
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FExampleComputeShaderDispatchParams Params,
		TFunction<void(const TArray<FSlime>& OutputVal)> AsyncCallback
	);

	// Executes this shader on the render thread from the game thread via EnqueueRenderThreadCommand
	static void DispatchGameThread(
		FExampleComputeShaderDispatchParams Params,
		TFunction<void(const TArray<FSlime>& OutputVal)> AsyncCallback
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
			[Params, AsyncCallback](FRHICommandListImmediate& RHICmdList)
			{
				DispatchRenderThread(RHICmdList, Params, AsyncCallback);
			});
	}

	// Dispatches this shader. Can be called from any thread
	static void Dispatch(
		FExampleComputeShaderDispatchParams Params,
		TFunction<void(const TArray<FSlime>& OutputVal)> AsyncCallback
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params, AsyncCallback);
		}
		else {
			DispatchGameThread(Params, AsyncCallback);
		}
	}
};



DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExampleComputeShaderLibrary_AsyncExecutionCompleted, const TArray<FSlime>&, Value);


UCLASS() // Change the _API to match your project
class COMPUTESHADER_API UExampleComputeShaderLibrary_AsyncExecution : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:

	// Execute the actual load
	virtual void Activate() override {
		// Create a dispatch parameters struct and fill it the input array with our args
		FExampleComputeShaderDispatchParams Params(1, 1, 1);
		Params.slimes = slimeArray;
		Params.DeltaTime = deltaTime;
		Params.TotalTime = totalTime;
		Params.MoveSpeed = moveSpeed;
		Params.TurnSpeed = turnSpeed;
		Params.SensorSize = sensorSize;
		Params.SensorDistance = sensorDistance;
		Params.RenderTarget = RT;
		Params.X = sizeX;
		Params.Y = sizeY;

		// Dispatch the compute shader and wait until it completes
		FExampleComputeShaderInterface::Dispatch(Params, [this](const TArray<FSlime>& OutputVal) {
			this->Completed.Broadcast(OutputVal);
			});
	}



	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", Category = "ComputeShader", WorldContext = "WorldContextObject"))
		static UExampleComputeShaderLibrary_AsyncExecution* ExecuteBaseComputeShader(UObject* WorldContextObject, UTextureRenderTarget2D* RT,
			const TArray<FSlime>& slimeArray, float deltaTime, float totalTime, float moveSpeed, float sensorDistance, int sensorSize, float turnSpeed) {
		UExampleComputeShaderLibrary_AsyncExecution* Action = NewObject<UExampleComputeShaderLibrary_AsyncExecution>();
		Action->deltaTime = deltaTime;
		Action->totalTime = totalTime;
		Action->moveSpeed = moveSpeed;
		Action->slimeArray = slimeArray;
		Action->sensorDistance = sensorDistance;
		Action->sensorSize = sensorSize;
		Action->turnSpeed = turnSpeed;
		Action->RT = RT->GameThread_GetRenderTargetResource();
		Action->sizeX = RT->SizeX;
		Action->sizeY = RT->SizeY;
		Action->RegisterWithGameInstance(WorldContextObject);

		return Action;
	}

	UPROPERTY(BlueprintAssignable)
		FOnExampleComputeShaderLibrary_AsyncExecutionCompleted Completed;

	FRenderTarget* RT;
	TArray<FSlime> slimeArray;
	//TArray<float> pixelArray;
	float deltaTime;
	float totalTime;
	float moveSpeed;
	float turnSpeed;
	float sensorDistance;
	int sensorSize;
	int sizeX;
	int sizeY;

};