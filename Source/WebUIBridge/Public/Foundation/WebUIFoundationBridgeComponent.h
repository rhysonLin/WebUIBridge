#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WebUIFoundationBridgeComponent.generated.h"

class ACesiumGeoreference;
class APlayerController;
class APawn;
class UWebUIFoundationCameraComponent;
class UWebUIFoundationMarkerManagerComponent;
class UWebUIFoundationActorControlComponent;
class FJsonObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebUIFoundationResponseReady, const FString&, ResponseJson);

/**
 * WebUIFoundation 的主桥接组件。
 *
 * 职责只保留三类：
 * 1. 接收前端 JSON 消息并做基础校验；
 * 2. 按 type 把命令分发给具体业务组件；
 * 3. 统一封装 UE -> 前端的 JSON 响应。
 *
 * 具体业务不要继续堆在这里。
 * 比如相机、滚轮、飞行：UWebUIFoundationCameraComponent
 * 比如 Marker：UWebUIFoundationMarkerManagerComponent
 * 楼层/房间/样板间通过场景 Attach 层级继承移动，具体移动由 ActorControl 处理。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEBUIBRIDGE_API UWebUIFoundationBridgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWebUIFoundationBridgeComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintAssignable, Category="WebUIFoundation")
	FWebUIFoundationResponseReady OnResponseReady;

	UFUNCTION(BlueprintCallable, Category="WebUIFoundation")
	void HandleWebUIFoundationMessage(const FString& DescriptorJson);

	UFUNCTION(BlueprintCallable, Category="WebUIFoundation")
	void ClearAllMarkers();

public:
	// ===== Common helpers for feature components =====

	bool GetPayloadNumber(
		const TSharedPtr<FJsonObject>& Payload,
		const FString& Key,
		double& OutValue
	) const;

	bool GetPayloadBool(
		const TSharedPtr<FJsonObject>& Payload,
		const FString& Key,
		bool& OutValue
	) const;

	FString GetPayloadString(
		const TSharedPtr<FJsonObject>& Payload,
		const FString& Key,
		const FString& DefaultValue
	) const;

	FVector ConvertLonLatHeightToWorld(double Lon, double Lat, double Height) const;

	// 将 Unreal 世界坐标转换为 WGS84 经度/纬度/椭球高。
	// 只有场景中存在 CesiumGeoreference 时才返回 true。
	bool ConvertWorldToLonLatHeight(
		const FVector& WorldPosition,
		FVector& OutLongitudeLatitudeHeight
	) const;

	bool HasCesiumGeoreference() const
	{
		return CachedGeoreference != nullptr;
	}

	APlayerController* GetMainPlayerController() const;
	APawn* GetControlledPawn() const;

	void SendSuccess(
		const FString& RequestId,
		const FString& Type,
		const TSharedPtr<FJsonObject>& Payload = nullptr
	);

	void SendError(
		const FString& RequestId,
		const FString& Type,
		const FString& Message
	);

private:
	UPROPERTY()
	ACesiumGeoreference* CachedGeoreference;

	UPROPERTY()
	UWebUIFoundationCameraComponent* CameraComponent;

	UPROPERTY()
	UWebUIFoundationMarkerManagerComponent* MarkerManagerComponent;

	UPROPERTY()
	UWebUIFoundationActorControlComponent* ActorControlComponent;

private:
	void CacheFeatureComponents();

	void HandleRequestSceneState(const FString& RequestId);

	FString ToJsonString(const TSharedPtr<FJsonObject>& Object) const;
};
