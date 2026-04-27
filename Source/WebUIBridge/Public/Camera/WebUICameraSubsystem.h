#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WebUICameraSubsystem.generated.h"

class APlayerController;
class ACameraActor;
class ACineCameraActor;

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

	UFUNCTION(BlueprintPure, Category = "Web UI|Camera")
	APlayerController* GetTargetPlayerController() const { return TargetPlayerController.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool SwitchToCameraActor(AActor* CameraActor, float BlendTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool SwitchToCameraByTag(FName CameraTag, float BlendTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool ReturnToPlayerView(float BlendTime = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Camera")
	bool FlyToWorldLocation(
		const FVector& TargetLocation,
		const FRotator& TargetRotation,
		float FOV = 90.0f,
		float BlendTime = 0.0f
	);

	UFUNCTION(BlueprintPure, Category = "Web UI|Camera")
	FWebUICameraViewInfo GetCurrentViewInfo() const;

protected:
	bool EnsureFlightCamera();
	void CachePlayerViewTargetIfNeeded();

protected:
	UPROPERTY(Transient)
	TObjectPtr<APlayerController> TargetPlayerController = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedPlayerViewTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> RuntimeFlightCamera = nullptr;
};