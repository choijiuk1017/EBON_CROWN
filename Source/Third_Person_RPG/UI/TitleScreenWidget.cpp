// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/UI/TitleScreenWidget.h"

#include "Components/TextBlock.h"
#include "Instance/LevelPreloadSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Third_Person_RPG/TPRSaveGame.h"
#include "Third_Person_RPG/Instance/TPRGameInstance.h"


void UTitleScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentIndex = 0;

	UpdateSelection();

	bIsFocusable = true;

	SetKeyboardFocus();

	if (StartAnim)
	{
		PlayAnimation(StartAnim, 0.f, 1, EUMGSequencePlayMode::Forward, 1.f);
	}
}

void UTitleScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (CreditsScreen && !CreditsScreen->IsInViewport())
	{
		CreditsScreen = nullptr;
		SetKeyboardFocus();
	}
}

FReply UTitleScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	if (CreditsScreen && CreditsScreen->IsInViewport())
	{
		if (!InKeyEvent.IsRepeat() && (Key == EKeys::X || Key == EKeys::Escape))
		{
			CreditsScreen->RemoveFromParent();
			CreditsScreen = nullptr;
			SetKeyboardFocus();
		}
		return FReply::Handled();
	}
	const int32 Count = CreditsText ? 4 : 3;
	if (Key == EKeys::Down)
	{
		CurrentIndex = (CurrentIndex + 1) % Count;
		UpdateSelection();
		return FReply::Handled();
	}

	if (Key == EKeys::Up)
	{
		CurrentIndex = (CurrentIndex - 1 + Count) % Count;
		UpdateSelection();
		return FReply::Handled();
	}

	if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		if (!InKeyEvent.IsRepeat()) ExecuteSelection();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UTitleScreenWidget::UpdateSelection()
{
	FSlateColor NormalColor = FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f));

	FSlateColor SelectedColor = FSlateColor(FLinearColor(1.0f, 0.8f, 0.3f));

	NewGameText->SetColorAndOpacity(NormalColor);
	LoadGameText->SetColorAndOpacity(NormalColor);
	ExitText->SetColorAndOpacity(NormalColor);
	if (CreditsText) CreditsText->SetColorAndOpacity(NormalColor);

	switch (CurrentIndex)
	{
	case 0: NewGameText->SetColorAndOpacity(SelectedColor); break;
	case 1: LoadGameText->SetColorAndOpacity(SelectedColor); break;
	case 2: ExitText->SetColorAndOpacity(SelectedColor); break;
	case 3: if (CreditsText) CreditsText->SetColorAndOpacity(SelectedColor); break;
	}
}

FReply UTitleScreenWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bStartingGame && !CreditsScreen && CreditsText && CreditsText->IsHovered() &&
		Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		CurrentIndex = 3;
		UpdateSelection();
		OpenCredits();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(Geometry, Event);
}

void UTitleScreenWidget::OpenCredits()
{
	if (CreditsScreen) return;
	UClass* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/BluePrint/UI_Widget/WBP_Credit.WBP_Credit_C"));
	if (!Class) return;
	CreditsScreen = CreateWidget<UUserWidget>(GetOwningPlayer(), Class);
	if (CreditsScreen)
	{
		CreditsScreen->AddToViewport(50);
		SetKeyboardFocus();
	}
}

void UTitleScreenWidget::ExecuteSelection()
{
	if (bStartingGame) return;
	if (CurrentIndex == 3)
	{
		OpenCredits();
		return;
	}
	if (CurrentIndex == 2)
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), nullptr, EQuitPreference::Quit, true);
		return;
	}
	UTPRGameInstance* GI = Cast<UTPRGameInstance>(GetGameInstance());
	if (!GI) return;
	FName TargetMap(TEXT("Start"));
	if (CurrentIndex == 0)
	{
		if (!GI->DeleteSaveData()) return;
	}
	else
	{
		if (UGameplayStatics::DoesSaveGameExist(TEXT("PlayerSaveSlot"), 0))
		{
			if (!GI->PrepareContinue(TargetMap)) return;
		}
		else if (!GI->DeleteSaveData()) return;
	}
	bStartingGame = true;
	if (LoadingScreen)
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(GetWorld(), LoadingScreen);
		if (LoadingScreenWidget) LoadingScreenWidget->AddToViewport(9999);
	}
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, TargetMap]()
		{
			// UI-only input belongs to the viewport and can survive world travel.
			// Restore it here, before destination BeginPlay can start a cutscene/UI.
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			{
				PC->FlushPressedKeys();
				PC->SetInputMode(FInputModeGameOnly());
				PC->bShowMouseCursor = false;
				UE_LOG(LogTemp, Log, TEXT("[TitleTravel] Restored game input before opening %s"), *TargetMap.ToString());
			}
			GetGameInstance()->GetSubsystem<ULevelPreloadSubsystem>()->OpenWithProgress(TargetMap);
		}), 0.1f, false);
}
