#include "Placement/CigPlacementVisuals.h"

#include "GameFramework/Actor.h"

namespace CigPlacementVisualMath
{
	FTransform MakeRelative(const FTransform& PlacementTransform, const FTransform& ActorTransform)
	{
		return ActorTransform.GetRelativeTransform(PlacementTransform);
	}

	FTransform Resolve(const FTransform& NewPlacementTransform, const FTransform& RelativeTransform)
	{
		return RelativeTransform * NewPlacementTransform;
	}
}

bool FCigPlacementVisualRegistry::Attach(FName StableId, AActor* Actor, const FTransform& PlacementTransform)
{
	if (StableId.IsNone() || !IsValid(Actor))
	{
		return false;
	}

	TArray<FCigPlacementVisual>& List = Visuals.FindOrAdd(StableId);

	// Attaching the same actor twice would move it twice per apply, which is a
	// no-op for a rigid transform but makes every count wrong.
	for (const FCigPlacementVisual& Existing : List)
	{
		if (Existing.Actor.Get() == Actor)
		{
			return false;
		}
	}

	FCigPlacementVisual Visual;
	Visual.Actor = Actor;
	Visual.RelativeTransform = CigPlacementVisualMath::MakeRelative(PlacementTransform, Actor->GetActorTransform());
	List.Add(Visual);
	return true;
}

int32 FCigPlacementVisualRegistry::Apply(FName StableId, const FTransform& NewPlacementTransform)
{
	TArray<FCigPlacementVisual>* List = Visuals.Find(StableId);
	if (!List)
	{
		return 0;
	}

	int32 Moved = 0;
	for (int32 Index = List->Num() - 1; Index >= 0; --Index)
	{
		AActor* Actor = (*List)[Index].Actor.Get();
		if (!IsValid(Actor))
		{
			List->RemoveAt(Index);
			continue;
		}
		Actor->SetActorTransform(
			CigPlacementVisualMath::Resolve(NewPlacementTransform, (*List)[Index].RelativeTransform));
		++Moved;
	}
	return Moved;
}

int32 FCigPlacementVisualRegistry::Release(FName StableId, bool bDestroyActors)
{
	TArray<FCigPlacementVisual> List;
	if (!Visuals.RemoveAndCopyValue(StableId, List))
	{
		return 0;
	}

	int32 Affected = 0;
	for (const FCigPlacementVisual& Visual : List)
	{
		AActor* Actor = Visual.Actor.Get();
		if (!IsValid(Actor))
		{
			continue;
		}
		++Affected;
		if (bDestroyActors)
		{
			Actor->Destroy();
		}
	}
	return Affected;
}

void FCigPlacementVisualRegistry::Reset()
{
	Visuals.Reset();
}

FName FCigPlacementVisualRegistry::FindByActor(const AActor* Actor) const
{
	// A destroyed actor must not match a stale entry: Apply and VisualCount both
	// drop dead entries lazily, so an id can still hold one when this is asked.
	if (!IsValid(Actor))
	{
		return NAME_None;
	}

	for (const TPair<FName, TArray<FCigPlacementVisual>>& Pair : Visuals)
	{
		for (const FCigPlacementVisual& Visual : Pair.Value)
		{
			if (Visual.Actor.Get() == Actor)
			{
				return Pair.Key;
			}
		}
	}
	return NAME_None;
}

bool FCigPlacementVisualRegistry::Contains(FName StableId) const
{
	return Visuals.Contains(StableId);
}

int32 FCigPlacementVisualRegistry::VisualCount(FName StableId) const
{
	const TArray<FCigPlacementVisual>* List = Visuals.Find(StableId);
	if (!List)
	{
		return 0;
	}
	int32 Live = 0;
	for (const FCigPlacementVisual& Visual : *List)
	{
		if (IsValid(Visual.Actor.Get()))
		{
			++Live;
		}
	}
	return Live;
}

TArray<FName> FCigPlacementVisualRegistry::PlacementIds() const
{
	TArray<FName> Ids;
	Visuals.GetKeys(Ids);
	return Ids;
}
