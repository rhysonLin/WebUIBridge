#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Marker/WebUIMarkerTypes.h"
#include "WebUIMarkerWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class WEBUIBRIDGE_API UWebUIMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "WebUIBridge|Marker")
	void SetMarkerVisual(const FText& InLabelText, const FWebUIMarkerStyle& InStyle);

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;

private:
	void BuildDefaultWidget();
	void ApplyVisual();

private:
	UPROPERTY()
	TObjectPtr<UBorder> RootBorder;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> RowBox;

	UPROPERTY()
	TObjectPtr<UImage> IconImage;

	UPROPERTY()
	TObjectPtr<UTextBlock> LabelTextBlock;

	UPROPERTY()
	FText CachedLabelText;

	UPROPERTY()
	FWebUIMarkerStyle CachedStyle;
};