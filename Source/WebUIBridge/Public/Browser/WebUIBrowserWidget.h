#pragma once

#include "CoreMinimal.h"
#include "WebBrowserWithBridge.h"
#include "WebUIBrowserWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWebUIBrowserWidgetSimpleSignature);

UCLASS(BlueprintType, meta = (DisplayName = "Web UI Browser Widget"))
class WEBUIBRIDGE_API UWebUIBrowserWidget : public UWebBrowserWithBridge
{
	GENERATED_BODY()

public:
	UWebUIBrowserWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Startup")
	FString StartupURL;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Startup", meta = (MultiLine = true))
	FString StartupHTML;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Startup")
	bool bLoadStartupURLOnSynchronize = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Startup")
	bool bPreferStartupHTML = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Bridge")
	bool bSendUEReadyAfterSetup = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Bridge")
	FString ReadyEventName = TEXT("UEReady");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Bridge")
	FString ReadyPayloadJson = TEXT("{\"ok\":true}");

	UPROPERTY(BlueprintAssignable, Category = "Web UI|Startup")
	FWebUIBrowserWidgetSimpleSignature OnBrowserWidgetInitialized;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Startup")
	void InitializeBrowserWidget();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Startup")
	void LoadStartupContent();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	void NotifyFrontendReady();

	virtual void SynchronizeProperties() override;

protected:
	UPROPERTY(Transient)
	bool bInitialized = false;
};