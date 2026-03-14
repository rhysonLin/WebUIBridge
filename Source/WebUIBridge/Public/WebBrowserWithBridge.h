#pragma once

#include "CoreMinimal.h"
#include "WebBrowser.h"
#include "WebUIBridge.h"
#include "WebBrowserWithBridge.generated.h"

UCLASS(BlueprintType, meta = (DisplayName = "Web Browser With Bridge"))
class WEBUIBRIDGE_API UWebBrowserWithBridge : public UWebBrowser
{
	GENERATED_BODY()

public:
	UWebBrowserWithBridge(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Web UI")
	FString BridgeObjectName = TEXT("ueBridge");

	UPROPERTY(BlueprintAssignable, Category = "Web UI")
	FWebUIBridgeEventSignature OnBrowserEvent;

	void LoadURL(FString NewURL);

	UFUNCTION(BlueprintPure, Category = "Web UI")
	UWebUIBridge* GetBridge() const;

	UFUNCTION(BlueprintPure, Category = "Web UI")
	FString GetJavascriptObjectPath() const;

	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UWebUIBridge> Bridge = nullptr;

	UFUNCTION()
	void HandleBridgeEvent(const FString& EventName, const FString& PayloadJson);

private:
	void AutoSetupBridge();
	void EnsureBridge();
	void BindBridgeDelegates();
};
