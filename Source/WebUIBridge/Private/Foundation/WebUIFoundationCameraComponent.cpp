#include "Foundation/WebUIFoundationCameraComponent.h"

#include "Foundation/WebUIFoundationBridgeComponent.h"
#include "Settings/WebUIBridgeSettings.h"
#include "Runtime/WebUIRuntimeSubsystem.h"
#include "Host/WebUIInputModeSubsystem.h"

#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

#include "Json.h"

class FWebUIFoundationMouseWheelInputProcessor : public IInputProcessor
{
public:
	explicit FWebUIFoundationMouseWheelInputProcessor(UWebUIFoundationCameraComponent* InOwner)
		: Owner(InOwner)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleMouseWheelOrGestureEvent(
		FSlateApplication& SlateApp,
		const FPointerEvent& InWheelEvent,
		const FPointerEvent* InGestureEvent
	) override
	{
		UWebUIFoundationCameraComponent* OwnerPtr = Owner.Get();
		if (!OwnerPtr)
		{
			return false;
		}

		// UI 区域 / UI 模式下绝不能把 Wheel 排进三维移动队列。
		// 事件本身仍返回 false，让 WebBrowser 的网页滚动正常工作。
		if (!OwnerPtr->ShouldAcceptSceneMouseWheelAtScreenPosition(InWheelEvent.GetScreenSpacePosition()))
		{
			return false;
		}

		const float WheelDelta = InWheelEvent.GetWheelDelta();
		if (!FMath::IsNearlyZero(WheelDelta))
		{
			OwnerPtr->QueueMouseWheelInput(WheelDelta, TEXT("SlatePreProcessor"));
		}

		return false;
	}

private:
	TWeakObjectPtr<UWebUIFoundationCameraComponent> Owner;
};

UWebUIFoundationCameraComponent::UWebUIFoundationCameraComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	bEnableMouseWheelMove = true;
	MouseWheelMoveStep = 1000.0f;
	bMouseWheelMoveIgnorePitch = false;
	bUseSlateMouseWheelPreProcessor = true;
	bLogMouseWheelMove = true;

	InitialViewTarget = nullptr;
	InitialViewTargetLocation = FVector::ZeroVector;
	InitialViewTargetRotation = FRotator::ZeroRotator;
	bHasInitialViewTargetTransform = false;

	InitialPawnLocation = FVector::ZeroVector;
	InitialPawnRotation = FRotator::ZeroRotator;
	bHasInitialPawnTransform = false;

	InitialControlRotation = FRotator::ZeroRotator;

	bHasInitialCharacterMovementState = false;
	InitialCharacterMovementMode = 0;
	InitialCharacterCustomMovementMode = 0;
	InitialCharacterGravityScale = 1.0f;

	bHasInitialViewState = false;

	bIsFlying = false;
	FlightElapsed = 0.0f;
	FlightDuration = 0.0f;
	FlightStartLocation = FVector::ZeroVector;
	FlightStartRotation = FRotator::ZeroRotator;
	FlightTargetLocation = FVector::ZeroVector;
	FlightTargetRotation = FRotator::ZeroRotator;

	bMouseWheelInputBoundToPlayerController = false;
	PendingMouseWheelValue = 0.0f;
}

void UWebUIFoundationCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	SetComponentTickEnabled(true);
	CacheInitialViewState();
	EnsureMouseWheelInputReady();
}

void UWebUIFoundationCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterSlateMouseWheelPreProcessor();

	Super::EndPlay(EndPlayReason);
}

void UWebUIFoundationCameraComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction
)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// PlayerController、Pawn、CameraActor 的 BeginPlay 顺序并不稳定。
	// 如果第一次 BeginPlay 缓存失败，就在后续 Tick 中继续尝试，直到拿到有效默认视角。
	if (!bHasInitialViewState)
	{
		CacheInitialViewState();
	}

	EnsureMouseWheelInputReady();

	// 鼠标滚轮优先：用户手动滚轮时，中断当前 smooth fly，避免两个位移逻辑互相打架。
	if (ConsumeMouseWheelMove())
	{
		bIsFlying = false;
		FlightElapsed = 0.0f;
		FlightDuration = 0.0f;
		return;
	}

	if (!bIsFlying)
	{
		return;
	}

	if (FlightDuration <= 0.0f)
	{
		FinishSmoothFly();
		return;
	}

	FlightElapsed += DeltaTime;

	const float AlphaRaw = FMath::Clamp(FlightElapsed / FlightDuration, 0.0f, 1.0f);

	// EaseInOut，让飞行不是线性硬滑。
	const float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, AlphaRaw, 2.0f);

	const FVector NewLocation = FMath::Lerp(
		FlightStartLocation,
		FlightTargetLocation,
		Alpha
	);

	const FRotator NewRotation = FMath::Lerp(
		FlightStartRotation,
		FlightTargetRotation,
		Alpha
	).GetNormalized();

	ApplyControlledViewTransform(NewLocation, NewRotation);

	if (AlphaRaw >= 1.0f)
	{
		FinishSmoothFly();
	}
}

void UWebUIFoundationCameraComponent::CacheInitialViewState()
{
	if (bHasInitialViewState)
	{
		return;
	}

	APlayerController* PC = GetMainPlayerController();
	if (!PC)
	{
		return;
	}

	InitialControlRotation = PC->GetControlRotation();

	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		InitialPawnLocation = Pawn->GetActorLocation();
		InitialPawnRotation = Pawn->GetActorRotation();
		bHasInitialPawnTransform = true;

		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (UCharacterMovementComponent* CharacterMove = Character->GetCharacterMovement())
			{
				bHasInitialCharacterMovementState = true;
				InitialCharacterMovementMode = static_cast<uint8>(CharacterMove->MovementMode);
				InitialCharacterCustomMovementMode = CharacterMove->CustomMovementMode;
				InitialCharacterGravityScale = CharacterMove->GravityScale;
			}
		}
	}

	InitialViewTarget = nullptr;

	const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
	if (
		Settings &&
		Settings->bPreferTaggedDefaultView &&
		!Settings->DefaultViewActorTag.IsNone()
	)
	{
		TArray<AActor*> TaggedActors;
		UGameplayStatics::GetAllActorsWithTag(
			GetWorld(),
			Settings->DefaultViewActorTag,
			TaggedActors
		);

		if (TaggedActors.Num() > 0)
		{
			InitialViewTarget = TaggedActors[0];
			InitialControlRotation = InitialViewTarget->GetActorRotation();
		}
	}

	if (!IsValid(InitialViewTarget))
	{
		InitialViewTarget = PC->GetViewTarget();
	}

	if (IsValid(InitialViewTarget))
	{
		InitialViewTargetLocation = InitialViewTarget->GetActorLocation();
		InitialViewTargetRotation = InitialViewTarget->GetActorRotation();
		bHasInitialViewTargetTransform = true;
	}

	bHasInitialViewState = bHasInitialPawnTransform || bHasInitialViewTargetTransform;

	if (bHasInitialViewState)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[WebUIFoundation] Initial view cached. Pawn=%s ViewTarget=%s"),
			Pawn ? *Pawn->GetName() : TEXT("None"),
			IsValid(InitialViewTarget) ? *InitialViewTarget->GetName() : TEXT("None")
		);
	}
}

APlayerController* UWebUIFoundationCameraComponent::GetMainPlayerController() const
{
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

APawn* UWebUIFoundationCameraComponent::GetControlledPawn() const
{
	APlayerController* PC = GetMainPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}

void UWebUIFoundationCameraComponent::EnsureMouseWheelInputReady()
{
	APlayerController* PC = GetMainPlayerController();
	if (!PC)
	{
		return;
	}

	// 只保留一套实际输入来源：
	// 1. Slate 可用且启用时，以 Slate 为主；
	// 2. InputComponent 仅作为回退，回调会在 Slate 活跃时自动忽略。
	EnsureMouseWheelInputBoundToPlayerController(PC);

	if (bUseSlateMouseWheelPreProcessor)
	{
		RegisterSlateMouseWheelPreProcessor();
	}
	else
	{
		UnregisterSlateMouseWheelPreProcessor();
	}
}

void UWebUIFoundationCameraComponent::EnsureMouseWheelInputBoundToPlayerController(APlayerController* PC)
{
	if (!PC || !PC->InputComponent)
	{
		return;
	}

	if (bMouseWheelInputBoundToPlayerController && BoundPlayerControllerInputComponent.Get() == PC->InputComponent)
	{
		return;
	}

	BindMouseWheelKeysOnInputComponent(PC->InputComponent, TEXT("PlayerControllerInputComponent"));
	BoundPlayerControllerInputComponent = PC->InputComponent;
	bMouseWheelInputBoundToPlayerController = true;
}

void UWebUIFoundationCameraComponent::RegisterSlateMouseWheelPreProcessor()
{
	if (!bUseSlateMouseWheelPreProcessor)
	{
		UnregisterSlateMouseWheelPreProcessor();
		return;
	}

	if (SlateMouseWheelInputProcessor.IsValid())
	{
		return;
	}

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	SlateMouseWheelInputProcessor = MakeShared<FWebUIFoundationMouseWheelInputProcessor>(this);
	FSlateApplication::Get().RegisterInputPreProcessor(SlateMouseWheelInputProcessor, 0);

	UE_LOG(LogTemp, Warning, TEXT("[WebUIFoundation] Mouse wheel Slate preprocessor registered."));
}

void UWebUIFoundationCameraComponent::UnregisterSlateMouseWheelPreProcessor()
{
	if (!SlateMouseWheelInputProcessor.IsValid())
	{
		return;
	}

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(SlateMouseWheelInputProcessor);
	}

	SlateMouseWheelInputProcessor.Reset();
}

void UWebUIFoundationCameraComponent::BindMouseWheelKeysOnInputComponent(UInputComponent* InputComponent, const TCHAR* SourceName)
{
	if (!InputComponent)
	{
		return;
	}

	// 回退输入只绑定 MouseWheelAxis。
	// 不再同时绑定 MouseScrollUp / MouseScrollDown，避免一次物理滚轮产生 3 次回调。
	FInputAxisKeyBinding& WheelAxisBinding = InputComponent->BindAxisKey(EKeys::MouseWheelAxis);
	WheelAxisBinding.AxisDelegate.GetDelegateForManualSet().BindUObject(
		this,
		&UWebUIFoundationCameraComponent::OnMouseWheelAxis
	);
	WheelAxisBinding.bConsumeInput = false;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation] Mouse wheel fallback axis bound on %s."),
		SourceName ? SourceName : TEXT("Unknown")
	);
}

bool UWebUIFoundationCameraComponent::IsSlateMouseWheelSourceActive() const
{
	return bUseSlateMouseWheelPreProcessor && SlateMouseWheelInputProcessor.IsValid();
}

bool UWebUIFoundationCameraComponent::ShouldAcceptSceneMouseWheelAtScreenPosition(
    const FVector2D& ScreenPosition
) const
{
    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UWebUIRuntimeSubsystem* Runtime = GI
        ? GI->GetSubsystem<UWebUIRuntimeSubsystem>()
        : nullptr;

    if (!Runtime)
    {
        return true;
    }

    if (Runtime->GetWebUIInputMode() == EWebUIInputMode::UI)
    {
        return false;
    }

    // 即使 UI/Scene 切换响应还在同一帧处理中，只要鼠标已经位于网页面板矩形，
    // 也不能让该格滚轮进入三维移动队列。
    return !Runtime->IsScreenPositionInsideWebUIRegion(ScreenPosition);
}

void UWebUIFoundationCameraComponent::QueueMouseWheelInput(float WheelDelta, const TCHAR* SourceName)
{
	if (FMath::IsNearlyZero(WheelDelta))
	{
		return;
	}

	// 同一物理滚轮事件可能被底层重复派发。
	// 将每帧累计值限制到 [-1, 1]，保证一格滚轮最多移动一个配置步长。
	PendingMouseWheelValue = FMath::Clamp(
		PendingMouseWheelValue + WheelDelta,
		-1.0f,
		1.0f
	);

	if (bLogMouseWheelMove)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[WebUIFoundation] MouseWheel queued. Source=%s Delta=%.3f Pending=%.3f"),
			SourceName ? SourceName : TEXT("Unknown"),
			WheelDelta,
			PendingMouseWheelValue
		);
	}
}

void UWebUIFoundationCameraComponent::OnMouseWheelAxis(float AxisValue)
{
	// Slate 是主输入源时，InputComponent 会收到同一事件的副本，必须忽略。
	if (IsSlateMouseWheelSourceActive())
	{
		return;
	}

	QueueMouseWheelInput(AxisValue, TEXT("InputComponentAxisFallback"));
}

bool UWebUIFoundationCameraComponent::ConsumeMouseWheelMove()
{
	// WebUI UI 模式以及当前指针位于 UI Hit Region 时，禁止三维滚轮移动。
	// 这里是第二道保险，防止模式切换同一帧留下的 PendingMouseWheelValue 被消费。
	if (GetWorld())
	{
		if (UGameInstance* GI = GetWorld()->GetGameInstance())
		{
			if (UWebUIRuntimeSubsystem* Runtime = GI->GetSubsystem<UWebUIRuntimeSubsystem>())
			{
				bool bBlockWheel = Runtime->GetWebUIInputMode() == EWebUIInputMode::UI;
				if (!bBlockWheel && FSlateApplication::IsInitialized())
				{
					bBlockWheel = Runtime->IsScreenPositionInsideWebUIRegion(
						FSlateApplication::Get().GetCursorPos()
					);
				}

				if (bBlockWheel)
				{
					PendingMouseWheelValue = 0.0f;
					return false;
				}
			}
		}
	}

	if (!bEnableMouseWheelMove || MouseWheelMoveStep <= 0.0f)
	{
		PendingMouseWheelValue = 0.0f;
		return false;
	}

	APlayerController* PC = GetMainPlayerController();
	if (!PC)
	{
		PendingMouseWheelValue = 0.0f;
		return false;
	}

	float WheelValue = PendingMouseWheelValue;
	PendingMouseWheelValue = 0.0f;

	// 兜底：有些工程里输入组件还没绑定成功，但 PlayerController 能直接读到滚轮值。
	if (FMath::IsNearlyZero(WheelValue))
	{
		WheelValue = PC->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
	}

	if (FMath::IsNearlyZero(WheelValue))
	{
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			WheelValue += 1.0f;
		}

		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			WheelValue -= 1.0f;
		}
	}

	if (FMath::IsNearlyZero(WheelValue))
	{
		return false;
	}

	// 一次 Tick 只执行一个逻辑滚轮步长，杜绝 -5 / +5 这类重复输入。
	WheelValue = FMath::Clamp(WheelValue, -1.0f, 1.0f);

	APawn* Pawn = PC->GetPawn();
	AActor* ViewTarget = Pawn ? Cast<AActor>(Pawn) : PC->GetViewTarget();
	if (!ViewTarget)
	{
		return false;
	}

	const FVector Forward = GetMouseWheelMoveForwardVector(PC, ViewTarget);
	if (Forward.IsNearlyZero())
	{
		return false;
	}

	const FVector CurrentLocation = ViewTarget->GetActorLocation();
	const FVector TargetLocation = CurrentLocation + Forward * WheelValue * MouseWheelMoveStep;
	const FRotator TargetRotation = PC->GetControlRotation().GetNormalized();

	StopCurrentPawnMovement();
	ApplyControlledViewTransform(TargetLocation, TargetRotation);

	if (bLogMouseWheelMove)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[WebUIFoundation] MouseWheel moved pawn/view. Wheel=%.3f Step=%.1f From=%s To=%s Forward=%s"),
			WheelValue,
			MouseWheelMoveStep,
			*CurrentLocation.ToCompactString(),
			*TargetLocation.ToCompactString(),
			*Forward.ToCompactString()
		);
	}

	return true;
}

FVector UWebUIFoundationCameraComponent::GetMouseWheelMoveForwardVector(
	const APlayerController* PC,
	const AActor* ViewTarget
) const
{
	FRotator MoveRotation = FRotator::ZeroRotator;

	if (PC)
	{
		MoveRotation = PC->GetControlRotation();
	}
	else if (ViewTarget)
	{
		MoveRotation = ViewTarget->GetActorRotation();
	}

	FVector Forward = MoveRotation.Vector();

	if (bMouseWheelMoveIgnorePitch)
	{
		Forward.Z = 0.0f;
	}

	if (!Forward.Normalize())
	{
		Forward = ViewTarget ? ViewTarget->GetActorForwardVector() : FVector::ForwardVector;

		if (bMouseWheelMoveIgnorePitch)
		{
			Forward.Z = 0.0f;
		}

		Forward.Normalize();
	}

	return Forward;
}

void UWebUIFoundationCameraComponent::StopCurrentPawnMovement()
{
	APawn* Pawn = GetControlledPawn();
	if (!Pawn)
	{
		return;
	}

	if (UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent())
	{
		MoveComp->StopMovementImmediately();
	}

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* CharacterMove = Character->GetCharacterMovement())
		{
			CharacterMove->StopMovementImmediately();
			CharacterMove->SetMovementMode(MOVE_Flying);
			CharacterMove->GravityScale = 0.0f;
		}
	}
}

void UWebUIFoundationCameraComponent::ApplyControlledViewTransform(
	const FVector& TargetLocation,
	const FRotator& TargetRotation
)
{
	APlayerController* PC = GetMainPlayerController();
	if (!PC)
	{
		return;
	}

	APawn* Pawn = PC->GetPawn();

	if (Pawn)
	{
		Pawn->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Pawn->SetActorRotation(TargetRotation, ETeleportType::TeleportPhysics);

		PC->SetControlRotation(TargetRotation);
		PC->SetViewTarget(Pawn);
		return;
	}

	AActor* ViewTarget = PC->GetViewTarget();
	if (ViewTarget)
	{
		ViewTarget->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
		ViewTarget->SetActorRotation(TargetRotation, ETeleportType::TeleportPhysics);
		PC->SetControlRotation(TargetRotation);
	}
}

void UWebUIFoundationCameraComponent::StartSmoothFlyTo(
	const FVector& TargetLocation,
	const FRotator& TargetRotation,
	float Duration
)
{
	APlayerController* PC = GetMainPlayerController();
	if (!PC)
	{
		return;
	}

	StopCurrentPawnMovement();

	APawn* Pawn = PC->GetPawn();
	AActor* ViewTarget = Pawn ? Cast<AActor>(Pawn) : PC->GetViewTarget();

	if (!ViewTarget)
	{
		return;
	}

	FlightStartLocation = ViewTarget->GetActorLocation();
	FlightStartRotation = PC->GetControlRotation();

	FlightTargetLocation = TargetLocation;
	FlightTargetRotation = TargetRotation;

	FlightElapsed = 0.0f;
	FlightDuration = FMath::Max(Duration, 0.0f);

	if (FlightDuration <= 0.01f)
	{
		ApplyControlledViewTransform(FlightTargetLocation, FlightTargetRotation);
		bIsFlying = false;
		return;
	}

	bIsFlying = true;
}

void UWebUIFoundationCameraComponent::FinishSmoothFly()
{
	ApplyControlledViewTransform(FlightTargetLocation, FlightTargetRotation);

	bIsFlying = false;
	FlightElapsed = 0.0f;
	FlightDuration = 0.0f;

	StopCurrentPawnMovement();
}

void UWebUIFoundationCameraComponent::HandleFlyTo(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	double Lon = 0.0;
	double Lat = 0.0;
	double Height = 300.0;
	double Heading = 0.0;
	double Pitch = -35.0;
	double Roll = 0.0;
	double Duration = 0.0;

	if (!Bridge->GetPayloadNumber(Payload, TEXT("lon"), Lon) ||
		!Bridge->GetPayloadNumber(Payload, TEXT("lat"), Lat))
	{
		Bridge->SendError(RequestId, TEXT("flyToResult"), TEXT("flyTo requires lon and lat."));
		return;
	}

	Bridge->GetPayloadNumber(Payload, TEXT("height"), Height);
	Bridge->GetPayloadNumber(Payload, TEXT("heading"), Heading);
	Bridge->GetPayloadNumber(Payload, TEXT("pitch"), Pitch);
	Bridge->GetPayloadNumber(Payload, TEXT("roll"), Roll);
	Bridge->GetPayloadNumber(Payload, TEXT("duration"), Duration);

	const FVector TargetLocation = Bridge->ConvertLonLatHeightToWorld(Lon, Lat, Height);

	const FRotator TargetRotation(
		static_cast<float>(Pitch),
		static_cast<float>(Heading),
		static_cast<float>(Roll)
	);

	StartSmoothFlyTo(
		TargetLocation,
		TargetRotation,
		static_cast<float>(Duration)
	);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("lon"), Lon);
	Data->SetNumberField(TEXT("lat"), Lat);
	Data->SetNumberField(TEXT("height"), Height);
	Data->SetNumberField(TEXT("heading"), Heading);
	Data->SetNumberField(TEXT("pitch"), Pitch);
	Data->SetNumberField(TEXT("roll"), Roll);
	Data->SetNumberField(TEXT("duration"), Duration);
	Data->SetStringField(TEXT("message"), TEXT("flyTo started"));

	Bridge->SendSuccess(RequestId, TEXT("flyToResult"), Data);
}

void UWebUIFoundationCameraComponent::HandleResetView(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId
)
{
	if (!Bridge)
	{
		return;
	}

	bIsFlying = false;
	FlightElapsed = 0.0f;
	FlightDuration = 0.0f;

	// BeginPlay 阶段可能还没有 PlayerController/Pawn，再给缓存一次机会。
	if (!bHasInitialViewState)
	{
		CacheInitialViewState();
	}

	if (!bHasInitialViewState)
	{
		Bridge->SendError(
			RequestId,
			TEXT("resetViewResult"),
			TEXT("Initial view state is not available yet.")
		);
		return;
	}

	APlayerController* PC = GetMainPlayerController();
	if (!PC)
	{
		Bridge->SendError(RequestId, TEXT("resetViewResult"), TEXT("PlayerController not found."));
		return;
	}

	bool bRestoredPawn = false;
	bool bRestoredViewTarget = false;

	APawn* CurrentPawn = PC->GetPawn();
	if (bHasInitialPawnTransform && IsValid(CurrentPawn))
	{
		CurrentPawn->SetActorLocation(
			InitialPawnLocation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
		CurrentPawn->SetActorRotation(
			InitialPawnRotation,
			ETeleportType::TeleportPhysics
		);
		bRestoredPawn = true;

		if (UPawnMovementComponent* MoveComp = CurrentPawn->GetMovementComponent())
		{
			MoveComp->StopMovementImmediately();
		}

		if (bHasInitialCharacterMovementState)
		{
			if (ACharacter* Character = Cast<ACharacter>(CurrentPawn))
			{
				if (UCharacterMovementComponent* CharacterMove = Character->GetCharacterMovement())
				{
					CharacterMove->GravityScale = InitialCharacterGravityScale;
					CharacterMove->SetMovementMode(
						static_cast<EMovementMode>(InitialCharacterMovementMode),
						InitialCharacterCustomMovementMode
					);
				}
			}
		}
	}

	if (bHasInitialViewTargetTransform && IsValid(InitialViewTarget))
	{
		// 初始 ViewTarget 可能是独立 CameraActor，也可能就是 Pawn。
		// Pawn 已经恢复时，不重复 Teleport 同一个 Actor。
		if (InitialViewTarget != CurrentPawn)
		{
			InitialViewTarget->SetActorLocation(
				InitialViewTargetLocation,
				false,
				nullptr,
				ETeleportType::TeleportPhysics
			);
			InitialViewTarget->SetActorRotation(
				InitialViewTargetRotation,
				ETeleportType::TeleportPhysics
			);
		}

		PC->SetViewTarget(InitialViewTarget);
		bRestoredViewTarget = true;
	}
	else if (IsValid(CurrentPawn))
	{
		PC->SetViewTarget(CurrentPawn);
	}

	PC->SetControlRotation(InitialControlRotation);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("view reset"));
	Data->SetBoolField(TEXT("restoredPawn"), bRestoredPawn);
	Data->SetBoolField(TEXT("restoredViewTarget"), bRestoredViewTarget);
	Data->SetStringField(
		TEXT("viewTargetName"),
		IsValid(InitialViewTarget) ? InitialViewTarget->GetName() : TEXT("")
	);

	Bridge->SendSuccess(RequestId, TEXT("resetViewResult"), Data);
}

void UWebUIFoundationCameraComponent::HandleSetInputEnabled(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	bool bEnabled = true;
	Bridge->GetPayloadBool(Payload, TEXT("enabled"), bEnabled);

	APlayerController* PC = GetMainPlayerController();
	if (PC)
	{
		if (bEnabled)
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
		}
		else
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("enabled"), bEnabled);
	Data->SetStringField(TEXT("message"), bEnabled ? TEXT("input enabled") : TEXT("input disabled"));

	Bridge->SendSuccess(RequestId, TEXT("setInputEnabledResult"), Data);
}

TSharedPtr<FJsonObject> UWebUIFoundationCameraComponent::BuildMouseWheelMoveConfigPayload() const
{
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("enabled"), bEnableMouseWheelMove);
	Data->SetNumberField(TEXT("step"), MouseWheelMoveStep);
	Data->SetBoolField(TEXT("ignorePitch"), bMouseWheelMoveIgnorePitch);
	Data->SetBoolField(TEXT("useSlatePreProcessor"), bUseSlateMouseWheelPreProcessor);
	Data->SetBoolField(TEXT("logMouseWheelMove"), bLogMouseWheelMove);
	Data->SetStringField(TEXT("message"), TEXT("mouse wheel move config"));
	return Data;
}

void UWebUIFoundationCameraComponent::HandleGetMouseWheelMoveConfig(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId
)
{
	if (!Bridge)
	{
		return;
	}

	Bridge->SendSuccess(
		RequestId,
		TEXT("getMouseWheelMoveConfigResult"),
		BuildMouseWheelMoveConfigPayload()
	);
}

void UWebUIFoundationCameraComponent::HandleGetViewPosition(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId
)
{
	if (!Bridge)
	{
		return;
	}

	APlayerController* PC = GetMainPlayerController();
	if (!PC)
	{
		Bridge->SendError(
			RequestId,
			TEXT("getViewPositionResult"),
			TEXT("PlayerController not found.")
		);
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector LongitudeLatitudeHeight = FVector::ZeroVector;
	if (!Bridge->ConvertWorldToLonLatHeight(ViewLocation, LongitudeLatitudeHeight))
	{
		Bridge->SendError(
			RequestId,
			TEXT("getViewPositionResult"),
			TEXT("CesiumGeoreference not found. Current Unreal view position is available, but true longitude/latitude/height cannot be calculated.")
		);
		return;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("lon"), LongitudeLatitudeHeight.X);
	Data->SetNumberField(TEXT("lat"), LongitudeLatitudeHeight.Y);
	Data->SetNumberField(TEXT("height"), LongitudeLatitudeHeight.Z);
	Data->SetNumberField(TEXT("worldX"), ViewLocation.X);
	Data->SetNumberField(TEXT("worldY"), ViewLocation.Y);
	Data->SetNumberField(TEXT("worldZ"), ViewLocation.Z);
	Data->SetNumberField(TEXT("heading"), ViewRotation.Yaw);
	Data->SetNumberField(TEXT("pitch"), ViewRotation.Pitch);
	Data->SetNumberField(TEXT("roll"), ViewRotation.Roll);
	Data->SetStringField(TEXT("coordinateSystem"), TEXT("WGS84 ellipsoid height"));
	Data->SetStringField(TEXT("viewTarget"), *GetNameSafe(PC->GetViewTarget()));

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation] Current view position. Lon=%.9f Lat=%.9f Height=%.3f World=%s Rot=%s"),
		LongitudeLatitudeHeight.X,
		LongitudeLatitudeHeight.Y,
		LongitudeLatitudeHeight.Z,
		*ViewLocation.ToCompactString(),
		*ViewRotation.ToCompactString()
	);

	Bridge->SendSuccess(RequestId, TEXT("getViewPositionResult"), Data);
}

void UWebUIFoundationCameraComponent::HandleSetMouseWheelMoveConfig(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	bool bEnabled = bEnableMouseWheelMove;
	if (Bridge->GetPayloadBool(Payload, TEXT("enabled"), bEnabled))
	{
		bEnableMouseWheelMove = bEnabled;
	}

	bool bIgnorePitch = bMouseWheelMoveIgnorePitch;
	if (Bridge->GetPayloadBool(Payload, TEXT("ignorePitch"), bIgnorePitch) ||
		Bridge->GetPayloadBool(Payload, TEXT("mouseWheelMoveIgnorePitch"), bIgnorePitch))
	{
		bMouseWheelMoveIgnorePitch = bIgnorePitch;
	}

	bool bUseSlate = bUseSlateMouseWheelPreProcessor;
	if (Bridge->GetPayloadBool(Payload, TEXT("useSlatePreProcessor"), bUseSlate) ||
		Bridge->GetPayloadBool(Payload, TEXT("useSlateMouseWheelPreProcessor"), bUseSlate))
	{
		bUseSlateMouseWheelPreProcessor = bUseSlate;
		EnsureMouseWheelInputReady();
	}

	bool bLogWheel = bLogMouseWheelMove;
	if (Bridge->GetPayloadBool(Payload, TEXT("log"), bLogWheel) ||
		Bridge->GetPayloadBool(Payload, TEXT("logMouseWheelMove"), bLogWheel))
	{
		bLogMouseWheelMove = bLogWheel;
	}

	double StepValue = MouseWheelMoveStep;
	const bool bHasStep =
		Bridge->GetPayloadNumber(Payload, TEXT("step"), StepValue) ||
		Bridge->GetPayloadNumber(Payload, TEXT("moveStep"), StepValue) ||
		Bridge->GetPayloadNumber(Payload, TEXT("mouseWheelMoveStep"), StepValue);

	if (bHasStep)
	{
		if (StepValue <= 0.0)
		{
			Bridge->SendError(
				RequestId,
				TEXT("setMouseWheelMoveConfigResult"),
				TEXT("Mouse wheel move step must be greater than 0.")
			);
			return;
		}

		MouseWheelMoveStep = static_cast<float>(FMath::Clamp(StepValue, 1.0, 10000000.0));
	}

	Bridge->SendSuccess(
		RequestId,
		TEXT("setMouseWheelMoveConfigResult"),
		BuildMouseWheelMoveConfigPayload()
	);
}

void UWebUIFoundationCameraComponent::AppendSceneState(const TSharedPtr<FJsonObject>& Data) const
{
	if (!Data.IsValid())
	{
		return;
	}

	Data->SetBoolField(TEXT("isFlying"), bIsFlying);
	Data->SetNumberField(TEXT("flightDuration"), FlightDuration);
	Data->SetNumberField(TEXT("flightElapsed"), FlightElapsed);
	Data->SetBoolField(TEXT("mouseWheelMoveEnabled"), bEnableMouseWheelMove);
	Data->SetNumberField(TEXT("mouseWheelMoveStep"), MouseWheelMoveStep);
	Data->SetBoolField(TEXT("mouseWheelMoveIgnorePitch"), bMouseWheelMoveIgnorePitch);
	Data->SetBoolField(TEXT("mouseWheelMoveUseSlatePreProcessor"), bUseSlateMouseWheelPreProcessor);
	Data->SetBoolField(TEXT("mouseWheelMoveLogEnabled"), bLogMouseWheelMove);
	Data->SetBoolField(TEXT("mouseWheelMoveBoundToPlayerController"), bMouseWheelInputBoundToPlayerController);
	Data->SetBoolField(TEXT("mouseWheelMoveBoundToOwnerActor"), false);
	Data->SetBoolField(TEXT("mouseWheelMoveSlateProcessorRegistered"), SlateMouseWheelInputProcessor.IsValid());
}
