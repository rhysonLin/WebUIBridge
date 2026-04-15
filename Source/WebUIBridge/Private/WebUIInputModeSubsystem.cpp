#include "WebUIInputModeSubsystem.h"

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

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);

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