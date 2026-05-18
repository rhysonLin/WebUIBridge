#include "Bridge/WebUIBridge.h"

#include "Bridge/WebUIBridgeProtocol.h"

void UWebUIBridge::SetupBridge(UWebBrowser* InBrowser, const FString& InObjectName)
{
	BindBrowser(InBrowser, InObjectName);
	InstallBridgeScript();
}
//绑定ue的UWebBrowser
void UWebUIBridge::BindBrowser(UWebBrowser* InBrowser, const FString& InObjectName)
{
	if (BoundBrowser == InBrowser &&
		ObjectName.Equals(InObjectName.IsEmpty() ? TEXT("ueBridge") : InObjectName, ESearchCase::CaseSensitive))
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
	//监听网页 console.log，接收前端发来的消息
	BoundBrowser->OnConsoleMessage.AddDynamic(this, &UWebUIBridge::HandleConsoleMessage);
	//网页换地址后，重新注入桥接脚本
	BoundBrowser->OnUrlChanged.AddDynamic(this, &UWebUIBridge::HandleUrlChanged);
}

void UWebUIBridge::UnbindBrowser()
{
	ReleaseBrowserBindings();
}

void UWebUIBridge::InstallBridgeScript()
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
	SendEventToPage(EventName, PayloadJson);
}

void UWebUIBridge::SendEventToPage(const FString& EventName, const FString& PayloadJson)
{
	if (!BoundBrowser)
	{
		return;
	}

	const FString SafeObjectName = EscapeForSingleQuotedJavascriptString(ObjectName);
	const FString SafeEventName = EscapeForSingleQuotedJavascriptString(EventName);
	const FString SafePayload = EscapeForSingleQuotedJavascriptString(PayloadJson.IsEmpty() ? TEXT("null") : PayloadJson);

	const FString Script = FString::Printf(TEXT(R"JS(
(function() {
	try {
		var bridgeName = '%s';
		var eventName = '%s';
		var payloadJson = '%s';
		var payload = null;

		try {
			payload = payloadJson ? JSON.parse(payloadJson) : null;
		} catch (e) {
			console.warn('[WebUIBridge] payload parse failed:', e);
			payload = null;
		}

		if (window[bridgeName] && typeof window[bridgeName].__dispatchFromUE === 'function') {
			window[bridgeName].__dispatchFromUE(eventName, payload);
		}

		window.dispatchEvent(new CustomEvent('ue-message', {
			detail: {
				type: eventName,
				data: payload
			}
		}));
	} catch (e) {
		console.warn('[WebUIBridge] UE -> JS dispatch failed:', e);
	}
})();
)JS"), *SafeObjectName, *SafeEventName, *SafePayload);

	ExecuteJavascript(Script);
}

void UWebUIBridge::ExecuteJavascript(const FString& Script)
{
	if (!BoundBrowser)
	{
		return;
	}

	BoundBrowser->ExecuteJavascript(Script);
}

void UWebUIBridge::BeginDestroy()
{
	ReleaseBrowserBindings();
	Super::BeginDestroy();
}

void UWebUIBridge::HandleUrlChanged(const FText& Text)
{
	(void)Text;
	InstallBridgeScript();
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

FString UWebUIBridge::EscapeForSingleQuotedJavascriptString(const FString& InValue) const
{
	FString Escaped = InValue;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("'"), TEXT("\\'"));
	Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	return Escaped;
}