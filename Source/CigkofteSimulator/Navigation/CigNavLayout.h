#pragma once

#include "CoreMinimal.h"
#include "Placement/CigPlacementTypes.h"

// Authored shop shell geometry, owned once.
//
// The walls used to be five literal SpawnBox calls inside BuildKitchen. That was
// fine while nothing else needed to know where they were; the moment navigation
// does, a second copy of those numbers is a second authority, and the first time
// one of them moved the two would disagree silently - a customer routing through
// a wall that is still standing, or refusing a doorway that has been widened.
// BuildKitchen now spawns these, so there is one set of numbers.
namespace CigNavLayout
{
	enum class EShellSurface : uint8
	{
		// Tiled where the pack is installed, painted cream where it is not.
		Tile,
		Brick
	};

	struct FCigShellWall
	{
		FCigPlacementRect Rect;
		EShellSurface Surface = EShellSurface::Tile;
	};

	// Five segments. The west side is deliberately two, leaving the shopfront
	// open between them: that gap is the way in, and it is geometry rather than a
	// flag, so nothing can mark the shop enterable while walling it off.
	const TArray<FCigShellWall>& ShellWalls();

	float WallCenterZ();
	float WallHalfHeight();

	// The navigable region. Wider than the shop floor on purpose: customers spawn
	// on the street, queue on the pavement outside the shopfront and leave the
	// same way, so a region that stopped at the wall would put both ends of most
	// real routes out of bounds.
	//
	// The street beyond this rectangle is not modelled as navigable. Nothing out
	// there can be moved by the player, so nothing out there can block a route -
	// which is what this stage is about. Movement outside is direct, and that is
	// recorded in KNOWN_LIMITATIONS.md rather than implied here.
	FCigPlacementBounds NavBounds();

	// A point on the pavement the queue runs back along. Routes start here rather
	// than at a customer's spawn because spawning is the customer system's
	// business and the queue axis is authored layout.
	FVector2D StreetApproach();

	// The shopfront opening, between the two west wall segments.
	//
	// Worth stating because it is not a door and the first version of this file
	// assumed it was: the service counter stands in the middle of this gap, which
	// is what a street çiğköfteci looks like. A customer being served never comes
	// inside at all - they walk up to the counter from the pavement. Only a
	// customer who has been given a seat crosses the line, and they go round the
	// counter to do it. Anything that treats the centre of this opening as
	// walkable floor is describing a different shop.
	float ShopfrontGapWidth();

	// The player is a real ACharacter, so this is its capsule radius and not an
	// estimate of it. A navigation answer measured against a different width than
	// the body that has to walk it is worth nothing.
	float PlayerAgentRadius();
	// Customers have no capsule to read - they are plain actors - so this is an
	// authored width, chosen slightly under the player's so a gap the player can
	// use is never one a customer refuses.
	float CustomerAgentRadius();

	float NavCellSize();
}
