#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WebUIHostWidget.generated.h"

class UCanvasPanel;
class UWebUIBrowserWidget;

UENUM(BlueprintType)
enum class EWebUIHostInteractionMode : uint8
{
	FullIntercept UMETA(DisplayName = "Full Intercept"),
	PassiveOverlay UMETA(DisplayName = "Passive Overlay"),
	Reserved UMETA(DisplayName = "Reserved")
};

UCLASS(BlueprintType, Blueprintable)
class WEBUIBRIDGE_API UWebUIHostWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWebUIHostWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Host")
	FString StartupURL = TEXT("http://localhost:5173");

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

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void InitializeHostWidget();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Host")
	void ReloadBrowser();

protected:
	UFUNCTION()
	void HandleBrowserEvent(const FString& EventName, const FString& PayloadJson);

	UFUNCTION()
	void HandleBrowserUrlChanged(const FString& Url);

protected:
	void BuildUI();
	void BindBrowserEvents();
	void UnbindBrowserEvents();
	void ApplyStartupConfig();

	void SendUEReady();
	void SendPong();
	void SendSceneState();

	FString BuildSceneStateJson() const;
	FString GetInteractionModeString() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWebUIBrowserWidget> MainBrowser = nullptr;

	UPROPERTY(Transient)
	bool bInitialized = false;

	UPROPERTY(Transient)
	FString CurrentUrl;

	UPROPERTY(Transient)
	FString LastBridgeEvent = TEXT("None");
};