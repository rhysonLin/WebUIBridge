#include "Foundation/WebUIFoundationActorControlComponent.h"

#include "Foundation/WebUIFoundationBridgeComponent.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Json.h"

namespace WebUIFoundationActorControl
{
	static FString NormalizeToken(const FString& Value)
	{
		FString Result = Value.TrimStartAndEnd().ToLower();
		Result.ReplaceInline(TEXT(" "), TEXT(""));
		Result.ReplaceInline(TEXT("_"), TEXT(""));
		Result.ReplaceInline(TEXT("-axis"), TEXT(""));
		return Result;
	}

	static bool IsExactActorNameMatch(const AActor* Actor, const FString& TargetName, FString& OutMatchedBy)
	{
		if (!IsValid(Actor))
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

	static bool IsPartialActorNameMatch(const AActor* Actor, const FString& TargetName, FString& OutMatchedBy)
	{
		if (!IsValid(Actor))
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

	static FString GetActorDisplayName(const AActor* Actor)
	{
		if (!Actor)
		{
			return TEXT("None");
		}

#if WITH_EDITOR
		return FString::Printf(TEXT("%s[%s]"), *Actor->GetActorLabel(), *Actor->GetName());
#else
		return Actor->GetName();
#endif
	}
}

UWebUIFoundationActorControlComponent::UWebUIFoundationActorControlComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	bAutoSetActorMovable = true;
	bAutoSetAttachedActorsMovable = true;
	bAllowPartialNameMatch = false;
	MaxAbsoluteMoveDistance = 1000000.0;
}

void UWebUIFoundationActorControlComponent::HandleMoveActor(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	const FString ResultType = TEXT("moveActorResult");
	const FString TargetName = Bridge->GetPayloadString(
		Payload,
		TEXT("actorName"),
		Bridge->GetPayloadString(Payload, TEXT("name"), TEXT(""))
	).TrimStartAndEnd();

	if (TargetName.IsEmpty())
	{
		Bridge->SendError(RequestId, ResultType, TEXT("Missing payload.actorName."));
		return;
	}

	const FString DirectionText = Bridge->GetPayloadString(Payload, TEXT("direction"), TEXT("")).TrimStartAndEnd();
	if (DirectionText.IsEmpty())
	{
		Bridge->SendError(RequestId, ResultType, TEXT("Missing payload.direction."));
		return;
	}

	double Distance = 0.0;
	if (!TryGetMoveDistance(Bridge, Payload, Distance))
	{
		Bridge->SendError(
			RequestId,
			ResultType,
			TEXT("Missing numeric payload.distance. Aliases amount, value, and parameter are also supported.")
		);
		return;
	}

	if (!FMath::IsFinite(Distance))
	{
		Bridge->SendError(RequestId, ResultType, TEXT("Move distance must be a finite number."));
		return;
	}

	if (MaxAbsoluteMoveDistance > 0.0 && FMath::Abs(Distance) > MaxAbsoluteMoveDistance)
	{
		Bridge->SendError(
			RequestId,
			ResultType,
			FString::Printf(
				TEXT("Move distance %.3f exceeds MaxAbsoluteMoveDistance %.3f."),
				Distance,
				MaxAbsoluteMoveDistance
			)
		);
		return;
	}

	bool bAllowPartialMatch = bAllowPartialNameMatch;
	Bridge->GetPayloadBool(Payload, TEXT("allowPartialMatch"), bAllowPartialMatch);

	FString MatchedBy;
	FString FindError;
	AActor* TargetActor = FindTargetActor(TargetName, bAllowPartialMatch, MatchedBy, FindError);
	if (!TargetActor)
	{
		Bridge->SendError(RequestId, ResultType, FindError);
		return;
	}

	if (!TargetActor->GetRootComponent())
	{
		Bridge->SendError(
			RequestId,
			ResultType,
			FString::Printf(TEXT("Actor %s has no RootComponent and cannot be moved."), *TargetActor->GetName())
		);
		return;
	}

	const FString SpaceText = Bridge->GetPayloadString(Payload, TEXT("space"), TEXT("world"));
	FVector MoveDirection = FVector::ZeroVector;
	FString NormalizedDirection;
	FString NormalizedSpace;
	FString DirectionError;

	if (!BuildMoveDirection(
		DirectionText,
		SpaceText,
		TargetActor,
		MoveDirection,
		NormalizedDirection,
		NormalizedSpace,
		DirectionError
	))
	{
		Bridge->SendError(RequestId, ResultType, DirectionError);
		return;
	}

	// 第一次移动时缓存。附着对象保存相对 Transform，独立对象保存世界 Transform。
	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	if (!OriginalActorTransforms.Contains(TargetKey))
	{
		FWebUIFoundationTrackedActorTransform Tracked;
		Tracked.OriginalWorldTransform = TargetActor->GetActorTransform();
		Tracked.OriginalAttachParent = TargetActor->GetAttachParentActor();
		Tracked.bWasAttached = Tracked.OriginalAttachParent.IsValid();
		Tracked.OriginalRelativeTransform = TargetActor->GetRootComponent()->GetRelativeTransform();
		OriginalActorTransforms.Add(TargetKey, Tracked);

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[WebUIFoundation] Cached actor transform. Actor=%s Parent=%s Mode=%s World=%s Relative=%s"),
			*TargetActor->GetName(),
			Tracked.OriginalAttachParent.IsValid() ? *Tracked.OriginalAttachParent->GetName() : TEXT("None"),
			Tracked.bWasAttached ? TEXT("relative") : TEXT("world"),
			*Tracked.OriginalWorldTransform.ToHumanReadableString(),
			*Tracked.OriginalRelativeTransform.ToHumanReadableString()
		);
	}

	int32 ChangedComponentCount = 0;
	int32 VisitedActorCount = 0;
	FString MobilityError;
	if (!EnsureActorHierarchyMovable(
		TargetActor,
		bAutoSetAttachedActorsMovable,
		ChangedComponentCount,
		VisitedActorCount,
		MobilityError
	))
	{
		Bridge->SendError(RequestId, ResultType, MobilityError);
		return;
	}

	bool bSweep = false;
	Bridge->GetPayloadBool(Payload, TEXT("sweep"), bSweep);

	const FVector OldLocation = TargetActor->GetActorLocation();
	const FVector OldRelativeLocation = TargetActor->GetRootComponent()->GetRelativeLocation();
	const FVector Delta = MoveDirection.GetSafeNormal() * Distance;
	const FVector RequestedLocation = OldLocation + Delta;

	FHitResult HitResult;
	const bool bMoveSucceeded = TargetActor->SetActorLocation(
		RequestedLocation,
		bSweep,
		&HitResult,
		ETeleportType::TeleportPhysics
	);

	const FVector NewLocation = TargetActor->GetActorLocation();
	const FVector NewRelativeLocation = TargetActor->GetRootComponent()->GetRelativeLocation();

	if (!bMoveSucceeded)
	{
		Bridge->SendError(
			RequestId,
			ResultType,
			FString::Printf(
				TEXT("SetActorLocation failed for %s. Check mobility, attachment, collision, or level state."),
				*TargetActor->GetName()
			)
		);
		return;
	}

	TArray<AActor*> DirectChildren;
	TargetActor->GetAttachedActors(DirectChildren, true, false);
	const int32 DescendantCount = GetAttachedDescendantCount(TargetActor);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("requestedName"), TargetName);
	Data->SetStringField(TEXT("actorName"), TargetActor->GetName());
	Data->SetStringField(TEXT("actorClass"), TargetActor->GetClass()->GetName());
	Data->SetStringField(TEXT("matchedBy"), MatchedBy);
	Data->SetStringField(TEXT("direction"), NormalizedDirection);
	Data->SetStringField(TEXT("space"), NormalizedSpace);
	Data->SetNumberField(TEXT("distance"), Distance);
	Data->SetBoolField(TEXT("sweep"), bSweep);
	Data->SetBoolField(TEXT("isAttached"), TargetActor->GetAttachParentActor() != nullptr);
	Data->SetStringField(
		TEXT("parentActor"),
		TargetActor->GetAttachParentActor() ? TargetActor->GetAttachParentActor()->GetName() : TEXT("None")
	);
	Data->SetNumberField(TEXT("directChildCount"), DirectChildren.Num());
	Data->SetNumberField(TEXT("descendantCount"), DescendantCount);
	Data->SetNumberField(TEXT("mobilityChangedComponentCount"), ChangedComponentCount);
	Data->SetNumberField(TEXT("mobilityVisitedActorCount"), VisitedActorCount);

	Data->SetNumberField(TEXT("oldX"), OldLocation.X);
	Data->SetNumberField(TEXT("oldY"), OldLocation.Y);
	Data->SetNumberField(TEXT("oldZ"), OldLocation.Z);
	Data->SetNumberField(TEXT("newX"), NewLocation.X);
	Data->SetNumberField(TEXT("newY"), NewLocation.Y);
	Data->SetNumberField(TEXT("newZ"), NewLocation.Z);
	Data->SetNumberField(TEXT("deltaX"), NewLocation.X - OldLocation.X);
	Data->SetNumberField(TEXT("deltaY"), NewLocation.Y - OldLocation.Y);
	Data->SetNumberField(TEXT("deltaZ"), NewLocation.Z - OldLocation.Z);

	Data->SetNumberField(TEXT("oldRelativeX"), OldRelativeLocation.X);
	Data->SetNumberField(TEXT("oldRelativeY"), OldRelativeLocation.Y);
	Data->SetNumberField(TEXT("oldRelativeZ"), OldRelativeLocation.Z);
	Data->SetNumberField(TEXT("newRelativeX"), NewRelativeLocation.X);
	Data->SetNumberField(TEXT("newRelativeY"), NewRelativeLocation.Y);
	Data->SetNumberField(TEXT("newRelativeZ"), NewRelativeLocation.Z);

	if (bSweep)
	{
		Data->SetBoolField(TEXT("blockingHit"), HitResult.bBlockingHit);
		Data->SetStringField(
			TEXT("hitActor"),
			HitResult.GetActor() ? HitResult.GetActor()->GetName() : TEXT("None")
		);
	}

#if WITH_EDITOR
	Data->SetStringField(TEXT("actorLabel"), TargetActor->GetActorLabel());
#endif

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation] moveActor success. Requested=%s Actor=%s Parent=%s Descendants=%d Direction=%s Space=%s Distance=%.3f Old=%s New=%s"),
		*TargetName,
		*TargetActor->GetName(),
		TargetActor->GetAttachParentActor() ? *TargetActor->GetAttachParentActor()->GetName() : TEXT("None"),
		DescendantCount,
		*NormalizedDirection,
		*NormalizedSpace,
		Distance,
		*OldLocation.ToString(),
		*NewLocation.ToString()
	);

	Bridge->SendSuccess(RequestId, ResultType, Data);
}

void UWebUIFoundationActorControlComponent::HandleRestoreActors(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
)
{
	if (!Bridge)
	{
		return;
	}

	const FString ResultType = TEXT("restoreActorsResult");
	const FString TargetName = Bridge->GetPayloadString(
		Payload,
		TEXT("actorName"),
		Bridge->GetPayloadString(Payload, TEXT("name"), TEXT(""))
	).TrimStartAndEnd();

	bool bAllowPartialMatch = bAllowPartialNameMatch;
	Bridge->GetPayloadBool(Payload, TEXT("allowPartialMatch"), bAllowPartialMatch);

	int32 RestoredCount = 0;
	int32 InvalidCount = 0;
	TArray<TSharedPtr<FJsonValue>> RestoredActorsJson;

	auto AppendRestoredActor = [&RestoredActorsJson](
		AActor* Actor,
		const FVector& OldLocation,
		const FVector& RestoredLocation
	)
	{
		TSharedPtr<FJsonObject> ActorData = MakeShared<FJsonObject>();
		ActorData->SetStringField(TEXT("actorName"), Actor ? Actor->GetName() : TEXT("None"));
		ActorData->SetNumberField(TEXT("oldX"), OldLocation.X);
		ActorData->SetNumberField(TEXT("oldY"), OldLocation.Y);
		ActorData->SetNumberField(TEXT("oldZ"), OldLocation.Z);
		ActorData->SetNumberField(TEXT("restoredX"), RestoredLocation.X);
		ActorData->SetNumberField(TEXT("restoredY"), RestoredLocation.Y);
		ActorData->SetNumberField(TEXT("restoredZ"), RestoredLocation.Z);

#if WITH_EDITOR
		if (Actor)
		{
			ActorData->SetStringField(TEXT("actorLabel"), Actor->GetActorLabel());
		}
#endif

		RestoredActorsJson.Add(MakeShared<FJsonValueObject>(ActorData));
	};

	if (!TargetName.IsEmpty())
	{
		FString MatchedBy;
		FString FindError;
		AActor* TargetActor = FindTargetActor(TargetName, bAllowPartialMatch, MatchedBy, FindError);
		if (!TargetActor)
		{
			Bridge->SendError(RequestId, ResultType, FindError);
			return;
		}

		FVector OldLocation = FVector::ZeroVector;
		FVector RestoredLocation = FVector::ZeroVector;
		FString RestoreError;
		if (!RestoreTrackedActor(TargetActor, OldLocation, RestoredLocation, RestoreError))
		{
			Bridge->SendError(RequestId, ResultType, RestoreError);
			return;
		}

		RestoredCount = 1;
		AppendRestoredActor(TargetActor, OldLocation, RestoredLocation);
	}
	else
	{
		TArray<TWeakObjectPtr<AActor>> InvalidKeys;
		TArray<AActor*> ActorsToRestore;

		for (const TPair<TWeakObjectPtr<AActor>, FWebUIFoundationTrackedActorTransform>& Pair : OriginalActorTransforms)
		{
			AActor* Actor = Pair.Key.Get();
			if (!IsValid(Actor))
			{
				InvalidKeys.Add(Pair.Key);
				++InvalidCount;
				continue;
			}
			ActorsToRestore.Add(Actor);
		}

		// 父级先恢复、子级后恢复。子级使用相对 Transform，因此结果与楼层当前世界位置无关。
		ActorsToRestore.Sort([](const AActor& A, const AActor& B)
		{
			return GetAttachmentDepth(&A) < GetAttachmentDepth(&B);
		});

		for (AActor* TargetActor : ActorsToRestore)
		{
			FVector OldLocation = FVector::ZeroVector;
			FVector RestoredLocation = FVector::ZeroVector;
			FString RestoreError;

			if (RestoreTrackedActor(TargetActor, OldLocation, RestoredLocation, RestoreError))
			{
				++RestoredCount;
				AppendRestoredActor(TargetActor, OldLocation, RestoredLocation);
			}
			else
			{
				UE_LOG(
					LogTemp,
					Error,
					TEXT("[WebUIFoundation] restoreActors failed for Actor=%s Error=%s"),
					*TargetActor->GetName(),
					*RestoreError
				);
			}
		}

		for (const TWeakObjectPtr<AActor>& InvalidKey : InvalidKeys)
		{
			OriginalActorTransforms.Remove(InvalidKey);
		}
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetNumberField(TEXT("restoredCount"), RestoredCount);
	Data->SetNumberField(TEXT("invalidCount"), InvalidCount);
	Data->SetNumberField(TEXT("trackedActorCount"), OriginalActorTransforms.Num());
	Data->SetArrayField(TEXT("actors"), RestoredActorsJson);
	Data->SetStringField(
		TEXT("message"),
		RestoredCount > 0 ? TEXT("actors restored") : TEXT("no moved actors to restore")
	);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation] restoreActors complete. Requested=%s Restored=%d Invalid=%d Tracked=%d"),
		TargetName.IsEmpty() ? TEXT("ALL") : *TargetName,
		RestoredCount,
		InvalidCount,
		OriginalActorTransforms.Num()
	);

	Bridge->SendSuccess(RequestId, ResultType, Data);
}

void UWebUIFoundationActorControlComponent::HandleGetActorInfo(
	UWebUIFoundationBridgeComponent* Bridge,
	const FString& RequestId,
	const TSharedPtr<FJsonObject>& Payload
) const
{
	if (!Bridge)
	{
		return;
	}

	const FString ResultType = TEXT("getActorInfoResult");
	const FString TargetName = Bridge->GetPayloadString(
		Payload,
		TEXT("actorName"),
		Bridge->GetPayloadString(Payload, TEXT("name"), TEXT(""))
	).TrimStartAndEnd();

	if (TargetName.IsEmpty())
	{
		Bridge->SendError(RequestId, ResultType, TEXT("Missing payload.actorName."));
		return;
	}

	bool bAllowPartialMatch = bAllowPartialNameMatch;
	Bridge->GetPayloadBool(Payload, TEXT("allowPartialMatch"), bAllowPartialMatch);

	FString MatchedBy;
	FString FindError;
	AActor* TargetActor = FindTargetActor(TargetName, bAllowPartialMatch, MatchedBy, FindError);
	if (!TargetActor)
	{
		Bridge->SendError(RequestId, ResultType, FindError);
		return;
	}

	USceneComponent* RootComponent = TargetActor->GetRootComponent();
	TArray<AActor*> DirectChildren;
	TargetActor->GetAttachedActors(DirectChildren, true, false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("requestedName"), TargetName);
	Data->SetStringField(TEXT("actorName"), TargetActor->GetName());
	Data->SetStringField(TEXT("actorClass"), TargetActor->GetClass()->GetName());
	Data->SetStringField(TEXT("matchedBy"), MatchedBy);
	Data->SetBoolField(TEXT("hasRootComponent"), RootComponent != nullptr);
	Data->SetBoolField(TEXT("isAttached"), TargetActor->GetAttachParentActor() != nullptr);
	Data->SetStringField(
		TEXT("parentActor"),
		TargetActor->GetAttachParentActor() ? TargetActor->GetAttachParentActor()->GetName() : TEXT("None")
	);
	Data->SetNumberField(TEXT("attachmentDepth"), GetAttachmentDepth(TargetActor));
	Data->SetNumberField(TEXT("directChildCount"), DirectChildren.Num());
	Data->SetNumberField(TEXT("descendantCount"), GetAttachedDescendantCount(TargetActor));
	Data->SetBoolField(TEXT("hasCachedRestoreTransform"), OriginalActorTransforms.Contains(TWeakObjectPtr<AActor>(TargetActor)));

	const FVector WorldLocation = TargetActor->GetActorLocation();
	Data->SetNumberField(TEXT("worldX"), WorldLocation.X);
	Data->SetNumberField(TEXT("worldY"), WorldLocation.Y);
	Data->SetNumberField(TEXT("worldZ"), WorldLocation.Z);

	if (RootComponent)
	{
		const FVector RelativeLocation = RootComponent->GetRelativeLocation();
		Data->SetNumberField(TEXT("relativeX"), RelativeLocation.X);
		Data->SetNumberField(TEXT("relativeY"), RelativeLocation.Y);
		Data->SetNumberField(TEXT("relativeZ"), RelativeLocation.Z);
		FString MobilityText = TEXT("Unknown");
		switch (RootComponent->Mobility)
		{
		case EComponentMobility::Static:
			MobilityText = TEXT("Static");
			break;
		case EComponentMobility::Stationary:
			MobilityText = TEXT("Stationary");
			break;
		case EComponentMobility::Movable:
			MobilityText = TEXT("Movable");
			break;
		default:
			break;
		}
		Data->SetStringField(TEXT("mobility"), MobilityText);
	}

	TArray<TSharedPtr<FJsonValue>> TagsJson;
	for (const FName& Tag : TargetActor->Tags)
	{
		TagsJson.Add(MakeShared<FJsonValueString>(Tag.ToString()));
	}
	Data->SetArrayField(TEXT("tags"), TagsJson);

	TArray<TSharedPtr<FJsonValue>> DirectChildrenJson;
	for (AActor* Child : DirectChildren)
	{
		if (IsValid(Child))
		{
			DirectChildrenJson.Add(MakeShared<FJsonValueString>(Child->GetName()));
		}
	}
	Data->SetArrayField(TEXT("directChildren"), DirectChildrenJson);

#if WITH_EDITOR
	Data->SetStringField(TEXT("actorLabel"), TargetActor->GetActorLabel());
#endif

	Bridge->SendSuccess(RequestId, ResultType, Data);
}

bool UWebUIFoundationActorControlComponent::RestoreTrackedActor(
	AActor* TargetActor,
	FVector& OutOldLocation,
	FVector& OutRestoredLocation,
	FString& OutError
)
{
	OutOldLocation = FVector::ZeroVector;
	OutRestoredLocation = FVector::ZeroVector;
	OutError.Reset();

	if (!IsValid(TargetActor) || !TargetActor->GetRootComponent())
	{
		OutError = TEXT("Target actor or its RootComponent is invalid.");
		return false;
	}

	const TWeakObjectPtr<AActor> TargetKey(TargetActor);
	const FWebUIFoundationTrackedActorTransform* Original = OriginalActorTransforms.Find(TargetKey);
	if (!Original)
	{
		OutError = FString::Printf(
			TEXT("Actor %s has no cached original transform. It must be moved through moveActor first."),
			*TargetActor->GetName()
		);
		return false;
	}

	int32 ChangedComponentCount = 0;
	int32 VisitedActorCount = 0;
	FString MobilityError;
	if (!EnsureActorHierarchyMovable(
		TargetActor,
		bAutoSetAttachedActorsMovable,
		ChangedComponentCount,
		VisitedActorCount,
		MobilityError
	))
	{
		OutError = MobilityError;
		return false;
	}

	OutOldLocation = TargetActor->GetActorLocation();

	if (Original->bWasAttached && Original->OriginalAttachParent.IsValid())
	{
		AActor* OriginalParent = Original->OriginalAttachParent.Get();
		if (TargetActor->GetAttachParentActor() != OriginalParent)
		{
			const bool bAttached = TargetActor->AttachToActor(
				OriginalParent,
				FAttachmentTransformRules::KeepWorldTransform
			);

			if (!bAttached)
			{
				OutError = FString::Printf(
					TEXT("Failed to reattach %s to original parent %s."),
					*TargetActor->GetName(),
					*OriginalParent->GetName()
				);
				return false;
			}
		}

		TargetActor->GetRootComponent()->SetRelativeTransform(
			Original->OriginalRelativeTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);
	}
	else
	{
		// 原始状态没有父级，恢复时也解除后来产生的 Attach 关系。
		if (TargetActor->GetAttachParentActor())
		{
			TargetActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}

		const bool bRestoreSucceeded = TargetActor->SetActorTransform(
			Original->OriginalWorldTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics
		);

		if (!bRestoreSucceeded)
		{
			OutError = FString::Printf(TEXT("SetActorTransform failed for %s."), *TargetActor->GetName());
			return false;
		}
	}

	OutRestoredLocation = TargetActor->GetActorLocation();
	return true;
}

void UWebUIFoundationActorControlComponent::AppendSceneState(const TSharedPtr<FJsonObject>& Data) const
{
	if (!Data.IsValid())
	{
		return;
	}

	int32 ValidTrackedActorCount = 0;
	for (const TPair<TWeakObjectPtr<AActor>, FWebUIFoundationTrackedActorTransform>& Pair : OriginalActorTransforms)
	{
		if (Pair.Key.IsValid())
		{
			++ValidTrackedActorCount;
		}
	}

	Data->SetBoolField(TEXT("hasActorControlComponent"), true);
	Data->SetBoolField(TEXT("actorControlSupportsAnyActor"), true);
	Data->SetBoolField(TEXT("actorControlSupportsAttachedHierarchy"), true);
	Data->SetBoolField(TEXT("actorControlUsesRelativeRestoreForAttachedActors"), true);
	Data->SetBoolField(TEXT("actorControlAutoSetMovable"), bAutoSetActorMovable);
	Data->SetBoolField(TEXT("actorControlAutoSetAttachedActorsMovable"), bAutoSetAttachedActorsMovable);
	Data->SetBoolField(TEXT("actorControlAllowPartialMatch"), bAllowPartialNameMatch);
	Data->SetNumberField(TEXT("actorControlMaxAbsoluteMoveDistance"), MaxAbsoluteMoveDistance);
	Data->SetNumberField(TEXT("actorControlRestorableActorCount"), ValidTrackedActorCount);
}

AActor* UWebUIFoundationActorControlComponent::FindTargetActor(
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
		if (!IsValid(Actor))
		{
			continue;
		}

		FString MatchSource;
		if (WebUIFoundationActorControl::IsExactActorNameMatch(Actor, TargetName, MatchSource))
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
			Names.Add(WebUIFoundationActorControl::GetActorDisplayName(ExactMatches[Index]));
		}

		OutError = FString::Printf(
			TEXT("Multiple Actors exactly match '%s': %s. Every controllable Actor Tag must be unique."),
			*TargetName,
			*FString::Join(Names, TEXT(", "))
		);
		return nullptr;
	}

	if (!bAllowPartialMatch)
	{
		OutError = FString::Printf(
			TEXT("Actor '%s' was not found. Match uses object name, Actor Tag, and editor-only Actor Label."),
			*TargetName
		);
		return nullptr;
	}

	TArray<AActor*> PartialMatches;
	TArray<FString> PartialMatchedBy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		FString MatchSource;
		if (WebUIFoundationActorControl::IsPartialActorNameMatch(Actor, TargetName, MatchSource))
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
			Names.Add(WebUIFoundationActorControl::GetActorDisplayName(PartialMatches[Index]));
		}

		OutError = FString::Printf(
			TEXT("Partial name '%s' matches multiple Actors: %s. Send an exact unique name or Actor Tag."),
			*TargetName,
			*FString::Join(Names, TEXT(", "))
		);
		return nullptr;
	}

	OutError = FString::Printf(TEXT("No Actor matches '%s'."), *TargetName);
	return nullptr;
}

bool UWebUIFoundationActorControlComponent::BuildMoveDirection(
	const FString& DirectionText,
	const FString& SpaceText,
	const AActor* TargetActor,
	FVector& OutDirection,
	FString& OutNormalizedDirection,
	FString& OutNormalizedSpace,
	FString& OutError
) const
{
	OutDirection = FVector::ZeroVector;
	OutNormalizedDirection.Reset();
	OutNormalizedSpace.Reset();
	OutError.Reset();

	if (!TargetActor)
	{
		OutError = TEXT("Target actor is invalid.");
		return false;
	}

	const FString Direction = WebUIFoundationActorControl::NormalizeToken(DirectionText);
	const FString Space = WebUIFoundationActorControl::NormalizeToken(SpaceText);

	const bool bLocalSpace =
		Space == TEXT("local") ||
		Space == TEXT("actor") ||
		Space == TEXT("relative") ||
		Space == TEXT("本地") ||
		Space == TEXT("自身");

	const bool bWorldSpace =
		Space.IsEmpty() ||
		Space == TEXT("world") ||
		Space == TEXT("global") ||
		Space == TEXT("世界") ||
		Space == TEXT("全局");

	if (!bLocalSpace && !bWorldSpace)
	{
		OutError = FString::Printf(TEXT("Unsupported space '%s'. Use world or local."), *SpaceText);
		return false;
	}

	OutNormalizedSpace = bLocalSpace ? TEXT("local") : TEXT("world");
	const FVector Forward = bLocalSpace ? TargetActor->GetActorForwardVector() : FVector::ForwardVector;
	const FVector Right = bLocalSpace ? TargetActor->GetActorRightVector() : FVector::RightVector;
	const FVector Up = bLocalSpace ? TargetActor->GetActorUpVector() : FVector::UpVector;

	if (
		Direction == TEXT("x") || Direction == TEXT("+x") || Direction == TEXT("forward") ||
		Direction == TEXT("front") || Direction == TEXT("前") || Direction == TEXT("前进") || Direction == TEXT("正x")
	)
	{
		OutDirection = Forward;
		OutNormalizedDirection = TEXT("forward");
	}
	else if (
		Direction == TEXT("-x") || Direction == TEXT("back") || Direction == TEXT("backward") ||
		Direction == TEXT("rear") || Direction == TEXT("后") || Direction == TEXT("后退") || Direction == TEXT("负x")
	)
	{
		OutDirection = -Forward;
		OutNormalizedDirection = TEXT("backward");
	}
	else if (
		Direction == TEXT("y") || Direction == TEXT("+y") || Direction == TEXT("right") ||
		Direction == TEXT("右") || Direction == TEXT("正y")
	)
	{
		OutDirection = Right;
		OutNormalizedDirection = TEXT("right");
	}
	else if (
		Direction == TEXT("-y") || Direction == TEXT("left") || Direction == TEXT("左") || Direction == TEXT("负y")
	)
	{
		OutDirection = -Right;
		OutNormalizedDirection = TEXT("left");
	}
	else if (
		Direction == TEXT("z") || Direction == TEXT("+z") || Direction == TEXT("up") ||
		Direction == TEXT("上") || Direction == TEXT("上升") || Direction == TEXT("正z")
	)
	{
		OutDirection = Up;
		OutNormalizedDirection = TEXT("up");
	}
	else if (
		Direction == TEXT("-z") || Direction == TEXT("down") || Direction == TEXT("下") ||
		Direction == TEXT("下降") || Direction == TEXT("负z")
	)
	{
		OutDirection = -Up;
		OutNormalizedDirection = TEXT("down");
	}
	else
	{
		OutError = FString::Printf(
			TEXT("Unsupported direction '%s'. Use X/-X/Y/-Y/Z/-Z or forward/backward/right/left/up/down."),
			*DirectionText
		);
		return false;
	}

	if (OutDirection.IsNearlyZero())
	{
		OutError = TEXT("Resolved move direction is zero.");
		return false;
	}

	return true;
}

bool UWebUIFoundationActorControlComponent::TryGetMoveDistance(
	UWebUIFoundationBridgeComponent* Bridge,
	const TSharedPtr<FJsonObject>& Payload,
	double& OutDistance
) const
{
	if (!Bridge)
	{
		return false;
	}

	if (Bridge->GetPayloadNumber(Payload, TEXT("distance"), OutDistance))
	{
		return true;
	}
	if (Bridge->GetPayloadNumber(Payload, TEXT("amount"), OutDistance))
	{
		return true;
	}
	if (Bridge->GetPayloadNumber(Payload, TEXT("value"), OutDistance))
	{
		return true;
	}
	return Bridge->GetPayloadNumber(Payload, TEXT("parameter"), OutDistance);
}

bool UWebUIFoundationActorControlComponent::EnsureActorHierarchyMovable(
	AActor* TargetActor,
	bool bIncludeAttachedActors,
	int32& OutChangedComponentCount,
	int32& OutVisitedActorCount,
	FString& OutError
) const
{
	OutChangedComponentCount = 0;
	OutVisitedActorCount = 0;
	OutError.Reset();

	if (!IsValid(TargetActor))
	{
		OutError = TEXT("Target actor is invalid.");
		return false;
	}

	TArray<AActor*> Actors;
	Actors.Add(TargetActor);
	if (bIncludeAttachedActors)
	{
		TargetActor->GetAttachedActors(Actors, false, true);
	}

	// 先把最深层子 Actor / 子组件设为 Movable，再处理父级。
	// 这样可以避免“Static 子组件挂在 Movable 父组件下”的过渡警告。
	Actors.Sort([](const AActor& A, const AActor& B)
	{
		return UWebUIFoundationActorControlComponent::GetAttachmentDepth(&A) >
			UWebUIFoundationActorControlComponent::GetAttachmentDepth(&B);
	});

	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		++OutVisitedActorCount;
		TArray<USceneComponent*> SceneComponents;
		Actor->GetComponents<USceneComponent>(SceneComponents);
		SceneComponents.Sort([](const USceneComponent& A, const USceneComponent& B)
		{
			auto GetComponentDepth = [](const USceneComponent* Component)
			{
				int32 Depth = 0;
				for (const USceneComponent* Current = Component ? Component->GetAttachParent() : nullptr;
					Current && Depth < 1024;
					Current = Current->GetAttachParent())
				{
					++Depth;
				}
				return Depth;
			};

			return GetComponentDepth(&A) > GetComponentDepth(&B);
		});

		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (!IsValid(SceneComponent) || SceneComponent->Mobility == EComponentMobility::Movable)
			{
				continue;
			}

			if (!bAutoSetActorMovable)
			{
				OutError = FString::Printf(
					TEXT("Actor %s contains non-Movable component %s. Enable bAutoSetActorMovable or set the hierarchy to Movable in the editor."),
					*Actor->GetName(),
					*SceneComponent->GetName()
				);
				return false;
			}

			SceneComponent->SetMobility(EComponentMobility::Movable);
			++OutChangedComponentCount;
		}
	}

	return true;
}

int32 UWebUIFoundationActorControlComponent::GetAttachmentDepth(const AActor* Actor)
{
	int32 Depth = 0;
	const AActor* Current = Actor ? Actor->GetAttachParentActor() : nullptr;
	while (Current && Depth < 1024)
	{
		++Depth;
		Current = Current->GetAttachParentActor();
	}
	return Depth;
}

int32 UWebUIFoundationActorControlComponent::GetAttachedDescendantCount(const AActor* Actor)
{
	if (!Actor)
	{
		return 0;
	}

	TArray<AActor*> Descendants;
	Actor->GetAttachedActors(Descendants, true, true);
	return Descendants.Num();
}
