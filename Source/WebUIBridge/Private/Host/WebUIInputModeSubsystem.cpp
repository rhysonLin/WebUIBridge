#include "Host/WebUIInputModeSubsystem.h"

#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UWebUIInputModeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentMode = EWebUIInputMode::Game;
}

void UWebUIInputModeSubsystem::Deinitialize()
{
	TargetPlayerController = nullptr;
	Super::Deinitialize();
}

void UWebUIInputModeSubsystem::SetTargetPlayerController(APlayerController* InPlayerController)
{
	TargetPlayerController = InPlayerController;

	if (TargetPlayerController)
	{
		UE_LOG(LogTemp, Log, TEXT("[WebUIInputModeSubsystem] Target PlayerController set: %s"), *GetNameSafe(TargetPlayerController));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIInputModeSubsystem] Target PlayerController cleared."));
	}
}

void UWebUIInputModeSubsystem::EnterUIMode()
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIInputModeSubsystem] EnterUIMode failed: TargetPlayerController is null."));
		return;
	}

	ApplyUIMode(PC);
	CurrentMode = EWebUIInputMode::UI;

	UE_LOG(LogTemp, Log, TEXT("[WebUIInputModeSubsystem] Switched to UI Mode."));
	ShowModeMessage(TEXT("UI Mode"), FColor::Green);
}

void UWebUIInputModeSubsystem::EnterGameMode()
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIInputModeSubsystem] EnterGameMode failed: TargetPlayerController is null."));
		return;
	}

	ApplyGameMode(PC);
	CurrentMode = EWebUIInputMode::Game;

	UE_LOG(LogTemp, Log, TEXT("[WebUIInputModeSubsystem] Switched to Game Mode."));
	ShowModeMessage(TEXT("Game Mode"), FColor::Yellow);
}

void UWebUIInputModeSubsystem::EnterAutomaticSceneMode()
{
	APlayerController* PC = TargetPlayerController.Get();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIInputModeSubsystem] EnterAutomaticSceneMode failed: TargetPlayerController is null."));
		return;
	}

	ApplyAutomaticSceneMode(PC);
	CurrentMode = EWebUIInputMode::Game;

	UE_LOG(LogTemp, Verbose, TEXT("[WebUIInputModeSubsystem] Switched to Automatic Scene Mode."));
}

void UWebUIInputModeSubsystem::ToggleInputMode()
{
	if (CurrentMode == EWebUIInputMode::Game)
	{
		EnterUIMode();
	}
	else
	{
		EnterGameMode();
	}
}

void UWebUIInputModeSubsystem::ApplyUIMode(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	// UIOnly 可以从 PlayerController 输入链路中彻底隔离滚轮和鼠标按键。
	// 旧版 GameAndUI 会把 WebBrowser 未消费的 Wheel 继续传给三维控制器。
	FInputModeUIOnly InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;

	PC->SetIgnoreMoveInput(bIgnoreMoveInputInUIMode);
	PC->SetIgnoreLookInput(bIgnoreLookInputInUIMode);
}

void UWebUIInputModeSubsystem::ApplyGameMode(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);

	PC->bShowMouseCursor = false;

	PC->SetIgnoreMoveInput(false);
	PC->SetIgnoreLookInput(false);
}

void UWebUIInputModeSubsystem::ApplyAutomaticSceneMode(APlayerController* PC)
{
	if (!PC)
	{
		return;
	}

	// Scene 模式必须回到真正的 GameOnly。
	// 全屏 SWebBrowser 在这一模式下同时会被设为 HitTestInvisible，
	// 因而右键按下、鼠标捕获和 MouseMove 能完整进入 PlayerController。
	// 重新进入 UI 由全局 InputProcessor 根据 EngineJS 同步的 UI 矩形判断，
	// 不再依赖 Browser 自己接收 mouseover。
	FInputModeGameOnly InputMode;
	PC->SetInputMode(InputMode);

	// 自动场景模式保持光标可见；项目自己的右键观察逻辑可在按住右键时隐藏/捕获。
	PC->bShowMouseCursor = true;
	PC->SetIgnoreMoveInput(false);
	PC->SetIgnoreLookInput(false);
}

void UWebUIInputModeSubsystem::ShowModeMessage(const FString& Message, const FColor& Color) const
{
	if (!bShowOnScreenDebugMessage)
	{
		return;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 1.2f, Color, Message);
	}
}