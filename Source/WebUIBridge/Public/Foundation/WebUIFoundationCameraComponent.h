#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WebUIFoundationCameraComponent.generated.h"

class AActor;
class APawn;
class APlayerController;
class FJsonObject;
class UWebUIFoundationBridgeComponent;
class FWebUIFoundationMouseWheelInputProcessor;
class UInputComponent;

/**
 * WebUIFoundation 相机/输入业务组件。
 *
 * 负责：
 * - flyTo 平滑飞行；
 * - resetView；
 * - setInputEnabled；
 * - 鼠标滚轮前后移动；
 * - 滚轮参数的前端接口。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEBUIBRIDGE_API UWebUIFoundationCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWebUIFoundationCameraComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction
	) override;

public:
	// 鼠标滚轮前后移动：滚轮向上沿当前视角前进，滚轮向下后退。
	// 不依赖 Project Settings 里的 Axis Mapping。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|Input")
	bool bEnableMouseWheelMove;

	// 每一格滚轮移动的 Unreal 单位距离。Cesium 场景一般需要稍大一点。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|Input", meta=(ClampMin="1.0"))
	float MouseWheelMoveStep;

	// true：只在水平面前后移动；false：沿视角方向移动，更接近伪缩放。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|Input")
	bool bMouseWheelMoveIgnorePitch;

	// true：启用 Slate 输入预处理器。WebBrowser / UMG 抢焦点时，普通 InputComponent 经常收不到滚轮，必须开这个。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|Input")
	bool bUseSlateMouseWheelPreProcessor;

	// true：打印滚轮输入日志。调试通过后可以关掉。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|Debug")
	bool bLogMouseWheelMove;

public:
	void HandleFlyTo(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	void HandleResetView(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId
	);

	void HandleSetInputEnabled(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	void HandleSetMouseWheelMoveConfig(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	void HandleGetMouseWheelMoveConfig(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId
	);

	// 获取当前实际渲染视角的位置，而不是仅返回 Pawn 原点。
	void HandleGetViewPosition(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId
	);

	void AppendSceneState(const TSharedPtr<FJsonObject>& Data) const;

	// 给 Slate 输入预处理器调用。只累计输入，实际移动统一放到 Tick 里执行。
	void QueueMouseWheelInput(float WheelDelta, const TCHAR* SourceName);

	/** Slate Wheel 预处理器调用；UI 模式 / UI Hit Region 返回 false。 */
	bool ShouldAcceptSceneMouseWheelAtScreenPosition(const FVector2D& ScreenPosition) const;

private:
	// resetView 必须分别记录初始 Pawn 与初始 ViewTarget。
	// 原实现只记录一组 Transform，并且在 BeginPlay 时只尝试一次，
	// PlayerController/Pawn 初始化较晚时会导致默认视角永久不可用。
	UPROPERTY()
	AActor* InitialViewTarget;

	FVector InitialViewTargetLocation;
	FRotator InitialViewTargetRotation;
	bool bHasInitialViewTargetTransform;

	FVector InitialPawnLocation;
	FRotator InitialPawnRotation;
	bool bHasInitialPawnTransform;

	FRotator InitialControlRotation;

	// flyTo / 滚轮会把 CharacterMovement 临时切到 Flying。
	// resetView 时恢复初始 MovementMode 与 GravityScale。
	bool bHasInitialCharacterMovementState;
	uint8 InitialCharacterMovementMode;
	uint8 InitialCharacterCustomMovementMode;
	float InitialCharacterGravityScale;

	bool bHasInitialViewState;

	// Smooth fly state
	bool bIsFlying;
	float FlightElapsed;
	float FlightDuration;
	FVector FlightStartLocation;
	FRotator FlightStartRotation;
	FVector FlightTargetLocation;
	FRotator FlightTargetRotation;

	// Mouse wheel input state
	bool bMouseWheelInputBoundToPlayerController;
	float PendingMouseWheelValue;

	TWeakObjectPtr<UInputComponent> BoundPlayerControllerInputComponent;
	TSharedPtr<FWebUIFoundationMouseWheelInputProcessor> SlateMouseWheelInputProcessor;

private:
	void CacheInitialViewState();

	APlayerController* GetMainPlayerController() const;
	APawn* GetControlledPawn() const;

	void EnsureMouseWheelInputReady();
	void EnsureMouseWheelInputBoundToPlayerController(APlayerController* PC);
	void RegisterSlateMouseWheelPreProcessor();
	void UnregisterSlateMouseWheelPreProcessor();
	void BindMouseWheelKeysOnInputComponent(UInputComponent* InputComponent, const TCHAR* SourceName);
	bool IsSlateMouseWheelSourceActive() const;

	void OnMouseWheelAxis(float AxisValue);
	bool ConsumeMouseWheelMove();

	FVector GetMouseWheelMoveForwardVector(
		const APlayerController* PC,
		const AActor* ViewTarget
	) const;

	void StopCurrentPawnMovement();

	void ApplyControlledViewTransform(
		const FVector& TargetLocation,
		const FRotator& TargetRotation
	);

	void StartSmoothFlyTo(
		const FVector& TargetLocation,
		const FRotator& TargetRotation,
		float Duration
	);

	void FinishSmoothFly();

	TSharedPtr<FJsonObject> BuildMouseWheelMoveConfigPayload() const;
};
