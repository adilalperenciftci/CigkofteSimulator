#include "Navigation/CigNavLayout.h"

namespace CigNavLayout
{
	const TArray<FCigShellWall>& ShellWalls()
	{
		// Matches the floor built at centre (150,0), 1750 x 2600. The two west
		// segments stop 500 either side of the axis, so the shopfront is a ten
		// metre opening rather than a door - which is what a street çiğköfteci is.
		static const TArray<FCigShellWall> Walls = {
			{ { FVector2D(1000.f,     0.f), FVector2D( 20.f, 1300.f) }, EShellSurface::Tile  },
			{ { FVector2D( 150.f,  1300.f), FVector2D(850.f,   20.f) }, EShellSurface::Tile  },
			{ { FVector2D( 150.f, -1300.f), FVector2D(850.f,   20.f) }, EShellSurface::Tile  },
			{ { FVector2D(-700.f,   900.f), FVector2D( 20.f,  400.f) }, EShellSurface::Brick },
			{ { FVector2D(-700.f,  -900.f), FVector2D( 20.f,  400.f) }, EShellSurface::Brick }
		};
		return Walls;
	}

	float WallCenterZ() { return 200.f; }
	float WallHalfHeight() { return 200.f; }

	FCigPlacementBounds NavBounds()
	{
		FCigPlacementBounds Result;
		// X reaches -2400 because the queue runs outward from -850 at 160 a head;
		// a region that stopped at the shopfront would push the back of a long
		// queue out of bounds and fail every path that started there.
		Result.Center = FVector2D(-700.f, 0.f);
		Result.HalfExtent = FVector2D(1700.f, 1300.f);
		Result.FloorZ = 0.f;
		Result.FloorTolerance = 2.f;
		return Result;
	}

	FVector2D StreetApproach() { return FVector2D(-1400.f, 0.f); }

	// The two west segments stop 500 either side of the axis.
	float ShopfrontGapWidth() { return 1000.f; }

	float PlayerAgentRadius() { return 38.f; }
	float CustomerAgentRadius() { return 35.f; }

	float NavCellSize() { return 25.f; }
}
