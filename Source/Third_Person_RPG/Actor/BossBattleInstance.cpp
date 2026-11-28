// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Actor/BossBattleInstance.h"

#include "Third_Person_RPG/Character/BossCharacter.h"
#include "Third_Person_RPG/Actor/BossSequenceTrigger.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"

#include "Third_Person_RPG/Instance/TPRGameInstance.h"

// Sets default values
ABossBattleInstance::ABossBattleInstance()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ABossBattleInstance::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(BossSequenceTrigger))
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), BossSequenceTag, FoundActors);
		for (AActor* Actor : FoundActors)
		{
			if (ABossSequenceTrigger* Trigger = Cast<ABossSequenceTrigger>(Actor))
			{
				BossSequenceTrigger = Trigger;
				break;
			}
		}
	}

	const UTPRGameInstance* GI = Cast<UTPRGameInstance>(GetGameInstance());
	if (IsValid(BossSequenceTrigger) && GI && GI->HasClearedBoss(BossSequenceTrigger->BossID))
	{
		for (AStaticMeshActor* BlockActor : BossRoomBlock)
		{
			if (IsValid(BlockActor)) BlockActor->Destroy();
		}
		// Dungeon ambience is attached to this actor and must survive a cleared encounter.
		SetActorTickEnabled(false);
		return;
	}

	if (IsValid(BossRef))
	{
		BossRef->OnBossDied.AddUniqueDynamic(this, &ABossBattleInstance::OnBossDiedHandler);
	}
	
}

// Called every frame
void ABossBattleInstance::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsValid(BossRef))
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), BossActorTag, FoundActors);

		if (FoundActors.Num() > 0)
		{
			ABossCharacter* FoundBoss = Cast<ABossCharacter>(FoundActors[0]);
			if (FoundBoss)
			{
				BossRef = FoundBoss;
				BossRef->OnBossDied.AddUniqueDynamic(this, &ABossBattleInstance::OnBossDiedHandler);
			}
		}
	}

	if (!IsValid(BossSequenceTrigger))
	{
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), BossSequenceTag, FoundActors);

		if (FoundActors.Num() > 0)
		{
			ABossSequenceTrigger* FoundBoss = Cast<ABossSequenceTrigger>(FoundActors[0]);
			if (FoundBoss)
			{
				BossSequenceTrigger = FoundBoss;
			}
		}
	}

}

void ABossBattleInstance::OnBossDiedHandler()
{
	for (AStaticMeshActor* BlockActor : BossRoomBlock)
	{
		if (IsValid(BlockActor))
		{
			BlockActor->Destroy();
		}
	}

	if (!IsValid(BossSequenceTrigger))
    {
        return;
    }

	BossSequenceTrigger->HandleBossDefeated();
	SetActorTickEnabled(false);
}
