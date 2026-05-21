#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "WebUIMarkerTypes.generated.h"

class UFont;
class UTexture2D;
class UWebUIMarkerWidget;

UENUM(BlueprintType)
enum class EWebUIMarkerLocationType : uint8
{
	WorldLocation UMETA(DisplayName = "World Location"),
	LongitudeLatitudeHeight UMETA(DisplayName = "Longitude Latitude Height")
};

USTRUCT(BlueprintType)
struct FWebUIMarkerStyle
{
	GENERATED_BODY()

	// =========================
	// Widget 面板整体
	// =========================

	// 自定义 Widget 类。为空时使用默认的 UWebUIMarkerWidget。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Widget")
	TSubclassOf<UWebUIMarkerWidget> WidgetClass = nullptr;

	// Widget 绘制尺寸，单位类似像素。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Widget")
	FVector2D WidgetDrawSize = FVector2D(360.0f, 96.0f);

	// Widget 世界缩放。觉得太大/太小就调这个。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Widget")
	float WidgetWorldScale = 1.0f;

	// 整个标签距离点位的高度偏移。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Widget")
	float TextHeightOffset = 180.0f;

	// 是否根据内容自适应大小。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Widget")
	bool bDrawAtDesiredSize = true;

	// 是否双面显示。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Widget")
	bool bTwoSided = true;

	// 是否始终朝向摄像机。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Widget")
	bool bFaceToCamera = true;

	// =========================
	// 背景面板
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Panel")
	bool bShowBackground = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Panel")
	FLinearColor BackgroundColor = FLinearColor(0.02f, 0.02f, 0.02f, 0.72f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Panel")
	FMargin PanelPadding = FMargin(12.0f, 8.0f, 14.0f, 8.0f);

	// =========================
	// 图标
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	bool bShowIcon = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	TObjectPtr<UTexture2D> IconTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	FVector2D IconSize = FVector2D(48.0f, 48.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	FLinearColor IconTint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	FMargin IconPadding = FMargin(0.0f, 0.0f, 10.0f, 0.0f);

	// 兼容旧字段：现在 Widget 版本里基本不用这个。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	float IconHeightOffset = 0.0f;

	// 兼容旧字段：Widget 版本里不再使用 Billboard 的屏幕缩放。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	bool bScreenSizeScaledIcon = false;

	// 兼容旧字段。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Icon")
	float IconScreenSize = 0.002f;

	// =========================
	// 文字
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Text")
	bool bShowText = true;

	// 中文字体。可以为空；如果默认字体仍显示方框，就指定一个支持中文的 UFont。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Text")
	TObjectPtr<UFont> TextFont = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Text")
	FColor TextColor = FColor::White;

	// 兼容旧字段：在 Widget 版本中表示字体大小。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Text")
	float TextWorldSize = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Text")
	bool bAutoWrapText = false;

	// =========================
	// 点击检测
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Collision")
	bool bEnableClickCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker|Collision")
	float ClickRadius = 120.0f;
};

USTRUCT(BlueprintType)
struct FWebUIMarkerData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	FName MarkerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	FText LabelText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	EWebUIMarkerLocationType LocationType = EWebUIMarkerLocationType::WorldLocation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	double Longitude = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	double Latitude = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	double Height = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WebUIBridge|Marker")
	FWebUIMarkerStyle Style;
};