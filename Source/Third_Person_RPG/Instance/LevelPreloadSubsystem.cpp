#include "Third_Person_RPG/Instance/LevelPreloadSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
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
	RemoveProgress();
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
		RemoveProgress();
		// The new world now owns its references. Do not retain assets from other maps.
		ReleasePreload();
	}
}

TSharedPtr<FStreamableHandle> ULevelPreloadSubsystem::PreloadLevel(FName LevelName)
{
	if (bTravelPending || LevelName.IsNone()) return nullptr;
	FString PackageName = FPackageName::ObjectPathToPackageName(LevelName.ToString());
	if (FPackageName::IsShortPackageName(PackageName))
	{
		PackageName = TEXT("/Game/Levels/") + PackageName;
	}
	if (!FPackageName::IsValidLongPackageName(PackageName)) return nullptr;
	if (RequestedMap == PackageName && PreloadHandle.IsValid()) return PreloadHandle;

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> MapAssets;
	Registry.GetAssetsByPackageName(FName(*PackageName), MapAssets);
	if (!MapAssets.ContainsByPredicate([](const FAssetData& Asset) { return (Asset.PackageFlags & PKG_ContainsMap) != 0; }))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Preload] Map not registered: %s"), *PackageName);
		return nullptr;
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
	if (Paths.IsEmpty()) return nullptr;
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
	return PreloadHandle;
}

void ULevelPreloadSubsystem::RemoveProgress()
{
	if (ProgressViewport.IsValid() && ProgressOverlay)
		ProgressViewport->RemoveViewportWidgetContent(ProgressOverlay.ToSharedRef());
	ProgressOverlay.Reset();
	ProgressViewport.Reset();
	bTravelPending = false;
}

void ULevelPreloadSubsystem::OpenWithProgress(FName LevelName)
{
	UWorld* World = GetWorld();
	if (!World || LevelName.IsNone() || bTravelPending) return;
	const TSharedPtr<FStreamableHandle> Pending = PreloadLevel(LevelName);
	bTravelPending = true;
	ProgressViewport = World->GetGameViewport();
	if (ProgressViewport.IsValid())
	{
		auto Ready = [Pending]() { return !Pending || Pending->HasLoadCompleted() || Pending->WasCanceled(); };
		ProgressOverlay = SNew(SBox)
			.Visibility(EVisibility::HitTestInvisible)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(60.f, 0.f, 60.f, 40.f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.f, 0.f, 0.f, 6.f)
				[
					SNew(STextBlock).Text_Lambda([Pending, Ready]()
					{
						if (Ready()) return NSLOCTEXT("Loading", "OpeningMap", "\uB9F5 \uC5EC\uB294 \uC911");
						return FText::Format(NSLOCTEXT("Loading", "AssetPercent", "\uC5D0\uC14B \uB85C\uB529 {0}%"),
							FText::AsNumber(FMath::Clamp(FMath::FloorToInt(Pending->GetProgress() * 100.f), 0, 99)));
					})
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SBox).HeightOverride(8.f)
					[
						SNew(SProgressBar).FillColorAndOpacity(FLinearColor(0.76f, 0.56f, 0.25f))
						.Percent_Lambda([Pending, Ready]() -> TOptional<float>
						{
							if (Ready()) return TOptional<float>();
							return Pending->GetProgress();
						})
					]
				]
			];
		ProgressViewport->AddViewportWidgetContent(ProgressOverlay.ToSharedRef(), 10000);
	}

	World->GetTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateWeakLambda(this,
		[this, Pending, LevelName, bReadyLastTick = false]() mutable
		{
			if (Pending && !Pending->HasLoadCompleted() && !Pending->WasCanceled()) return;
			if (!bReadyLastTick) { bReadyLastTick = true; return; }
			GetWorld()->GetTimerManager().ClearTimer(TravelTimer);
			UGameplayStatics::OpenLevel(this, LevelName);
		}), 0.05f, true);
}
