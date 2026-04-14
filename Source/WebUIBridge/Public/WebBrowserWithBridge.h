#pragma once

#include "CoreMinimal.h"
#include "WebBrowser.h"
#include "WebUIBridge.h"
#include "WebBrowserWithBridge.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWebBrowserWithBridgeUrlSignature, const FString&, Url);

UCLASS(BlueprintType, meta = (DisplayName = "Web Browser With Bridge"))
class WEBUIBRIDGE_API UWebBrowserWithBridge : public UWebBrowser
{
	GENERATED_BODY()

public:
	UWebBrowserWithBridge(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Bridge")
	FString BridgeObjectName = TEXT("ueBridge");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI|Bridge")
	bool bAutoSetupBridge = true;

	UPROPERTY(BlueprintAssignable, Category = "Web UI|Bridge")
	FWebUIBridgeEventSignature OnBrowserEvent;

	UPROPERTY(BlueprintAssignable, Category = "Web UI|Bridge")
	FWebBrowserWithBridgeUrlSignature OnPageUrlChanged;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	void SetupBridge();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	void ReinstallBridgeScript();

	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	void SendEventToPage(const FString& EventName, const FString& PayloadJson);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Bridge")
	void ExecuteBridgeJavascript(const FString& Script);

	UFUNCTION(BlueprintPure, Category = "Web UI|Bridge")
	UWebUIBridge* GetBridge() const;

	UFUNCTION(BlueprintPure, Category = "Web UI|Bridge")
	FString GetJavascriptObjectPath() const;

	UFUNCTION(BlueprintCallable, Category = "Web UI|Browser")
	void LoadURLWithBridge(const FString& NewURL);

	UFUNCTION(BlueprintCallable, Category = "Web UI|Browser")
	void LoadHTMLWithBridge(const FString& Contents, const FString& DummyURL = TEXT("http://localhost/"));

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UWebUIBridge> Bridge = nullptr;

	UFUNCTION()
	void HandleBridgeEvent(const FString& EventName, const FString& PayloadJson);

	UFUNCTION()
	void HandleBrowserUrlChanged(const FText& Text);

private:
	void AutoSetupBridge();
	void EnsureBridge();
	void BindBridgeDelegates();
};