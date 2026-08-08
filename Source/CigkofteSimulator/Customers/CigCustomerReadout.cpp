#include "Customers/CigCustomerReadout.h"

namespace CigCustomerReadout
{
	FCigCustomerReadout Resolve(const FCigCustomerReadoutInput& In)
	{
		FCigCustomerReadout Out;

		// Leaving first, and above everything including seated: the customer is
		// walking out and nothing the player does now reaches them. A body still
		// playing urgency here would send somebody running for a sale that has
		// already been decided.
		if (In.bLeaving)
		{
			Out.Pose = ECigCustomerPose::Leaving;
			Out.Urgency = 0.f;
			return Out;
		}

		// Ambient before seated and before the bands. A street passer-by has no
		// order and no patience; every other field on them is meaningless.
		if (In.bAmbient)
		{
			Out.Pose = ECigCustomerPose::Ambient;
			Out.Urgency = 0.f;
			return Out;
		}

		if (In.bSeated)
		{
			// Served and sitting. UCigCustomerSystem only counts patience down
			// for the queue, so a seated customer's PatienceFrac is whatever it
			// happened to be when they sat - stale, and not a thing to draw.
			Out.Pose = ECigCustomerPose::Seated;
			Out.Urgency = 0.f;
			return Out;
		}

		// Still walking to their slot. Patience does not start until bArrived,
		// so any urgency shown here would be invented rather than measured.
		if (!In.bArrived)
		{
			Out.Pose = ECigCustomerPose::Approaching;
			Out.Urgency = 0.f;
			return Out;
		}

		const float Frac = FMath::Clamp(In.PatienceFrac, 0.f, 1.f);

		if (Frac < AboutToLeaveBelow)
		{
			Out.Pose = ECigCustomerPose::AboutToLeave;
			// Ramps to 1 as the last of the patience goes, so the pose gets more
			// insistent rather than switching on and sitting still.
			Out.Urgency = 1.f - (Frac / AboutToLeaveBelow);
			return Out;
		}

		if (Frac < RestlessBelow)
		{
			Out.Pose = ECigCustomerPose::Restless;
			// Rescaled across this band alone, so it reaches 1 at the boundary
			// and AboutToLeave takes over from a full-strength restless rather
			// than from a half-hearted one.
			Out.Urgency = (RestlessBelow - Frac) / (RestlessBelow - AboutToLeaveBelow);
			return Out;
		}

		Out.Pose = ECigCustomerPose::Waiting;
		Out.Urgency = 0.f;
		return Out;
	}
}
