#pragma once

#include "CoreMinimal.h"

// What the shop is called.
//
// Until now it was a literal in the middle of the world builder:
// SpawnWorldText(..., TEXT("CIGKOFTECI"), ...) on the shopfront board, not even
// routed through CigText. Nothing owned it, nothing could change it, and nothing
// could save it.
//
// A name is not translated. If a Turkish player names their shop and switches the
// interface to English, the sign must still say what they called it - so the
// default is a fixed string rather than a text key, and the language setting has
// no opinion about it. That is the one decision here that is not obvious, and it
// is why CigText is deliberately not involved.

enum class ECigShopNameFault : uint8
{
	None = 0,
	// Empty, or nothing but whitespace. A shop with no name on its board is not a
	// stylistic choice, it is a missing sign.
	Empty,
	TooLong,
	// Control characters, tabs and line breaks. A newline in a name is a sign with
	// half a word on it and a HUD field that grows a row.
	InvalidCharacter
};

namespace CigShopIdentity
{
	// The board is one line on a shopfront read from across a street. Long enough
	// for a real name with a surname in it, short enough that the text renderer is
	// not asked to fit a sentence.
	constexpr int32 MaxNameLength = 24;

	// What a shop is called before anyone names it. The literal the sign used to
	// carry, now with a single owner.
	const FString& DefaultName();

	// Trims the ends and nothing else. Inner spacing is the player's business:
	// "Cig  Kofte" is a name somebody chose, and quietly rewriting it is worse
	// than leaving it.
	FString Normalize(const FString& Raw);

	// Judged on the normalized form, because leading spaces are not a reason to
	// refuse a name and trailing ones are not a reason to call it too long.
	ECigShopNameFault Validate(const FString& Raw);

	// The name to actually put on the board: the normalized candidate when it is
	// acceptable, and the default when it is not. Never returns an empty string,
	// because every caller of this would otherwise need its own fallback and one
	// of them would forget.
	FString Resolve(const FString& Raw);

	const TCHAR* FaultText(ECigShopNameFault Fault);
}
