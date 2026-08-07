#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WebUIFoundationMarkerWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

UCLASS()
class WEBUIBRIDGE_API UWebUIFoundationMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "WebUIFoundation|Marker")
	void SetMarkerData(
		const FString& InMarkerId,
		const FString& InLabel,
		const FLinearColor& InMarkerColor,
		const FLinearColor& InLabelColor,
		const FLinearColor& InImageTintColor,
		const FLinearColor& InBackgroundColor,
		float InBackgroundOpacity,
		float InCornerRadius,
		float InPaddingX,
		float InPaddingY,
		float InIconTextGap,
		float InSize,
		float InVisualSize,
		float InLabelFontSize,
		const FString& InLayoutStyle,
		const FString& InVisualType,
		bool bInShowLabel,
		const FString& InPointText,
		const FString& InImagePath
	);

	// 设置由 MarkerActor 持有的默认图标硬引用。
	// 该引用用于保证打包 Cook，并在动态加载默认路径失败时兜底。
	void SetFallbackImageTexture(UTexture2D* InTexture);

	// 返回 WidgetComponent 应使用的归一化 Pivot。
	// classic 模式锚定点/图片中心；badge 模式锚定左侧图标中心，
	// 确保楼层世界坐标不会落在“图标 + 文字”整体中心。
	FVector2D GetVisualAnchorPivot() const;

private:
	UPROPERTY()
	UOverlay* RootOverlay;

	// 经典布局：图标在上，文字在下。
	UPROPERTY()
	UVerticalBox* ClassicRootBox;

	UPROPERTY()
	UTextBlock* ClassicIconText;

	UPROPERTY()
	UImage* ClassicIconImage;

	UPROPERTY()
	UTextBlock* ClassicLabelText;

	// 徽标布局：图标在左，文字在右，后方有底板。
	UPROPERTY()
	UBorder* BadgeBackground;

	UPROPERTY()
	UHorizontalBox* BadgeRow;

	UPROPERTY()
	USizeBox* BadgeVisualSizeBox;

	UPROPERTY()
	UOverlay* BadgeVisualOverlay;

	UPROPERTY()
	UTextBlock* BadgeIconText;

	UPROPERTY()
	UImage* BadgeIconImage;

	UPROPERTY()
	UTextBlock* BadgeLabelText;

	UPROPERTY()
	UTexture2D* LoadedImageTexture;

	UPROPERTY()
	UTexture2D* FallbackImageTexture;

	UPROPERTY()
	FString MarkerId;

	UPROPERTY()
	FString LayoutStyle;

	float CurrentPaddingX;
	float CurrentVisualSize;
	bool bCurrentVisualVisible;

private:
	void BuildWidgetTreeIfNeeded();
	UTexture2D* LoadMarkerTexture(const FString& ImagePath);
};
