#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WebUIFoundationSceneGroupActor.generated.h"

class USceneComponent;

/**
 * 可选的空分组 Actor。
 *
 * 用途：
 * - 当现有楼层没有稳定根 Actor 时，创建一个空父级；
 * - 作为楼层、房间、样板间的逻辑根；
 * - 普通 Mesh、床、门通过 World Outliner Attach 到它下面；
 * - 运行时只移动这个根，所有后代自然跟随。
 *
 * 现有楼层 Actor 已经能作为父级时，不强制使用本类。
 */
UCLASS(BlueprintType, Blueprintable)
class WEBUIBRIDGE_API AWebUIFoundationSceneGroupActor : public AActor
{
	GENERATED_BODY()

public:
	AWebUIFoundationSceneGroupActor();

	virtual void OnConstruction(const FTransform& Transform) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="WebUIFoundation|SceneGroup")
	USceneComponent* GroupRoot;

	// 前端控制使用的唯一标识，例如 B_F08、A_F13、A_F13_SampleRoom01。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|SceneGroup")
	FName GroupId;

	// 自动把 GroupId 同步为本 Actor 的唯一 Tag，供 moveActor 精确查找。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|SceneGroup")
	bool bSynchronizeGroupIdToActorTag;

	// 一次性迁移旧场景时使用：把带有该 Tag 的 Actor 批量 Attach 到本分组。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|SceneGroup|Editor Tools")
	FName ImportActorTag;

	// 批量挂接时自动将导入 Actor 及其后代组件设为 Movable。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|SceneGroup|Editor Tools")
	bool bSetImportedHierarchyMovable;

	// 导入完成后移除成员上的 ImportActorTag，避免它们与分组根使用相同 Tag 时产生重复匹配。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WebUIFoundation|SceneGroup|Editor Tools")
	bool bRemoveImportTagAfterAttach;

	// 最近一次由本 Actor 自动同步到 Tags 的 GroupId，用于改名时移除旧 Tag。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="WebUIFoundation|SceneGroup", AdvancedDisplay)
	FName SynchronizedGroupTag;

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category="WebUIFoundation|SceneGroup|Editor Tools")
	void SynchronizeGroupTag();

	// 查找关卡中所有带 ImportActorTag 的 Actor，保持世界位置并挂到本分组。
	// 会保留候选对象之间原有父子结构，只挂接候选层级的最上层对象。
	UFUNCTION(BlueprintCallable, CallInEditor, Category="WebUIFoundation|SceneGroup|Editor Tools")
	void AttachActorsByImportTag();

	UFUNCTION(BlueprintCallable, CallInEditor, Category="WebUIFoundation|SceneGroup|Editor Tools")
	void SetAttachedHierarchyMovable();

	// 只解除直接子 Actor，保持世界 Transform；其各自内部子层级不变。
	UFUNCTION(BlueprintCallable, CallInEditor, Category="WebUIFoundation|SceneGroup|Editor Tools")
	void DetachDirectChildrenKeepWorld();

	UFUNCTION(BlueprintPure, Category="WebUIFoundation|SceneGroup")
	int32 GetAttachedDescendantCount() const;

private:
	static void SetActorAndDescendantsMovable(AActor* Actor, int32& OutActorCount, int32& OutComponentCount);
	static bool WouldCreateAttachmentCycle(const AActor* Child, const AActor* NewParent);
};
