// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/UI/SavePointUI/SavePointMenu.h"
#include "Components/VerticalBox.h"
#include "Third_Person_RPG/UI/SavePointUI/SavePointMenuEntry.h"
#include "Third_Person_RPG/UI/SavePointUI/WorldTravelMenu.h"
#include "Third_Person_RPG/UI/SavePointUI/LevelUpMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "InputCoreTypes.h"

USavePointMenu::USavePointMenu(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	SetIsFocusable(true);
}


void USavePointMenu::NativeConstruct()
{
	Super::NativeConstruct();

	//// 초기화
	EntryWidgets.Empty();

	if (!EntryClass || !VerticalBox_MenuList) return;

	UE_LOG(LogTemp, Verbose, TEXT("Menu Options Num: %d"), MenuOptions.Num());

	for (int32 i = 0; i < MenuOptions.Num(); ++i)
	{
		USavePointMenuEntry* NewEntry = CreateWidget<USavePointMenuEntry>(this, EntryClass);
		if (!NewEntry)
		{
			UE_LOG(LogTemp, Error, TEXT("NewEntry is null at index %d"), i);
			continue;
		}

		NewEntry->SetMenuText(MenuOptions[i]);
		VerticalBox_MenuList->AddChild(NewEntry);
		EntryWidgets.Add(NewEntry);

		UE_LOG(LogTemp, Warning, TEXT("Created Entry %d with text: %s"), i, *MenuOptions[i].ToString());

		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateWeakLambda(NewEntry, [=]()
			{
				if (!NewEntry) return;

				FGeometry Geo = NewEntry->GetCachedGeometry();
				FVector2D AbsPos = Geo.GetAbsolutePosition();
				FVector2D Size = Geo.GetLocalSize();

				UE_LOG(LogTemp, Warning, TEXT("Entry[%d] Pos: (%.1f, %.1f), Size: (%.1f, %.1f)"),
					i, AbsPos.X, AbsPos.Y, Size.X, Size.Y);

			}), 0.f, false);
	}

	UpdateSelectionVisual(); 

	this->SetKeyboardFocus();
	FSlateApplication::Get().SetKeyboardFocus(this->TakeWidget(), EFocusCause::SetDirectly);
}


void USavePointMenu::NativeDestruct()
{
	if (ActiveSubmenu) ActiveSubmenu->RemoveFromParent();
	ActiveSubmenu = nullptr;
	Super::NativeDestruct();
}

FReply USavePointMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();
	if (PressedKey == EKeys::X)
	{
		if (InKeyEvent.IsRepeat()) return FReply::Handled();
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwningPlayerPawn()))
			Player->EndInteractSavePoint();
		return FReply::Handled();
	}
	UE_LOG(LogTemp, Warning, TEXT("Pressed Key: %s"), *PressedKey.ToString());

	if (PressedKey == EKeys::W || PressedKey == EKeys::Up)
	{
		MoveSelectionUp();
		return FReply::Handled();
	}
	else if (PressedKey == EKeys::S || PressedKey == EKeys::Down)
	{
		MoveSelectionDown();
		return FReply::Handled();
	}
	else if (PressedKey == EKeys::Enter)
	{
		if (!InKeyEvent.IsRepeat()) ConfirmSelection();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USavePointMenu::MoveSelectionUp()
{
	if (EntryWidgets.Num() == 0) return;

	SelectedIndex = (SelectedIndex - 1 + EntryWidgets.Num()) % EntryWidgets.Num();
	UpdateSelectionVisual();
}

void USavePointMenu::MoveSelectionDown()
{
	if (EntryWidgets.Num() == 0) return;

	SelectedIndex = (SelectedIndex + 1) % EntryWidgets.Num();
	UpdateSelectionVisual();
}

void USavePointMenu::UpdateSelectionVisual()
{
	for (int32 i = 0; i < EntryWidgets.Num(); ++i)
	{
		if (EntryWidgets[i])
		{
			const bool bIsSelected = (i == SelectedIndex);
			EntryWidgets[i]->SetSelected(bIsSelected);
		}
	}
}

void USavePointMenu::ConfirmSelection()
{
	if (!EntryWidgets.IsValidIndex(SelectedIndex)) return;

	const FString& SelectedText = MenuOptions[SelectedIndex].ToString();

	if (SelectedText == TEXT("떠난다"))
	{
		RemoveFromParent();
		APlayerController* PC = GetOwningPlayer();
		if (PC)
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;

			APawn* Pawn = PC->GetPawn();
			if (Pawn)
			{
				APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(Pawn);
				if (PlayerCharacter)
				{
					PlayerCharacter->EndInteractSavePoint();

					PlayerCharacter->SavePointMenuInstance = nullptr;
				}
			}
			
		}
	}
	else if (SelectedText == TEXT("레벨 업"))
	{
		ActiveSubmenu = CreateWidget<ULevelUpMenu>(GetOwningPlayer(), LevelUpMenuClass);
	}
	else if (SelectedText == TEXT("화톳불 이동"))
	{
		ActiveSubmenu = CreateWidget<UWorldTravelMenu>(GetOwningPlayer(), WorldTravelMenuClass);
	}
	if (ActiveSubmenu)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		ActiveSubmenu->AddToViewport();
		APlayerController* PC = GetOwningPlayer();
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(ActiveSubmenu->TakeWidget());
		PC->SetInputMode(Mode);
	}
}

void USavePointMenu::ReturnFromSubmenu(UUserWidget& Child)
{
	APlayerController* PC = Child.GetOwningPlayer();
	APlayerCharacter* Player = Cast<APlayerCharacter>(Child.GetOwningPlayerPawn());
	USavePointMenu* Menu = Player ? Player->SavePointMenuInstance : nullptr;
	Child.RemoveFromParent();
	if (!PC || !IsValid(Menu) || Menu->ActiveSubmenu != &Child || !Menu->IsInViewport()) return;
	Menu->ActiveSubmenu = nullptr;
	Menu->SetVisibility(ESlateVisibility::Visible);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Menu->TakeWidget());
	PC->SetInputMode(Mode);
	Menu->SetUserFocus(PC);
}
