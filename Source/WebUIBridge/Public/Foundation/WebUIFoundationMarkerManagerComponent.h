#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WebUIFoundationMarkerManagerComponent.generated.h"

class AActor;
class AWebUIFoundationMarkerActor;
class FJsonObject;
class UWebUIFoundationBridgeComponent;

/**
 * WebUIFoundation Marker 业务组件。
 *
 * 负责：
 * - addMarker；
 * - updateMarker；
 * - removeMarker；
 * - clearMarkers；
 * - Marker 数量写入 sceneState；
 * - 可选将 Marker 附着到楼层/房间 Actor，使标签跟随分层移动和一键还原；
 * - 支持 world 与 screen 两种标签空间，其中 screen 在视口中保持固定像素大小。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEBUIBRIDGE_API UWebUIFoundationMarkerManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWebUIFoundationMarkerManagerComponent();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	void HandleAddMarker(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	void HandleUpdateMarker(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	void HandleRemoveMarker(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	void HandleClearMarkers(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId
	);

	void ClearAllMarkers();

	void AppendSceneState(const TSharedPtr<FJsonObject>& Data) const;

private:
	UPROPERTY()
	TMap<FString, AWebUIFoundationMarkerActor*> Markers;

private:
	AActor* FindFollowActor(
		const FString& TargetName,
		bool bAllowPartialMatch,
		FString& OutMatchedBy,
		FString& OutError
	) const;

	bool ResolveMarkerWorldLocation(
		UWebUIFoundationBridgeComponent* Bridge,
		const TSharedPtr<FJsonObject>& Payload,
		const AActor* FollowActor,
		FVector& OutWorldLocation,
		FString& OutCoordinateSource,
		FString& OutError
	) const;

	void ApplyMarkerAttachment(
		AWebUIFoundationMarkerActor* Marker,
		AActor* FollowActor,
		bool bDetach
	) const;

	void AppendAttachmentData(
		const AWebUIFoundationMarkerActor* Marker,
		const FString& RequestedFollowActorName,
		const FString& MatchedBy,
		const TSharedPtr<FJsonObject>& Data
	) const;
};
