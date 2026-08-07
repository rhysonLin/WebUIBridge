#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Host/WebUIInputModeSubsystem.h"
#include "WebUIHostWidget.generated.h"

class AWebUIFoundationBridgeActor;
class UCanvasPanel;
class UWebUIFoundationBridgeComponent;
class UWebUIBrowserWidget;
class UWorld;

UENUM(BlueprintType)
enum class EWebUIHostInteractionMode : uint8
{
	FullIntercept UMETA(DisplayName = "Full Intercept"),
	PassiveOverlay UMETA(DisplayName = "Passive Overlay"),
	Reserved UMETA(DisplayName = "Reserved")
};

/**
 * UE 内嵌 HTML 的宿主 Widget。
 *
 * 除旧版 WebUIBridge 事件外，本版本增加 WebUIFoundation 通用协议通道：
 *
 * JS -> UE:
 *   window.ueBridge.sendEvent("PixelFoundation", engineMessage)
 *
 * UE -> JS:
 *   ue-message.detail.type = "PixelFoundation.Response"
 *   ue-message.detail.data = 通用标准响应对象
 *
 * 因此 Pixel 与 WebUI 两种运行模式使用完全相同的 type / payload / response。
 */
UCLASS(BlueprintType, Blueprintable)
class WEBUIBRIDGE_API UWebUIHostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWebUIHostWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnMouseWheel(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent
	) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	FString StartupURL = TEXT("http://localhost:5173/?runtime=webui");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	bool bAutoLoadStartupURL = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	bool bAutoHandleBaseEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	EWebUIHostInteractionMode InteractionMode = EWebUIHostInteractionMode::FullIntercept;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	FString BrowserBridgeObjectName = TEXT("ueBridge");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	bool bBrowserSendUEReadyAfterSetup = false;

	// ===== WebUIFoundation parity transport =====

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Foundation")
	bool bEnableFoundationProtocol = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Foundation")
	bool bAutoSpawnFoundationBridgeActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Foundation")
	bool bDestroyAutoSpawnedFoundationBridgeOnDestruct = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Foundation")
	TSubclassOf<AWebUIFoundationBridgeActor> FoundationBridgeActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Foundation")
	FString FoundationCommandEventName = TEXT("PixelFoundation");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Foundation")
	FString FoundationResponseEventName = TEXT("PixelFoundation.Response");

	// 允许不经过通用 envelope，直接用事件名发送 flyTo / moveActor 等命令。
	// 正式 EngineJS 默认使用 PixelFoundation 通用事件；此开关仅用于旧页面兼容。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Foundation")
	bool bAcceptDirectFoundationCommandEvents = true;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void InitializeHostWidget();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void ReloadBrowser();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Foundation")
	bool InitializeFoundationBridge();

	UFUNCTION(BlueprintPure, Category = "Web UI|Foundation")
	bool IsFoundationBridgeReady() const;

	/** 由 GameInstance RuntimeSubsystem 在关卡切换后重新绑定当前 World。 */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Runtime")
	void HandleWorldChanged(UWorld* NewWorld);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Runtime")
	void PrepareForWorldCleanup(UWorld* CleaningWorld);

	/** UI 模式可点击网页；Scene 模式页面继续渲染但不拦截鼠标。 */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Input")
	void SetBrowserInteractionEnabled(bool bEnabled);

	/** 向 EngineJS 广播由快捷键或 C++ 触发的输入模式变化。 */
	UFUNCTION(BlueprintCallable, Category = "Web UI|Input")
	void NotifyWebUIInputModeChanged(EWebUIInputMode NewMode, const FString& ToggleKeyName);

protected:
	UFUNCTION()
	void HandleBrowserEvent(const FString& EventName, const FString& PayloadJson);

	UFUNCTION()
	void HandleBrowserUrlChanged(const FString& Url);

	UFUNCTION()
	void HandleFoundationResponseReady(const FString& ResponseJson);

protected:
	void BuildUI();
	void BindBrowserEvents();
	void UnbindBrowserEvents();
	void ApplyStartupConfig();

	void SendUEReady();
	void SendPong();
	void SendSceneState();

	// WebUIFoundation 通用协议
	void ShutdownFoundationBridge();
	void HandleFoundationDescriptor(const FString& DescriptorJson);
	void HandleDirectFoundationCommand(const FString& CommandType, const FString& PayloadJson);
	void SendFoundationTransportError(
		const FString& DescriptorJson,
		const FString& FallbackType,
		const FString& Message
	);
	FString BuildFoundationDescriptor(
		const FString& CommandType,
		const FString& PayloadJson
	) const;
	bool IsFoundationCommandType(const FString& EventName) const;

	// 旧版兼容事件
	void HandleMoveToWorld(const FString& PayloadJson);
	void HandleMoveToGeo(const FString& PayloadJson);
	void SendCameraMoveResult(const FString& EventName, bool bOk, const FString& ErrorMessage);

	FString BuildSceneStateJson() const;
	FString GetInteractionModeString() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWebUIBrowserWidget> MainBrowser = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<AWebUIFoundationBridgeActor> FoundationBridgeActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWebUIFoundationBridgeComponent> FoundationBridgeComponent = nullptr;

	UPROPERTY(Transient)
	bool bOwnsFoundationBridgeActor = false;

	TWeakObjectPtr<UWorld> BoundFoundationWorld;

	UPROPERTY(Transient)
	bool bInitialized = false;

	UPROPERTY(Transient)
	FString CurrentUrl;

	UPROPERTY(Transient)
	FString LastBridgeEvent = TEXT("None");
};
