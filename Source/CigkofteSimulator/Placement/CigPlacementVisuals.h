#pragma once

#include "CoreMinimal.h"

class AActor;

// What a placement record looks like.
//
// The placement authority owns records; UCigWorldBuilder spawns meshes. Nothing
// joined the two. A table is a record with a 160x280 footprint and, separately,
// four actors nobody could find from its StableId: the table, its plate setting
// and two chairs. So restoring a moved table from a save would have moved the
// record - and with it navigation, validation and seat capacity - while the mesh
// stayed where the world builder put it. The record and the thing the player
// looks at would disagree, which is worse than not restoring it at all.
//
// This is the join. It is written before any save schema because every later
// step of placement persistence depends on being able to answer "which actors is
// this record" and "put them where the record now says".

// One actor belonging to a placement, held in the placement's own frame.
//
// Relative rather than absolute on purpose: a move is applied to the record, and
// the actors have to follow it without anybody re-deriving where a chair sits
// beside its table. The offset is captured once, against the record's normalized
// transform, so what is stored is the authored relationship rather than a pair of
// world positions that a snap could put out of step.
struct FCigPlacementVisual
{
	TWeakObjectPtr<AActor> Actor;
	FTransform RelativeTransform = FTransform::Identity;
};

// The two operations, separated from actors so they can be checked without a
// world. Everything the registry does to a transform happens here.
namespace CigPlacementVisualMath
{
	// The actor's transform expressed in the placement's frame.
	FTransform MakeRelative(const FTransform& PlacementTransform, const FTransform& ActorTransform);

	// Where that actor goes when the placement moves to NewPlacementTransform.
	// Resolve(P, MakeRelative(P, A)) == A for any P and A, which is the property
	// the round-trip test pins.
	FTransform Resolve(const FTransform& NewPlacementTransform, const FTransform& RelativeTransform);
}

class FCigPlacementVisualRegistry
{
public:
	// Records Actor as belonging to StableId, in the frame of PlacementTransform.
	// A null actor is ignored rather than stored: a missing optional mesh must not
	// leave a hole that Apply would later dereference.
	bool Attach(FName StableId, AActor* Actor, const FTransform& PlacementTransform);

	// Moves every actor of StableId to follow the placement. Returns how many were
	// moved. Actors that have since been destroyed are dropped rather than counted,
	// so a pooled or cleaned-up world does not accumulate dead entries.
	int32 Apply(FName StableId, const FTransform& NewPlacementTransform);

	// Forgets a placement's actors, optionally destroying them. Returns the number
	// of live actors affected. Used by removal, and by a load that replaces the
	// authored default layout wholesale.
	int32 Release(FName StableId, bool bDestroyActors = false);

	void Reset();

	// Which placement this actor belongs to, or NAME_None.
	//
	// The reverse of Attach, and the question selection asks: the player's trace
	// hits one actor and build mode needs the record behind it. It answers for any
	// attached actor rather than only the main one, so looking at a chair selects
	// the table it was authored beside - the record is what moves, and the chair
	// has no independent existence in the authority.
	FName FindByActor(const AActor* Actor) const;

	bool Contains(FName StableId) const;
	// Live actors only. A count that included destroyed ones would make the
	// "every record has a visual" invariant pass on nothing.
	int32 VisualCount(FName StableId) const;
	int32 PlacementCount() const { return Visuals.Num(); }
	TArray<FName> PlacementIds() const;

private:
	TMap<FName, TArray<FCigPlacementVisual>> Visuals;
};
