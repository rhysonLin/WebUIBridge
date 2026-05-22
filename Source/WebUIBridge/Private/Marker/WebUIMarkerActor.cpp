#include "Marker/WebUIMarkerActor.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Marker/WebUIMarkerWidget.h"

AWebUIMarkerActor::AWebUIMarkerActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	MarkerWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("MarkerWidgetComponent"));
	MarkerWidgetComponent->SetupAttachment(Root);
	MarkerWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	MarkerWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	MarkerWidgetComponent->SetWidgetClass(UWebUIMarkerWidget::StaticClass());
	MarkerWidgetComponent->SetDrawSize(FVector2D(360.0f, 96.0f));
	MarkerWidgetComponent->SetDrawAtDesiredSize(false);
	MarkerWidgetComponent->SetTwoSided(true);
	MarkerWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));
	MarkerWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 关键：不开透明混合，半透明背景容易变成纯黑块。
	MarkerWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);

	// 关键：WidgetComponent 的世界缩放不能太大。
	MarkerWidgetComponent->SetWorldScale3D(FVector(0.2f));

	ClickComponent = CreateDefaultSubobject<USphereComponent>(TEXT("ClickComponent"));
	ClickComponent->SetupAttachment(Root);
	ClickComponent->InitSphereRadius(120.0f);
	ClickComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickComponent->SetHiddenInGame(true);
}

void AWebUIMarkerActor::BeginPlay()
{
	Super::BeginPlay();

	UpdateVisual();
}

void AWebUIMarkerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (MarkerData.Style.bFaceToCamera)
	{
		FaceToCamera();
	}
}

void AWebUIMarkerActor::InitMarker(const FWebUIMarkerData& InData)
{
	MarkerData = InData;

	if (MarkerData.LocationType == EWebUIMarkerLocationType::WorldLocation)
	{
		SetActorLocation(MarkerData.WorldLocation);
	}

	UpdateVisual();
}

void AWebUIMarkerActor::SetLabelText(const FText& InText)
{
	MarkerData.LabelText = InText;
	UpdateVisual();
}

void AWebUIMarkerActor::SetMarkerStyle(const FWebUIMarkerStyle& InStyle)
{
	MarkerData.Style = InStyle;
	UpdateVisual();
}

void AWebUIMarkerActor::SetMarkerWorldLocation(FVector InWorldLocation)
{
	MarkerData.LocationType = EWebUIMarkerLocationType::WorldLocation;
	MarkerData.WorldLocation = InWorldLocation;

	SetActorLocation(InWorldLocation);
}

FName AWebUIMarkerActor::GetMarkerId() const
{
	return MarkerData.MarkerId;
}

const FWebUIMarkerData& AWebUIMarkerActor::GetMarkerData() const
{
	return MarkerData;
}

void AWebUIMarkerActor::UpdateVisual()
{
	const FWebUIMarkerStyle& Style = MarkerData.Style;

	if (MarkerWidgetComponent)
	{
		TSubclassOf<UWebUIMarkerWidget> WidgetClass = Style.WidgetClass;

		if (!WidgetClass)
		{
			WidgetClass = UWebUIMarkerWidget::StaticClass();
		}

		const FVector2D SafeDrawSize(
			FMath::Max(Style.WidgetDrawSize.X, 64.0f),
			FMath::Max(Style.WidgetDrawSize.Y, 32.0f)
		);

		const float SafeWorldScale = FMath::Clamp(
			Style.WidgetWorldScale,
			0.02f,
			2.0f
		);

		MarkerWidgetComponent->SetWidgetClass(WidgetClass);
		MarkerWidgetComponent->SetDrawSize(SafeDrawSize);
		MarkerWidgetComponent->SetDrawAtDesiredSize(Style.bDrawAtDesiredSize);
		MarkerWidgetComponent->SetTwoSided(Style.bTwoSided);
		MarkerWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, Style.TextHeightOffset));
		MarkerWidgetComponent->SetWorldScale3D(FVector(SafeWorldScale));
		MarkerWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MarkerWidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);
		MarkerWidgetComponent->SetPivot(FVector2D(0.5f, 0.5f));

		MarkerWidgetComponent->InitWidget();

		UUserWidget* UserWidget = MarkerWidgetComponent->GetUserWidgetObject();
		if (UWebUIMarkerWidget* MarkerWidget = Cast<UWebUIMarkerWidget>(UserWidget))
		{
			const FText FinalText = MarkerData.LabelText.IsEmpty()
				? FText::FromName(MarkerData.MarkerId)
				: MarkerData.LabelText;

			MarkerWidget->SetMarkerVisual(FinalText, Style);
		}
		else
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[WebUIMarkerActor] Marker widget is not UWebUIMarkerWidget. MarkerId=%s"),
				*MarkerData.MarkerId.ToString()
			);
		}
	}

	if (ClickComponent)
	{
		ClickComponent->SetSphereRadius(FMath::Max(Style.ClickRadius, 1.0f));
		ClickComponent->SetHiddenInGame(!bDebugClickRange);

		if (Style.bEnableClickCollision)
		{
			ClickComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			ClickComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
			ClickComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		}
		else
		{
			ClickComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void AWebUIMarkerActor::FaceToCamera()
{
	UWorld* World = GetWorld();
	if (!World || !MarkerWidgetComponent)
	{
		return;
	}

	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(World, 0);
	if (!CameraManager)
	{
		return;
	}

	const FVector CameraLocation = CameraManager->GetCameraLocation();
	const FVector WidgetLocation = MarkerWidgetComponent->GetComponentLocation();

	const FVector Direction = CameraLocation - WidgetLocation;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	MarkerWidgetComponent->SetWorldRotation(Direction.Rotation());
}