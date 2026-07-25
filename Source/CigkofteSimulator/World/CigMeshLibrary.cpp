#include "World/CigMeshLibrary.h"
#include "Engine/StaticMesh.h"
#include "Core/CigLog.h"

namespace CigMesh
{
	static TMap<FString, TWeakObjectPtr<UStaticMesh>> GCache;
	static TSet<FString> GMissing;

	// Shared loader: takes a full package path and caches the result, misses included.
	static UStaticMesh* LoadFromFolder(const FString& Folder, const TCHAR* Name)
	{
		const FString Key = FString::Printf(TEXT("%s/%s"), *Folder, Name);
		if (const TWeakObjectPtr<UStaticMesh>* Found = GCache.Find(Key))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		if (GMissing.Contains(Key))
		{
			return nullptr;
		}

		// On FBX import the asset name matches the file name: /Game/<Folder>/<Name>.<Name>
		const FString Path = FString::Printf(TEXT("/Game/%s/%s.%s"), *Folder, Name, Name);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (Mesh)
		{
			GCache.Add(Key, Mesh);
			UE_LOG(LogCig, Verbose, TEXT("Mesh yuklendi: %s"), *Path);
		}
		else
		{
			GMissing.Add(Key);
			UE_LOG(LogCig, Warning, TEXT("Mesh bulunamadi: %s"), *Path);
		}
		return Mesh;
	}

	UStaticMesh* LoadAt(const TCHAR* Root, const TCHAR* Name)
	{
		return LoadFromFolder(FString(Root), Name);
	}

	UStaticMesh* Park(const TCHAR* Sub, const TCHAR* Name)
	{
		return LoadFromFolder(FString::Printf(TEXT("CityPark/Meshes/%s"), Sub), Name);
	}

	UStaticMesh* Bazaar(const TCHAR* Folder, const TCHAR* Name)
	{
		return LoadFromFolder(FString::Printf(TEXT("Scene_Bazaar_Vol1/Assets/MS/3D/%s"), Folder), Name);
	}

	UStaticMesh* Load(const TCHAR* Category, const TCHAR* Name)
	{
		const FString Key = FString::Printf(TEXT("%s/%s"), Category, Name);
		if (const TWeakObjectPtr<UStaticMesh>* Found = GCache.Find(Key))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		if (GMissing.Contains(Key))
		{
			return nullptr;
		}

		// On Kenney FBX import the asset name matches the file name.
		const FString Path = FString::Printf(TEXT("/Game/LowPoly/%s/%s.%s"), Category, Name, Name);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
		if (Mesh)
		{
			GCache.Add(Key, Mesh);
			UE_LOG(LogCig, Verbose, TEXT("Mesh yuklendi: %s"), *Path);
		}
		else
		{
			GMissing.Add(Key);
			UE_LOG(LogCig, Warning, TEXT("Mesh bulunamadi: %s"), *Path);
		}
		return Mesh;
	}
}
