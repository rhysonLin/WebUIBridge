#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Marker/WebUIMarkerTypes.h"
#include "WebUIMarkerStyleDataAsset.generated.h"

UCLASS(BlueprintType)
class WEBUIBRIDGE_API UWebUIMarkerStyleDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WebUIBridge|Marker|Style")
	FWebUIMarkerStyle Style;
};