// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Character/BossDragon.h"
#include "HAL/IConsoleManager.h"
#include "DrawDebugHelpers.h"
#include "Third_Person_RPG/Character/CombatBalance.h"
#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "Third_Person_RPG/Character/EnemyAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"


#define CHANNEL_ACTION ECollisionChannel::ECC_GameTraceChannel2

ABossDragon::ABossDragon()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ABossDragon::BeginPlay()
{
    Super::BeginPlay();
    OnBossDied.AddUniqueDynamic(this, &ABossDragon::HandleDragonDeath);
}

void ABossDragon::HandleDragonDeath()
{
    GetWorldTimerManager().ClearTimer(FlyFallTimerHandle);
    EndBreath();
    bIsFly = false;
    bFalling = false;
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
}

void ABossDragon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(FlyFallTimerHandle);
    EndBreath();
    Super::EndPlay(EndPlayReason);
}

void ABossDragon::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bIsDead) return;

    if (bIsFly)
    {
        FlyTick(DeltaSeconds);
        return;
    }

    if (!bHasTriggeredFlyPhase && BossStats.CurrentHP <= BossStats.MaxHP * 0.5f)
    {
        bHasTriggeredFlyPhase = true;
        StartFlyPhase();
    }
}

void ABossDragon::StartFlyPhase()
{
    if (bIsDead) return;
    bIsFly = true;
    bFalling = false;

    bPlayedLandImpactSFX = false;

    if (AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(GetController()))
    {
        EnemyAI->PauseAI();
    }

    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->StopAllMontages(0.1f);
    }

    GetCharacterMovement()->GravityScale = 0.f;
    GetCharacterMovement()->SetMovementMode(MOVE_Flying);
    GetCharacterMovement()->RotationRate = FRotator(0, 0, 0);

    FVector Loc = GetActorLocation();
    Loc.Z += 10.f;
    SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);

    GetWorld()->GetTimerManager().SetTimer(
        FlyFallTimerHandle,
        this,
        &ABossDragon::StartFalling,
        5.0f,   
        false
    );
}

void ABossDragon::StartFalling()
{
    if (bIsDead) return;
    bFalling = true;

    APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (Player)
    {
        TargetFallLocation = Player->GetActorLocation();
    }
}

void ABossDragon::FlyTick(float DeltaSeconds)
{
    if (!bFalling)
    {
        AddActorWorldOffset(FVector(0, 0, FlyUpSpeed * DeltaSeconds), false);
        return;
    }

    FVector Direction3D = (TargetFallLocation - GetActorLocation()).GetSafeNormal();

    float HorizontalSpeed = 1200.f;
    FVector HorizontalMove = Direction3D;
    HorizontalMove.Z = 0; 
    HorizontalMove.Normalize();

    AddActorWorldOffset(HorizontalMove * HorizontalSpeed * DeltaSeconds, true);

    FVector FallMove = FVector(0, 0, -FallSpeed * DeltaSeconds);

    FHitResult Hit;
    AddActorWorldOffset(FallMove, true, &Hit);

    if (Hit.bBlockingHit)
    {
        OnLand(Hit.Location);
    }
}

void ABossDragon::OnLand(const FVector& LandPos)
{
    if (bIsDead) return;
    if (!bPlayedLandImpactSFX && LandImpactSFX)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this,
            LandImpactSFX,
            LandPos,
            LandImpactVolume,
            LandImpactPitch
        );
        bPlayedLandImpactSFX = true;
    }

    bIsFly = false;
    bFalling = false;

    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    GetCharacterMovement()->GravityScale = 1.f;
    GetCharacterMovement()->RotationRate = FRotator(0, 200.f, 0);

    if (LandAttackEffect)
    {
        FVector EffectPos = LandPos;
        EffectPos.Z -= 150.f;

        UGameplayStatics::SpawnEmitterAtLocation(
            GetWorld(),
            LandAttackEffect,
            EffectPos,
            FRotator::ZeroRotator,
            FVector(10.0f) 
        );
    }

    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByChannel(
        Overlaps,
        LandPos,
        FQuat::Identity,
        CHANNEL_ACTION,
        FCollisionShape::MakeSphere(FMath::Max(1.f, LandAttackRadius))
        );
    {
        TSet<APlayerCharacter*> DamagedPlayers;
        for (auto& O : Overlaps)
        {
            APlayerCharacter* P = Cast<APlayerCharacter>(O.GetActor());
            if (P && !DamagedPlayers.Contains(P))
            {
                DamagedPlayers.Add(P);
                P->TakeDamage(bUseBalanceProfile ? CombatBalance::PatternDamage(this, LandAttackDamage) : LandAttackDamage);
            }
        }
    }
    if (AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(GetController()))
    {
        EnemyAI->ResumeAI();
    }

}

void ABossDragon::StartBreath()
{
    if (bIsDead) return;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 0.f, 0.f);
	if (!BreathEffect) return;

	USkeletalMeshComponent* MeshComp = GetMesh();
	const FName SocketName = "FireBreathSocket";

	BreathPSC = UGameplayStatics::SpawnEmitterAttached(
		BreathEffect,
		MeshComp,
		SocketName,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget
	);

	GetWorld()->GetTimerManager().SetTimer(
		BreathTimerHandle,
		this,
		&ABossDragon::ApplyBreathDamage,
		0.2f,
		true
	);
}

void ABossDragon::ApplyBreathDamage()
{
    if (bIsDead || BreathRadius <= 0.f) return;
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const FTransform Socket = GetMesh()->GetSocketTransform("FireBreathSocket");
    const FVector Dir = Socket.GetUnitAxis(EAxis::Z).GetSafeNormal();
    const float Range = FMath::Max(0.f, BreathRange);
    const FVector Center = Socket.GetLocation() + Dir * Range * 0.5f;
    const float HalfHeight = BreathRadius + Range * 0.5f;
    const FQuat Rotation = FRotationMatrix::MakeFromZ(Dir).ToQuat();
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DragonBreath), false, this);
    GetWorld()->OverlapMultiByChannel(Overlaps, Center, Rotation, CHANNEL_ACTION,
        FCollisionShape::MakeCapsule(BreathRadius, HalfHeight), Params);

    TSet<APlayerCharacter*> DamagedPlayers;
    for (const FOverlapResult& O : Overlaps)
    {
        APlayerCharacter* Player = Cast<APlayerCharacter>(O.GetActor());
        if (!Player || DamagedPlayers.Contains(Player)) continue;
        DamagedPlayers.Add(Player);
        const float* LastTime = DamageCooldownMap.Find(Player);
        if (LastTime && CurrentTime - *LastTime < FMath::Max(0.f, BreathDamageCooldown)) continue;
        Player->TakeDamage(bUseBalanceProfile ? CombatBalance::PatternDamage(this, 150.f) : 150);
        DamageCooldownMap.Add(Player, CurrentTime);
    }
    const IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("tpr.DebugBossHitboxes"));
    if (Debug && Debug->GetInt() != 0)
        DrawDebugCapsule(GetWorld(), Center, HalfHeight, BreathRadius, Rotation, FColor::Orange, false, 0.2f);
}

void ABossDragon::EndBreath()
{
    GetCharacterMovement()->RotationRate = FRotator(0.f, 200.f, 0.f);
	GetWorld()->GetTimerManager().ClearTimer(BreathTimerHandle);

	if (BreathPSC)
	{
		BreathPSC->Deactivate();      
		BreathPSC->DestroyComponent();  
		BreathPSC = nullptr;
	}
}


