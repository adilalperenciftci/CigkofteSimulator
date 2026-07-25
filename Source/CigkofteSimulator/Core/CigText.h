#pragma once

#include "CoreMinimal.h"

// Player-facing text.
//
// WHY NOT LOCTEXT: LOCTEXT/FText depend on compiled .locres files produced by
// UE's localization dashboard. That pipeline needs editor commands, its output
// is binary so it never diffs in git, and correcting one translation costs a
// full rebuild. Balance (Config/Balance) and customer dialogue
// (Config/Dialogue) already come from CSV here, so reading UI text from the
// same place beats introducing a third mechanism. Because CSV is text, a
// wording change shows up in the commit history, and Reload() picks it up
// without closing the game.
//
// Rule: do not write NEW player-facing text straight into TEXT() - add a key to
// Config/Text/Strings.csv and read it back with CigText::Get. Log, debug and
// console output stays in TEXT(); none of that is translated.
//
// A field containing a comma must be quoted in the CSV (`key,"a, b",c`).
// Unquoted, the columns shift and one language silently loses its text.
namespace CigText
{
	// Returns the text for a key in the current language. An unknown key comes
	// back as the key itself, so it stands out on screen, and warns once.
	FString Get(const TCHAR* Key);

	// Formatted text: the template comes from the CSV, the arguments from here.
	// Placeholders are ordered: "Unlocks at level {0}".
	//
	// Why not printf: the template arrives from a file at runtime, while
	// FString::Printf wants a compile-time constant format string. {0} also
	// suits translation better - argument order can differ between languages,
	// which %d cannot express.
	template <typename... TArgs>
	FString Format(const TCHAR* Key, TArgs... Args)
	{
		const FStringFormatOrderedArguments Ordered{ FStringFormatArg(Args)... };
		return FString::Format(*Get(Key), Ordered);
	}

	// 0 = Turkish, 1 = English. Changed from the settings menu.
	int32 GetLanguage();
	void SetLanguage(int32 Language);
	int32 LanguageCount();
	// The language's name in its own language ("Türkçe", "English").
	const TCHAR* LanguageName(int32 Language);

	// Re-reads Config/Text/Strings.csv.
	void Reload();
	// Number of loaded keys (for verification and tests).
	int32 KeyCount();
}
