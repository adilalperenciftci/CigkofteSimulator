#pragma once

#include "CoreMinimal.h"

// What a customer's body should be saying.
//
// Until now a customer's urgency had exactly one channel: the colour of the
// floating order label, lerped green to red. That has three problems, and the
// third is the one that matters.
//
// It shares a channel with the order. The same text carries what they want and
// how long they have, so reading one means reading the other.
//
// It is colour alone. A player who cannot separate red from green has no signal
// at all, and this is the signal the whole queue loop is built on.
//
// And it is only legible up close, because it is small text. From across the
// shop - which is where a player stands while cooking - the queue is a row of
// bodies and none of them is saying anything.
//
// So urgency gets a second channel that is a *shape* rather than a colour: the
// pose. It reads at silhouette distance and it survives any colour vision.
//
// This decides the pose. Applying it to components is the actor's business, and
// keeping the decision here means it can be tested without a world - which
// matters because the interesting part is the precedence, not the animation.

enum class ECigCustomerPose : uint8
{
	// A passer-by on the street. Nothing to read; they are scenery.
	Ambient = 0,
	// Walking in, not yet at the counter. Patience is not running, so showing
	// urgency here would be inventing it.
	Approaching,
	// At the counter with time in hand.
	Waiting,
	// Patience is going. Visible, but not yet an emergency.
	Restless,
	// The last stretch. This one has to be unmistakable from across the room,
	// because it is the only warning the player gets before losing the sale.
	AboutToLeave,
	// At a table. Patience does not tick here - the customer system only counts
	// it down for the queue - so a seated body says "served", not "hurry".
	Seated,
	// Walking out, happy or not. Already gone as far as the player is concerned.
	Leaving
};

struct FCigCustomerReadoutInput
{
	// 1 = full patience, 0 = out of it. Ignored unless the customer is queued
	// and has arrived, because that is the only state where it decreases.
	float PatienceFrac = 1.f;
	bool bAmbient = false;
	bool bLeaving = false;
	bool bSeated = false;
	// Reached their queue slot. Before this the customer system does not touch
	// their patience.
	bool bArrived = false;
};

struct FCigCustomerReadout
{
	ECigCustomerPose Pose = ECigCustomerPose::Ambient;

	// 0..1, how hard the pose should be played - lean angle, fidget rate. Zero
	// wherever urgency is not a real quantity, so a body never animates worry it
	// does not have.
	float Urgency = 0.f;
};

// Deliberately not here: whether the order label should be visible.
//
// It was, briefly. The label's visibility already has six owners - Leave,
// GoToSeat, Deactivate, InitAmbient, ApplyOrderVisuals and the staff system -
// and each sets it at a moment that means something. A seventh writer running
// every tick would quietly outrank all of them, and the first symptom would be
// a pooled customer's label reappearing after Deactivate turned it off.
//
// The pose is a property this module can own outright, because nothing else
// touches it. That is the whole reason it is a good place to put a decision.

namespace CigCustomerReadout
{
	// Where the bands sit. Named because two of them are a design decision about
	// how much warning a player gets, not arithmetic - and a test that hard-coded
	// 0.5 would silently stop testing the rule the day the rule moved.
	inline constexpr float RestlessBelow = 0.5f;
	inline constexpr float AboutToLeaveBelow = 0.2f;

	// The decision.
	//
	// Precedence, outermost first: leaving, seated, ambient, then the patience
	// bands. Leaving wins over everything because a customer already walking out
	// cannot be saved, and a body that still looked desperate would send the
	// player chasing a sale that is gone.
	FCigCustomerReadout Resolve(const FCigCustomerReadoutInput& In);
}
