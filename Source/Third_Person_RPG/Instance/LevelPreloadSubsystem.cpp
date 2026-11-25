#include "Third_Person_RPG/Instance/LevelPreloadSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

void ULevelPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULevelPreloadSubsystem::OnMapLoaded);
}

void ULevelPreloadSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
	ReleasePreload();
	Super::Deinitialize();
}

void ULevelPreloadSubsystem::ReleasePreload()
{
	if (PreloadHandle.IsValid())
	{
		PreloadHandle->CancelHandle();
		PreloadHandle.Reset();
	}
	RequestedMap.Reset();
}

void ULevelPreloadSubsystem::OnMapLoaded(UWorld* World)
{
	if (World && World->GetGameInstance() == GetGameInstance())
	{
		// The new world now owns its references. Do not retain assets from other maps.
		ReleasePreload();
	}
}

void ULevelPreloadSubsystem::PreloadLevel(FName LevelName)
{
	if (LevelName.IsNone()) return;
	FString PackageName = FPackageName::ObjectPathToPackageName(LevelName.ToString());
	if (FPackageName::IsShortPackageName(PackageName))
	{
		PackageName = TEXT("/Game/Levels/") + PackageName;
	}
	if (!FPackageName::IsValidLongPackageName(PackageName)) return;
	if (RequestedMap == PackageName && PreloadHandle.IsValid()) return;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> MapAssets;
	Registry.GetAssetsByPackageName(FName(*PackageName), MapAssets);
	if (!MapAssets.ContainsByPredicate([](const FAssetData& Asset) { return (Asset.PackageFlags & PKG_ContainsMap) != 0; }))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preload] Map not registered: %s"), *PackageName);
		return;
	}

	TSet<FName> Visited;
	TArray<FName> Pending { FName(*PackageName) };
	TArray<FSoftObjectPath> Paths;
	Visited.Add(Pending[0]);
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		TArray<FName> Dependencies;
		Registry.GetDependencies(Pending[Index], Dependencies,
			UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);
		for (FName Dependency : Dependencies)
		{
			if (Visited.Contains(Dependency) || Dependency.ToString().StartsWith(TEXT("/Script/"))) continue;
			Visited.Add(Dependency);
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Dependency, Assets);
			// Never instantiate another map, including maps referenced by portals.
			if (Assets.ContainsByPredicate([](const FAssetData& Asset) { return (Asset.PackageFlags & PKG_ContainsMap) != 0; })) continue;
			Pending.Add(Dependency);
			for (const FAssetData& Asset : Assets)
			{
				if (Asset.IsValid()) Paths.AddUnique(Asset.GetSoftObjectPath());
			}
		}
	}
	if (Paths.IsEmpty()) return;
	ReleasePreload();
	RequestedMap = PackageName;
	const double StartTime = FPlatformTime::Seconds();
	const int32 AssetCount = Paths.Num();
	PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this, PackageName, StartTime, AssetCount]()
		{
			if (RequestedMap == PackageName)
			{
				UE_LOG(LogTemp, Log, TEXT("[Preload] Request completed: %s, %d assets, %.2fs"),
					*PackageName, AssetCount, FPlatformTime::Seconds() - StartTime);
			}
		}));
	UE_LOG(LogTemp, Log, TEXT("[Preload] Requested %s (%d assets)"), *PackageName, AssetCount);
}
