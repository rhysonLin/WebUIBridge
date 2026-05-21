#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Marker/WebUIMarkerActor.h"
#include "WebUIMarkerSubsystem.generated.h"

class UWebUIMarkerStyleDataAsset;

UCLASS()
class WEBUIBRIDGE_API UWebUIMarkerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	AWebUIMarkerActor* AddMarkerByWorldLocation(
		FName MarkerId,
		const FText& LabelText,
		FVector WorldLocation,
		const FWebUIMarkerStyle& Style
	);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	AWebUIMarkerActor* AddMarkerByWorldLocationWithStyleAsset(
		FName MarkerId,
		const FText& LabelText,
		FVector WorldLocation,
		UWebUIMarkerStyleDataAsset* StyleAsset
	);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	AWebUIMarkerActor* AddSimpleMarkerByWorldLocation(
		FName MarkerId,
		const FText& LabelText,
		FVector WorldLocation
	);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	AWebUIMarkerActor* AddMarkerByLongitudeLatitudeHeight(
		FName MarkerId,
		const FText& LabelText,
		double Longitude,
		double Latitude,
		double Height,
		const FWebUIMarkerStyle& Style
	);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	AWebUIMarkerActor* AddMarkerByLongitudeLatitudeHeightWithStyleAsset(
		FName MarkerId,
		const FText& LabelText,
		double Longitude,
		double Latitude,
		double Height,
		UWebUIMarkerStyleDataAsset* StyleAsset
	);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	AWebUIMarkerActor* AddSimpleMarkerByLongitudeLatitudeHeight(
		FName MarkerId,
		const FText& LabelText,
		double Longitude,
		double Latitude,
		double Height
	);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	bool RemoveMarker(FName MarkerId);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	void ClearMarkers();

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	AWebUIMarkerActor* GetMarker(FName MarkerId) const;

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	TArray<AWebUIMarkerActor*> GetAllMarkers() const;

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	bool UpdateMarkerText(FName MarkerId, const FText& NewText);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	bool UpdateMarkerStyle(FName MarkerId, const FWebUIMarkerStyle& NewStyle);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	bool UpdateMarkerWorldLocation(FName MarkerId, FVector NewWorldLocation);

private:
	AWebUIMarkerActor* SpawnMarkerActor(const FWebUIMarkerData& MarkerData);

	FWebUIMarkerStyle GetDefaultStyle() const;

private:
	UPROPERTY()
	TMap<FName, TObjectPtr<AWebUIMarkerActor>> MarkerMap;
};