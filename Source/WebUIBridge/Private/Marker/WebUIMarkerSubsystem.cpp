#include "Marker/WebUIMarkerSubsystem.h"

#include "Engine/World.h"
#include "Marker/WebUIMarkerStyleDataAsset.h"

#if WEBUIBRIDGE_WITH_CESIUM
#include "CesiumGlobeAnchorComponent.h"
#endif

AWebUIMarkerActor* UWebUIMarkerSubsystem::AddMarkerByWorldLocation(
	FName MarkerId,
	const FText& LabelText,
	FVector WorldLocation,
	const FWebUIMarkerStyle& Style
)
{
	if (MarkerId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddMarkerByWorldLocation failed: MarkerId is None."));
		return nullptr;
	}

	if (MarkerMap.Contains(MarkerId))
	{
		RemoveMarker(MarkerId);
	}

	FWebUIMarkerData MarkerData;
	MarkerData.MarkerId = MarkerId;
	MarkerData.LabelText = LabelText;
	MarkerData.LocationType = EWebUIMarkerLocationType::WorldLocation;
	MarkerData.WorldLocation = WorldLocation;
	MarkerData.Style = Style;

	return SpawnMarkerActor(MarkerData);
}

AWebUIMarkerActor* UWebUIMarkerSubsystem::AddMarkerByWorldLocationWithStyleAsset(
	FName MarkerId,
	const FText& LabelText,
	FVector WorldLocation,
	UWebUIMarkerStyleDataAsset* StyleAsset
)
{
	const FWebUIMarkerStyle Style = StyleAsset ? StyleAsset->Style : GetDefaultStyle();

	return AddMarkerByWorldLocation(
		MarkerId,
		LabelText,
		WorldLocation,
		Style
	);
}

AWebUIMarkerActor* UWebUIMarkerSubsystem::AddSimpleMarkerByWorldLocation(
	FName MarkerId,
	const FText& LabelText,
	FVector WorldLocation
)
{
	return AddMarkerByWorldLocation(
		MarkerId,
		LabelText,
		WorldLocation,
		GetDefaultStyle()
	);
}

AWebUIMarkerActor* UWebUIMarkerSubsystem::AddMarkerByLongitudeLatitudeHeight(
	FName MarkerId,
	const FText& LabelText,
	double Longitude,
	double Latitude,
	double Height,
	const FWebUIMarkerStyle& Style
)
{
	if (MarkerId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddMarkerByLongitudeLatitudeHeight failed: MarkerId is None."));
		return nullptr;
	}

	if (MarkerMap.Contains(MarkerId))
	{
		RemoveMarker(MarkerId);
	}

	FWebUIMarkerData MarkerData;
	MarkerData.MarkerId = MarkerId;
	MarkerData.LabelText = LabelText;
	MarkerData.LocationType = EWebUIMarkerLocationType::LongitudeLatitudeHeight;
	MarkerData.Longitude = Longitude;
	MarkerData.Latitude = Latitude;
	MarkerData.Height = Height;
	MarkerData.Style = Style;

	AWebUIMarkerActor* MarkerActor = SpawnMarkerActor(MarkerData);
	if (!IsValid(MarkerActor))
	{
		return nullptr;
	}

#if WEBUIBRIDGE_WITH_CESIUM
	UCesiumGlobeAnchorComponent* GlobeAnchor =
		MarkerActor->FindComponentByClass<UCesiumGlobeAnchorComponent>();

	if (!GlobeAnchor)
	{
		GlobeAnchor = NewObject<UCesiumGlobeAnchorComponent>(
			MarkerActor,
			UCesiumGlobeAnchorComponent::StaticClass(),
			TEXT("CesiumGlobeAnchor")
		);

		GlobeAnchor->SetupAttachment(MarkerActor->GetRootComponent());
		GlobeAnchor->RegisterComponent();
	}

	if (GlobeAnchor)
	{
		GlobeAnchor->MoveToLongitudeLatitudeHeight(
			FVector(Longitude, Latitude, Height)
		);
	}
#else
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("WEBUIBRIDGE_WITH_CESIUM is disabled. Longitude/Latitude marker cannot be positioned correctly.")
	);
#endif

	return MarkerActor;
}

AWebUIMarkerActor* UWebUIMarkerSubsystem::AddMarkerByLongitudeLatitudeHeightWithStyleAsset(
	FName MarkerId,
	const FText& LabelText,
	double Longitude,
	double Latitude,
	double Height,
	UWebUIMarkerStyleDataAsset* StyleAsset
)
{
	const FWebUIMarkerStyle Style = StyleAsset ? StyleAsset->Style : GetDefaultStyle();

	return AddMarkerByLongitudeLatitudeHeight(
		MarkerId,
		LabelText,
		Longitude,
		Latitude,
		Height,
		Style
	);
}

AWebUIMarkerActor* UWebUIMarkerSubsystem::AddSimpleMarkerByLongitudeLatitudeHeight(
	FName MarkerId,
	const FText& LabelText,
	double Longitude,
	double Latitude,
	double Height
)
{
	return AddMarkerByLongitudeLatitudeHeight(
		MarkerId,
		LabelText,
		Longitude,
		Latitude,
		Height,
		GetDefaultStyle()
	);
}

bool UWebUIMarkerSubsystem::RemoveMarker(FName MarkerId)
{
	if (MarkerId.IsNone())
	{
		return false;
	}

	TObjectPtr<AWebUIMarkerActor>* FoundMarker = MarkerMap.Find(MarkerId);
	if (!FoundMarker)
	{
		return false;
	}

	AWebUIMarkerActor* MarkerActor = FoundMarker->Get();
	MarkerMap.Remove(MarkerId);

	if (IsValid(MarkerActor))
	{
		MarkerActor->Destroy();
	}

	return true;
}

void UWebUIMarkerSubsystem::ClearMarkers()
{
	for (TPair<FName, TObjectPtr<AWebUIMarkerActor>>& Pair : MarkerMap)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}

	MarkerMap.Empty();
}

AWebUIMarkerActor* UWebUIMarkerSubsystem::GetMarker(FName MarkerId) const
{
	const TObjectPtr<AWebUIMarkerActor>* FoundMarker = MarkerMap.Find(MarkerId);
	if (!FoundMarker)
	{
		return nullptr;
	}

	return FoundMarker->Get();
}

TArray<AWebUIMarkerActor*> UWebUIMarkerSubsystem::GetAllMarkers() const
{
	TArray<AWebUIMarkerActor*> Result;

	for (const TPair<FName, TObjectPtr<AWebUIMarkerActor>>& Pair : MarkerMap)
	{
		if (IsValid(Pair.Value))
		{
			Result.Add(Pair.Value.Get());
		}
	}

	return Result;
}

bool UWebUIMarkerSubsystem::UpdateMarkerText(FName MarkerId, const FText& NewText)
{
	AWebUIMarkerActor* MarkerActor = GetMarker(MarkerId);
	if (!IsValid(MarkerActor))
	{
		return false;
	}

	MarkerActor->SetLabelText(NewText);
	return true;
}

bool UWebUIMarkerSubsystem::UpdateMarkerStyle(FName MarkerId, const FWebUIMarkerStyle& NewStyle)
{
	AWebUIMarkerActor* MarkerActor = GetMarker(MarkerId);
	if (!IsValid(MarkerActor))
	{
		return false;
	}

	MarkerActor->SetMarkerStyle(NewStyle);
	return true;
}

bool UWebUIMarkerSubsystem::UpdateMarkerWorldLocation(FName MarkerId, FVector NewWorldLocation)
{
	AWebUIMarkerActor* MarkerActor = GetMarker(MarkerId);
	if (!IsValid(MarkerActor))
	{
		return false;
	}

	MarkerActor->SetMarkerWorldLocation(NewWorldLocation);
	return true;
}

AWebUIMarkerActor* UWebUIMarkerSubsystem::SpawnMarkerActor(
	const FWebUIMarkerData& MarkerData
)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(
		World,
		AWebUIMarkerActor::StaticClass(),
		MarkerData.MarkerId
	);
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector SpawnLocation =
		MarkerData.LocationType == EWebUIMarkerLocationType::WorldLocation
			? MarkerData.WorldLocation
			: FVector::ZeroVector;

	AWebUIMarkerActor* MarkerActor =
		World->SpawnActor<AWebUIMarkerActor>(
			AWebUIMarkerActor::StaticClass(),
			SpawnLocation,
			FRotator::ZeroRotator,
			SpawnParams
		);

	if (!IsValid(MarkerActor))
	{
		return nullptr;
	}

	MarkerActor->InitMarker(MarkerData);
	MarkerMap.Add(MarkerData.MarkerId, MarkerActor);

	return MarkerActor;
}

FWebUIMarkerStyle UWebUIMarkerSubsystem::GetDefaultStyle() const
{
	FWebUIMarkerStyle Style;

	Style.WidgetDrawSize = FVector2D(360.0f, 96.0f);
	Style.WidgetWorldScale = 1.0f;
	Style.TextHeightOffset = 180.0f;
	Style.bDrawAtDesiredSize = true;
	Style.bTwoSided = true;
	Style.bFaceToCamera = true;

	Style.bShowBackground = true;
	Style.BackgroundColor = FLinearColor(0.02f, 0.02f, 0.02f, 0.72f);
	Style.PanelPadding = FMargin(12.0f, 8.0f, 14.0f, 8.0f);

	Style.bShowIcon = true;
	Style.IconTexture = nullptr;
	Style.IconSize = FVector2D(48.0f, 48.0f);
	Style.IconTint = FLinearColor::White;
	Style.IconPadding = FMargin(0.0f, 0.0f, 10.0f, 0.0f);

	Style.bShowText = true;
	Style.TextColor = FColor::White;
	Style.TextWorldSize = 32.0f;
	Style.bAutoWrapText = false;

	Style.bEnableClickCollision = true;
	Style.ClickRadius = 120.0f;

	return Style;
}