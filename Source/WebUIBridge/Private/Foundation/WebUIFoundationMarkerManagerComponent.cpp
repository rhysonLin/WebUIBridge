#include "Foundation/WebUIFoundationMarkerManagerComponent.h"

#include "Foundation/WebUIFoundationBridgeComponent.h"
#include "Foundation/WebUIFoundationMarkerActor.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Json.h"

namespace WebUIFoundationMarkerManager
{
	static bool IsExactActorMatch(const AActor* Actor, const FString& TargetName, FString& OutMatchedBy)
	{
		if (!Actor)
		{
			return false;
		}

		if (Actor->GetName().Equals(TargetName, ESearchCase::IgnoreCase))
		{
			OutMatchedBy = TEXT("objectName");
			return true;
		}

		for (const FName& Tag : Actor->Tags)
		{
			if (Tag.ToString().Equals(TargetName, ESearchCase::IgnoreCase))
			{
				OutMatchedBy = TEXT("actorTag");
				return true;
			}                         
		}

#if WITH_EDITOR
		if (Actor->GetActorLabel().Equals(TargetName, ESearchCase::IgnoreCase))
		{
			OutMatchedBy = TEXT("actorLabel");
			return true;
		}
#endif

		return false;
	}

	static bool IsPartialActorMatch(const AActor* Actor, const FString& TargetName, FString& OutMatchedBy)
	{
		if (!Actor)
		{
			return false;
		}

		if (Actor->GetName().Contains(TargetName, ESearchCase::IgnoreCase))
		{
			OutMatchedBy = TEXT("objectNameContains");
			return true;
		}

		for (const FName& Tag : Actor->Tags)
		{
			if (Tag.ToString().Contains(TargetName, ESearchCase::IgnoreCase))
			{
				OutMatchedBy = TEXT("actorTagContains");
				return true;
			}
		}

#if WITH_EDITOR
		if (Actor->GetActorLabel().Contains(TargetName, ESearchCase::IgnoreCase))
		{
			OutMatchedBy = TEXT("actorLabelContains");
			return true;
		}
#endif

		return false;
	}

	static bool TryGetNumberAlias(
		UWebUIFoundationBridgeComponent* Bridge,
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* PrimaryKey,
		const TCHAR* AliasKey,
		double& OutValue
	)
	{
		return Bridge &&
			(Bridge->GetPayloadNumber(Payload, PrimaryKey, OutValue) ||
			 Bridge->GetPayloadNumber(Payload, AliasKey, OutValue));
	}

	static FString GetFollowActorName(
		UWebUIFoundationBridgeComponent* Bridge,
		const TSharedPtr<FJsonObject>& Payload
	)
	{
		if (!Bridge)
		{
			return TEXT("");
		}

		FString Name = Bridge->GetPayloadString(Payload, TEXT("followActorName"), TEXT(""));
		if (Name.IsEmpty())
		{
			Name = Bridge->GetPayloadString(Payload, TEXT("attachActorName"), TEXT(""));
		}
		if (Name.IsEmpty())
		{
			Name = Bridge->GetPayloadString(Payload, TEXT("parentActorName"), TEXT(""));
		}
		return Name.TrimStartAndEnd();
	}

	static bool HasFollowActorField(const TSharedPtr<FJsonObject>& Payload)
	{
		return Payload.IsValid() &&
			(Payload->HasField(TEXT("followActorName")) ||
			 Payload->HasField(TEXT("attachActorName")) ||
			 Payload->HasField(TEXT("parentActorName")));
	}

	static bool HasPositionField(const TSharedPtr<FJsonObject>& Payload)
	{
		if (!Payload.IsValid())
		{
			return false;
		}

		static const TCHAR* PositionKeys[] =
		{
			TEXT("lon"), TEXT("lat"), TEXT("height"),
			TEXT("worldX"), TEXT("worldY"), TEXT("worldZ"), 
			TEXT("x"), TEXT("y"), TEXT("z"),
			TEXT("offsetX"), TEXT("offsetY"), TEXT("offsetZ"),
			TEXT("anchorMode"), TEXT("positionAnchor")
		};

		for (const TCHAR* Key : PositionKeys)
		{
			if (Payload->HasField(Key))
			{
				return true;
			}
		}

		return false;
	}

	static void AccumulateActorBoundsRecursive(
		const AActor* Actor,
		bool bIncludeAttachedActors,
		FBox& InOutBounds,
		TSet<const AActor*>& VisitedActors
	)
	{
		if (!IsValid(Actor) || VisitedActors.Contains(Actor) ||
			Actor->ActorHasTag(FName(TEXT("WebUIFoundationMarker"))))
		{
			return;
		}

		VisitedActors.Add(Actor);

		const FBox ActorBounds = Actor->GetComponentsBoundingBox(true, true);
		if (ActorBounds.IsValid)
		{
			InOutBounds += ActorBounds;
		}

		if (!bIncludeAttachedActors)
		{
			return;
		}

		TArray<AActor*> AttachedActors;
		Actor->GetAttachedActors(AttachedActors);
		for (const AActor* AttachedActor : AttachedActors)
		{
			AccumulateActorBoundsRecursive(
				AttachedActor,
				true,
				InOutBounds,
				VisitedActors
			);
		}
	}

	static bool ResolveActorBoundsTopLocation(
		UWebUIFoundationBridgeComponent* Bridge,
		const TSharedPtr<FJsonObject>& Payload,
		const AActor* FollowActor,
		FVector& OutWorldLocation,
		FString& OutError
	)
	{
		if (!Bridge || !IsValid(FollowActor))
		{
			OutError = TEXT("actorBoundsTop requires a valid followActorName.");
			return false;
		}

		bool bIncludeAttachedActors = true;
		Bridge->GetPayloadBool(Payload, TEXT("includeAttachedActors"), bIncludeAttachedActors);

		FBox CombinedBounds(ForceInit);
		TSet<const AActor*> VisitedActors;
		AccumulateActorBoundsRecursive(
			FollowActor,
			bIncludeAttachedActors,
			CombinedBounds,
			VisitedActors
		);

		if (!CombinedBounds.IsValid)
		{
			OutError = FString::Printf(
				TEXT("Follow actor '%s' has no valid component bounds. Use world coordinates or an Actor with visible components."),
				*FollowActor->GetName()
			);
			return false;
		}

		double OffsetX = 0.0;
		double OffsetY = 0.0;
		double OffsetZ = 0.0;
		Bridge->GetPayloadNumber(Payload, TEXT("offsetX"), OffsetX);
		Bridge->GetPayloadNumber(Payload, TEXT("offsetY"), OffsetY);
		Bridge->GetPayloadNumber(Payload, TEXT("offsetZ"), OffsetZ);

		FVector Offset(OffsetX, OffsetY, OffsetZ);
		const FString OffsetSpace = Bridge->GetPayloadString(Payload, TEXT("offsetSpace"), TEXT("world"));
		if (OffsetSpace.Equals(TEXT("local"), ESearchCase::IgnoreCase))
		{
			Offset = FollowActor->GetActorTransform().TransformVectorNoScale(Offset);
		}

		const FVector Center = CombinedBounds.GetCenter();
		OutWorldLocation = FVector(Center.X, Center.Y, CombinedBounds.Max.Z) + Offset;
		return true;
	}

	struct FMarkerStyle
	{
		FLinearColor MarkerColor = FLinearColor(0.0f, 0.65f, 1.0f, 1.0f);
		FLinearColor LabelColor = FLinearColor::White;
		FLinearColor ImageTintColor = FLinearColor::White;
		FLinearColor BackgroundColor = FLinearColor(0.16f, 0.17f, 0.19f, 1.0f);
		float BackgroundOpacity = 0.88f;
		float CornerRadius = 8.0f;
		float PaddingX = 10.0f;
		float PaddingY = 6.0f;
		float IconTextGap = 8.0f;
		float Size = 120.0f;
		float VisualSize = 56.0f;
		float LabelFontSize = 24.0f;
		FString SpaceMode = TEXT("world");
		FString LayoutStyle = TEXT("classic");
		FString VisualType = TEXT("point");
		bool bShowLabel = true;
		FString PointText = TEXT("●");
		FString ImagePath;
	};

	static bool TryParseColorString(const FString& Value, FLinearColor& OutColor)
	{
		FString Hex = Value.TrimStartAndEnd();
		Hex.RemoveFromStart(TEXT("#"));

		if (Hex.Len() != 6 && Hex.Len() != 8)
		{
			return false;
		}

		const FColor ParsedColor = FColor::FromHex(Hex);
		OutColor = FLinearColor::FromSRGBColor(ParsedColor);
		OutColor.A = ParsedColor.A / 255.0f;
		return true;
	}

	static void ReadColor(
		UWebUIFoundationBridgeComponent* Bridge,
		const TSharedPtr<FJsonObject>& Payload,
		const TCHAR* PrimaryKey,
		const TCHAR* AliasKey,
		FLinearColor& InOutColor
	)
	{
		if (!Bridge)
		{
			return;
		}

		FString ColorText = Bridge->GetPayloadString(Payload, PrimaryKey, TEXT(""));
		if (ColorText.IsEmpty() && AliasKey)
		{
			ColorText = Bridge->GetPayloadString(Payload, AliasKey, TEXT(""));
		}

		FLinearColor ParsedColor;
		if (!ColorText.IsEmpty() && TryParseColorString(ColorText, ParsedColor))
		{
			InOutColor = ParsedColor;
		}
	}

	static FMarkerStyle ResolveMarkerStyle(
		UWebUIFoundationBridgeComponent* Bridge,
		const TSharedPtr<FJsonObject>& Payload,
		const AWebUIFoundationMarkerActor* ExistingMarker
	)
	{
		FMarkerStyle Style;

		if (ExistingMarker)
		{
			Style.MarkerColor = ExistingMarker->GetMarkerColor();
			Style.LabelColor = ExistingMarker->GetLabelColor();
			Style.ImageTintColor = ExistingMarker->GetImageTintColor();
			Style.BackgroundColor = ExistingMarker->GetBackgroundColor();
			Style.BackgroundOpacity = ExistingMarker->GetBackgroundOpacity();
			Style.CornerRadius = ExistingMarker->GetCornerRadius();
			Style.PaddingX = ExistingMarker->GetPaddingX();
			Style.PaddingY = ExistingMarker->GetPaddingY();
			Style.IconTextGap = ExistingMarker->GetIconTextGap();
			Style.Size = ExistingMarker->GetMarkerSize();
			Style.VisualSize = ExistingMarker->GetVisualSize();
			Style.LabelFontSize = ExistingMarker->GetLabelFontSize();
			Style.SpaceMode = ExistingMarker->GetSpaceMode();
			Style.LayoutStyle = ExistingMarker->GetLayoutStyle();
			Style.VisualType = ExistingMarker->GetVisualType();
			Style.bShowLabel = ExistingMarker->GetShowLabel();
			Style.PointText = ExistingMarker->GetPointText();
			Style.ImagePath = ExistingMarker->GetImagePath();
		}

		double NumberValue = 0.0;
		if (Bridge->GetPayloadNumber(Payload, TEXT("size"), NumberValue))
		{
			Style.Size = static_cast<float>(NumberValue);
		}
		if (Bridge->GetPayloadNumber(Payload, TEXT("visualSize"), NumberValue) ||
			Bridge->GetPayloadNumber(Payload, TEXT("iconSize"), NumberValue))
		{
			Style.VisualSize = static_cast<float>(NumberValue);
		}
		if (Bridge->GetPayloadNumber(Payload, TEXT("labelFontSize"), NumberValue) ||
			Bridge->GetPayloadNumber(Payload, TEXT("fontSize"), NumberValue))
		{
			Style.LabelFontSize = static_cast<float>(NumberValue);
		}
		if (Bridge->GetPayloadNumber(Payload, TEXT("backgroundOpacity"), NumberValue) ||
			Bridge->GetPayloadNumber(Payload, TEXT("bgOpacity"), NumberValue))
		{
			Style.BackgroundOpacity = static_cast<float>(NumberValue);
		}
		if (Bridge->GetPayloadNumber(Payload, TEXT("cornerRadius"), NumberValue))
		{
			Style.CornerRadius = static_cast<float>(NumberValue);
		}
		if (Bridge->GetPayloadNumber(Payload, TEXT("paddingX"), NumberValue))
		{
			Style.PaddingX = static_cast<float>(NumberValue);
		}
		if (Bridge->GetPayloadNumber(Payload, TEXT("paddingY"), NumberValue))
		{
			Style.PaddingY = static_cast<float>(NumberValue);
		}
		if (Bridge->GetPayloadNumber(Payload, TEXT("iconTextGap"), NumberValue) ||
			Bridge->GetPayloadNumber(Payload, TEXT("gap"), NumberValue))
		{
			Style.IconTextGap = static_cast<float>(NumberValue);
		}

		ReadColor(Bridge, Payload, TEXT("markerColor"), TEXT("color"), Style.MarkerColor);
		ReadColor(Bridge, Payload, TEXT("labelColor"), TEXT("textColor"), Style.LabelColor);
		ReadColor(Bridge, Payload, TEXT("imageTintColor"), TEXT("imageColor"), Style.ImageTintColor);
		ReadColor(Bridge, Payload, TEXT("backgroundColor"), TEXT("bgColor"), Style.BackgroundColor);

		FString RequestedSpaceMode = Bridge->GetPayloadString(Payload, TEXT("spaceMode"), TEXT(""));
		if (RequestedSpaceMode.IsEmpty())
		{
			RequestedSpaceMode = Bridge->GetPayloadString(Payload, TEXT("markerSpace"), TEXT(""));
		}
		if (!RequestedSpaceMode.IsEmpty())
		{
			Style.SpaceMode = RequestedSpaceMode.TrimStartAndEnd().ToLower();
		}

		bool bFixedScreenSize = false;
		if (Bridge->GetPayloadBool(Payload, TEXT("fixedScreenSize"), bFixedScreenSize))
		{
			Style.SpaceMode = bFixedScreenSize ? TEXT("screen") : TEXT("world");
		}

		if (Style.SpaceMode == TEXT("screenfixed") || Style.SpaceMode == TEXT("fixed") || Style.SpaceMode == TEXT("viewport"))
		{
			Style.SpaceMode = TEXT("screen");
		}
		if (Style.SpaceMode != TEXT("screen"))
		{
			Style.SpaceMode = TEXT("world");
		}

		Style.LayoutStyle = Bridge->GetPayloadString(Payload, TEXT("layoutStyle"), Style.LayoutStyle).TrimStartAndEnd().ToLower();
		if (Style.LayoutStyle == TEXT("card") || Style.LayoutStyle == TEXT("horizontal") || Style.LayoutStyle == TEXT("pill"))
		{
			Style.LayoutStyle = TEXT("badge");
		}
		if (Style.LayoutStyle != TEXT("badge"))
		{
			Style.LayoutStyle = TEXT("classic");
		}

		Style.VisualType = Bridge->GetPayloadString(Payload, TEXT("visualType"), Style.VisualType).TrimStartAndEnd().ToLower();
		if (Style.VisualType != TEXT("point") && Style.VisualType != TEXT("image") && Style.VisualType != TEXT("none"))
		{
			Style.VisualType = TEXT("point");
		}

		Bridge->GetPayloadBool(Payload, TEXT("showLabel"), Style.bShowLabel);
		Style.PointText = Bridge->GetPayloadString(Payload, TEXT("pointText"), Style.PointText);
		Style.ImagePath = Bridge->GetPayloadString(Payload, TEXT("imagePath"), Style.ImagePath).TrimStartAndEnd();

		Style.BackgroundOpacity = FMath::Clamp(Style.BackgroundOpacity, 0.0f, 1.0f);
		Style.CornerRadius = FMath::Clamp(Style.CornerRadius, 0.0f, 64.0f);
		Style.PaddingX = FMath::Clamp(Style.PaddingX, 0.0f, 64.0f);
		Style.PaddingY = FMath::Clamp(Style.PaddingY, 0.0f, 64.0f);
		Style.IconTextGap = FMath::Clamp(Style.IconTextGap, 0.0f, 64.0f);
		Style.Size = FMath::Clamp(Style.Size, 20.0f, 1200.0f);
		Style.VisualSize = FMath::Clamp(Style.VisualSize, 8.0f, 512.0f);
		Style.LabelFontSize = FMath::Clamp(Style.LabelFontSize, 8.0f, 128.0f);
		return Style;
	}

	static FString ColorToHex(const FLinearColor& Color)
	{
		return FString::Printf(TEXT("#%s"), *Color.ToFColorSRGB().ToHex());
	}

	static void AppendMarkerStyleData(
		const FMarkerStyle& Style,
		const TSharedPtr<FJsonObject>& Data
	)
	{
		if (!Data.IsValid())
		{
			return;
		}

		Data->SetNumberField(TEXT("size"), Style.Size);
		Data->SetNumberField(TEXT("visualSize"), Style.VisualSize);
		Data->SetNumberField(TEXT("labelFontSize"), Style.LabelFontSize);
		Data->SetNumberField(TEXT("backgroundOpacity"), Style.BackgroundOpacity);
		Data->SetNumberField(TEXT("cornerRadius"), Style.CornerRadius);
		Data->SetNumberField(TEXT("paddingX"), Style.PaddingX);
		Data->SetNumberField(TEXT("paddingY"), Style.PaddingY);
		Data->SetNumberField(TEXT("iconTextGap"), Style.IconTextGap);
		Data->SetStringField(TEXT("spaceMode"), Style.SpaceMode);
		Data->SetBoolField(TEXT("fixedScreenSize"), Style.SpaceMode == TEXT("screen"));
		Data->SetStringField(TEXT("layoutStyle"), Style.LayoutStyle);
		Data->SetStringField(TEXT("markerColor"), ColorToHex(Style.MarkerColor));
		Data->SetStringField(TEXT("labelColor"), ColorToHex(Style.LabelColor));
		Data->SetStringField(TEXT("imageTintColor"), ColorToHex(Style.ImageTintColor));
		Data->SetStringField(TEXT("backgroundColor"), ColorToHex(Style.BackgroundColor));
		Data->SetStringField(TEXT("visualType"), Style.VisualType);
		Data->SetBoolField(TEXT("showLabel"), Style.bShowLabel);
		Data->SetStringField(TEXT("pointText"), Style.PointText);
		Data->SetStringField(TEXT("imagePath"), Style.ImagePath);
	}

}

UWebUIFoundationMarkerManagerComponent::UWebUIFoundationMarkerManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWebUIFoundationMarkerManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllMarkers();

	Super::EndPlay(EndPlayReason);
}

void UWebUIFoundationMarkerManagerComponent::HandleAddMarker(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	const FString MarkerId = Bridge->GetPayloadString(
		Payload,
		TEXT("id"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits)
	);

	const FString Label = Bridge->GetPayloadString(Payload, TEXT("label"), MarkerId);
	const FString FollowActorName = WebUIFoundationMarkerManager::GetFollowActorName(Bridge, Payload);

	bool bAllowPartialMatch = false;
	Bridge->GetPayloadBool(Payload, TEXT("allowPartialMatch"), bAllowPartialMatch);

	AActor* FollowActor = nullptr;
	FString FollowMatchedBy;
	if (!FollowActorName.IsEmpty())
	{
		FString FindError;
		FollowActor = FindFollowActor(
			FollowActorName,
			bAllowPartialMatch,
			FollowMatchedBy,
			FindError
		);

		if (!FollowActor)
		{
			Bridge->SendError(RequestId, TEXT("addMarkerResult"), FindError);
			return;
		}
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FString CoordinateSource;
	FString LocationError;
	if (!ResolveMarkerWorldLocation(
		Bridge,
		Payload,
		FollowActor,
		SpawnLocation,
		CoordinateSource,
		LocationError
	))
	{
		Bridge->SendError(RequestId, TEXT("addMarkerResult"), LocationError);
		return;
	}

	AWebUIFoundationMarkerActor* Marker = nullptr;
	bool bUpdatedExisting = false;

	if (Markers.Contains(MarkerId))
	{
		Marker = Markers[MarkerId];
		if (IsValid(Marker))
		{
			bUpdatedExisting = true;
		}
		else
		{
			Markers.Remove(MarkerId);
			Marker = nullptr;
		}
	}

	if (!Marker)
	{
		FActorSpawnParameters Params;
		Params.Name = NAME_None;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Marker = GetWorld()->SpawnActor<AWebUIFoundationMarkerActor>(
			AWebUIFoundationMarkerActor::StaticClass(),
			SpawnLocation,
			FRotator::ZeroRotator,
			Params
		);

		if (!Marker)
		{
			Bridge->SendError(RequestId, TEXT("addMarkerResult"), TEXT("Failed to spawn marker."));
			return;
		}

		Marker->Tags.Add(FName(TEXT("WebUIFoundationMarker")));
		Marker->Tags.Add(FName(*MarkerId));
		Markers.Add(MarkerId, Marker);
	}

	const WebUIFoundationMarkerManager::FMarkerStyle MarkerStyle =
		WebUIFoundationMarkerManager::ResolveMarkerStyle(Bridge, Payload, bUpdatedExisting ? Marker : nullptr);

	Marker->SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Marker->SetupMarker(
		MarkerId,
		Label,
		MarkerStyle.MarkerColor,
		MarkerStyle.LabelColor,
		MarkerStyle.ImageTintColor,
		MarkerStyle.BackgroundColor,
		MarkerStyle.BackgroundOpacity,
		MarkerStyle.CornerRadius,
		MarkerStyle.PaddingX,
		MarkerStyle.PaddingY,
		MarkerStyle.IconTextGap,
		MarkerStyle.Size,
		MarkerStyle.VisualSize,
		MarkerStyle.LabelFontSize,
		MarkerStyle.SpaceMode,
		MarkerStyle.LayoutStyle,
		MarkerStyle.VisualType,
		MarkerStyle.bShowLabel,
		MarkerStyle.PointText,
		MarkerStyle.ImagePath
	);

	ApplyMarkerAttachment(Marker, FollowActor, false);

#if WITH_EDITOR
	Marker->SetActorLabel(FString::Printf(TEXT("PF_Marker_%s"), *MarkerId));
	Marker->SetFolderPath(TEXT("WebUIFoundation/Markers"));
#endif

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation] Marker %s. Id=%s WorldLocation=%s Size=%.1f SpaceMode=%s FollowActor=%s Source=%s"),
		bUpdatedExisting ? TEXT("updated") : TEXT("created"),
		*MarkerId,
		*SpawnLocation.ToString(),
		MarkerStyle.Size,
		*MarkerStyle.SpaceMode,
		*GetNameSafe(FollowActor),
		*CoordinateSource
	);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("id"), MarkerId);
	Data->SetStringField(TEXT("label"), Label);
	Data->SetNumberField(TEXT("worldX"), SpawnLocation.X);
	Data->SetNumberField(TEXT("worldY"), SpawnLocation.Y);
	Data->SetNumberField(TEXT("worldZ"), SpawnLocation.Z);
	Data->SetStringField(TEXT("coordinateSource"), CoordinateSource);
	WebUIFoundationMarkerManager::AppendMarkerStyleData(MarkerStyle, Data);

	double ResponseLon = 0.0;
	double ResponseLat = 0.0;
	double ResponseHeight = 0.0;
	if (Bridge->GetPayloadNumber(Payload, TEXT("lon"), ResponseLon) &&
		Bridge->GetPayloadNumber(Payload, TEXT("lat"), ResponseLat))
	{
		Bridge->GetPayloadNumber(Payload, TEXT("height"), ResponseHeight);
		Data->SetNumberField(TEXT("lon"), ResponseLon);
		Data->SetNumberField(TEXT("lat"), ResponseLat);
		Data->SetNumberField(TEXT("height"), ResponseHeight);
	}

	Data->SetStringField(TEXT("message"), bUpdatedExisting ? TEXT("marker updated by addMarker") : TEXT("marker created"));
	AppendAttachmentData(Marker, FollowActorName, FollowMatchedBy, Data);

	Bridge->SendSuccess(RequestId, TEXT("addMarkerResult"), Data);
}

void UWebUIFoundationMarkerManagerComponent::HandleUpdateMarker(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	const FString MarkerId = Bridge->GetPayloadString(Payload, TEXT("id"), TEXT(""));
	if (MarkerId.IsEmpty() || !Markers.Contains(MarkerId))
	{
		Bridge->SendError(RequestId, TEXT("updateMarkerResult"), TEXT("Marker not found."));
		return;
	}

	AWebUIFoundationMarkerActor* Marker = Markers[MarkerId];
	if (!IsValid(Marker))
	{
		Markers.Remove(MarkerId);
		Bridge->SendError(RequestId, TEXT("updateMarkerResult"), TEXT("Marker invalid."));
		return;
	}

	const bool bHasFollowActorField = WebUIFoundationMarkerManager::HasFollowActorField(Payload);
	const FString FollowActorName = WebUIFoundationMarkerManager::GetFollowActorName(Bridge, Payload);

	bool bDetach = false;
	Bridge->GetPayloadBool(Payload, TEXT("detach"), bDetach);
	if (!bDetach)
	{
		Bridge->GetPayloadBool(Payload, TEXT("detachFromActor"), bDetach);
	}

	bool bAllowPartialMatch = false;
	Bridge->GetPayloadBool(Payload, TEXT("allowPartialMatch"), bAllowPartialMatch);

	AActor* FollowActor = nullptr;
	FString FollowMatchedBy;
	if (bHasFollowActorField && !FollowActorName.IsEmpty() && !bDetach)
	{
		FString FindError;
		FollowActor = FindFollowActor(
			FollowActorName,
			bAllowPartialMatch,
			FollowMatchedBy,
			FindError
		);

		if (!FollowActor)
		{
			Bridge->SendError(RequestId, TEXT("updateMarkerResult"), FindError);
			return;
		}
	}

	FString CoordinateSource = TEXT("unchanged");
	if (WebUIFoundationMarkerManager::HasPositionField(Payload))
	{
		const AActor* LocationReferenceActor = FollowActor ? FollowActor : Marker->GetAttachParentActor();
		FVector NewWorldLocation = FVector::ZeroVector;
		FString LocationError;

		if (!ResolveMarkerWorldLocation(
			Bridge,
			Payload,
			LocationReferenceActor,
			NewWorldLocation,
			CoordinateSource,
			LocationError
		))
		{
			Bridge->SendError(RequestId, TEXT("updateMarkerResult"), LocationError);
			return;
		}

		Marker->SetActorLocation(NewWorldLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	const FString Label = Bridge->GetPayloadString(Payload, TEXT("label"), Marker->GetMarkerLabel());
	const WebUIFoundationMarkerManager::FMarkerStyle MarkerStyle =
		WebUIFoundationMarkerManager::ResolveMarkerStyle(Bridge, Payload, Marker);

	Marker->SetupMarker(
		MarkerId,
		Label,
		MarkerStyle.MarkerColor,
		MarkerStyle.LabelColor,
		MarkerStyle.ImageTintColor,
		MarkerStyle.BackgroundColor,
		MarkerStyle.BackgroundOpacity,
		MarkerStyle.CornerRadius,
		MarkerStyle.PaddingX,
		MarkerStyle.PaddingY,
		MarkerStyle.IconTextGap,
		MarkerStyle.Size,
		MarkerStyle.VisualSize,
		MarkerStyle.LabelFontSize,
		MarkerStyle.SpaceMode,
		MarkerStyle.LayoutStyle,
		MarkerStyle.VisualType,
		MarkerStyle.bShowLabel,
		MarkerStyle.PointText,
		MarkerStyle.ImagePath
	);

	if (bDetach || bHasFollowActorField)
	{
		ApplyMarkerAttachment(Marker, FollowActor, bDetach || FollowActorName.IsEmpty());
	}

#if WITH_EDITOR
	Marker->SetActorLabel(FString::Printf(TEXT("PF_Marker_%s"), *MarkerId));
	Marker->SetFolderPath(TEXT("WebUIFoundation/Markers"));
#endif

	const FVector MarkerLocation = Marker->GetActorLocation();
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("id"), MarkerId);
	Data->SetStringField(TEXT("label"), Label);
	Data->SetNumberField(TEXT("worldX"), MarkerLocation.X);
	Data->SetNumberField(TEXT("worldY"), MarkerLocation.Y);
	Data->SetNumberField(TEXT("worldZ"), MarkerLocation.Z);
	Data->SetStringField(TEXT("coordinateSource"), CoordinateSource);
	WebUIFoundationMarkerManager::AppendMarkerStyleData(MarkerStyle, Data);

	double ResponseLon = 0.0;
	double ResponseLat = 0.0;
	double ResponseHeight = 0.0;
	if (Bridge->GetPayloadNumber(Payload, TEXT("lon"), ResponseLon) &&
		Bridge->GetPayloadNumber(Payload, TEXT("lat"), ResponseLat))
	{
		Bridge->GetPayloadNumber(Payload, TEXT("height"), ResponseHeight);
		Data->SetNumberField(TEXT("lon"), ResponseLon);
		Data->SetNumberField(TEXT("lat"), ResponseLat);
		Data->SetNumberField(TEXT("height"), ResponseHeight);
	}

	AppendAttachmentData(Marker, FollowActorName, FollowMatchedBy, Data);

	Bridge->SendSuccess(RequestId, TEXT("updateMarkerResult"), Data);
}

void UWebUIFoundationMarkerManagerComponent::HandleRemoveMarker(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	const FString MarkerId = Bridge->GetPayloadString(Payload, TEXT("id"), TEXT(""));

	if (MarkerId.IsEmpty() || !Markers.Contains(MarkerId))
	{
		Bridge->SendError(RequestId, TEXT("removeMarkerResult"), TEXT("Marker not found."));
		return;
	}

	AWebUIFoundationMarkerActor* Marker = Markers[MarkerId];
	if (IsValid(Marker))
	{
		Marker->Destroy();
	}

	Markers.Remove(MarkerId);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("id"), MarkerId);

	Bridge->SendSuccess(RequestId, TEXT("removeMarkerResult"), Data);
}

void UWebUIFoundationMarkerManagerComponent::HandleClearMarkers(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId
)
{
	if (!Bridge)
	{
		return;
	}

	ClearAllMarkers();

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("message"), TEXT("all markers cleared"));

	Bridge->SendSuccess(RequestId, TEXT("clearMarkersResult"), Data);
}

void UWebUIFoundationMarkerManagerComponent::ClearAllMarkers()
{
	for (auto& Pair : Markers)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}

	Markers.Empty();
}

void UWebUIFoundationMarkerManagerComponent::AppendSceneState(const TSharedPtr<FJsonObject>& Data) const
{
	if (!Data.IsValid())
	{
		return;
	}

	int32 AttachedMarkerCount = 0;
	for (const TPair<FString, AWebUIFoundationMarkerActor*>& Pair : Markers)
	{
		if (IsValid(Pair.Value) && Pair.Value->GetAttachParentActor())
		{
			++AttachedMarkerCount;
		}
	}

	Data->SetNumberField(TEXT("markerCount"), Markers.Num());
	Data->SetNumberField(TEXT("attachedMarkerCount"), AttachedMarkerCount);
}

AActor* UWebUIFoundationMarkerManagerComponent::FindFollowActor(
	const FString& TargetName,
	bool bAllowPartialMatch,
	FString& OutMatchedBy,
	FString& OutError
) const
{
	OutMatchedBy.Reset();
	OutError.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		OutError = TEXT("World is not available.");
		return nullptr;
	}

	TArray<AActor*> ExactMatches;
	TArray<FString> ExactMatchedBy;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor->ActorHasTag(FName(TEXT("WebUIFoundationMarker"))))
		{
			continue;
		}

		FString MatchSource;
		if (WebUIFoundationMarkerManager::IsExactActorMatch(Actor, TargetName, MatchSource))
		{
			ExactMatches.Add(Actor);
			ExactMatchedBy.Add(MatchSource);
		}
	}

	if (ExactMatches.Num() == 1)
	{
		OutMatchedBy = ExactMatchedBy[0];
		return ExactMatches[0];
	}

	if (ExactMatches.Num() > 1)
	{
		TArray<FString> Names;
		for (int32 Index = 0; Index < ExactMatches.Num() && Index < 8; ++Index)
		{
			Names.Add(ExactMatches[Index]->GetName());
		}

		OutError = FString::Printf(
			TEXT("Multiple actors exactly match followActorName '%s': %s. Use a unique object name or Actor Tag."),
			*TargetName,
			*FString::Join(Names, TEXT(", "))
		);
		return nullptr;
	}

	if (!bAllowPartialMatch)
	{
		OutError = FString::Printf(
			TEXT("Follow actor '%s' was not found. Match uses object name, Actor Tag, and editor-only Actor Label."),
			*TargetName
		);
		return nullptr;
	}

	TArray<AActor*> PartialMatches;
	TArray<FString> PartialMatchedBy;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor->ActorHasTag(FName(TEXT("WebUIFoundationMarker"))))
		{
			continue;
		}

		FString MatchSource;
		if (WebUIFoundationMarkerManager::IsPartialActorMatch(Actor, TargetName, MatchSource))
		{
			PartialMatches.Add(Actor);
			PartialMatchedBy.Add(MatchSource);
		}
	}

	if (PartialMatches.Num() == 1)
	{
		OutMatchedBy = PartialMatchedBy[0];
		return PartialMatches[0];
	}

	if (PartialMatches.Num() > 1)
	{
		TArray<FString> Names;
		for (int32 Index = 0; Index < PartialMatches.Num() && Index < 8; ++Index)
		{
			Names.Add(PartialMatches[Index]->GetName());
		}

		OutError = FString::Printf(
			TEXT("Partial followActorName '%s' matches multiple actors: %s. Send an exact unique name or Actor Tag."),
			*TargetName,
			*FString::Join(Names, TEXT(", "))
		);
		return nullptr;
	}

	OutError = FString::Printf(TEXT("No actor matches followActorName '%s'."), *TargetName);
	return nullptr;
}

bool UWebUIFoundationMarkerManagerComponent::ResolveMarkerWorldLocation(
	UWebUIFoundationBridgeComponent* Bridge,
	const TSharedPtr<FJsonObject>& Payload,
	const AActor* FollowActor,
	FVector& OutWorldLocation,
	FString& OutCoordinateSource,
	FString& OutError
) const
{
	OutWorldLocation = FVector::ZeroVector;
	OutCoordinateSource.Reset();
	OutError.Reset();

	if (!Bridge)
	{
		OutError = TEXT("Bridge is not available.");
		return false;
	}

	double Lon = 0.0;
	double Lat = 0.0;
	double Height = 0.0;
	const bool bHasLon = Bridge->GetPayloadNumber(Payload, TEXT("lon"), Lon);
	const bool bHasLat = Bridge->GetPayloadNumber(Payload, TEXT("lat"), Lat);

	if (bHasLon || bHasLat)
	{
		if (!bHasLon || !bHasLat)
		{
			OutError = TEXT("Marker geodetic position requires both lon and lat.");
			return false;
		}

		Bridge->GetPayloadNumber(Payload, TEXT("height"), Height);
		OutWorldLocation = Bridge->ConvertLonLatHeightToWorld(Lon, Lat, Height);
		OutCoordinateSource = TEXT("longitudeLatitudeHeight");
		return true;
	}

	double WorldX = 0.0;
	double WorldY = 0.0;
	double WorldZ = 0.0;
	const bool bHasWorldX = WebUIFoundationMarkerManager::TryGetNumberAlias(
		Bridge, Payload, TEXT("worldX"), TEXT("x"), WorldX
	);
	const bool bHasWorldY = WebUIFoundationMarkerManager::TryGetNumberAlias(
		Bridge, Payload, TEXT("worldY"), TEXT("y"), WorldY
	);
	const bool bHasWorldZ = WebUIFoundationMarkerManager::TryGetNumberAlias(
		Bridge, Payload, TEXT("worldZ"), TEXT("z"), WorldZ
	);

	if (bHasWorldX || bHasWorldY || bHasWorldZ)
	{
		if (!bHasWorldX || !bHasWorldY || !bHasWorldZ)
		{
			OutError = TEXT("Marker Unreal position requires worldX/worldY/worldZ (aliases x/y/z are supported).");
			return false;
		}

		OutWorldLocation = FVector(WorldX, WorldY, WorldZ);
		OutCoordinateSource = TEXT("unrealWorld");
		return true;
	}

	if (FollowActor)
	{
		FString AnchorMode = Bridge->GetPayloadString(Payload, TEXT("anchorMode"), TEXT(""));
		if (AnchorMode.IsEmpty())
		{
			AnchorMode = Bridge->GetPayloadString(Payload, TEXT("positionAnchor"), TEXT("actorOrigin"));
		}
		AnchorMode = AnchorMode.TrimStartAndEnd().ToLower();

		if (AnchorMode == TEXT("actorboundstop") ||
			AnchorMode == TEXT("boundstop") ||
			AnchorMode == TEXT("topcenter") ||
			AnchorMode == TEXT("actortop"))
		{
			if (!WebUIFoundationMarkerManager::ResolveActorBoundsTopLocation(
				Bridge,
				Payload,
				FollowActor,
				OutWorldLocation,
				OutError
			))
			{
				return false;
			}

			OutCoordinateSource = TEXT("followActorBoundsTop");
			return true;
		}

		double OffsetX = 0.0;
		double OffsetY = 0.0;
		double OffsetZ = 0.0;
		Bridge->GetPayloadNumber(Payload, TEXT("offsetX"), OffsetX);
		Bridge->GetPayloadNumber(Payload, TEXT("offsetY"), OffsetY);
		Bridge->GetPayloadNumber(Payload, TEXT("offsetZ"), OffsetZ);

		const FVector Offset(OffsetX, OffsetY, OffsetZ);
		const FString OffsetSpace = Bridge->GetPayloadString(Payload, TEXT("offsetSpace"), TEXT("local"));

		if (OffsetSpace.Equals(TEXT("world"), ESearchCase::IgnoreCase))
		{
			OutWorldLocation = FollowActor->GetActorLocation() + Offset;
			OutCoordinateSource = TEXT("followActorWorldOffset");
		}
		else
		{
			OutWorldLocation = FollowActor->GetActorTransform().TransformPosition(Offset);
			OutCoordinateSource = TEXT("followActorLocalOffset");
		}

		return true;
	}

	OutError = TEXT("addMarker requires one position source: lon/lat/height, worldX/worldY/worldZ, or followActorName with optional offsetX/offsetY/offsetZ.");
	return false;
}

void UWebUIFoundationMarkerManagerComponent::ApplyMarkerAttachment(
	AWebUIFoundationMarkerActor* Marker,
	AActor* FollowActor,
	bool bDetach
) const
{
	if (!IsValid(Marker))
	{
		return;
	}

	if (bDetach)
	{
		Marker->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		return;
	}

	if (IsValid(FollowActor))
	{
		Marker->AttachToActor(FollowActor, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void UWebUIFoundationMarkerManagerComponent::AppendAttachmentData(
	const AWebUIFoundationMarkerActor* Marker,
	const FString& RequestedFollowActorName,
	const FString& MatchedBy,
	const TSharedPtr<FJsonObject>& Data
) const
{
	if (!Data.IsValid())
	{
		return;
	}

	const AActor* ParentActor = IsValid(Marker) ? Marker->GetAttachParentActor() : nullptr;
	Data->SetBoolField(TEXT("attached"), ParentActor != nullptr);
	Data->SetStringField(TEXT("requestedFollowActorName"), RequestedFollowActorName);
	Data->SetStringField(TEXT("followActorName"), ParentActor ? ParentActor->GetName() : TEXT("None"));
	Data->SetStringField(TEXT("followActorMatchedBy"), MatchedBy);

#if WITH_EDITOR
	if (ParentActor)
	{
		Data->SetStringField(TEXT("followActorLabel"), ParentActor->GetActorLabel());
	}
#endif
}
