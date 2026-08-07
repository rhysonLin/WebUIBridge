#include "Foundation/WebUIFoundationMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "UObject/UObjectGlobals.h"

void UWebUIFoundationMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildWidgetTreeIfNeeded();
}


void UWebUIFoundationMarkerWidget::SetFallbackImageTexture(UTexture2D* InTexture)
{
	FallbackImageTexture = InTexture;
}

void UWebUIFoundationMarkerWidget::BuildWidgetTreeIfNeeded()
{
	if (!WidgetTree || RootOverlay)
	{
		return;
	}

	CurrentPaddingX = 10.0f;
	CurrentVisualSize = 24.0f;
	bCurrentVisualVisible = true;
	LayoutStyle = TEXT("classic");

	RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	RootOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = RootOverlay;

	// =========================
	// 经典布局
	// =========================
	ClassicRootBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ClassicRootBox"));
	ClassicRootBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* ClassicSlot = RootOverlay->AddChildToOverlay(ClassicRootBox))
	{
		ClassicSlot->SetHorizontalAlignment(HAlign_Center);
		ClassicSlot->SetVerticalAlignment(VAlign_Center);
	}

	ClassicIconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClassicIconText"));
	ClassicIconText->SetText(FText::FromString(TEXT("●")));
	ClassicIconText->SetColorAndOpacity(FSlateColor(FLinearColor(0.0f, 0.75f, 1.0f, 1.0f)));
	ClassicIconText->SetJustification(ETextJustify::Center);
	ClassicIconText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	ClassicIconText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));

	if (UVerticalBoxSlot* IconTextSlot = ClassicRootBox->AddChildToVerticalBox(ClassicIconText))
	{
		IconTextSlot->SetHorizontalAlignment(HAlign_Center);
		IconTextSlot->SetVerticalAlignment(VAlign_Center);
		IconTextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	ClassicIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ClassicIconImage"));
	ClassicIconImage->SetVisibility(ESlateVisibility::Collapsed);

	if (UVerticalBoxSlot* IconImageSlot = ClassicRootBox->AddChildToVerticalBox(ClassicIconImage))
	{
		IconImageSlot->SetHorizontalAlignment(HAlign_Center);
		IconImageSlot->SetVerticalAlignment(VAlign_Center);
		IconImageSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	ClassicLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ClassicLabelText"));
	ClassicLabelText->SetText(FText::FromString(TEXT("项目点位")));
	ClassicLabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ClassicLabelText->SetJustification(ETextJustify::Center);
	ClassicLabelText->SetAutoWrapText(false);
	ClassicLabelText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	ClassicLabelText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));

	if (UVerticalBoxSlot* LabelSlot = ClassicRootBox->AddChildToVerticalBox(ClassicLabelText))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
	}

	// =========================
	// Badge 布局：图标左、文字右、底板
	// =========================
	BadgeBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BadgeBackground"));
	BadgeBackground->SetVisibility(ESlateVisibility::Collapsed);
	BadgeBackground->SetHorizontalAlignment(HAlign_Fill);
	BadgeBackground->SetVerticalAlignment(VAlign_Fill);
	BadgeBackground->SetPadding(FMargin(10.0f, 6.0f));

	if (UOverlaySlot* BadgeSlot = RootOverlay->AddChildToOverlay(BadgeBackground))
	{
		BadgeSlot->SetHorizontalAlignment(HAlign_Center);
		BadgeSlot->SetVerticalAlignment(VAlign_Center);
	}

	BadgeRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BadgeRow"));
	BadgeRow->SetVisibility(ESlateVisibility::HitTestInvisible);
	BadgeBackground->SetContent(BadgeRow);

	BadgeVisualSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BadgeVisualSizeBox"));
	BadgeVisualSizeBox->SetWidthOverride(24.0f);
	BadgeVisualSizeBox->SetHeightOverride(24.0f);

	if (UHorizontalBoxSlot* VisualBoxSlot = BadgeRow->AddChildToHorizontalBox(BadgeVisualSizeBox))
	{
		VisualBoxSlot->SetHorizontalAlignment(HAlign_Center);
		VisualBoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	BadgeVisualOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("BadgeVisualOverlay"));
	BadgeVisualOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	BadgeVisualSizeBox->SetContent(BadgeVisualOverlay);

	BadgeIconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BadgeIconText"));
	BadgeIconText->SetText(FText::FromString(TEXT("●")));
	BadgeIconText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	BadgeIconText->SetJustification(ETextJustify::Center);
	BadgeIconText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	BadgeIconText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));

	if (UOverlaySlot* BadgeIconTextSlot = BadgeVisualOverlay->AddChildToOverlay(BadgeIconText))
	{
		BadgeIconTextSlot->SetHorizontalAlignment(HAlign_Center);
		BadgeIconTextSlot->SetVerticalAlignment(VAlign_Center);
	}

	BadgeIconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BadgeIconImage"));
	BadgeIconImage->SetVisibility(ESlateVisibility::Collapsed);

	if (UOverlaySlot* BadgeIconImageSlot = BadgeVisualOverlay->AddChildToOverlay(BadgeIconImage))
	{
		BadgeIconImageSlot->SetHorizontalAlignment(HAlign_Center);
		BadgeIconImageSlot->SetVerticalAlignment(VAlign_Center);
	}

	BadgeLabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BadgeLabelText"));
	BadgeLabelText->SetText(FText::FromString(TEXT("2号楼")));
	BadgeLabelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	BadgeLabelText->SetJustification(ETextJustify::Left);
	BadgeLabelText->SetAutoWrapText(false);
	BadgeLabelText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	BadgeLabelText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));

	if (UHorizontalBoxSlot* BadgeLabelSlot = BadgeRow->AddChildToHorizontalBox(BadgeLabelText))
	{
		BadgeLabelSlot->SetHorizontalAlignment(HAlign_Left);
		BadgeLabelSlot->SetVerticalAlignment(VAlign_Center);
		BadgeLabelSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	}
}

void UWebUIFoundationMarkerWidget::SetMarkerData(
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
)
{
	BuildWidgetTreeIfNeeded();
	MarkerId = InMarkerId;

	const float SafeVisualSize = FMath::Clamp(InVisualSize, 8.0f, 512.0f);
	const float SafeLabelFontSize = FMath::Clamp(InLabelFontSize, 8.0f, 128.0f);
	const float SafeBackgroundOpacity = FMath::Clamp(InBackgroundOpacity, 0.0f, 1.0f);
	const float SafeCornerRadius = FMath::Clamp(InCornerRadius, 0.0f, 64.0f);
	const float SafePaddingX = FMath::Clamp(InPaddingX, 0.0f, 64.0f);
	const float SafePaddingY = FMath::Clamp(InPaddingY, 0.0f, 64.0f);
	const float SafeIconTextGap = FMath::Clamp(InIconTextGap, 0.0f, 64.0f);
	const FString VisualType = InVisualType.TrimStartAndEnd().ToLower();

	LayoutStyle = InLayoutStyle.TrimStartAndEnd().ToLower();
	if (LayoutStyle == TEXT("card") || LayoutStyle == TEXT("horizontal") || LayoutStyle == TEXT("pill"))
	{
		LayoutStyle = TEXT("badge");
	}
	if (LayoutStyle != TEXT("badge"))
	{
		LayoutStyle = TEXT("classic");
	}

	CurrentPaddingX = SafePaddingX;
	CurrentVisualSize = SafeVisualSize;
	bCurrentVisualVisible = VisualType != TEXT("none");

	if (ClassicRootBox)
	{
		ClassicRootBox->SetVisibility(LayoutStyle == TEXT("classic")
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
	if (BadgeBackground)
	{
		BadgeBackground->SetVisibility(LayoutStyle == TEXT("badge")
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// 图片只加载一次，经典和 Badge 两种布局共用同一个 Texture2D。
	bool bImageLoaded = false;
	if (VisualType == TEXT("image") && !InImagePath.IsEmpty())
	{
		LoadedImageTexture = LoadMarkerTexture(InImagePath);
		bImageLoaded = LoadedImageTexture != nullptr;
	}

	const bool bShowPoint = VisualType == TEXT("point") || (VisualType == TEXT("image") && !bImageLoaded);
	const FString SafePointText = InPointText.IsEmpty() ? TEXT("●") : InPointText;
	const FString SafeLabel = InLabel.IsEmpty() ? InMarkerId : InLabel;

	// 经典布局视觉。
	if (ClassicIconText)
	{
		ClassicIconText->SetVisibility(bShowPoint ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ClassicIconText->SetText(FText::FromString(SafePointText));
		ClassicIconText->SetColorAndOpacity(FSlateColor(InMarkerColor));
		FSlateFontInfo FontInfo = ClassicIconText->GetFont();
		FontInfo.Size = FMath::RoundToInt(SafeVisualSize);
		ClassicIconText->SetFont(FontInfo);
	}

	if (ClassicIconImage)
	{
		if (bImageLoaded)
		{
			ClassicIconImage->SetBrushFromTexture(LoadedImageTexture, false);
			FSlateBrush Brush = ClassicIconImage->GetBrush();
			Brush.ImageSize = FVector2D(SafeVisualSize, SafeVisualSize);
			ClassicIconImage->SetBrush(Brush);
			ClassicIconImage->SetColorAndOpacity(InImageTintColor);
			ClassicIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ClassicIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ClassicLabelText)
	{
		ClassicLabelText->SetVisibility(bInShowLabel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		ClassicLabelText->SetText(FText::FromString(SafeLabel));
		ClassicLabelText->SetColorAndOpacity(FSlateColor(InLabelColor));
		FSlateFontInfo FontInfo = ClassicLabelText->GetFont();
		FontInfo.Size = FMath::RoundToInt(SafeLabelFontSize);
		ClassicLabelText->SetFont(FontInfo);
	}

	// Badge 底板。
	if (BadgeBackground)
	{
		FLinearColor EffectiveBackgroundColor = InBackgroundColor;
		EffectiveBackgroundColor.A = SafeBackgroundOpacity;

		FSlateBrush BackgroundBrush;
		BackgroundBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
		BackgroundBrush.TintColor = FSlateColor(EffectiveBackgroundColor);
		BackgroundBrush.OutlineSettings.CornerRadii = FVector4(
			SafeCornerRadius,
			SafeCornerRadius,
			SafeCornerRadius,
			SafeCornerRadius
		);
		BackgroundBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		BadgeBackground->SetBrush(BackgroundBrush);
		BadgeBackground->SetPadding(FMargin(SafePaddingX, SafePaddingY));
	}

	if (BadgeVisualSizeBox)
	{
		BadgeVisualSizeBox->SetWidthOverride(SafeVisualSize);
		BadgeVisualSizeBox->SetHeightOverride(SafeVisualSize);
		BadgeVisualSizeBox->SetVisibility(bCurrentVisualVisible
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	if (BadgeIconText)
	{
		BadgeIconText->SetVisibility(bShowPoint ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		BadgeIconText->SetText(FText::FromString(SafePointText));
		BadgeIconText->SetColorAndOpacity(FSlateColor(InMarkerColor));
		FSlateFontInfo FontInfo = BadgeIconText->GetFont();
		FontInfo.Size = FMath::RoundToInt(SafeVisualSize);
		BadgeIconText->SetFont(FontInfo);
	}

	if (BadgeIconImage)
	{
		if (bImageLoaded)
		{
			BadgeIconImage->SetBrushFromTexture(LoadedImageTexture, false);
			FSlateBrush Brush = BadgeIconImage->GetBrush();
			Brush.ImageSize = FVector2D(SafeVisualSize, SafeVisualSize);
			BadgeIconImage->SetBrush(Brush);
			BadgeIconImage->SetColorAndOpacity(InImageTintColor);
			BadgeIconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			BadgeIconImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (BadgeLabelText)
	{
		BadgeLabelText->SetVisibility(bInShowLabel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		BadgeLabelText->SetText(FText::FromString(SafeLabel));
		BadgeLabelText->SetColorAndOpacity(FSlateColor(InLabelColor));
		FSlateFontInfo FontInfo = BadgeLabelText->GetFont();
		FontInfo.Size = FMath::RoundToInt(SafeLabelFontSize);
		BadgeLabelText->SetFont(FontInfo);

		if (UHorizontalBoxSlot* BadgeLabelSlot = Cast<UHorizontalBoxSlot>(BadgeLabelText->Slot))
		{
			BadgeLabelSlot->SetPadding(FMargin(
				bCurrentVisualVisible ? SafeIconTextGap : 0.0f,
				0.0f,
				0.0f,
				0.0f
			));
		}
	}
}

FVector2D UWebUIFoundationMarkerWidget::GetVisualAnchorPivot() const
{
	if (LayoutStyle == TEXT("badge"))
	{
		if (!BadgeBackground)
		{
			return FVector2D(0.5f, 0.5f);
		}

		const FVector2D RootDesiredSize = BadgeBackground->GetDesiredSize();
		if (RootDesiredSize.X <= KINDA_SMALL_NUMBER || RootDesiredSize.Y <= KINDA_SMALL_NUMBER)
		{
			return FVector2D(0.5f, 0.5f);
		}

		// Badge 的世界锚点必须落在左侧图标中心，而不是整块底板中心。
		// 若视觉被关闭，则退回到整块 Badge 中心。
		if (!bCurrentVisualVisible)
		{
			return FVector2D(0.5f, 0.5f);
		}

		const float PivotX = FMath::Clamp(
			(CurrentPaddingX + CurrentVisualSize * 0.5f) / RootDesiredSize.X,
			0.01f,
			0.99f
		);

		return FVector2D(PivotX, 0.5f);
	}

	if (!ClassicRootBox)
	{
		return FVector2D(0.5f, 0.5f);
	}

	const FVector2D RootDesiredSize = ClassicRootBox->GetDesiredSize();
	if (RootDesiredSize.Y <= KINDA_SMALL_NUMBER)
	{
		return FVector2D(0.5f, 0.5f);
	}

	const UWidget* ActiveVisualWidget = nullptr;
	if (ClassicIconImage && ClassicIconImage->GetVisibility() != ESlateVisibility::Collapsed &&
		ClassicIconImage->GetVisibility() != ESlateVisibility::Hidden)
	{
		ActiveVisualWidget = ClassicIconImage;
	}
	else if (ClassicIconText && ClassicIconText->GetVisibility() != ESlateVisibility::Collapsed &&
		ClassicIconText->GetVisibility() != ESlateVisibility::Hidden)
	{
		ActiveVisualWidget = ClassicIconText;
	}

	if (!ActiveVisualWidget)
	{
		return FVector2D(0.5f, 0.5f);
	}

	const float VisualHeight = ActiveVisualWidget->GetDesiredSize().Y;
	if (VisualHeight <= KINDA_SMALL_NUMBER)
	{
		return FVector2D(0.5f, 0.5f);
	}

	const float PivotY = FMath::Clamp(
		(VisualHeight * 0.5f) / RootDesiredSize.Y,
		0.01f,
		0.99f
	);

	return FVector2D(0.5f, PivotY);
}

UTexture2D* UWebUIFoundationMarkerWidget::LoadMarkerTexture(const FString& ImagePath)
{
	FString NormalizedPath = ImagePath.TrimStartAndEnd();

	// 兼容 UE“复制引用”得到的：
	// /Script/Engine.Texture2D'/Game/UI/T_Room.T_Room'
	// 以及直接填写的：/Game/UI/T_Room.T_Room
	int32 FirstQuoteIndex = INDEX_NONE;
	if (NormalizedPath.FindChar(TEXT('\''), FirstQuoteIndex) && NormalizedPath.EndsWith(TEXT("'")))
	{
		NormalizedPath = NormalizedPath.Mid(FirstQuoteIndex + 1, NormalizedPath.Len() - FirstQuoteIndex - 2);
	}

	if (NormalizedPath.IsEmpty())
	{
		return nullptr;
	}

	static const FString DefaultBuildingIconPath =
		TEXT("/Game/WebUIFoundation/Markers/T_Marker_Building_White.T_Marker_Building_White");

	// 默认甲方图标由 MarkerActor 构造函数硬引用。
	// 打包时它会作为资产依赖进入 Cook；运行时优先直接使用该对象。
	if (FallbackImageTexture &&
		(NormalizedPath.Equals(DefaultBuildingIconPath, ESearchCase::IgnoreCase) ||
		 NormalizedPath.Equals(FallbackImageTexture->GetPathName(), ESearchCase::IgnoreCase)))
	{
		return FallbackImageTexture;
	}

	UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(
		UTexture2D::StaticClass(),
		nullptr,
		*NormalizedPath
	));

	if (!Texture)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[WebUIFoundation] Marker image asset could not be loaded. Path=%s MarkerId=%s"),
			*NormalizedPath,
			*MarkerId
		);
	}

	return Texture;
}
