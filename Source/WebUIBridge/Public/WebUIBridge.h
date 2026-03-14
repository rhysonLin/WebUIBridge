#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WebBrowser.h"
#include "WebUIBridge.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWebUIBridgeEventSignature, const FString&, EventName, const FString&, PayloadJson);
UCLASS(BlueprintType, Blueprintable)
class WEBUIBRIDGE_API UWebUIBridge : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Web UI")
	FWebUIBridgeEventSignature OnBrowserEvent;

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	void SetupBridge(UWebBrowser* InBrowser, const FString& InObjectName = TEXT("ueBridge"));

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	void BindBrowser(UWebBrowser* InBrowser, const FString& InObjectName = TEXT("ueBridge"));

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	void UnbindBrowser();

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	void InstallClickListener();

	UFUNCTION(BlueprintPure, Category = "Web UI")
	bool IsBrowserBound() const;

	UFUNCTION(BlueprintPure, Category = "Web UI")
	FString GetJavascriptObjectPath() const;

	UFUNCTION(BlueprintCallable, Category = "Web UI")
	void SendEvent(const FString& EventName, const FString& PayloadJson);

	virtual void BeginDestroy() override;

protected:
	UFUNCTION()
	void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

	UFUNCTION()
	void HandleUrlChanged(const FText& Text);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Web UI")
	TObjectPtr<UWebBrowser> BoundBrowser = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI")
	FString ObjectName = TEXT("ueBridge");

private:
	void ReleaseBrowserBindings();
	void SetObjectName(const FString& InObjectName);
};
