#pragma once

#include "CoreMinimal.h"

namespace WebUIBridgeProtocol
{
struct FBridgeMessage
{
	FString Type;
	FString DataJson = TEXT("{}");
};

FString BuildBridgeScript(const FString& InObjectName);
FString GetConsolePrefix();
bool TryParseConsoleMessage(const FString& Message, FBridgeMessage& OutMessage);
}
