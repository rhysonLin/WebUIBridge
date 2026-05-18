#include "Camera/WebUICameraSubsystem.h"

#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "CesiumGeoreference.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float WebUISmoothFlyDuration = 3.0f;
	constexpr float WebUISmoothFlyTickInterval = 1.0f / 60.0f;
}

void UWebUICameraSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TargetPlayerController = nullptr;
	CachedPlayerViewTarget = nullptr;
	Georeference = nullptr;
}

void UWebUICameraSubsystem::Deinitialize()
{
	CancelSmoothFlight();

	TargetPlayerController = nullptr;
	CachedPlayerViewTarget = nullptr;
	Georeference = nullptr;

	Super::Deinitialize();
}

void UWebUICameraSubsystem::SetTargetPlayerController(APlayerController* InPlayerController)
{
	TargetPlayerController = InPlayerController;

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] Target PlayerController set: %s"),
		*GetNameSafe(TargetPlayerController.Get()));
}

void UWebUICameraSubsystem::SetGeoreference(ACesiumGeoreference* InGeoreference)
{
	Georeference = InGeoreference;

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] Georeference set: %s"),
		*GetNameSafe(Georeference.Get()));
}

bool UWebUICameraSubsystem::SwitchToCameraActor(AActor* CameraActor, float BlendTime)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC || !CameraActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] SwitchToCameraActor failed."));
		return false;
	}

	CachePlayerViewTargetIfNeeded();

	PC->SetViewTargetWithBlend(CameraActor, BlendTime);

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] SwitchToCameraActor: %s"),
		*GetNameSafe(CameraActor));

	return true;
}

bool UWebUICameraSubsystem::SwitchToCameraByTag(FName CameraTag, float BlendTime)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC || !PC->GetWorld())
	{
		return false;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(PC->GetWorld(), CameraTag, FoundActors);

	if (FoundActors.Num() <= 0 || !FoundActors[0])
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] No camera found with tag: %s"), *CameraTag.ToString());
		return false;
	}

	return SwitchToCameraActor(FoundActors[0], BlendTime);
}

bool UWebUICameraSubsystem::ReturnToPlayerView(float BlendTime)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		return false;
	}

	AActor* Target = CachedPlayerViewTarget.Get();
	if (!Target)
	{
		Target = PC->GetPawn();
	}

	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] ReturnToPlayerView failed: no target."));
		return false;
	}

	PC->SetViewTargetWithBlend(Target, BlendTime);

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] ReturnToPlayerView: %s"), *GetNameSafe(Target));
	return true;
}

bool UWebUICameraSubsystem::MoveControlledPawnToWorldLocation(
	const FVector& TargetLocation,
	const FRotator& TargetRotation
)
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] MoveControlledPawnToWorldLocation failed: PC is null."));
		return false;
	}

	APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] MoveControlledPawnToWorldLocation failed: Pawn is null."));
		return false;
	}

	// 如果当前正在看 CameraActor，先切回 Pawn，否则你移动 Pawn 也看不到。
	if (PC->GetViewTarget() != Pawn)
	{
		PC->SetViewTargetWithBlend(Pawn, 0.0f);
	}

	return StartSmoothPawnMove(Pawn, TargetLocation, TargetRotation);
}


bool UWebUICameraSubsystem::MoveControlledPawnToGeoLocation(
	double Longitude,
	double Latitude,
	double Height,
	const FRotator& TargetRotation
)
{
	ACesiumGeoreference* Geo = ResolveGeoreference();
	if (!Geo)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] MoveControlledPawnToGeoLocation failed: no CesiumGeoreference."));
		return false;
	}

	const FVector LongitudeLatitudeHeight(
		static_cast<float>(Longitude),
		static_cast<float>(Latitude),
		static_cast<float>(Height)
	);

	const FVector UnrealLocation = Geo->TransformLongitudeLatitudeHeightPositionToUnreal(LongitudeLatitudeHeight);

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] GeoToWorld: Lon=%.8f Lat=%.8f Height=%.2f -> %s"),
		Longitude,
		Latitude,
		Height,
		*UnrealLocation.ToString());

	return MoveControlledPawnToWorldLocation(UnrealLocation, TargetRotation);
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

bool UWebUICameraSubsystem::StartSmoothPawnMove(
	APawn* Pawn,
	const FVector& TargetLocation,
	const FRotator& TargetRotation
)
{
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] StartSmoothPawnMove failed: Pawn is null."));
		return false;
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUICameraSubsystem] StartSmoothPawnMove failed: World is null."));
		return false;
	}

	CancelSmoothFlight();

	SmoothFlyPawn = Pawn;
	SmoothFlyStartLocation = Pawn->GetActorLocation();
	SmoothFlyTargetLocation = TargetLocation;

	APlayerController* PC = TargetPlayerController.Get();
	if (PC)
	{
		SmoothFlyStartRotation = PC->GetControlRotation();
	}
	else
	{
		SmoothFlyStartRotation = Pawn->GetActorRotation();
	}

	SmoothFlyTargetRotation = TargetRotation;
	SmoothFlyStartWorldTime = World->GetTimeSeconds();
	SmoothFlyDuration = WebUISmoothFlyDuration;

	World->GetTimerManager().SetTimer(
		SmoothFlyTimerHandle,
		this,
		&UWebUICameraSubsystem::TickSmoothPawnMove,
		WebUISmoothFlyTickInterval,
		true
	);

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] Start Smooth Move Pawn: From=%s To=%s FromRot=%s ToRot=%s Duration=%.2f Pawn=%s"),
		*SmoothFlyStartLocation.ToString(),
		*SmoothFlyTargetLocation.ToString(),
		*SmoothFlyStartRotation.ToString(),
		*SmoothFlyTargetRotation.ToString(),
		SmoothFlyDuration,
		*GetNameSafe(Pawn));

	return true;
}

void UWebUICameraSubsystem::TickSmoothPawnMove()
{
	APawn* Pawn = SmoothFlyPawn.Get();
	if (!Pawn)
	{
		FinishSmoothPawnMove(false);
		return;
	}

	UWorld* World = Pawn->GetWorld();
	if (!World)
	{
		FinishSmoothPawnMove(false);
		return;
	}

	const float Alpha = GetSmoothFlightAlpha();

	const FVector NewLocation = FMath::Lerp(
		SmoothFlyStartLocation,
		SmoothFlyTargetLocation,
		Alpha
	);

	const FRotator NewRotation = FMath::Lerp(
		SmoothFlyStartRotation,
		SmoothFlyTargetRotation,
		Alpha
	).GetNormalized();

	Pawn->SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);

	APlayerController* PC = TargetPlayerController.Get();
	if (PC)
	{
		PC->SetControlRotation(NewRotation);
	}

	// Pawn 自身只同步 Yaw，避免 Pitch/Roll 把 Pawn 转歪。
	Pawn->SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));

	if (Alpha >= 1.0f)
	{
		FinishSmoothPawnMove(true);
	}
}

void UWebUICameraSubsystem::FinishSmoothPawnMove(bool bApplyTarget)
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SmoothFlyTimerHandle);
	}

	APawn* Pawn = SmoothFlyPawn.Get();

	if (bApplyTarget && Pawn)
	{
		Pawn->SetActorLocation(SmoothFlyTargetLocation, false, nullptr, ETeleportType::None);

		APlayerController* PC = TargetPlayerController.Get();
		if (PC)
		{
			PC->SetControlRotation(SmoothFlyTargetRotation);
		}

		Pawn->SetActorRotation(FRotator(0.f, SmoothFlyTargetRotation.Yaw, 0.f));
	}

	UE_LOG(LogTemp, Log, TEXT("[WebUICameraSubsystem] Finish Smooth Move Pawn: ApplyTarget=%s Pawn=%s"),
		bApplyTarget ? TEXT("true") : TEXT("false"),
		*GetNameSafe(Pawn));

	SmoothFlyPawn = nullptr;
	SmoothFlyStartWorldTime = 0.0f;
}

void UWebUICameraSubsystem::CancelSmoothFlight()
{
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(SmoothFlyTimerHandle);
	}

	SmoothFlyPawn = nullptr;
	SmoothFlyStartWorldTime = 0.0f;
}

float UWebUICameraSubsystem::GetSmoothFlightAlpha() const
{
	if (SmoothFlyDuration <= 0.0f)
	{
		return 1.0f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 1.0f;
	}

	const float ElapsedTime = World->GetTimeSeconds() - SmoothFlyStartWorldTime;
	const float RawAlpha = FMath::Clamp(ElapsedTime / SmoothFlyDuration, 0.0f, 1.0f);

	// SmoothStep：起步和结束更柔和。
	return RawAlpha * RawAlpha * (3.0f - 2.0f * RawAlpha);
}

void UWebUICameraSubsystem::CachePlayerViewTargetIfNeeded()
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		return;
	}

	AActor* Current = PC->GetViewTarget();
	if (Current)
	{
		CachedPlayerViewTarget = Current;
	}
}

ACesiumGeoreference* UWebUICameraSubsystem::ResolveGeoreference() const
{
	if (Georeference)
	{
		return Georeference.Get();
	}

	APlayerController* PC = TargetPlayerController.Get();
	if (!PC || !PC->GetWorld())
	{
		return nullptr;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(PC->GetWorld(), ACesiumGeoreference::StaticClass(), FoundActors);

	if (FoundActors.Num() > 0)
	{
		return Cast<ACesiumGeoreference>(FoundActors[0]);
	}

	return nullptr;
}