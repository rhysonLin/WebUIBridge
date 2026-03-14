#include "WebBrowserWithBridge.h"

UWebBrowserWithBridge::UWebBrowserWithBridge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWebBrowserWithBridge::LoadURL(FString NewURL)
{
	AutoSetupBridge();
	Super::LoadURL(MoveTemp(NewURL));
}

UWebUIBridge* UWebBrowserWithBridge::GetBridge() const
{
	return Bridge;
}

FString UWebBrowserWithBridge::GetJavascriptObjectPath() const
{
	if (Bridge)
	{
		return Bridge->GetJavascriptObjectPath();
	}

	const FString EffectiveObjectName = BridgeObjectName.IsEmpty() ? TEXT("ueBridge") : BridgeObjectName;
	return FString::Printf(TEXT("window.%s"), *EffectiveObjectName);
}

void UWebBrowserWithBridge::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	AutoSetupBridge();
}

void UWebBrowserWithBridge::ReleaseSlateResources(bool bReleaseChildren)
{
	if (Bridge)
	{
		Bridge->UnbindBrowser();
	}

	Super::ReleaseSlateResources(bReleaseChildren);
}

void UWebBrowserWithBridge::HandleBridgeEvent(const FString& EventName, const FString& PayloadJson)
{
	OnBrowserEvent.Broadcast(EventName, PayloadJson);
}

void UWebBrowserWithBridge::AutoSetupBridge()
{
	EnsureBridge();
	if (!Bridge)
	{
		return;
	}

	BindBridgeDelegates();
	Bridge->SetupBridge(this, BridgeObjectName);
}

void UWebBrowserWithBridge::EnsureBridge()
{
	if (Bridge || HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	Bridge = NewObject<UWebUIBridge>(this, TEXT("WebUIBridgeInstance"));
}

void UWebBrowserWithBridge::BindBridgeDelegates()
{
	if (!Bridge)
	{
		return;
	}

	Bridge->OnBrowserEvent.RemoveDynamic(this, &UWebBrowserWithBridge::HandleBridgeEvent);
	Bridge->OnBrowserEvent.AddDynamic(this, &UWebBrowserWithBridge::HandleBridgeEvent);
}
