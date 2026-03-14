#include "WebUIBridge.h"

#include "WebUIBridgeProtocol.h"

void UWebUIBridge::SetupBridge(UWebBrowser* InBrowser, const FString& InObjectName)
{
	BindBrowser(InBrowser, InObjectName);
	InstallClickListener();
}

void UWebUIBridge::BindBrowser(UWebBrowser* InBrowser, const FString& InObjectName)
{
	if (BoundBrowser == InBrowser && ObjectName.Equals(InObjectName.IsEmpty() ? TEXT("ueBridge") : InObjectName, ESearchCase::CaseSensitive))
	{
		return;
	}

	ReleaseBrowserBindings();
	BoundBrowser = InBrowser;
	SetObjectName(InObjectName);

	if (!BoundBrowser)
	{
		return;
	}

	BoundBrowser->OnConsoleMessage.AddDynamic(this, &UWebUIBridge::HandleConsoleMessage);
	BoundBrowser->OnUrlChanged.AddDynamic(this, &UWebUIBridge::HandleUrlChanged);
}

void UWebUIBridge::UnbindBrowser()
{
	ReleaseBrowserBindings();
}

void UWebUIBridge::InstallClickListener()
{
	if (!BoundBrowser)
	{
		return;
	}

	BoundBrowser->ExecuteJavascript(WebUIBridgeProtocol::BuildBridgeScript(ObjectName));
}

bool UWebUIBridge::IsBrowserBound() const
{
	return BoundBrowser != nullptr;
}

FString UWebUIBridge::GetJavascriptObjectPath() const
{
	return FString::Printf(TEXT("window.%s"), *ObjectName);
}

void UWebUIBridge::SendEvent(const FString& EventName, const FString& PayloadJson)
{
	OnBrowserEvent.Broadcast(EventName, PayloadJson);
}

void UWebUIBridge::BeginDestroy()
{
	ReleaseBrowserBindings();
	Super::BeginDestroy();
}

void UWebUIBridge::HandleUrlChanged(const FText& Text)
{
	(void)Text;
	InstallClickListener();
}

void UWebUIBridge::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
	(void)Source;
	(void)Line;

	WebUIBridgeProtocol::FBridgeMessage ParsedMessage;
	if (!WebUIBridgeProtocol::TryParseConsoleMessage(Message, ParsedMessage))
	{
		return;
	}

	OnBrowserEvent.Broadcast(ParsedMessage.Type, ParsedMessage.DataJson);
}

void UWebUIBridge::ReleaseBrowserBindings()
{
	if (!BoundBrowser)
	{
		return;
	}

	BoundBrowser->OnConsoleMessage.RemoveDynamic(this, &UWebUIBridge::HandleConsoleMessage);
	BoundBrowser->OnUrlChanged.RemoveDynamic(this, &UWebUIBridge::HandleUrlChanged);
	BoundBrowser = nullptr;
}

void UWebUIBridge::SetObjectName(const FString& InObjectName)
{
	ObjectName = InObjectName.IsEmpty() ? TEXT("ueBridge") : InObjectName;
}
