#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WebUIFoundationActorControlComponent.generated.h"

class AActor;
class FJsonObject;
class UWebUIFoundationBridgeComponent;

/**
 * 首次通过 moveActor 控制 Actor 时保存的恢复基准。
 *
 * 有父级：保存相对 Transform。这样楼层已经移动时，单独恢复床/门只会恢复它在楼层内部的位置。
 * 无父级：保存世界 Transform。适用于楼层根、独立 Actor 等。
 */
struct FWebUIFoundationTrackedActorTransform
{
	TWeakObjectPtr<AActor> OriginalAttachParent;
	FTransform OriginalWorldTransform = FTransform::Identity;
	FTransform OriginalRelativeTransform = FTransform::Identity;
	bool bWasAttached = false;
};

/**
 * WebUIFoundation 场景 Actor 控制组件。
 *
 * 核心规则：
 * - moveActor 可以控制任意带 RootComponent 的 AActor，不再局限于 AStaticMeshActor；
 * - 楼层、房间、样板间通过编辑器中的 Attach 父子层级自然继承移动；
 * - 移动父 Actor 时，代码只移动父 Actor，不会再遍历并重复移动子 Actor；
 * - 只有需要单独控制的床、门、设备才需要唯一 Actor Tag；
 * - 恢复附着对象时使用原始相对 Transform，避免父级已经移动后恢复到错误世界坐标。
 *
 * 稳定标识建议：
 * - 楼层根：B_F08、A_F13 等唯一 Actor Tag；
 * - 单独对象：B_F08_Bedroom_Bed1 等唯一 Actor Tag；
 * - 编辑器 Actor Label 只作为编辑器环境辅助匹配，打包后不要依赖。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WEBUIBRIDGE_API UWebUIFoundationActorControlComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWebUIFoundationActorControlComponent();

public:
	// 移动前自动把目标 Actor 的 SceneComponent 切换为 Movable。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|ActorControl")
	bool bAutoSetActorMovable;

	// 移动父 Actor 前，同时把所有附着子 Actor 的组件切换为 Movable。
	// 楼层下挂有大量 StaticMeshActor 时建议保持开启。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|ActorControl")
	bool bAutoSetAttachedActorsMovable;

	// 默认是否允许 actorName 做部分匹配。建议保持 false，避免误移动。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|ActorControl")
	bool bAllowPartialNameMatch;

	// 单次移动距离绝对值上限，单位为 Unreal Unit（默认 1 UU = 1 cm）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|ActorControl", meta=(ClampMin="0.0"))
	double MaxAbsoluteMoveDistance;

public:
	void HandleMoveActor(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	// payload.actorName 可选：为空时恢复全部；有值时只恢复指定 Actor。
	void HandleRestoreActors(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	);

	// 查询目标的父级、子级数量、Tag、相对/世界坐标，用于验证楼层层级。
	void HandleGetActorInfo(
		UWebUIFoundationBridgeComponent* Bridge,
		const FString& RequestId,
		const TSharedPtr<FJsonObject>& Payload
	) const;

	void AppendSceneState(const TSharedPtr<FJsonObject>& Data) const;

private:
	// 只记录实际通过 moveActor 控制过的对象；首次移动时缓存，后续不覆盖。
	TMap<TWeakObjectPtr<AActor>, FWebUIFoundationTrackedActorTransform> OriginalActorTransforms;

private:
	AActor* FindTargetActor(
		const FString& TargetName,
		bool bAllowPartialMatch,
		FString& OutMatchedBy,
		FString& OutError
	) const;

	bool BuildMoveDirection(
		const FString& DirectionText,
		const FString& SpaceText,
		const AActor* TargetActor,
		FVector& OutDirection,
		FString& OutNormalizedDirection,
		FString& OutNormalizedSpace,
		FString& OutError
	) const;

	bool TryGetMoveDistance(
		UWebUIFoundationBridgeComponent* Bridge,
		const TSharedPtr<FJsonObject>& Payload,
		double& OutDistance
	) const;

	bool EnsureActorHierarchyMovable(
		AActor* TargetActor,
		bool bIncludeAttachedActors,
		int32& OutChangedComponentCount,
		int32& OutVisitedActorCount,
		FString& OutError
	) const;

	bool RestoreTrackedActor(
		AActor* TargetActor,
		FVector& OutOldLocation,
		FVector& OutRestoredLocation,
		FString& OutError
	);

	static int32 GetAttachmentDepth(const AActor* Actor);
	static int32 GetAttachedDescendantCount(const AActor* Actor);
};
