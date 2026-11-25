// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Instance/TPRGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "Third_Person_RPG/TPRSaveGame.h"
#include "Third_Person_RPG/UI/Tutorial/TutorialWidget.h"
#include "Third_Person_RPG/Inventory/InventoryItem.h"
#include "Third_Person_RPG/Data/ItemData/WeaponItemData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"

void UTPRGameInstance::Init()
{
	Super::Init();
	LoadGameData();
}

void UTPRGameInstance::RegisterSavePoint(const FSavePointInfo& SavePointInfo)
{
	if (!DiscoveredSavePoints.Contains(SavePointInfo.SavePointID))
	{
		DiscoveredSavePoints.Add(SavePointInfo.SavePointID, SavePointInfo);
	}

	UE_LOG(LogTemp, Warning, TEXT("RegisterSavePoint: %s, %s, %s"),
		*SavePointInfo.SavePointID.ToString(),
		*SavePointInfo.DisplayName.ToString(),
		*SavePointInfo.MapName.ToString());
}

const TMap<FName, FSavePointInfo>& UTPRGameInstance::GetSavePointMap() const
{
	return DiscoveredSavePoints;
}

TArray<FString> UTPRGameInstance::GetActivatedSavePointNames() const
{
	TArray<FString> Names;
	for (const auto& Pair : DiscoveredSavePoints)
	{
		Names.Add(Pair.Key.ToString());
	}
	return Names;
}

void UTPRGameInstance::ClearSavePoints()
{
	DiscoveredSavePoints.Empty();
}

void UTPRGameInstance::SaveGameData()
{
	// Capture current state immediately before writing the save.
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		if (UInventoryComponent* Inventory = Player->GetInventoryComponent())
		{
			CacheInventory(Inventory->GetAllItems());
			CacheEquippedWeapon(Inventory->EquippedWeaponItem);
		}
		RegisterPlayerStatFromPlayer(Player);
	}
	UTPRSaveGame* SaveGameInstance = Cast<UTPRSaveGame>(UGameplayStatics::CreateSaveGameObject(UTPRSaveGame::StaticClass()));

	if (SaveGameInstance)
	{
		SaveGameInstance->LastSavedMapName = FName(*UGameplayStatics::GetCurrentLevelName(GetWorld(), true));

		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				SaveGameInstance->LastSavedLocation = Pawn->GetActorLocation();
			}
		}

		for (const auto& Pair : DiscoveredSavePoints)
		{
			SaveGameInstance->AllDiscoveredSavePoints.Add(Pair.Value);
			SaveGameInstance->ActivatedSavePointNames.Add(Pair.Key.ToString());
		}

		SaveGameInstance->PlayerStat = CachedPlayerStat;

		SaveGameInstance->MetNPCs = MetNPCs;

		SaveGameInstance->ClearedBoss = ClearedBoss;

		SaveGameInstance->CollectedItemAssets = CollectedItemAssets;

		SaveGameInstance->bHiddenBossUnlocked = bHiddenBossUnlocked;

		SaveGameInstance->ItemEnhanceLevels.Empty();

		SaveGameInstance->EquippedWeaponAssetId = CachedEquippedWeaponAssetId;
		SaveGameInstance->LastRestedSavePointID = LastRestedSavePointID;
		SaveGameInstance->bHasInventorySnapshot = true;

		for (const FInventoryItemSaveData& It : CachedInventoryItems)
		{
			if (!It.ItemData) continue;
			FStoredInventoryItem StoredItem;
			StoredItem.ItemData = It.ItemData;
			StoredItem.Quantity = It.Quantity;
			StoredItem.EnhanceLevel = It.EnhanceLevel;
			StoredItem.bEquipped = It.bEquipped;
			SaveGameInstance->InventoryItems.Add(StoredItem);

			const FPrimaryAssetId Id = It.ItemData->GetPrimaryAssetId();
			SaveGameInstance->ItemEnhanceLevels.Add(Id, It.EnhanceLevel); // ¡Ú ÇÙ½É
		}

		UGameplayStatics::SaveGameToSlot(SaveGameInstance, SaveSlotName, UserIndex);
	}
}

bool UTPRGameInstance::LoadGameData()
{
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex))
	{
		UTPRSaveGame* LoadedGame = Cast<UTPRSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, UserIndex));
		if (LoadedGame)
		{
			DiscoveredSavePoints.Empty();

			for (const FSavePointInfo& Info : LoadedGame->AllDiscoveredSavePoints)
			{
				DiscoveredSavePoints.Add(Info.SavePointID, Info);
			}

			CachedPlayerStat = LoadedGame->PlayerStat;

			MetNPCs = LoadedGame->MetNPCs;

			bHasLoadedStat = true;

			CollectedItemAssets = LoadedGame->CollectedItemAssets;
			bHiddenBossUnlocked = LoadedGame->bHiddenBossUnlocked;

			ClearedBoss = LoadedGame->ClearedBoss;

			LoadedEnhanceLevels = LoadedGame->ItemEnhanceLevels;

			CachedEquippedWeaponAssetId = LoadedGame->EquippedWeaponAssetId;
			LastRestedSavePointID = LoadedGame->LastRestedSavePointID;
			CachedInventoryItems.Empty();

			if (LoadedGame->bHasInventorySnapshot)
			{
				for (const FStoredInventoryItem& StoredItem : LoadedGame->InventoryItems)
				{
					UTPRItemData* Data = StoredItem.ItemData.LoadSynchronous();
					if (!Data || StoredItem.Quantity <= 0)
					{
						UE_LOG(LogTemp, Warning, TEXT("Unable to restore inventory item: %s"), *StoredItem.ItemData.ToString());
						continue;
					}
					FInventoryItemSaveData Item;
					Item.ItemData = Data;
					Item.Quantity = StoredItem.Quantity;
					Item.EnhanceLevel = StoredItem.EnhanceLevel;
					Item.bEquipped = StoredItem.bEquipped;
					CachedInventoryItems.Add(Item);
				}
			}
			else
			{
				// Legacy saves contain IDs but no quantities: recover one of each.
				FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				TArray<FAssetData> Assets;
				Registry.Get().ScanPathsSynchronous({ TEXT("/Game/Data/Item_Data") });
				Registry.Get().GetAssetsByPath(FName(TEXT("/Game/Data/Item_Data")), Assets, true);
				TSet<FPrimaryAssetId> RecoveredIds;
				for (const FAssetData& Asset : Assets)
				{
					const FPrimaryAssetId Id(TEXT("TPRItemData"), Asset.AssetName);
					if ((!CollectedItemAssets.Contains(Id) && !LoadedEnhanceLevels.Contains(Id) && Id != CachedEquippedWeaponAssetId)
						|| RecoveredIds.Contains(Id)) continue;
					UTPRItemData* Data = Cast<UTPRItemData>(Asset.GetAsset());
					if (!Data || Data->GetPrimaryAssetId() != Id) continue;
					FInventoryItemSaveData Item;
					Item.ItemData = Data;
					Item.Quantity = 1;
					Item.EnhanceLevel = LoadedEnhanceLevels.FindRef(Id);
					Item.bEquipped = Id == CachedEquippedWeaponAssetId;
					CachedInventoryItems.Add(Item);
					RecoveredIds.Add(Id);
				}

				// Infer the legacy checkpoint only within the saved map.
				double NearestDistance = TNumericLimits<double>::Max();
				for (const auto& Pair : DiscoveredSavePoints)
				{
					const FSavePointInfo& Info = Pair.Value;
					if (Info.MapName.IsNone() || FPackageName::GetShortFName(Info.MapName) != FPackageName::GetShortFName(LoadedGame->LastSavedMapName)) continue;
					const double Distance = FVector::DistSquared(Info.Location, LoadedGame->LastSavedLocation);
					if (Distance < NearestDistance)
					{
						NearestDistance = Distance;
						LastRestedSavePointID = Pair.Key;
					}
				}
			}

			return true;
		}
	}
	return false;
}

void UTPRGameInstance::CacheInventory(const TArray<UInventoryItem*>& Items)
{
	CachedInventoryItems.Empty();

	for (UInventoryItem* Item : Items)
	{
		if (Item && Item->ItemData)
		{
			FInventoryItemSaveData SaveData;
			SaveData.ItemData = Item->ItemData;
			SaveData.Quantity = Item->Quantity;
			SaveData.bEquipped = Item->bEquipped;

			SaveData.EnhanceLevel = Item->EnhanceLevel;

			CachedInventoryItems.Add(SaveData);
		}
	}
}

const TArray<FInventoryItemSaveData>& UTPRGameInstance::GetCachedInventory() const
{
	return CachedInventoryItems;
}

void UTPRGameInstance::RegisterPlayerStatFromPlayer(const APlayerCharacter* Player)
{
	if (!Player) return;

	Player->FillSaveData(CachedPlayerStat);
	bHasLoadedStat = true;
}

void UTPRGameInstance::ApplyLoadedPlayerStatTo(APlayerCharacter* Player)
{
	if (!Player) return;

	if (!bHasLoadedStat) return;

	Player->ApplySaveData(CachedPlayerStat);
}

void UTPRGameInstance::RegisterTutorialWidget(UTutorialWidget* Widget)
{
	if (!Widget) return;
	Widget->OnTutorialFinished.AddDynamic(this, &UTPRGameInstance::OnTutorialFinished);
}

void UTPRGameInstance::OnTutorialFinished(FName TutorialID)
{
	if (!FinishedTutorials.Contains(TutorialID))
	{
		FinishedTutorials.Add(TutorialID);
	}
}

void UTPRGameInstance::RegisterMetNPC(FName NPCID)
{
	if (NPCID.IsNone()) return;
	MetNPCs.Add(NPCID);
}

bool UTPRGameInstance::HasMetNPC(FName NPCID) const
{
	if (NPCID.IsNone()) return false;
	return MetNPCs.Contains(NPCID);
}

void UTPRGameInstance::RegisterClearedBoss(FName BossID)
{
	if (BossID.IsNone()) return;
	ClearedBoss.Add(BossID);
}

bool UTPRGameInstance::HasClearedBoss(FName BossID) const
{
	if (BossID.IsNone()) return false;
	return ClearedBoss.Contains(BossID);
}

bool UTPRGameInstance::DeleteSaveData()
{
	// An absent save is already a clean start. Do not continue if deletion fails.
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex))
	{
		if (!UGameplayStatics::DeleteGameInSlot(SaveSlotName, UserIndex)
			|| UGameplayStatics::DoesSaveGameExist(SaveSlotName, UserIndex))
		{
			UE_LOG(LogTemp, Error, TEXT("New game cancelled: failed to delete save slot %s"), *SaveSlotName);
			return false;
		}
	}

	DiscoveredSavePoints.Empty();
	CachedInventoryItems.Empty();
	MetNPCs.Empty();
	ClearedBoss.Empty();
	bHasLoadedStat = false;
	PendingSavePointID = NAME_None;
	LastRestedSavePointID = NAME_None;
	LastActivatedSavePoint.Empty();
	CollectedItemAssets.Empty();
	LoadedEnhanceLevels.Empty();
	FinishedTutorials.Empty();
	CachedEquippedWeaponAssetId = FPrimaryAssetId();
	CachedPlayerStat = FPlayerStatSaveData();
	bHiddenBossUnlocked = false;
	bShouldRespawn = false;
	RespawnLocation = FVector::ZeroVector;
	RespawnRotation = FRotator::ZeroRotator;

	return true;
}


bool UTPRGameInstance::CheckAndUnlockHiddenBoss(const TArray<FPrimaryAssetId>& RequiredWeaponAssets)
{
	if (bHiddenBossUnlocked) return true;

	for (const FPrimaryAssetId& Req : RequiredWeaponAssets)
	{
		if (!CollectedItemAssets.Contains(Req))
		{
			return false;
		}
	}

	bHiddenBossUnlocked = true;

	return true;
}

void UTPRGameInstance::CacheEquippedWeapon(UInventoryItem* EquippedWeaponItem)
{
	CachedEquippedWeaponAssetId = FPrimaryAssetId(); 

	if (!EquippedWeaponItem || !EquippedWeaponItem->ItemData) return;

	if (!Cast<UWeaponItemData>(EquippedWeaponItem->ItemData)) return;

	CachedEquippedWeaponAssetId = EquippedWeaponItem->ItemData->GetPrimaryAssetId();
}

