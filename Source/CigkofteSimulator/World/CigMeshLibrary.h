#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

// Access to imported meshes by name.
// Returns nullptr when the asset is missing; the caller falls back to a primitive.
namespace CigMesh
{
	// Path: /Game/LowPoly/<Category>/<Name>. The result is cached.
	UStaticMesh* Load(const TCHAR* Category, const TCHAR* Name);

	// Path: /Game/<Root>/<Name>. For non-Kenney packs, whose folder layout differs.
	UStaticMesh* LoadAt(const TCHAR* Root, const TCHAR* Name);

	inline UStaticMesh* Food(const TCHAR* Name) { return Load(TEXT("Food"), Name); }
	inline UStaticMesh* Furniture(const TCHAR* Name) { return Load(TEXT("Furniture"), Name); }

	// The restaurant prop pack taken from the EG_OYUN_CIGKOFTE_1 project
	// (/Game/dukkan/Geometries): plates, bowls, glasses, greens, bread, tables.
	// Without the pack this returns nullptr and the game falls back to Kenney or
	// primitive visuals.
	inline UStaticMesh* Dukkan(const TCHAR* Name) { return LoadAt(TEXT("dukkan/Geometries"), Name); }

	// The CityPark pack. Its meshes are split across subfolders:
	// /Game/CityPark/Meshes/<Sub>/<Name> - Sub: Props, Buildings, Flora/Trees, ParkSquare
	UStaticMesh* Park(const TCHAR* Sub, const TCHAR* Name);

	// Scene_Bazaar_Vol1 (an Ottoman/eastern bazaar). Each mesh sits in its own
	// folder: /Game/Scene_Bazaar_Vol1/Assets/MS/3D/<Folder>/<Name>
	UStaticMesh* Bazaar(const TCHAR* Folder, const TCHAR* Name);
}
