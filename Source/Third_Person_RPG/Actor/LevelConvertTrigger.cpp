// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Actor/LevelConvertTrigger.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Third_Person_RPG/Instance/TPRGameInstance.h"
#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Engine/StreamableManager.h"
#include "Instance/LevelPreloadSubsystem.h"

// Sets default values
ALevelConvertTrigger::ALevelConvertTrigger()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	RootComponent = TriggerBox;
	TriggerBox->SetCollisionProfileName(TEXT("Trigger"));
	TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ALevelConvertTrigger::OnOverlapBegin);

}

// Called when the game starts or when spawned
void ALevelConvertTrigger::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ALevelConvertTrigger::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


void ALevelConvertTrigger::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !OtherActor->ActorHasTag("Player")) return;

	if (bTransitioning) return;      
	bTransitioning = true;

	UWorld* World = GetWorld();
	if (!World) return;




	if (APlayerCharacter* PC = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		if (UTPRGameInstance* GI = Cast<UTPRGameInstance>(PC->GetGameInstance()))
		{
			GI->CachePlayerForTravel(PC);
		}
	}

	
	if (LoadingScreen)
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(GetWorld(), LoadingScreen);
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->AddToViewport(9999);
		}
	}

	GetGameInstance()->GetSubsystem<ULevelPreloadSubsystem>()->OpenWithProgress(NextLevelName);

	UE_LOG(LogTemp, Warning, TEXT("Trying to convert level: %s"), *NextLevelName.ToString());

}

void ALevelConvertTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LevelTransitionTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}
