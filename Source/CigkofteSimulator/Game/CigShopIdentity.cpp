#include "Game/CigShopIdentity.h"

namespace
{
	// Everything below the space, plus DEL. Covers tabs and both halves of a
	// CRLF without naming them one at a time.
	bool IsShopNameControlChar(TCHAR Ch)
	{
		return Ch < TEXT(' ') || Ch == TCHAR(0x7F);
	}
}

const FString& CigShopIdentity::DefaultName()
{
	// The literal the shopfront board carried before anything owned it. Kept
	// exactly, so an existing shop reads the same after this change as before it.
	static const FString Name = TEXT("CIGKOFTECI");
	return Name;
}

FString CigShopIdentity::Normalize(const FString& Raw)
{
	return Raw.TrimStartAndEnd();
}

ECigShopNameFault CigShopIdentity::Validate(const FString& Raw)
{
	const FString Name = Normalize(Raw);

	if (Name.IsEmpty())
	{
		return ECigShopNameFault::Empty;
	}
	for (const TCHAR Ch : Name)
	{
		if (IsShopNameControlChar(Ch))
		{
			return ECigShopNameFault::InvalidCharacter;
		}
	}
	// Counted after the control-character check so a name that is both is
	// reported as the more specific fault.
	//
	// Len() is UTF-16 code units, which is what the text renderer and the save
	// field both measure. It is not a grapheme count: "ğ" is one unit and an
	// emoji is two, so an emoji costs double. That is a real limitation and the
	// honest one to have, because the board's width is measured the same way.
	if (Name.Len() > MaxNameLength)
	{
		return ECigShopNameFault::TooLong;
	}
	return ECigShopNameFault::None;
}

FString CigShopIdentity::Resolve(const FString& Raw)
{
	return Validate(Raw) == ECigShopNameFault::None ? Normalize(Raw) : DefaultName();
}

const TCHAR* CigShopIdentity::FaultText(ECigShopNameFault Fault)
{
	switch (Fault)
	{
	case ECigShopNameFault::None:             return TEXT("sorun yok");
	case ECigShopNameFault::Empty:            return TEXT("bos ad");
	case ECigShopNameFault::TooLong:          return TEXT("cok uzun");
	case ECigShopNameFault::InvalidCharacter: return TEXT("gecersiz karakter");
	}
	return TEXT("bilinmeyen");
}
