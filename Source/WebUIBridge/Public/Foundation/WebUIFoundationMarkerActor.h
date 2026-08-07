#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WebUIFoundationMarkerActor.generated.h"

class UWidgetComponent;
class UWebUIFoundationMarkerWidget;
class UTexture2D;

UCLASS()
class WEBUIBRIDGE_API AWebUIFoundationMarkerActor : public AActor
{
	GENERATED_BODY()

public:
	AWebUIFoundationMarkerActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetupMarker(
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
		const FString& InSpaceMode,
		const FString& InLayoutStyle,
		const FString& InVisualType,
		bool bInShowLabel,
		const FString& InPointText,
		const FString& InImagePath
	);

	const FString& GetMarkerId() const { return MarkerId; }
	const FString& GetMarkerLabel() const { return MarkerLabel; }
	const FLinearColor& GetMarkerColor() const { return MarkerColor; }
	const FLinearColor& GetLabelColor() const { return LabelColor; }
	const FLinearColor& GetImageTintColor() const { return ImageTintColor; }
	const FLinearColor& GetBackgroundColor() const { return BackgroundColor; }
	float GetBackgroundOpacity() const { return BackgroundOpacity; }
	float GetCornerRadius() const { return CornerRadius; }
	float GetPaddingX() const { return PaddingX; }
	float GetPaddingY() const { return PaddingY; }
	float GetIconTextGap() const { return IconTextGap; }
	float GetMarkerSize() const { return MarkerSize; }
	float GetVisualSize() const { return VisualSize; }
	float GetLabelFontSize() const { return LabelFontSize; }
	const FString& GetSpaceMode() const { return SpaceMode; }
	const FString& GetLayoutStyle() const { return LayoutStyle; }
	const FString& GetVisualType() const { return VisualType; }
	bool GetShowLabel() const { return bShowLabel; }
	const FString& GetPointText() const { return PointText; }
	const FString& GetImagePath() const { return ImagePath; }

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIFoundation|Marker")
	bool bFaceCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIFoundation|Marker")
	bool bLockBillboardPitch;

private:
	UPROPERTY(VisibleAnywhere)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere)
	UWidgetComponent* MarkerWidgetComponent;

	// 对甲方默认楼栋图标建立硬引用，确保动态字符串加载时该资源仍会进入 Cook。
	UPROPERTY()
	UTexture2D* DefaultBuildingMarkerTexture;

	UPROPERTY()
	FString MarkerId;

	UPROPERTY()
	FString MarkerLabel;

	UPROPERTY()
	FLinearColor MarkerColor;

	UPROPERTY()
	FLinearColor LabelColor;

	UPROPERTY()
	FLinearColor ImageTintColor;

	UPROPERTY()
	FLinearColor BackgroundColor;

	UPROPERTY()
	float BackgroundOpacity;

	UPROPERTY()
	float CornerRadius;

	UPROPERTY()
	float PaddingX;

	UPROPERTY()
	float PaddingY;

	UPROPERTY()
	float IconTextGap;

	UPROPERTY()
	float MarkerSize;

	UPROPERTY()
	float VisualSize;

	UPROPERTY()
	float LabelFontSize;

	UPROPERTY()
	FString SpaceMode;

	UPROPERTY()
	FString LayoutStyle;

	UPROPERTY()
	FString VisualType;

	UPROPERTY()
	bool bShowLabel;

	UPROPERTY()
	FString PointText;

	UPROPERTY()
	FString ImagePath;

private:
	void FacePlayerCamera();
	void RefreshWidget();
};
