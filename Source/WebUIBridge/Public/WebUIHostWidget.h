#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WebUIHostWidget.generated.h"

class UWebUIBrowserWidget;
class UWidget;
class UTextBlock;
class UButton;
class UMultiLineEditableTextBox;

UENUM(BlueprintType)
enum class EWebUIHostInteractionMode : uint8
{
	FullIntercept UMETA(DisplayName = "Full Intercept"),
	PassiveOverlay UMETA(DisplayName = "Passive Overlay"),
	Reserved UMETA(DisplayName = "Reserved")
};

USTRUCT(BlueprintType)
struct FWebUIHostLogEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Web UI|Host")
	FString Timestamp;

	UPROPERTY(BlueprintReadOnly, Category = "Web UI|Host")
	FString Category;

	UPROPERTY(BlueprintReadOnly, Category = "Web UI|Host")
	FString Message;
};

UCLASS(BlueprintType, Blueprintable)
class WEBUIBRIDGE_API UWebUIHostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 宿主自己的启动地址。若填写，会覆盖 MainBrowser 上的 StartupURL。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	FString StartupURL = TEXT("http://localhost:5173");

	/** 是否在 Construct 时自动加载 StartupURL。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	bool bAutoLoadStartupURL = true;

	/** 是否在启动时显示调试面板。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	bool bShowDebugPanelOnStart = true;

	/** 是否自动处理基础闭环事件。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	bool bAutoHandleBaseEvents = true;

	/** 最大日志条数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host", meta = (ClampMin = "1", ClampMax = "500"))
	int32 MaxLogEntries = 50;

	/** 第一版交互模式，先作为状态保留。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	EWebUIHostInteractionMode InteractionMode = EWebUIHostInteractionMode::FullIntercept;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void InitializeHostWidget();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void ReloadBrowser();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void ClearLogs();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void SetDebugPanelVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Web UI|Host")
	bool IsDebugPanelVisible() const { return bDebugPanelVisible; }

	UFUNCTION(BlueprintPure, Category = "Web UI|Host")
	const TArray<FWebUIHostLogEntry>& GetLogs() const { return LogEntries; }

protected:
	/** 蓝图中拖入的浏览器控件，变量名必须叫 MainBrowser */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWebUIBrowserWidget> MainBrowser = nullptr;

	/** 调试面板根节点，可用 Border / Overlay / VerticalBox，变量名必须叫 DebugPanel */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DebugPanel = nullptr;

	/** 状态文本，变量名必须叫 StatusText */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText = nullptr;

	/** 日志文本框，变量名必须叫 LogTextBox */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UMultiLineEditableTextBox> LogTextBox = nullptr;

	/** 重载按钮，变量名必须叫 ReloadButton */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReloadButton = nullptr;

	/** 清空日志按钮，变量名必须叫 ClearLogsButton */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ClearLogsButton = nullptr;

	/** 显示/隐藏调试面板按钮，变量名必须叫 ToggleDebugButton */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ToggleDebugButton = nullptr;

protected:
	UFUNCTION()
	void HandleBrowserEvent(const FString& EventName, const FString& PayloadJson);

	UFUNCTION()
	void HandleBrowserUrlChanged(const FString& Url);

	UFUNCTION()
	void HandleReloadClicked();

	UFUNCTION()
	void HandleClearLogsClicked();

	UFUNCTION()
	void HandleToggleDebugClicked();

	void ApplyStartupConfig();
	void BindBrowserEvents();
	void UnbindBrowserEvents();
	void UpdateStatusText();
	void RefreshLogText();
	void AddLog(const FString& Category, const FString& Message);

	void SendUEReady();
	void SendPong();
	void SendSceneState();

	FString BuildSceneStateJson() const;
	FString GetInteractionModeString() const;
	FString GetNowTimeString() const;

protected:
	UPROPERTY(Transient)
	bool bInitialized = false;

	UPROPERTY(Transient)
	bool bDebugPanelVisible = true;

	UPROPERTY(Transient)
	FString CurrentUrl;

	UPROPERTY(Transient)
	FString LastBridgeEvent = TEXT("None");

	UPROPERTY(Transient)
	TArray<FWebUIHostLogEntry> LogEntries;
};