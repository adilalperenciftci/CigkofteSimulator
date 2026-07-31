#pragma once

#include "CoreMinimal.h"

class ACigkofteGameMode;

// Verifies a packaged build from inside itself, for the configuration that
// cannot be verified from outside.
//
// A Shipping build here compiles out UE_LOG and the cheat manager, so the packaged
// smoke test - whose every check is "the log did not complain about this" - can
// say nothing about it. The first Shipping package this project produced was
// reported as never having started, about a build that had started perfectly well
// and run for twenty-five seconds. Raising logging in Shipping is not available:
// the installed engine's binaries do not export the symbols, and the Test
// configuration cannot be built with a launcher engine at all.
//
// So the build reports on itself. It runs the checks a log would otherwise have
// evidenced, writes a result file, and exits with a status. Neither channel needs
// logging and neither needs a console.
//
// Reached only by the -CigReleaseSelfTest command line switch. Deliberately not a
// console command and not a key binding: it must be impossible to enter from a
// running game, and there is nothing in it a player could be given.
namespace CigReleaseSelfTest
{
	// Result of reading the one value-bearing option owned by this mode. Kept
	// separate from path validation so exact command-line token behaviour can be
	// covered by fast automation without starting a packaged game.
	enum class EOutputArgumentState : uint8
	{
		NotProvided,
		Provided,
		Invalid
	};

	// True when -CigReleaseSelfTest is on the command line. The only reader is
	// ACigkofteGameMode::InitGame.
	bool IsRequested();

	// Reads exactly -CigReleaseSelfTestOut=<value> from CommandLine. A similarly
	// named token is ignored; a missing assignment, empty/whitespace value,
	// malformed quoting, or any duplicate is Invalid. Quoted paths retain their
	// spaces and are returned without the surrounding quotes.
	EOutputArgumentState ParseOutputArgument(const TCHAR* CommandLine, FString& OutValue, FString& OutError);

	// Returned when the verdict could not be put on disk at all: the path was
	// refused, its directory could not be created, or a write failed. Above any
	// check index on purpose, so it can never be read as "check 90 failed", and
	// non-zero so a harness that only looks at the status still fails.
	constexpr int32 ReportUnwritable = 90;

	// Runs the checks and writes the report named by -CigReleaseSelfTestOut=.
	// Returns 0 when everything passed, otherwise the 1-based index of the first
	// failing check - so the process exit status alone names which one went - or
	// ReportUnwritable when there is no report to read that status against.
	//
	// Must be called with the game mode fully built: systems created, world built.
	// Writes no save and reads none.
	int32 Run(ACigkofteGameMode& GameMode);
}
