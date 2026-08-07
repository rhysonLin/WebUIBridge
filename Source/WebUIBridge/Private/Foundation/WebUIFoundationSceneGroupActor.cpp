#include "Foundation/WebUIFoundationSceneGroupActor.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace WebUIFoundationSceneGroup
{
	static int32 GetActorDepth(const AActor* Actor)
	{
		int32 Depth = 0;
		for (const AActor* Current = Actor ? Actor->GetAttachParentActor() : nullptr;
			Current && Depth < 1024;
			Current = Current->GetAttachParentActor())
		{
			++Depth;
		}
		return Depth;
	}

	static int32 GetComponentDepth(const USceneComponent* Component)
	{
		int32 Depth = 0;
		for (const USceneComponent* Current = Component ? Component->GetAttachParent() : nullptr;
			Current && Depth < 1024;
			Current = Current->GetAttachParent())
		{
			++Depth;
		}
		return Depth;
	}
}

AWebUIFoundationSceneGroupActor::AWebUIFoundationSceneGroupActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GroupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GroupRoot"));
	SetRootComponent(GroupRoot);
	// 编辑阶段保持 Static，便于把 Datasmith/StaticMeshActor 直接挂到该分组下。
	// 首次 moveActor 时 ActorControl 会按“子级先、父级后”的顺序自动切换为 Movable。
	GroupRoot->SetMobility(EComponentMobility::Static);

	bSynchronizeGroupIdToActorTag = true;
	bSetImportedHierarchyMovable = true;
	bRemoveImportTagAfterAttach = true;
	SetActorEnableCollision(false);
}

void AWebUIFoundationSceneGroupActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bSynchronizeGroupIdToActorTag)
	{
		SynchronizeGroupTag();
	}
}

void AWebUIFoundationSceneGroupActor::SynchronizeGroupTag()
{
	const bool bNeedsOldTagRemoval =
		!SynchronizedGroupTag.IsNone() &&
		(!bSynchronizeGroupIdToActorTag || SynchronizedGroupTag != GroupId);

	const bool bNeedsNewTag =
		bSynchronizeGroupIdToActorTag &&
		!GroupId.IsNone() &&
		!Tags.Contains(GroupId);

	const FName DesiredSynchronizedTag =
		(bSynchronizeGroupIdToActorTag && !GroupId.IsNone()) ? GroupId : NAME_None;

	const bool bStateChanged =
		bNeedsOldTagRemoval ||
		bNeedsNewTag ||
		SynchronizedGroupTag != DesiredSynchronizedTag;

	if (!bStateChanged)
	{
		return;
	}

#if WITH_EDITOR
	Modify();
#endif

	if (bNeedsOldTagRemoval)
	{
		Tags.Remove(SynchronizedGroupTag);
	}

	if (!DesiredSynchronizedTag.IsNone())
	{
		Tags.AddUnique(DesiredSynchronizedTag);
	}

	SynchronizedGroupTag = DesiredSynchronizedTag;

#if WITH_EDITOR
	MarkPackageDirty();
#endif
}

void AWebUIFoundationSceneGroupActor::AttachActorsByImportTag()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[WebUIFoundation][SceneGroup] World is unavailable."));
		return;
	}

	const FName TagToImport = ImportActorTag.IsNone() ? GroupId : ImportActorTag;
	if (TagToImport.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("[WebUIFoundation][SceneGroup] ImportActorTag and GroupId are both empty."));
		return;
	}

	TArray<AActor*> Candidates;
	TSet<AActor*> CandidateSet;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == this || !Actor->ActorHasTag(TagToImport))
		{
			continue;
		}

		if (Actor->GetLevel() != GetLevel())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[WebUIFoundation][SceneGroup] Skip cross-level Actor=%s Tag=%s"),
				*Actor->GetName(),
				*TagToImport.ToString()
			);
			continue;
		}

		if (WouldCreateAttachmentCycle(Actor, this))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[WebUIFoundation][SceneGroup] Skip Actor=%s because attachment would create a cycle."),
				*Actor->GetName()
			);
			continue;
		}

		Candidates.Add(Actor);
		CandidateSet.Add(Actor);
	}

	// 如果候选 Actor 的某个祖先也在候选集中，只挂候选层级的最上层，避免破坏原有层级。
	TArray<AActor*> TopLevelCandidates;
	for (AActor* Candidate : Candidates)
	{
		bool bHasCandidateAncestor = false;
		for (AActor* Parent = Candidate->GetAttachParentActor(); Parent; Parent = Parent->GetAttachParentActor())
		{
			if (CandidateSet.Contains(Parent))
			{
				bHasCandidateAncestor = true;
				break;
			}
		}

		if (!bHasCandidateAncestor)
		{
			TopLevelCandidates.Add(Candidate);
		}
	}

	int32 AttachedCount = 0;
	int32 FailedCount = 0;
	int32 MobilityActorCount = 0;
	int32 MobilityComponentCount = 0;

	Modify();
	for (AActor* Actor : TopLevelCandidates)
	{
		if (!IsValid(Actor))
		{
			++FailedCount;
			continue;
		}

		Actor->Modify();
		if (bSetImportedHierarchyMovable)
		{
			SetActorAndDescendantsMovable(Actor, MobilityActorCount, MobilityComponentCount);
		}

		const bool bAttached = Actor->AttachToComponent(
			GroupRoot,
			FAttachmentTransformRules::KeepWorldTransform
		);

		if (bAttached)
		{
			++AttachedCount;

			if (bRemoveImportTagAfterAttach)
			{
				TArray<AActor*> ImportedHierarchy;
				ImportedHierarchy.Add(Actor);
				Actor->GetAttachedActors(ImportedHierarchy, false, true);
				for (AActor* ImportedActor : ImportedHierarchy)
				{
					if (IsValid(ImportedActor) && ImportedActor->ActorHasTag(TagToImport))
					{
						ImportedActor->Modify();
						ImportedActor->Tags.Remove(TagToImport);
						ImportedActor->MarkPackageDirty();
					}
				}
			}

			Actor->MarkPackageDirty();
		}
		else
		{
			++FailedCount;
			UE_LOG(
				LogTemp,
				Error,
				TEXT("[WebUIFoundation][SceneGroup] Attach failed. Group=%s Actor=%s"),
				*GetName(),
				*Actor->GetName()
			);
		}
	}

	MarkPackageDirty();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation][SceneGroup] Import complete. Group=%s GroupId=%s Tag=%s Matched=%d TopLevel=%d Attached=%d Failed=%d MobilityActors=%d MobilityComponents=%d DescendantsNow=%d"),
		*GetName(),
		*GroupId.ToString(),
		*TagToImport.ToString(),
		Candidates.Num(),
		TopLevelCandidates.Num(),
		AttachedCount,
		FailedCount,
		MobilityActorCount,
		MobilityComponentCount,
		GetAttachedDescendantCount()
	);
}

void AWebUIFoundationSceneGroupActor::SetAttachedHierarchyMovable()
{
	int32 ActorCount = 0;
	int32 ComponentCount = 0;
	SetActorAndDescendantsMovable(this, ActorCount, ComponentCount);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation][SceneGroup] Set hierarchy Movable. Group=%s Actors=%d ComponentsChanged=%d"),
		*GetName(),
		ActorCount,
		ComponentCount
	);
}

void AWebUIFoundationSceneGroupActor::DetachDirectChildrenKeepWorld()
{
	TArray<AActor*> DirectChildren;
	GetAttachedActors(DirectChildren, true, false);

	int32 DetachedCount = 0;
	for (AActor* Child : DirectChildren)
	{
		if (!IsValid(Child))
		{
			continue;
		}

		Child->Modify();
		Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Child->MarkPackageDirty();
		++DetachedCount;
	}

	MarkPackageDirty();
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIFoundation][SceneGroup] Detached direct children. Group=%s Count=%d"),
		*GetName(),
		DetachedCount
	);
}

int32 AWebUIFoundationSceneGroupActor::GetAttachedDescendantCount() const
{
	TArray<AActor*> Descendants;
	GetAttachedActors(Descendants, true, true);
	return Descendants.Num();
}

void AWebUIFoundationSceneGroupActor::SetActorAndDescendantsMovable(
	AActor* Actor,
	int32& OutActorCount,
	int32& OutComponentCount
)
{
	if (!IsValid(Actor))
	{
		return;
	}

	TArray<AActor*> Actors;
	Actors.Add(Actor);
	Actor->GetAttachedActors(Actors, false, true);

	Actors.Sort([](const AActor& A, const AActor& B)
	{
		return WebUIFoundationSceneGroup::GetActorDepth(&A) > WebUIFoundationSceneGroup::GetActorDepth(&B);
	});

	for (AActor* CurrentActor : Actors)
	{
		if (!IsValid(CurrentActor))
		{
			continue;
		}

		++OutActorCount;
		CurrentActor->Modify();

		TArray<USceneComponent*> SceneComponents;
		CurrentActor->GetComponents<USceneComponent>(SceneComponents);
		SceneComponents.Sort([](const USceneComponent& A, const USceneComponent& B)
		{
			return WebUIFoundationSceneGroup::GetComponentDepth(&A) >
				WebUIFoundationSceneGroup::GetComponentDepth(&B);
		});

		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (IsValid(SceneComponent) && SceneComponent->Mobility != EComponentMobility::Movable)
			{
				SceneComponent->Modify();
				SceneComponent->SetMobility(EComponentMobility::Movable);
				++OutComponentCount;
			}
		}

		CurrentActor->MarkPackageDirty();
	}
}

bool AWebUIFoundationSceneGroupActor::WouldCreateAttachmentCycle(const AActor* Child, const AActor* NewParent)
{
	if (!Child || !NewParent || Child == NewParent)
	{
		return true;
	}

	// 如果 NewParent 已经位于 Child 的后代链中，把 Child 挂到 NewParent 会形成环。
	for (const AActor* Current = NewParent; Current; Current = Current->GetAttachParentActor())
	{
		if (Current == Child)
		{
			return true;
		}
	}

	return false;
}
