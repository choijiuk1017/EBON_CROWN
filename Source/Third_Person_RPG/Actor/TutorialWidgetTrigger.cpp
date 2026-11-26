// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Actor/TutorialWidgetTrigger.h"

#include "Components/BoxComponent.h"
#include "UObject/UnrealType.h"  
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Third_Person_RPG/UI/Tutorial/TutorialWidget.h"
#include "Third_Person_RPG/Instance/TPRGameInstance.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
ATutorialWidgetTrigger::ATutorialWidgetTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;

	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ATutorialWidgetTrigger::OnTriggerOverlap);

}

// Called when the game starts or when spawned
void ATutorialWidgetTrigger::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATutorialWidgetTrigger::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ATutorialWidgetTrigger::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bHasPlayed) return;

	if (!OtherActor || !OtherActor->ActorHasTag("Player")) return;

	UTPRGameInstance* GI = Cast<UTPRGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));
	if (!GI) return;

	if (!TutorialWidgetClass) return;
	// Missing IDs fall back to the widget class, so unrelated tutorials do not share None.
	const FName EffectiveID = TriggerID.IsNone() ? FName(*TutorialWidgetClass->GetPathName()) : TriggerID;
	if (GI->FinishedTutorials.Contains(EffectiveID)) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	if (TutorialWidgetClass)
	{
		UTutorialWidget* TutorialWidget = CreateWidget<UTutorialWidget>(PC, TutorialWidgetClass);
		if (TutorialWidget)
		{
			TutorialWidget->TutorialID = EffectiveID;
			TutorialWidget->OnTutorialFinished.AddUniqueDynamic(GI, &UTPRGameInstance::OnTutorialFinished);
			TutorialWidget->AddToViewport(999);
			bHasPlayed = true;

			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(TutorialWidget->GetCachedWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PC->SetInputMode(InputMode);
			PC->bShowMouseCursor = true;

			TutorialWidget->SetKeyboardFocus();
			// Completion is bound before widget construction.
		
		}

	}

}