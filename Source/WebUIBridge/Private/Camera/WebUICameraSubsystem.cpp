#include "Camera/WebUICameraSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "CineCameraActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UWebUICameraSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TargetPlayerController = nullptr;
	CachedPlayerViewTarget = nullptr;
	RuntimeFlightCamera = nullptr;
}

void UWebUICameraSubsystem::Deinitialize()
{
	TargetPlayerController = nullptr;
	CachedPlayerViewTarget = nullptr;
	RuntimeFlightCamera = nullptr;

	Super::Deinitialize();
}

void UWebUICameraSubsystem::SetTargetPlayerController(APlayerController* InPlayerController)
{
	TargetPlayerController = InPlayerController;

	if (TargetPlayerController)
	{
		UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] Target PlayerController set: %s"), *GetNameSafe(TargetPlayerController));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] Target PlayerController cleared."));
	}
}

bool UWebUICameraSubsystem::SwitchToCameraActor(AActor* CameraActor, float BlendTime)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] SwitchToCameraActor failed: TargetPlayerController is null."));
		return false;
	}

	if (!CameraActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] SwitchToCameraActor failed: CameraActor is null."));
		return false;
	}

	CachePlayerViewTargetIfNeeded();

	PC->SetViewTargetWithBlend(CameraActor, BlendTime);

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] Switched to camera actor: %s"), *GetNameSafe(CameraActor));
	return true;
}

bool UWebUICameraSubsystem::SwitchToCameraByTag(FName CameraTag, float BlendTime)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] SwitchToCameraByTag failed: TargetPlayerController is null."));
		return false;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(PC->GetWorld(), CameraTag, FoundActors);

	if (FoundActors.Num() <= 0 || !FoundActors[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] No actor found with tag: %s"), *CameraTag.ToString());
		return false;
	}

	return SwitchToCameraActor(FoundActors[0], BlendTime);
}

bool UWebUICameraSubsystem::ReturnToPlayerView(float BlendTime)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] ReturnToPlayerView failed: TargetPlayerController is null."));
		return false;
	}

	AActor* TargetView = CachedPlayerViewTarget.Get();
	if (!TargetView)
	{
		TargetView = PC->GetPawn();
	}

	if (!TargetView)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] ReturnToPlayerView failed: no cached player view target and no pawn."));
		return false;
	}

	PC->SetViewTargetWithBlend(TargetView, BlendTime);

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] Returned to player view: %s"), *GetNameSafe(TargetView));
	return true;
}

bool UWebUICameraSubsystem::FlyToWorldLocation(
	const FVector& TargetLocation,
	const FRotator& TargetRotation,
	float FOV,
	float BlendTime
)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] FlyToWorldLocation failed: TargetPlayerController is null."));
		return false;
	}

	if (!EnsureFlightCamera())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] FlyToWorldLocation failed: RuntimeFlightCamera not available."));
		return false;
	}

	CachePlayerViewTargetIfNeeded();

	RuntimeFlightCamera->SetActorLocation(TargetLocation);
	RuntimeFlightCamera->SetActorRotation(TargetRotation);

	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetFOV(FOV);
	}

	PC->SetViewTargetWithBlend(RuntimeFlightCamera, BlendTime);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[WebUICameraSubsystem] FlyToWorldLocation -> Location=%s Rotation=%s FOV=%.2f"),
		*TargetLocation.ToString(),
		*TargetRotation.ToString(),
		FOV
	);

	return true;
}

FWebUICameraViewInfo UWebUICameraSubsystem::GetCurrentViewInfo() const
{
	FWebUICameraViewInfo Result;

	APlayerController* PC = TargetPlayerController.Get();
	if (!PC || !PC->PlayerCameraManager)
	{
		return Result;
	}

	Result.Location = PC->PlayerCameraManager->GetCameraLocation();
	Result.Rotation = PC->PlayerCameraManager->GetCameraRotation();
	Result.FOV = PC->PlayerCameraManager->GetFOVAngle();

	return Result;
}

bool UWebUICameraSubsystem::EnsureFlightCamera()
{
	if (RuntimeFlightCamera)
	{
		return true;
	}

	APlayerController* PC = TargetPlayerController.Get();
	if (!PC || !PC->GetWorld())
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = TEXT("WebUIRuntimeFlightCamera");
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	RuntimeFlightCamera = PC->GetWorld()->SpawnActor<ACameraActor>(
		ACameraActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (RuntimeFlightCamera)
	{
		RuntimeFlightCamera->SetActorHiddenInGame(true);
	}

	return RuntimeFlightCamera != nullptr;
}

void UWebUICameraSubsystem::CachePlayerViewTargetIfNeeded()
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		return;
	}

	AActor* CurrentViewTarget = PC->GetViewTarget();
	if (!CurrentViewTarget)
	{
		return;
	}

	if (CurrentViewTarget != RuntimeFlightCamera)
	{
		CachedPlayerViewTarget = CurrentViewTarget;
	}
}