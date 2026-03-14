#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WebUIBridgeProtocol.h"

namespace
{
bool DeserializeJsonObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
{
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebUIBridgeUnifiedMessageTest, "WebUIBridge.Protocol.UnifiedMessage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebUIBridgeGenericClickTypeTest, "WebUIBridge.Protocol.GenericClickType", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWebUIBridgeScriptApiTest, "WebUIBridge.Protocol.ScriptApi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWebUIBridgeUnifiedMessageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	WebUIBridgeProtocol::FBridgeMessage Message;
	const FString Input = WebUIBridgeProtocol::GetConsolePrefix() + TEXT(R"({"type":"InventoryUpdate","data":{"page":3,"items":[{"id":1,"count":2}]}})");
	TestTrue(TEXT("Unified protocol message should parse"), WebUIBridgeProtocol::TryParseConsoleMessage(Input, Message));
	TestEqual(TEXT("Type should match"), Message.Type, TEXT("InventoryUpdate"));

	TSharedPtr<FJsonObject> DataObject;
	TestTrue(TEXT("Data json should deserialize"), DeserializeJsonObject(Message.DataJson, DataObject));
	int32 Page = 0;
	TestTrue(TEXT("Data.page should exist"), DataObject->TryGetNumberField(TEXT("page"), Page));
	TestEqual(TEXT("Data.page should match"), Page, 3);

	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	TestTrue(TEXT("Data.items should exist"), DataObject->TryGetArrayField(TEXT("items"), Items));
	TestEqual(TEXT("Data.items count should match"), Items->Num(), 1);

	return true;
}

bool FWebUIBridgeGenericClickTypeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	WebUIBridgeProtocol::FBridgeMessage Message;
	const FString Input = WebUIBridgeProtocol::GetConsolePrefix() + TEXT(R"({"type":"click","data":{"id":"StartButton","tag":"button","text":"Start","href":"","event":"StartGame"}})");
	TestTrue(TEXT("Click message should parse"), WebUIBridgeProtocol::TryParseConsoleMessage(Input, Message));
	TestEqual(TEXT("Click message should remain a normal type"), Message.Type, TEXT("click"));

	TSharedPtr<FJsonObject> DataObject;
	TestTrue(TEXT("Click data should deserialize"), DeserializeJsonObject(Message.DataJson, DataObject));
	FString Text;
	TestTrue(TEXT("Click text should exist"), DataObject->TryGetStringField(TEXT("text"), Text));
	TestEqual(TEXT("Click text should match"), Text, TEXT("Start"));
	return true;
}

bool FWebUIBridgeScriptApiTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString Script = WebUIBridgeProtocol::BuildBridgeScript(TEXT("ueBridge"));
	TestTrue(TEXT("Script should expose emit API"), Script.Contains(TEXT(".emit = function(message)")));
	TestTrue(TEXT("Script should expose sendEvent API"), Script.Contains(TEXT(".sendEvent = function(type, data)")));
	TestTrue(TEXT("Script should emit BridgeReady"), Script.Contains(TEXT("emitToUE('BridgeReady'")));
	TestTrue(TEXT("Script should use unified data field"), Script.Contains(TEXT("data: data === undefined ? {} : data")));
	return true;
}

#endif
