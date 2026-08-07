#include "Foundation/WebUIFoundationMarkerActor.h"

#include "Foundation/WebUIFoundationMarkerWidget.h"

#include "Components/WidgetComponent.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AWebUIFoundationMarkerActor::AWebUIFoundationMarkerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	bFaceCamera = true;
	bLockBillboardPitch = false;

	MarkerId = TEXT("");
	MarkerLabel = TEXT("项目点位");
	MarkerColor = FLinearColor(0.0f, 0.65f, 1.0f, 1.0f);
	LabelColor = FLinearColor::White;
	ImageTintColor = FLinearColor::White;
	BackgroundColor = FLinearColor(0.16f, 0.17f, 0.19f, 1.0f);
	BackgroundOpacity = 0.88f;
	CornerRadius = 8.0f;
	PaddingX = 10.0f;
	PaddingY = 6.0f;
	IconTextGap = 8.0f;
	MarkerSize = 160.0f;
	VisualSize = 56.0f;
	LabelFontSize = 24.0f;
	SpaceMode = TEXT("world");
	LayoutStyle = TEXT("classic");
	VisualType = TEXT("point");
	bShowLabel = true;
	PointText = TEXT("●");
	ImagePath = TEXT("");

	// 使用构造期硬引用让 Cooker 能追踪到前端通过字符串指定的默认甲方楼栋图标。
	static ConstructorHelpers::FObjectFinder<UTexture2D> DefaultBuildingIconFinder(
		TEXT("/Game/WebUIFoundation/Markers/T_Marker_Building_White.T_Marker_Building_White")
	);
	DefaultBuildingMarkerTexture = DefaultBuildingIconFinder.Succeeded()
		? DefaultBuildingIconFinder.Object
		: nullptr;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MarkerWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MarkerWidget"));
	MarkerWidgetComponent->SetupAttachment(Root);
	MarkerWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	MarkerWidgetComponent->SetTwoSided(false);
	MarkerWidgetComponent->SetDrawAtDesiredSize(true);
	MarkerWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MarkerWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
	MarkerWidgetComponent->SetDrawSize(FVector2D(512.0f, 256.0f));
	MarkerWidgetComponent->SetWidgetClass(UWebUIFoundationMarkerWidget::StaticClass());
	MarkerWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
}

void AWebUIFoundationMarkerActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshWidget();
}

void AWebUIFoundationMarkerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFaceCamera && SpaceMode != TEXT("screen"))
	{
		FacePlayerCamera();
	}
}

void AWebUIFoundationMarkerActor::SetupMarker(
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
)
{
	MarkerId = InMarkerId;
	MarkerLabel = InLabel.IsEmpty() ? InMarkerId : InLabel;

	MarkerColor = InMarkerColor;
	MarkerColor.A = FMath::Clamp(MarkerColor.A, 0.05f, 1.0f);

	LabelColor = InLabelColor;
	LabelColor.A = FMath::Clamp(LabelColor.A, 0.05f, 1.0f);

	ImageTintColor = InImageTintColor;
	ImageTintColor.A = FMath::Clamp(ImageTintColor.A, 0.05f, 1.0f);

	BackgroundColor = InBackgroundColor;
	BackgroundColor.A = 1.0f;
	BackgroundOpacity = FMath::Clamp(InBackgroundOpacity, 0.0f, 1.0f);
	CornerRadius = FMath::Clamp(InCornerRadius, 0.0f, 64.0f);
	PaddingX = FMath::Clamp(InPaddingX, 0.0f, 64.0f);
	PaddingY = FMath::Clamp(InPaddingY, 0.0f, 64.0f);
	IconTextGap = FMath::Clamp(InIconTextGap, 0.0f, 64.0f);

	MarkerSize = FMath::Clamp(InSize, 20.0f, 1200.0f);
	VisualSize = FMath::Clamp(InVisualSize, 8.0f, 512.0f);
	LabelFontSize = FMath::Clamp(InLabelFontSize, 8.0f, 128.0f);

	SpaceMode = InSpaceMode.TrimStartAndEnd().ToLower();
	if (SpaceMode == TEXT("screenfixed") || SpaceMode == TEXT("fixed") || SpaceMode == TEXT("viewport"))
	{
		SpaceMode = TEXT("screen");
	}
	if (SpaceMode != TEXT("screen"))
	{
		SpaceMode = TEXT("world");
	}

	LayoutStyle = InLayoutStyle.TrimStartAndEnd().ToLower();
	if (LayoutStyle == TEXT("card") || LayoutStyle == TEXT("horizontal") || LayoutStyle == TEXT("pill"))
	{
		LayoutStyle = TEXT("badge");
	}
	if (LayoutStyle != TEXT("badge"))
	{
		LayoutStyle = TEXT("classic");
	}

	VisualType = InVisualType.TrimStartAndEnd().ToLower();
	if (VisualType != TEXT("point") && VisualType != TEXT("image") && VisualType != TEXT("none"))
	{
		VisualType = TEXT("point");
	}

	bShowLabel = bInShowLabel;
	PointText = InPointText.IsEmpty() ? TEXT("●") : InPointText;
	ImagePath = InImagePath.TrimStartAndEnd();

	const bool bUseScreenSpace = SpaceMode == TEXT("screen");
	MarkerWidgetComponent->SetWidgetSpace(bUseScreenSpace ? EWidgetSpace::Screen : EWidgetSpace::World);

	if (bUseScreenSpace)
	{
		MarkerWidgetComponent->SetWorldScale3D(FVector::OneVector);
		MarkerWidgetComponent->SetRelativeLocation(FVector::ZeroVector);
		MarkerWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	}
	else
	{
		const float WidgetScale = MarkerSize / 360.0f;
		MarkerWidgetComponent->SetWorldScale3D(FVector(WidgetScale, WidgetScale, WidgetScale));
		MarkerWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, MarkerSize * 0.85f));
		MarkerWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
	}

	RefreshWidget();
}

void AWebUIFoundationMarkerActor::RefreshWidget()
{
	if (!MarkerWidgetComponent)
	{
		return;
	}

	MarkerWidgetComponent->InitWidget();

	UWebUIFoundationMarkerWidget* MarkerWidget =
		Cast<UWebUIFoundationMarkerWidget>(MarkerWidgetComponent->GetWidget());

	if (!MarkerWidget)
	{
		return;
	}

	MarkerWidget->SetFallbackImageTexture(DefaultBuildingMarkerTexture);

	MarkerWidget->SetMarkerData(
		MarkerId,
		MarkerLabel,
		MarkerColor,
		LabelColor,
		ImageTintColor,
		BackgroundColor,
		BackgroundOpacity,
		CornerRadius,
		PaddingX,
		PaddingY,
		IconTextGap,
		MarkerSize,
		VisualSize,
		LabelFontSize,
		LayoutStyle,
		VisualType,
		bShowLabel,
		PointText,
		ImagePath
	);

	if (SpaceMode == TEXT("screen"))
	{
		// 强制刷新 DesiredSize 后，以实际视觉锚点设置 Pivot。
		// badge 模式锚定左侧图标中心；classic 模式锚定点/图片中心。
		MarkerWidget->ForceLayoutPrepass();
		MarkerWidgetComponent->SetPivot(MarkerWidget->GetVisualAnchorPivot());
	}
}

void AWebUIFoundationMarkerActor::FacePlayerCamera()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector MarkerLocation = MarkerWidgetComponent
		? MarkerWidgetComponent->GetComponentLocation()
		: GetActorLocation();

	FVector Direction = CameraLocation - MarkerLocation;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	if (bLockBillboardPitch)
	{
		Direction.Z = 0.0f;
	}

	if (!Direction.Normalize())
	{
		return;
	}

	const FRotator LookAtRotation = Direction.Rotation();
	SetActorRotation(FRotator(
		bLockBillboardPitch ? 0.0f : LookAtRotation.Pitch,
		LookAtRotation.Yaw,
		0.0f
	));
}
