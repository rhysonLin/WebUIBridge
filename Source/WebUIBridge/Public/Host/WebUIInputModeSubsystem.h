#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WebUIInputModeSubsystem.generated.h"

class APlayerController;

UENUM(BlueprintType)
enum class EWebUIInputMode : uint8
{
	Game UMETA(DisplayName = "Game"),
	UI UMETA(DisplayName = "UI")
};

UCLASS()
class WEBUIBRIDGE_API UWebUIInputModeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Input")
	void SetTargetPlayerController(APlayerController* InPlayerController);

	UFUNCTION(BlueprintPure, Category = "Web UI|Input")
	APlayerController* GetTargetPlayerController() const { return TargetPlayerController.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Web UI|Input")
	void EnterUIMode();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Input")
	void EnterGameMode();

	/**
	 * 自动路由下的场景模式。
	 * 使用真正的 GameOnly；Browser 在 Scene 模式退出 Slate 命中链。
	 * EngineJS 把 data-engine-ui 矩形同步给 UE，全局 InputProcessor 负责自动切回 UI。
	 */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Input")
	void EnterAutomaticSceneMode();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Input")
	void ToggleInputMode();

	UFUNCTION(BlueprintPure, Category = "Web UI|Input")
	EWebUIInputMode GetCurrentMode() const { return CurrentMode; }

	UFUNCTION(BlueprintPure, Category = "Web UI|Input")
	bool IsUIMode() const { return CurrentMode == EWebUIInputMode::UI; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Web UI|Input")
	bool bIgnoreMoveInputInUIMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Web UI|Input")
	bool bIgnoreLookInputInUIMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Web UI|Input")
	bool bShowOnScreenDebugMessage = true;

protected:
	void ApplyUIMode(APlayerController* PC);
	void ApplyGameMode(APlayerController* PC);
	void ApplyAutomaticSceneMode(APlayerController* PC);
	void ShowModeMessage(const FString& Message, const FColor& Color) const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<APlayerController> TargetPlayerController = nullptr;

	UPROPERTY(Transient)
	EWebUIInputMode CurrentMode = EWebUIInputMode::Game;
};