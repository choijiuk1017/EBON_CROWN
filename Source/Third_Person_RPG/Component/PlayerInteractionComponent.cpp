// Fill out your copyright notice in the Description page of Project Settings.


#include "Component/PlayerInteractionComponent.h"

#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "Third_Person_RPG/Inventory/InventoryComponent.h"
#include "Third_Person_RPG/Item/Item.h"
#include "Third_Person_RPG/Item/Weapon/TPRWeapon.h"
#include "Third_Person_RPG/Actor/SavePoint.h"
#include "Third_Person_RPG/Actor/NPC.h"
#include "Third_Person_RPG/Instance/TPRGameInstance.h"

#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UPlayerInteractionComponent::UPlayerInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!OwnerPlayer)
	{
		OwnerPlayer = Cast<APlayerCharacter>(GetOwner());
	}
}

void UPlayerInteractionComponent::InitializeInteractionComponent(APlayerCharacter* InOwner)
{
	OwnerPlayer = InOwner;
}

void UPlayerInteractionComponent::SetOverlappingItem(AItem* Item)
{
	if (!OwnerPlayer) return;

	OwnerPlayer->OverlappingItem = Item;
}

void UPlayerInteractionComponent::ReSetOverlappingItem()
{
	if (!OwnerPlayer) return;

	OwnerPlayer->OverlappingItem = nullptr;
}

void UPlayerInteractionComponent::SetCurrentNPC(ANPC* InNPC)
{
	if (!OwnerPlayer) return;

	OwnerPlayer->CurrentNPC = InNPC;
}

void UPlayerInteractionComponent::Interact()
{
	if (!OwnerPlayer) return;

	if (OwnerPlayer->bIsRoll ||
		OwnerPlayer->bIsSkillActing ||
		OwnerPlayer->bIsAttacking ||
		OwnerPlayer->bIsInteracting)
	{
		return;
	}

	if (OwnerPlayer->bIsDead) return;

	UE_LOG(LogTemp, Warning, TEXT("상호작용 키 입력됨"));

	if (IsValid(OwnerPlayer->OverlappingItem) && OwnerPlayer->OverlappingItem->ItemData)
	{
		AItem* Item = OwnerPlayer->OverlappingItem;
		if (Item->WeaponClass && OwnerPlayer->CanSetWeapon())
		{
			UAnimInstance* Anim = OwnerPlayer->GetMesh()->GetAnimInstance();
			if (Anim && OwnerPlayer->PickUpMontage && Anim->Montage_Play(OwnerPlayer->PickUpMontage, 1.7f) > 0.f)
			{
				PendingPickup = Item;
				OwnerPlayer->bIsInteracting = true;
				OwnerPlayer->GetCharacterMovement()->SetMovementMode(MOVE_None);
				FOnMontageEnded EndDelegate;
				EndDelegate.BindUObject(this, &UPlayerInteractionComponent::OnEquipAnimationEnd);
				Anim->Montage_SetEndDelegate(EndDelegate, OwnerPlayer->PickUpMontage);
				return;
			}
		}
		TryCollectItem(Item);
		return;
	}

	if (OwnerPlayer->OverlappingSavePoint)
	{
		OwnerPlayer->bIsInteracting = true;

		if (OwnerPlayer->CurrentWeapon)
		{
			OwnerPlayer->CurrentWeapon->SetActorHiddenInGame(true);
			OwnerPlayer->CurrentWeapon->SetActorEnableCollision(false);
		}

		if (OwnerPlayer->InteractionWidgetInstance)
		{
			OwnerPlayer->InteractionWidgetInstance->SetVisibility(ESlateVisibility::Hidden);
		}

		OwnerPlayer->GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_None);

		OwnerPlayer->SetStatusHUDVisible(false);

		UAnimInstance* AnimInstance = OwnerPlayer->GetMesh()->GetAnimInstance();

		if (AnimInstance && OwnerPlayer->KneelingDownMontage)
		{
			AnimInstance->Montage_Play(OwnerPlayer->KneelingDownMontage, 1.0f);

			OwnerPlayer->bIsKneeling = true;

			FOnMontageEnded EndDelegate;
			EndDelegate.BindUObject(OwnerPlayer, &APlayerCharacter::InteractingSavePoint);
			AnimInstance->Montage_SetEndDelegate(EndDelegate, OwnerPlayer->KneelingDownMontage);

			return;
		}

		if (UTPRGameInstance* GI =
			Cast<UTPRGameInstance>(UGameplayStatics::GetGameInstance(OwnerPlayer->GetWorld())))
		{
			GI->SaveGameData();
		}
	}

	if (OwnerPlayer->CurrentNPC)
	{
		OwnerPlayer->CurrentNPC->StartTalk(OwnerPlayer);
		return;
	}
}

bool UPlayerInteractionComponent::TryCollectItem(AItem* Item)
{
	if (!OwnerPlayer || !IsValid(Item) || !Item->ItemData) return false;
	UInventoryComponent* Inventory = OwnerPlayer->GetInventoryComponent();
	if (!Inventory || !Inventory->AddItemByData(Item->ItemData, 1))
	{
		OwnerPlayer->ShowInteractionUI(FText::FromString(TEXT("\uC778\uBCA4\uD1A0\uB9AC\uAC00 \uAC00\uB4DD \uCC3C\uC2B5\uB2C8\uB2E4.")));
		return false;
	}
	if (UTPRGameInstance* GI = Cast<UTPRGameInstance>(OwnerPlayer->GetGameInstance()))
		GI->RegisterCollectedItemAsset(Item->ItemData->GetPrimaryAssetId());
	if (OwnerPlayer->OverlappingItem == Item)
	{
		OwnerPlayer->OverlappingItem = nullptr;
		OwnerPlayer->HideInteractionUI();
	}
	Item->Destroy();
	return true;
}

void UPlayerInteractionComponent::OnEquipAnimationEnd(UAnimMontage* Montage, bool bInterrupted)
{
	if (!OwnerPlayer) return;
	AItem* Item = PendingPickup.Get();
	PendingPickup.Reset();
	OwnerPlayer->bIsInteracting = false;
	if (!OwnerPlayer->bIsDead && !OwnerPlayer->bIsKneeling)
		OwnerPlayer->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (!bInterrupted && !OwnerPlayer->bIsDead) TryCollectItem(Item);
}
