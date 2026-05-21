#include "Marker/WebUIMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"

void UWebUIMarkerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildDefaultWidget();
	ApplyVisual();
}

void UWebUIMarkerWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	BuildDefaultWidget();
	ApplyVisual();
}

void UWebUIMarkerWidget::SetMarkerVisual(const FText& InLabelText, const FWebUIMarkerStyle& InStyle)
{
	CachedLabelText = InLabelText;
	CachedStyle = InStyle;

	BuildDefaultWidget();
	ApplyVisual();
}

void UWebUIMarkerWidget::BuildDefaultWidget()
{
	if (!WidgetTree)
	{
		return;
	}

	if (RootBorder && RowBox && IconImage && LabelTextBlock)
	{
		return;
	}

	RootBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("MarkerRootBorder")
	);

	RowBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(),
		TEXT("MarkerRowBox")
	);

	IconImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		TEXT("MarkerIconImage")
	);

	LabelTextBlock = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		TEXT("MarkerLabelText")
	);

	if (!RootBorder || !RowBox || !IconImage || !LabelTextBlock)
	{
		return;
	}

	WidgetTree->RootWidget = RootBorder;

	RootBorder->SetContent(RowBox);

	UHorizontalBoxSlot* IconSlot = RowBox->AddChildToHorizontalBox(IconImage);
	if (IconSlot)
	{
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetHorizontalAlignment(HAlign_Center);
	}

	UHorizontalBoxSlot* TextSlot = RowBox->AddChildToHorizontalBox(LabelTextBlock);
	if (TextSlot)
	{
		TextSlot->SetVerticalAlignment(VAlign_Center);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
	}

	LabelTextBlock->SetJustification(ETextJustify::Center);
	LabelTextBlock->SetText(FText::FromString(TEXT("Marker")));
}

void UWebUIMarkerWidget::ApplyVisual()
{
	if (!RootBorder || !RowBox || !IconImage || !LabelTextBlock)
	{
		return;
	}

	const FWebUIMarkerStyle& Style = CachedStyle;

	// 背景
	RootBorder->SetVisibility(Style.bShowBackground ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible);
	RootBorder->SetBrushColor(Style.bShowBackground ? Style.BackgroundColor : FLinearColor::Transparent);
	RootBorder->SetPadding(Style.PanelPadding);

	// 图标
	if (Style.bShowIcon && Style.IconTexture)
	{
		IconImage->SetVisibility(ESlateVisibility::Visible);
		IconImage->SetBrushFromTexture(Style.IconTexture, true);
		IconImage->SetDesiredSizeOverride(Style.IconSize);
		IconImage->SetBrushTintColor(FSlateColor(Style.IconTint));
	}
	else
	{
		IconImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(IconImage->Slot))
	{
		IconSlot->SetPadding(Style.IconPadding);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// 文字
	if (Style.bShowText)
	{
		LabelTextBlock->SetVisibility(ESlateVisibility::Visible);

		const FText FinalText = CachedLabelText.IsEmpty()
			? FText::FromString(TEXT("Marker"))
			: CachedLabelText;

		LabelTextBlock->SetText(FinalText);
		LabelTextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(Style.TextColor)));
		LabelTextBlock->SetAutoWrapText(Style.bAutoWrapText);

		const int32 FontSize = FMath::Max(1, FMath::RoundToInt(Style.TextWorldSize));

		if (Style.TextFont)
		{
			LabelTextBlock->SetFont(FSlateFontInfo(Style.TextFont, FontSize));
		}
		else
		{
			FSlateFontInfo CurrentFont = LabelTextBlock->GetFont();
			CurrentFont.Size = FontSize;
			LabelTextBlock->SetFont(CurrentFont);
		}
	}
	else
	{
		LabelTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
}