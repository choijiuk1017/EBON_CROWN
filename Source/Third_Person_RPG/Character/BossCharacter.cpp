// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Character/BossCharacter.h"
#include "Third_Person_RPG/Character/CombatBalance.h"
#include "Character/DeathRagdoll.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "Third_Person_RPG/Character/EnemyAIController.h"
#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "Third_Person_RPG/Character/TPRPlayerController.h"
#include "Third_Person_RPG/Data/SkillData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Components/ProgressBar.h"
#include "Kismet/GameplayStatics.h"
#include "BehaviorTree/BlackboardComponent.h"

#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarDebugBossHitboxes(
	TEXT("tpr.DebugBossHitboxes"), 0, TEXT("Draw actual boss attack shapes (0/1)."), ECVF_Cheat);

#define CHANNEL_ACTION ECollisionChannel::ECC_GameTraceChannel2
// Sets default values
ABossCharacter::ABossCharacter()
{
	Tags.Add("Boss");

	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));


	WeaponMesh->SetupAttachment(GetMesh(), TEXT("WeaponSocket"));

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AEnemyAIController::StaticClass();

	GetCapsuleComponent()->SetCollisionResponseToChannel(CHANNEL_ACTION, ECR_Overlap);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);

}

// Called when the game starts or when spawned
void ABossCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (bUseBalanceProfile)
	{
		CombatBalance::Apply(this, BossStats.MaxHP, BossStats.CurrentHP,
			BossStats.AttackPower, BossStats.Defense, CurrencyReward);
	}

	PlayerRef = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (HPBarWidgetClass)
		{
			HPBarWidget = CreateWidget<UHPBar>(PC, HPBarWidgetClass);
			if (HPBarWidget)
			{
				HPBarWidget->AddToViewport(10);
				HPBarWidget->SetVisibility(ESlateVisibility::Hidden);
			}
		}
	}

}

// Called every frame
void ABossCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!PlayerRef || !HPBarWidget) return;

	// 플레이어와 거리 계산
	float Distance = FVector::Dist(PlayerRef->GetActorLocation(), GetActorLocation());

	if (Distance <= 1200.f)
	{
		if (HPBarWidget->GetVisibility() != ESlateVisibility::Visible)
		{
			HPBarWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}
	else
	{
		if (HPBarWidget->GetVisibility() != ESlateVisibility::Hidden)
		{
			HPBarWidget->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

// Called to bind functionality to input
void ABossCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void ABossCharacter::BaseAttackCheck()
{
	if (bIsDead || AttackRange <= 0.f) return;
	TArray<FOverlapResult> OverlapResults;

	// 충돌 탐지를 위한 시작 지점
	FVector Start = GetActorLocation() + (GetActorForwardVector() * GetCapsuleComponent()->GetScaledCapsuleRadius());

	// 충돌 탐지 끝 지점
	FVector End = Start + (GetActorForwardVector() * AttackRange);

	// 오버랩 중심점 (Capsule처럼 보이게)
	FVector CapsuleOrigin = Start + (End - Start) * 0.5f;
	FQuat CapsuleRotation = FRotationMatrix::MakeFromZ(GetActorForwardVector()).ToQuat();

	// 오버랩 영역 설정
	const float Radius = FMath::Clamp(AttackRange * 0.4f, 20.f, 70.f);
	const float HalfHeight = Radius + AttackRange * 0.5f;
	FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(Radius, HalfHeight);  // Radius, HalfHeight

	// 쿼리 파라미터
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttackOverlap), false, this);

	// 오버랩 실행
	bool bHasHit = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		CapsuleOrigin,
		CapsuleRotation,
		CHANNEL_ACTION,
		CollisionShape,
		Params
	);

	// 피격 판정 처리
	if (!OverlapResults.IsEmpty())
	{
		TSet<APlayerCharacter*> DamagedPlayers;

		for (const FOverlapResult& Result : OverlapResults)
		{
			if (APlayerCharacter* Player = Cast<APlayerCharacter>(Result.GetActor()))
			{
				if (!DamagedPlayers.Contains(Player))
				{
					DamagedPlayers.Add(Player);
					UE_LOG(LogTemp, Warning, TEXT("Player Damaged via Overlap"));

					Player->TakeDamage(BossStats.AttackPower);
				}
			}
		}
	}

	// 디버그용 캡슐 시각화
	FColor DrawColor = bHasHit ? FColor::Green : FColor::Red;
	if (CVarDebugBossHitboxes.GetValueOnGameThread() != 0)
		DrawDebugCapsule(GetWorld(), CapsuleOrigin, HalfHeight, Radius, CapsuleRotation, DrawColor, false, 0.5f);
}

void ABossCharacter::PlayAttackMontageByIndex(int32 Index)
{
	if (bIsDead) return;
	if (AttackMontages.IsValidIndex(Index) && GetMesh() && GetMesh()->GetAnimInstance())
	{
		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		AnimInstance->Montage_Play(AttackMontages[Index]);

		// 종료 콜백
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &ABossCharacter::OnAttackMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, AttackMontages[Index]);
	}
}

void ABossCharacter::PlayPatternMontage(int32 Index)
{
	if (bIsDead || !PatternDatas.IsValidIndex(Index) || !IsValid(PatternDatas[Index])) return;
	UAnimMontage* Montage = PatternDatas[Index]->SkillMontage;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (!AnimInstance || !IsValid(Montage) || AnimInstance->Montage_IsActive(Montage)) return;
	if (AnimInstance->Montage_Play(Montage) <= 0.f) return;
	CurrentPatternIndex = Index;
	if (BossType == 1)
	{
		if (Index == 1)
		{
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		}
		else if (Index == 2)
		{
			GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
		}
		else
		{
			GetCharacterMovement()->StopMovementImmediately();
			GetCharacterMovement()->DisableMovement();
		}

		if (Index == 1)
		{
			GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Overlap);
		}
		
	}
	FOnMontageEnded EndDelegate;
	EndDelegate.BindUObject(this, &ABossCharacter::PatternEnd);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
}
void ABossCharacter::PatternEnd(UAnimMontage* Montage, bool IsEnded)
{
	if (bIsDead) return;
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Block);
}

void ABossCharacter::SkillAttackCheckByIndex(int32 Index)
{
	if (bIsDead || !PatternDatas.IsValidIndex(Index)) return;
	const USkillData* Data = PatternDatas[Index];
	if (!IsValid(Data)) return;

	SpawnSkillEffectByData(Data);
	float Radius = FMath::Max(0.f, Data->SkillRadius);
	float Range = FMath::Max(0.f, Data->SkillRange);
	if (bUsePatternCollisionProfiles && GConfig)
	{
		const FString Section = TEXT("EbonCrown.PatternCollision.") + Data->GetPathName();
		GConfig->GetFloat(*Section, TEXT("Radius"), Radius, GGameIni);
		GConfig->GetFloat(*Section, TEXT("Range"), Range, GGameIni);
	}
	if (!FMath::IsFinite(Radius) || !FMath::IsFinite(Range) || Radius <= 0.f) return;
	Range = FMath::Max(0.f, Range);

	const FVector Start = GetSkillOrigin(Data);
	const FVector Forward = GetActorForwardVector();
	const FVector Center = Start + Forward * (Range * 0.5f);
	const float HalfHeight = Radius + Range * 0.5f;
	const FQuat Rotation = FRotationMatrix::MakeFromZ(Forward).ToQuat();
	const FCollisionShape Shape = Range > KINDA_SMALL_NUMBER
		? FCollisionShape::MakeCapsule(Radius, HalfHeight)
		: FCollisionShape::MakeSphere(Radius);
	TArray<FOverlapResult> Results;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BossPattern), false, this);
	GetWorld()->OverlapMultiByChannel(Results, Center, Rotation, CHANNEL_ACTION, Shape, Params);

	// OverlapMulti's bool reports blocking hits; overlap-only player components still count.
	TSet<APlayerCharacter*> DamagedPlayers;
	for (const FOverlapResult& Result : Results)
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(Result.GetActor()))
		{
			if (!DamagedPlayers.Contains(Player))
			{
				DamagedPlayers.Add(Player);
				Player->TakeDamage(bUseBalanceProfile
					? CombatBalance::PatternDamage(this, Data->SkillDamage) : Data->SkillDamage);
			}
		}
	}
	if (CVarDebugBossHitboxes.GetValueOnGameThread() != 0)
	{
		const FColor Color = DamagedPlayers.IsEmpty() ? FColor::Red : FColor::Green;
		if (Range > KINDA_SMALL_NUMBER)
			DrawDebugCapsule(GetWorld(), Center, HalfHeight, Radius, Rotation, Color, false, 0.5f);
		else
			DrawDebugSphere(GetWorld(), Center, Radius, 24, Color, false, 0.5f);
	}
}

FVector ABossCharacter::GetSkillOrigin(const USkillData* Data) const
{
	switch (Data->SpawnType)
	{
	case ESkillEffectSpawnType::Forward:
		return GetActorLocation() + GetActorForwardVector() * 200.f;
	case ESkillEffectSpawnType::Ground:
		return GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	case ESkillEffectSpawnType::Custom:
		return GetActorLocation() + GetActorRotation().RotateVector(Data->CustomSpawnOffset);
	default:
		return GetActorLocation();
	}
}

void ABossCharacter::SpawnSkillEffectByData(const USkillData* Data)
{
	if (!Data || !Data->SkillEffect) return;

	const FVector SpawnLocation = GetSkillOrigin(Data);
	const FRotator SpawnRotation = GetActorRotation();

	UGameplayStatics::SpawnEmitterAtLocation(
		GetWorld(),
		Data->SkillEffect,
		SpawnLocation,
		SpawnRotation
	);
}


void ABossCharacter::OnAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (bIsDead) return;

	CurrentAttackStep++;

	if (CurrentAttackStep >= AttackMontages.Num())
	{
		CurrentAttackStep = 0;
		return;
	}
}

void ABossCharacter::PlayHitReactMontage()
{
	if (HitReactMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(HitReactMontage);
	}
}

void ABossCharacter::RegisterAttacker(AActor* Attacker)
{
	if (!Attacker) return;
	if (bIsDead) return;

	if (APlayerCharacter* PC = Cast<APlayerCharacter>(Attacker))
	{
		LastAttacker = PC;
	}
}

void ABossCharacter::GrantCurrencyToKiller()
{
	if (bRewardGranted) return;
	if (!LastAttacker.IsValid()) return;
	UE_LOG(LogTemp, Warning, TEXT("LastAttacker Reward"));
	LastAttacker->AddCurrency(CurrencyReward);
	bRewardGranted = true;
}

void ABossCharacter::TakeDamage(int32 DamageAmount)
{
	if (bIsDead || DamageAmount <= 0) return;
	const bool bWasAlive = !bIsDead && BossStats.CurrentHP > 0;
	int32 Defense = BossStats.Defense;

	float DamageMultiplier = 100.f / (100.f + static_cast<float>(Defense));
	float FinalDamage = DamageAmount * DamageMultiplier;

	BossStats.CurrentHP -= FinalDamage;
	if (bWasAlive && FinalDamage > 0.f && LastAttacker.IsValid())
	{
		if (ATPRPlayerController* PC = Cast<ATPRPlayerController>(LastAttacker->GetController()))
		{
			PC->PlayCombatHitShake(false);
		}
	}

	CurrentGroggy += 25;

	if (BossStats.CurrentHP <= 0)
	{
		// 사망 처리
		BossStats.CurrentHP = 0;
		bIsDead = true;

		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(0.1f); 
		}

		if (AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController()))
		{
			AIController->PauseAI();
			AIController->StopMovement();
			AIController->UnPossess();
		}

		OnBossDied.Broadcast();

		GrantCurrencyToKiller();

		if (HPBarWidget)
		{
			GetWorld()->GetTimerManager().ClearTimer(HideHPBarTimerHandle);
			HPBarWidget->RemoveFromParent();
			HPBarWidget = nullptr;
		}

		GetWorldTimerManager().ClearTimer(HitReactTimerHandle);
		PlayDeathThenRagdoll(*this);

		UE_LOG(LogTemp, Verbose, TEXT("Monster Dead"));

		return;
	}

	if (CurrentGroggy == MaxGroggy)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			if (HitReactMontage)
			{
				AnimInstance->Montage_Play(HitReactMontage);

				if (AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(GetController()))
				{
					EnemyAI->PauseAI();
				}

				GetCharacterMovement()->DisableMovement();

				GetWorld()->GetTimerManager().SetTimer(
					HitReactTimerHandle,
					this,
					&ABossCharacter::EndHitReact,
					HitReactDuration,
					false
				);

				CurrentGroggy = 0;
			}
		}
	}

	if (HPBarWidget)
	{
		float Percent = static_cast<float>(BossStats.CurrentHP) / static_cast<float>(BossStats.MaxHP);
		HPBarWidget->SetHPBarPercent(Percent);

		HPBarWidget->SetVisibility(ESlateVisibility::Visible);

		GetWorld()->GetTimerManager().ClearTimer(HideHPBarTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(
			HideHPBarTimerHandle,
			[this]()
			{
				if (HPBarWidget && !bIsDead)
				{
					HPBarWidget->SetVisibility(ESlateVisibility::Hidden);
				}
			},
			1.0f,
			false
		);
	}
}

void ABossCharacter::EndHitReact()
{
	if (!bIsDead)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		if (AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(GetController()))
		{
			EnemyAI->ResumeAI();
		}
	}
}

