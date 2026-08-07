#include "Foundation/WebUIFoundationBridgeComponent.h"

#include "Foundation/WebUIFoundationCameraComponent.h"
#include "Foundation/WebUIFoundationMarkerManagerComponent.h"
#include "Foundation/WebUIFoundationActorControlComponent.h"
#include "Runtime/WebUIRuntimeSubsystem.h"
#include "Host/WebUIInputModeSubsystem.h"
#include "Settings/WebUIBridgeSettings.h"

#include "CesiumGeoreference.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "Json.h"
#include "JsonUtilities.h"

UWebUIFoundationBridgeComponent::UWebUIFoundationBridgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	CachedGeoreference = nullptr;
	CameraComponent = nullptr;
	MarkerManagerComponent = nullptr;
	ActorControlComponent = nullptr;
}

void UWebUIFoundationBridgeComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedGeoreference = nullptr;

	TArray<AActor*> FoundGeorefs;
	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		ACesiumGeoreference::StaticClass(),
		FoundGeorefs
	);

	if (FoundGeorefs.Num() > 0)
	{
		CachedGeoreference = Cast<ACesiumGeoreference>(FoundGeorefs[0]);
	}

	CacheFeatureComponents();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation] Bridge BeginPlay. CesiumGeoreference=%s, Camera=%s, Marker=%s, ActorControl=%s"),
		CachedGeoreference ? TEXT("Valid") : TEXT("None"),
		CameraComponent ? TEXT("Valid") : TEXT("None"),
		MarkerManagerComponent ? TEXT("Valid") : TEXT("None"),
		ActorControlComponent ? TEXT("Valid") : TEXT("None")
	);
}

void UWebUIFoundationBridgeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllMarkers();

	Super::EndPlay(EndPlayReason);
}

void UWebUIFoundationBridgeComponent::CacheFeatureComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	CameraComponent = Owner->FindComponentByClass<UWebUIFoundationCameraComponent>();
	MarkerManagerComponent = Owner->FindComponentByClass<UWebUIFoundationMarkerManagerComponent>();
	ActorControlComponent = Owner->FindComponentByClass<UWebUIFoundationActorControlComponent>();
}

void UWebUIFoundationBridgeComponent::HandleWebUIFoundationMessage(const FString& DescriptorJson)
{
	UE_LOG(LogTemp, Warning, TEXT("[WebUIFoundation] Received: %s"), *DescriptorJson);

	if (!CameraComponent || !MarkerManagerComponent || !ActorControlComponent)
	{
		CacheFeatureComponents();
	}

	TSharedPtr<FJsonObject> RootObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(DescriptorJson);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		SendError(TEXT("unknown"), TEXT("invalidJson"), TEXT("Invalid JSON from WebUIFoundation."));
		return;
	}

	FString Source;
	if (!RootObject->TryGetStringField(TEXT("source"), Source))
	{
		SendError(TEXT("unknown"), TEXT("invalidMessage"), TEXT("Missing source field."));
		return;
	}

	if (Source != TEXT("enginejs"))
	{
		return;
	}

	FString Type;
	if (!RootObject->TryGetStringField(TEXT("type"), Type))
	{
		SendError(TEXT("unknown"), TEXT("invalidMessage"), TEXT("Missing type field."));
		return;
	}

	const FString RequestId = RootObject->HasField(TEXT("requestId"))
		? RootObject->GetStringField(TEXT("requestId"))
		: TEXT("unknown");

	const TSharedPtr<FJsonObject>* PayloadPtr = nullptr;
	TSharedPtr<FJsonObject> Payload;

	if (RootObject->TryGetObjectField(TEXT("payload"), PayloadPtr))
	{
		Payload = *PayloadPtr;
	}
	else
	{
		Payload = MakeShared<FJsonObject>();
	}

	// ===== Camera / input commands =====
	if (Type == TEXT("flyTo"))
	{
		CameraComponent
			? CameraComponent->HandleFlyTo(this, RequestId, Payload)
			: SendError(RequestId, TEXT("flyToResult"), TEXT("Camera component not found."));
	}
	else if (Type == TEXT("resetView"))
	{
		CameraComponent
			? CameraComponent->HandleResetView(this, RequestId)
			: SendError(RequestId, TEXT("resetViewResult"), TEXT("Camera component not found."));
	}
	else if (Type == TEXT("setInputEnabled"))
	{
		CameraComponent
			? CameraComponent->HandleSetInputEnabled(this, RequestId, Payload)
			: SendError(RequestId, TEXT("setInputEnabledResult"), TEXT("Camera component not found."));
	}
	else if (Type == TEXT("setMouseWheelMoveConfig"))
	{
		CameraComponent
			? CameraComponent->HandleSetMouseWheelMoveConfig(this, RequestId, Payload)
			: SendError(RequestId, TEXT("setMouseWheelMoveConfigResult"), TEXT("Camera component not found."));
	}
	else if (Type == TEXT("getMouseWheelMoveConfig"))
	{
		CameraComponent
			? CameraComponent->HandleGetMouseWheelMoveConfig(this, RequestId)
			: SendError(RequestId, TEXT("getMouseWheelMoveConfigResult"), TEXT("Camera component not found."));
	}
	else if (Type == TEXT("getViewPosition"))
	{
		CameraComponent
			? CameraComponent->HandleGetViewPosition(this, RequestId)
			: SendError(RequestId, TEXT("getViewPositionResult"), TEXT("Camera component not found."));
	}
	else if (Type == TEXT("setWebUIInputMode"))
	{
		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		UWebUIRuntimeSubsystem* Runtime = GI
			? GI->GetSubsystem<UWebUIRuntimeSubsystem>()
			: nullptr;

		if (!Runtime)
		{
			SendError(RequestId, TEXT("setWebUIInputModeResult"), TEXT("WebUIRuntimeSubsystem not found."));
		}
		else
		{
			const FString ModeText = GetPayloadString(Payload, TEXT("mode"), TEXT("ui")).ToLower();
			const EWebUIInputMode NewMode =
				(ModeText == TEXT("scene") || ModeText == TEXT("game"))
					? EWebUIInputMode::Game
					: EWebUIInputMode::UI;
			const bool bApplied = Runtime->SetWebUIInputMode(NewMode, false);

			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(TEXT("mode"), NewMode == EWebUIInputMode::UI ? TEXT("ui") : TEXT("scene"));
			Data->SetStringField(TEXT("toggleKey"), Runtime->GetInputModeToggleKeyName());
			Data->SetBoolField(TEXT("appliedToPlayerController"), bApplied);
			SendSuccess(RequestId, TEXT("setWebUIInputModeResult"), Data);
		}
	}
	else if (Type == TEXT("setWebUIInputRegions"))
	{
		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		UWebUIRuntimeSubsystem* Runtime = GI
			? GI->GetSubsystem<UWebUIRuntimeSubsystem>()
			: nullptr;

		if (!Runtime)
		{
			SendError(RequestId, TEXT("setWebUIInputRegionsResult"), TEXT("WebUIRuntimeSubsystem not found."));
		}
		else
		{
			TArray<FBox2D> Regions;
			const TArray<TSharedPtr<FJsonValue>>* RegionValues = nullptr;
			if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("regions"), RegionValues) && RegionValues)
			{
				const int32 MaxRegions = FMath::Min(RegionValues->Num(), 256);
				Regions.Reserve(MaxRegions);

				for (int32 Index = 0; Index < MaxRegions; ++Index)
				{
					const TSharedPtr<FJsonObject> RegionObject = (*RegionValues)[Index].IsValid()
						? (*RegionValues)[Index]->AsObject()
						: nullptr;
					if (!RegionObject.IsValid())
					{
						continue;
					}

					double Left = 0.0;
					double Top = 0.0;
					double Right = 0.0;
					double Bottom = 0.0;
					if (
						RegionObject->TryGetNumberField(TEXT("left"), Left) &&
						RegionObject->TryGetNumberField(TEXT("top"), Top) &&
						RegionObject->TryGetNumberField(TEXT("right"), Right) &&
						RegionObject->TryGetNumberField(TEXT("bottom"), Bottom)
					)
					{
						Regions.Emplace(
							FVector2D(Left, Top),
							FVector2D(Right, Bottom)
						);
					}
				}
			}

			Runtime->SetWebUIInputRegions(Regions);

			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetNumberField(TEXT("regionCount"), Runtime->GetWebUIInputRegionCount());
			Data->SetStringField(
				TEXT("mode"),
				Runtime->GetWebUIInputMode() == EWebUIInputMode::UI ? TEXT("ui") : TEXT("scene")
			);
			SendSuccess(RequestId, TEXT("setWebUIInputRegionsResult"), Data);
		}
	}
	else if (Type == TEXT("getWebUIInputMode"))
	{
		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		UWebUIRuntimeSubsystem* Runtime = GI
			? GI->GetSubsystem<UWebUIRuntimeSubsystem>()
			: nullptr;

		if (!Runtime)
		{
			SendError(RequestId, TEXT("getWebUIInputModeResult"), TEXT("WebUIRuntimeSubsystem not found."));
		}
		else
		{
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(
				TEXT("mode"),
				Runtime->GetWebUIInputMode() == EWebUIInputMode::UI ? TEXT("ui") : TEXT("scene")
			);
			Data->SetStringField(TEXT("toggleKey"), Runtime->GetInputModeToggleKeyName());
			SendSuccess(RequestId, TEXT("getWebUIInputModeResult"), Data);
		}
	}
	else if (Type == TEXT("toggleWebUIInputMode"))
	{
		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		UWebUIRuntimeSubsystem* Runtime = GI
			? GI->GetSubsystem<UWebUIRuntimeSubsystem>()
			: nullptr;

		if (!Runtime)
		{
			SendError(RequestId, TEXT("toggleWebUIInputModeResult"), TEXT("WebUIRuntimeSubsystem not found."));
		}
		else
		{
			const bool bApplied = Runtime->ToggleWebUIInputMode(false);
			TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
			Data->SetStringField(
				TEXT("mode"),
				Runtime->GetWebUIInputMode() == EWebUIInputMode::UI ? TEXT("ui") : TEXT("scene")
			);
			Data->SetStringField(TEXT("toggleKey"), Runtime->GetInputModeToggleKeyName());
			Data->SetBoolField(TEXT("appliedToPlayerController"), bApplied);
			SendSuccess(RequestId, TEXT("toggleWebUIInputModeResult"), Data);
		}
	}

	// ===== Marker commands =====
	else if (Type == TEXT("addMarker"))
	{
		MarkerManagerComponent
			? MarkerManagerComponent->HandleAddMarker(this, RequestId, Payload)
			: SendError(RequestId, TEXT("addMarkerResult"), TEXT("Marker manager component not found."));
	}
	else if (Type == TEXT("updateMarker"))
	{
		MarkerManagerComponent
			? MarkerManagerComponent->HandleUpdateMarker(this, RequestId, Payload)
			: SendError(RequestId, TEXT("updateMarkerResult"), TEXT("Marker manager component not found."));
	}
	else if (Type == TEXT("removeMarker"))
	{
		MarkerManagerComponent
			? MarkerManagerComponent->HandleRemoveMarker(this, RequestId, Payload)
			: SendError(RequestId, TEXT("removeMarkerResult"), TEXT("Marker manager component not found."));
	}
	else if (Type == TEXT("clearMarkers"))
	{
		MarkerManagerComponent
			? MarkerManagerComponent->HandleClearMarkers(this, RequestId)
			: SendError(RequestId, TEXT("clearMarkersResult"), TEXT("Marker manager component not found."));
	}

	// ===== Scene actor commands =====
	else if (Type == TEXT("moveActor"))
	{
		ActorControlComponent
			? ActorControlComponent->HandleMoveActor(this, RequestId, Payload)
			: SendError(RequestId, TEXT("moveActorResult"), TEXT("Actor control component not found."));
	}
	else if (Type == TEXT("restoreActors"))
	{
		ActorControlComponent
			? ActorControlComponent->HandleRestoreActors(this, RequestId, Payload)
			: SendError(RequestId, TEXT("restoreActorsResult"), TEXT("Actor control component not found."));
	}
	else if (Type == TEXT("getActorInfo"))
	{
		ActorControlComponent
			? ActorControlComponent->HandleGetActorInfo(this, RequestId, Payload)
			: SendError(RequestId, TEXT("getActorInfoResult"), TEXT("Actor control component not found."));
	}

	// ===== Bridge/common commands =====
	else if (Type == TEXT("requestSceneState"))
	{
		HandleRequestSceneState(RequestId);
	}
	else if (Type == TEXT("info"))
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("info command received"));
		SendSuccess(RequestId, TEXT("infoResult"), Data);
	}
	else if (Type == TEXT("engineInit"))
	{
		TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("message"), TEXT("UE WebUIFoundation bridge ready"));
		Data->SetBoolField(TEXT("hasCesiumGeoreference"), CachedGeoreference != nullptr);
		Data->SetBoolField(TEXT("hasCameraComponent"), CameraComponent != nullptr);
		Data->SetBoolField(TEXT("hasMarkerManagerComponent"), MarkerManagerComponent != nullptr);
		Data->SetBoolField(TEXT("hasActorControlComponent"), ActorControlComponent != nullptr);
		Data->SetBoolField(TEXT("supportsRestoreActors"), ActorControlComponent != nullptr);
		Data->SetBoolField(TEXT("supportsAnyActorMovement"), ActorControlComponent != nullptr);
		Data->SetBoolField(TEXT("supportsAttachedActorHierarchy"), ActorControlComponent != nullptr);
		Data->SetBoolField(TEXT("supportsRelativeRestoreForAttachedActors"), ActorControlComponent != nullptr);
		Data->SetBoolField(TEXT("supportsGetActorInfo"), ActorControlComponent != nullptr);
		Data->SetBoolField(TEXT("supportsAttachedMarkers"), MarkerManagerComponent != nullptr);
		Data->SetBoolField(TEXT("supportsMarkerVisualStyle"), MarkerManagerComponent != nullptr);
		Data->SetBoolField(TEXT("supportsMarkerImageAsset"), MarkerManagerComponent != nullptr);
		Data->SetBoolField(TEXT("supportsScreenSpaceMarkers"), MarkerManagerComponent != nullptr);
		Data->SetBoolField(TEXT("supportsBadgeMarkerLayout"), MarkerManagerComponent != nullptr);
		Data->SetBoolField(TEXT("supportsGetViewPosition"), CameraComponent != nullptr && CachedGeoreference != nullptr);

		UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		UWebUIRuntimeSubsystem* Runtime = GI
			? GI->GetSubsystem<UWebUIRuntimeSubsystem>()
			: nullptr;
		Data->SetBoolField(TEXT("supportsWebUIInputMode"), Runtime != nullptr);
		const UWebUIBridgeSettings* WebUISettings = GetDefault<UWebUIBridgeSettings>();
		Data->SetBoolField(
			TEXT("supportsAutomaticWebUIInputRouting"),
			Runtime != nullptr
		);
		Data->SetBoolField(
			TEXT("automaticWebUIInputRoutingEnabled"),
			WebUISettings && WebUISettings->bEnableAutomaticInputRouting
		);
		Data->SetBoolField(TEXT("supportsWebUIInputHitRegions"), Runtime != nullptr);
		if (Runtime)
		{
			Data->SetNumberField(TEXT("webUIInputRegionCount"), Runtime->GetWebUIInputRegionCount());
		}
		if (Runtime)
		{
			Data->SetStringField(
				TEXT("webUIInputMode"),
				Runtime->GetWebUIInputMode() == EWebUIInputMode::UI ? TEXT("ui") : TEXT("scene")
			);
			Data->SetStringField(TEXT("webUIInputToggleKey"), Runtime->GetInputModeToggleKeyName());
		}
		SendSuccess(RequestId, TEXT("engineInitResult"), Data);
	}
	else
	{
		SendError(
			RequestId,
			Type + TEXT("Result"),
			FString::Printf(TEXT("Unknown command: %s"), *Type)
		);
	}
}

void UWebUIFoundationBridgeComponent::ClearAllMarkers()
{
	if (!MarkerManagerComponent)
	{
		CacheFeatureComponents();
	}

	if (MarkerManagerComponent)
	{
		MarkerManagerComponent->ClearAllMarkers();
	}
}

bool UWebUIFoundationBridgeComponent::GetPayloadNumber(
	const TSharedPtr<FJsonObject>& Payload,
	const FString& Key,
	double& OutValue
) const
{
	if (!Payload.IsValid())
	{
		return false;
	}

	return Payload->TryGetNumberField(Key, OutValue);
}

bool UWebUIFoundationBridgeComponent::GetPayloadBool(
	const TSharedPtr<FJsonObject>& Payload,
	const FString& Key,
	bool& OutValue
) const
{
	if (!Payload.IsValid())
	{
		return false;
	}

	return Payload->TryGetBoolField(Key, OutValue);
}

FString UWebUIFoundationBridgeComponent::GetPayloadString(
	const TSharedPtr<FJsonObject>& Payload,
	const FString& Key,
	const FString& DefaultValue
) const
{
	if (!Payload.IsValid())
	{
		return DefaultValue;
	}

	FString Value;
	if (Payload->TryGetStringField(Key, Value))
	{
		return Value;
	}

	return DefaultValue;
}

FVector UWebUIFoundationBridgeComponent::ConvertLonLatHeightToWorld(
	double Lon,
	double Lat,
	double Height
) const
{
	if (CachedGeoreference)
	{
		return CachedGeoreference->TransformLongitudeLatitudeHeightPositionToUnreal(
			FVector(Lon, Lat, Height)
		);
	}

	const double Scale = 100000.0;
	return FVector(Lon * Scale, Lat * Scale, Height);
}

bool UWebUIFoundationBridgeComponent::ConvertWorldToLonLatHeight(
	const FVector& WorldPosition,
	FVector& OutLongitudeLatitudeHeight
) const
{
	OutLongitudeLatitudeHeight = FVector::ZeroVector;

	if (!CachedGeoreference)
	{
		return false;
	}

	OutLongitudeLatitudeHeight =
		CachedGeoreference->TransformUnrealPositionToLongitudeLatitudeHeight(WorldPosition);
	return true;
}

APlayerController* UWebUIFoundationBridgeComponent::GetMainPlayerController() const
{
	return UGameplayStatics::GetPlayerController(GetWorld(), 0);
}

APawn* UWebUIFoundationBridgeComponent::GetControlledPawn() const
{
	APlayerController* PC = GetMainPlayerController();
	return PC ? PC->GetPawn() : nullptr;
}

void UWebUIFoundationBridgeComponent::HandleRequestSceneState(const FString& RequestId)
{
	APlayerController* PC = GetMainPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	AActor* ViewTarget = PC ? PC->GetViewTarget() : nullptr;

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetBoolField(TEXT("hasCesiumGeoreference"), CachedGeoreference != nullptr);
	Data->SetBoolField(TEXT("hasPlayerController"), PC != nullptr);
	Data->SetBoolField(TEXT("hasPawn"), Pawn != nullptr);
	Data->SetStringField(TEXT("viewTarget"), ViewTarget ? ViewTarget->GetName() : TEXT("None"));

	if (CameraComponent)
	{
		CameraComponent->AppendSceneState(Data);
	}

	if (MarkerManagerComponent)
	{
		MarkerManagerComponent->AppendSceneState(Data);
	}

	if (ActorControlComponent)
	{
		ActorControlComponent->AppendSceneState(Data);
	}

	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UWebUIRuntimeSubsystem* Runtime = GI ? GI->GetSubsystem<UWebUIRuntimeSubsystem>() : nullptr)
	{
		const UWebUIBridgeSettings* WebUISettings = GetDefault<UWebUIBridgeSettings>();
		Data->SetBoolField(
			TEXT("automaticWebUIInputRoutingEnabled"),
			WebUISettings && WebUISettings->bEnableAutomaticInputRouting
		);
		Data->SetStringField(
			TEXT("webUIInputMode"),
			Runtime->GetWebUIInputMode() == EWebUIInputMode::UI ? TEXT("ui") : TEXT("scene")
		);
		Data->SetStringField(TEXT("webUIInputToggleKey"), Runtime->GetInputModeToggleKeyName());
	}

	if (Pawn)
	{
		const FVector Loc = Pawn->GetActorLocation();
		const FRotator Rot = Pawn->GetActorRotation();

		Data->SetNumberField(TEXT("pawnX"), Loc.X);
		Data->SetNumberField(TEXT("pawnY"), Loc.Y);
		Data->SetNumberField(TEXT("pawnZ"), Loc.Z);
		Data->SetNumberField(TEXT("pawnPitch"), Rot.Pitch);
		Data->SetNumberField(TEXT("pawnYaw"), Rot.Yaw);
		Data->SetNumberField(TEXT("pawnRoll"), Rot.Roll);
	}

	SendSuccess(RequestId, TEXT("sceneStateResult"), Data);
}

void UWebUIFoundationBridgeComponent::SendSuccess(
	const FString& RequestId,
	const FString& Type,
	const TSharedPtr<FJsonObject>& Payload
)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("source"), TEXT("ue"));
	Root->SetStringField(TEXT("type"), Type);
	Root->SetStringField(TEXT("requestId"), RequestId);
	Root->SetBoolField(TEXT("success"), true);

	if (Payload.IsValid())
	{
		Root->SetObjectField(TEXT("payload"), Payload);
	}
	else
	{
		Root->SetObjectField(TEXT("payload"), MakeShared<FJsonObject>());
	}

	const FString Response = ToJsonString(Root);
	UE_LOG(LogTemp, Warning, TEXT("[WebUIFoundation] SendSuccess: %s"), *Response);
	OnResponseReady.Broadcast(Response);
}

void UWebUIFoundationBridgeComponent::SendError(
	const FString& RequestId,
	const FString& Type,
	const FString& Message
)
{
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("message"), Message);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("source"), TEXT("ue"));
	Root->SetStringField(TEXT("type"), Type);
	Root->SetStringField(TEXT("requestId"), RequestId);
	Root->SetBoolField(TEXT("success"), false);
	Root->SetObjectField(TEXT("payload"), Payload);

	const FString Response = ToJsonString(Root);
	UE_LOG(LogTemp, Error, TEXT("[WebUIFoundation] SendError: %s"), *Response);
	OnResponseReady.Broadcast(Response);
}

FString UWebUIFoundationBridgeComponent::ToJsonString(const TSharedPtr<FJsonObject>& Object) const
{
	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
	return Output;
}
