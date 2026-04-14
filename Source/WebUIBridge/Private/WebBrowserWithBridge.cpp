#include "WebBrowserWithBridge.h"

UWebBrowserWithBridge::UWebBrowserWithBridge(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	OnUrlChanged.AddDynamic(this, &UWebBrowserWithBridge::HandleBrowserUrlChanged);
}

void UWebBrowserWithBridge::SetupBridge()
{
	AutoSetupBridge();
}

void UWebBrowserWithBridge::ReinstallBridgeScript()
{
	if (Bridge)
	{
		Bridge->InstallBridgeScript();
	}
}

void UWebBrowserWithBridge::SendEventToPage(const FString& EventName, const FString& PayloadJson)
{
	AutoSetupBridge();

	if (Bridge)
	{
		Bridge->SendEventToPage(EventName, PayloadJson);
	}
}

void UWebBrowserWithBridge::ExecuteBridgeJavascript(const FString& Script)
{
	AutoSetupBridge();

	if (Bridge)
	{
		Bridge->ExecuteJavascript(Script);
	}
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

void UWebBrowserWithBridge::LoadURLWithBridge(const FString& NewURL)
{
	AutoSetupBridge();
	Super::LoadURL(NewURL);
}

void UWebBrowserWithBridge::LoadHTMLWithBridge(const FString& Contents, const FString& DummyURL)
{
	AutoSetupBridge();
	LoadString(Contents, DummyURL);
}

void UWebBrowserWithBridge::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (bAutoSetupBridge)
	{
		AutoSetupBridge();
	}
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

void UWebBrowserWithBridge::HandleBrowserUrlChanged(const FText& Text)
{
	OnPageUrlChanged.Broadcast(Text.ToString());
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