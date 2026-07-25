#pragma once

#include "CoreMinimal.h"

// The shared UI colour palette.
//
// Why its own header: these constants were originally declared separately in
// anonymous namespaces in both CigkofteHUD.cpp and CigTabletData.cpp. When the
// unity build put those two files in one translation unit it produced a
// redefinition error - a trap this project has hit before. Shared inline
// constants both end the clash and let the palette change in one place.
namespace CigUI
{
	inline constexpr FLinearColor White(0.92f, 0.92f, 0.95f);
	inline constexpr FLinearColor Dim(0.62f, 0.62f, 0.68f);
	inline constexpr FLinearColor Gold(1.f, 0.85f, 0.35f);
	inline constexpr FLinearColor Good(0.55f, 0.95f, 0.6f);
	inline constexpr FLinearColor Bad(1.f, 0.45f, 0.4f);
	inline constexpr FLinearColor Info(0.65f, 0.8f, 1.f);
}
