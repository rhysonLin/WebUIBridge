#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WebUICameraSubsystem.generated.h"

class APlayerController;
class ACameraActor;
class ACesiumGeoreference;

USTRUCT(BlueprintType)
struct FWebUICameraViewInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Web UI|Camera")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Web UI|Camera")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Web UI|Camera")
	float FOV = 90.0f;
};

UCLASS()
class WEBUIBRIDGE_API UWebUICameraSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	void SetTargetPlayerController(APlayerController* InPlayerController);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	void SetGeoreference(ACesiumGeoreference* InGeoreference);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool SwitchToCameraActor(AActor* CameraActor, float BlendTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool SwitchToCameraByTag(FName CameraTag, float BlendTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool ReturnToPlayerView(float BlendTime = 0.0f);

	/** 移动当前受控 Pawn 到 UE 世界坐标，移动后仍可 WASD/EQ */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool MoveControlledPawnToWorldLocation(
		const FVector& TargetLocation,
		const FRotator& TargetRotation
	);

	/** 经纬度高度飞行：Longitude/Latitude/Height 单位为度/米 */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool MoveControlledPawnToGeoLocation(
		double Longitude,
		double Latitude,
		double Height,
		const FRotator& TargetRotation
	);

	UFUNCTION(BlueprintPure, Category = "Web UI|Camera")
	FWebUICameraViewInfo GetCurrentViewInfo() const;

protected:
	void CachePlayerViewTargetIfNeeded();
	ACesiumGeoreference* ResolveGeoreference() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<APlayerController> TargetPlayerController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedPlayerViewTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACesiumGeoreference> Georeference = nullptr;
};