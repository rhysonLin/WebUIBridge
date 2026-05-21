#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Marker/WebUIMarkerTypes.h"
#include "WebUIMarkerActor.generated.h"

class USphereComponent;
class UWidgetComponent;

UCLASS()
class WEBUIBRIDGE_API AWebUIMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	AWebUIMarkerActor();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	void InitMarker(const FWebUIMarkerData& InData);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	void SetLabelText(const FText& InText);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	void SetMarkerStyle(const FWebUIMarkerStyle& InStyle);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	void SetMarkerWorldLocation(FVector InWorldLocation);

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	FName GetMarkerId() const;

	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	const FWebUIMarkerData& GetMarkerData() const;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateVisual();
	void FaceToCamera();

private:
	UPROPERTY(VisibleAnywhere, Category = "WebUIBridge|Marker")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "WebUIBridge|Marker")
	TObjectPtr<UWidgetComponent> MarkerWidgetComponent;

	UPROPERTY(VisibleAnywhere, Category = "WebUIBridge|Marker")
	TObjectPtr<USphereComponent> ClickComponent;

	UPROPERTY(EditAnywhere, Category = "WebUIBridge|Marker")
	bool bDebugClickRange = false;

	UPROPERTY()
	FWebUIMarkerData MarkerData;
};