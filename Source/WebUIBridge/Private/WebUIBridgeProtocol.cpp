#include "WebUIBridgeProtocol.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr TCHAR UEWebUIBridgeConsolePrefix[] = TEXT("__UEWEBUIBRIDGE__");

FString EscapeForSingleQuotedJavascriptString(const FString& InValue)
{
	FString Escaped = InValue;
	Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Escaped.ReplaceInline(TEXT("'"), TEXT("\\'"));
	Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
	Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	return Escaped;
}

FString SerializeJsonValue(const TSharedPtr<FJsonValue>& InValue)
{
	if (!InValue.IsValid())
	{
		return TEXT("{}");
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(InValue.ToSharedRef(), TEXT(""), Writer);
	return JsonString;
}

FString GetDataJson(const TSharedPtr<FJsonObject>& RootObject)
{
	if (!RootObject.IsValid())
	{
		return TEXT("{}");
	}

	const TSharedPtr<FJsonValue> DataValue = RootObject->TryGetField(TEXT("data"));
	if (DataValue.IsValid())
	{
		return SerializeJsonValue(DataValue);
	}

	return TEXT("{}");
}
}

namespace WebUIBridgeProtocol
{
FString BuildBridgeScript(const FString& InObjectName)
{
	const FString EscapedObjectName = EscapeForSingleQuotedJavascriptString(InObjectName);

	return FString::Printf(TEXT(R"JS(
(function() {
	var bridgeName = '%s';
	var consolePrefix = '%s';
	var globalState = window.__ueBridgeState = window.__ueBridgeState || {};
	var bridgeState = globalState[bridgeName] = globalState[bridgeName] || {};

	function emitToUE(type, data) {
		console.log(consolePrefix + JSON.stringify({
			type: type || '',
			data: data === undefined ? {} : data
		}));
	}

	function ensureBridgeObject() {
		if (!window[bridgeName]) {
			window[bridgeName] = {};
		}
		if (!window[bridgeName].emit) {
			window[bridgeName].emit = function(message) {
				if (!message || typeof message !== 'object') {
					return;
				}
				emitToUE(message.type, message.data);
			};
		}
		if (!window[bridgeName].sendEvent) {
			window[bridgeName].sendEvent = function(type, data) {
				if (type && typeof type === 'object') {
					window[bridgeName].emit(type);
					return;
				}
				emitToUE(type, data);
			};
		}
	}

	function resolveNode(target) {
		if (!target) {
			return null;
		}
		if (target.closest) {
			return target.closest('[data-ue-click],button,a,[id]') || target;
		}
		return target;
	}

	function emitClick(target) {
		var node = resolveNode(target);
		if (!node) {
			return;
		}

		var data = {
			id: node.id || '',
			tag: (node.tagName || '').toLowerCase(),
			text: ((node.innerText || node.textContent || '').trim()).substring(0, 256),
			href: node.href || '',
			event: (node.dataset && node.dataset.ueClick) ? node.dataset.ueClick : ''
		};

		emitToUE('click', data);
	}

	function install() {
		ensureBridgeObject();
		if (!bridgeState.readySent) {
			bridgeState.readySent = true;
			emitToUE('BridgeReady', {
				bridge: bridgeName
			});
		}
		if (bridgeState.clickInstalled) {
			return;
		}

		bridgeState.clickInstalled = true;
		document.addEventListener('click', function(evt) {
			emitClick(evt.target);
		}, true);
	}

	if (document.readyState === 'loading') {
		document.addEventListener('DOMContentLoaded', install, { once: true });
	} else {
		install();
	}
})();
)JS"), *EscapedObjectName, UEWebUIBridgeConsolePrefix);
}

FString GetConsolePrefix()
{
	return UEWebUIBridgeConsolePrefix;
}

bool TryParseConsoleMessage(const FString& Message, FBridgeMessage& OutMessage)
{
	if (!Message.StartsWith(UEWebUIBridgeConsolePrefix))
	{
		return false;
	}

	const FString JsonMessage = Message.RightChop(FCString::Strlen(UEWebUIBridgeConsolePrefix));
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonMessage);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	if (!RootObject->TryGetStringField(TEXT("type"), OutMessage.Type))
	{
		return false;
	}

	OutMessage.DataJson = GetDataJson(RootObject);
	return true;
}
}
