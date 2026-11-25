#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Kismet/GameplayStatics.h"
#include "Third_Person_RPG/TPRSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventorySaveRoundTripTest,
	"EBONCROWN.Save.InventoryAndCheckpointRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FInventorySaveRoundTripTest::RunTest(const FString& Parameters)
{
	UTPRSaveGame* Save = NewObject<UTPRSaveGame>();
	TestFalse(TEXT("Legacy saves default to no inventory snapshot"), Save->bHasInventorySnapshot);
	Save->bHasInventorySnapshot = true;
	Save->LastRestedSavePointID = TEXT("CampCheckpoint");
	FSavePointInfo Point;
	Point.SavePointID = Save->LastRestedSavePointID;
	Point.MapName = TEXT("Camp");
	Point.Location = FVector(100, 200, 300);
	Save->AllDiscoveredSavePoints.Add(Point);
	Save->EquippedWeaponAssetId = FPrimaryAssetId(TEXT("TPRItemData"), TEXT("DA_Sword_Item"));

	FStoredInventoryItem Item;
	Item.ItemData = TSoftObjectPtr<UTPRItemData>(FSoftObjectPath(TEXT("/Game/Data/Item_Data/Weapon/DA_Sword_Item.DA_Sword_Item")));
	Item.Quantity = 3;
	Item.EnhanceLevel = 4;
	Item.bEquipped = true;
	Save->InventoryItems.Add(Item);
	// Two entries with the same asset must not collapse into one map entry.
	Item.Quantity = 1;
	Item.EnhanceLevel = 2;
	Item.bEquipped = false;
	Save->InventoryItems.Add(Item);

	TArray<uint8> Bytes;
	if (!TestTrue(TEXT("Serialize save without touching the user's slot"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
	UTPRSaveGame* Loaded = Cast<UTPRSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Deserialize save"), Loaded)) return false;
	TestTrue(TEXT("Snapshot version survives"), Loaded->bHasInventorySnapshot);
	TestEqual(TEXT("Checkpoint ID survives"), Loaded->LastRestedSavePointID, Save->LastRestedSavePointID);
	TestEqual(TEXT("Equipped weapon ID survives"), Loaded->EquippedWeaponAssetId, Save->EquippedWeaponAssetId);
	if (!TestEqual(TEXT("Checkpoint count"), Loaded->AllDiscoveredSavePoints.Num(), 1)) return false;
	TestEqual(TEXT("Respawn map survives"), Loaded->AllDiscoveredSavePoints[0].MapName, Point.MapName);
	TestEqual(TEXT("Respawn location survives"), Loaded->AllDiscoveredSavePoints[0].Location, Point.Location);
	if (!TestEqual(TEXT("Both inventory entries survive"), Loaded->InventoryItems.Num(), 2)) return false;
	TestEqual(TEXT("Asset path survives"), Loaded->InventoryItems[0].ItemData.ToSoftObjectPath(), Item.ItemData.ToSoftObjectPath());
	TestEqual(TEXT("Quantity survives"), Loaded->InventoryItems[0].Quantity, 3);
	TestEqual(TEXT("First enhancement survives"), Loaded->InventoryItems[0].EnhanceLevel, 4);
	TestEqual(TEXT("Second enhancement survives"), Loaded->InventoryItems[1].EnhanceLevel, 2);
	TestTrue(TEXT("Equipped state survives"), Loaded->InventoryItems[0].bEquipped);
	TestFalse(TEXT("Unequipped state survives"), Loaded->InventoryItems[1].bEquipped);
	TestNotNull(TEXT("Saved asset resolves in this project"), Loaded->InventoryItems[0].ItemData.LoadSynchronous());
	return true;
}

#endif
