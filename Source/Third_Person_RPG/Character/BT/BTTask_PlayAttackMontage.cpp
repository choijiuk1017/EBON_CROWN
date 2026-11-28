// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Character/BT/BTTask_PlayAttackMontage.h"

#include "AIController.h"
#include "GameFramework/Character.h"

#include "Animation/AnimInstance.h"
#include "Third_Person_RPG/Character/EnemyCharacter.h"
#include "Third_Person_RPG/Character/BossCharacter.h"
#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_PlayAttackMontage::UBTTask_PlayAttackMontage()
{
	NodeName = TEXT("Play Attack Montage");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_PlayAttackMontage::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController) return EBTNodeResult::Failed;

    AActor* ControlledPawn = AIController->GetPawn();
    if (!ControlledPawn) return EBTNodeResult::Failed;

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(ControlledPawn);
    ABossCharacter* Boss = Cast<ABossCharacter>(ControlledPawn);
    const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    APlayerCharacter* Player = Blackboard ? Cast<APlayerCharacter>(Blackboard->GetValueAsObject(TEXT("Target"))) : nullptr;
    if (!IsValid(Player) || Player->bIsDead) return EBTNodeResult::Failed;

    if ((!Enemy && !Boss) || (Enemy && Enemy->bIsDead) || (Boss && Boss->bIsDead)) return EBTNodeResult::Failed;
    ACharacter* Character = CastChecked<ACharacter>(ControlledPawn);
    UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
    const auto& Montages = Enemy ? Enemy->AttackMontages : Boss->AttackMontages;
    const int32 Step = Enemy ? Enemy->CurrentAttackStep : Boss->CurrentAttackStep;
    if (!Anim || !Montages.IsValidIndex(Step) || !IsValid(Montages[Step]) || !Montages[Step]->GetSkeleton())
        return EBTNodeResult::Failed;

    ActiveMontage = Montages[Step];
    if (!Anim->Montage_IsActive(ActiveMontage.Get()))
    {
        const FVector Direction = (Player->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
        if (!Direction.IsNearlyZero()) Character->SetActorRotation(Direction.Rotation());
        if (Enemy) Enemy->PlayAttackMontageByIndex(Step);
        else Boss->PlayAttackMontageByIndex(Step);
    }
    return Anim->Montage_IsActive(ActiveMontage.Get()) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_PlayAttackMontage::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    const auto* AI = OwnerComp.GetAIOwner();
    const ACharacter* Character = AI ? Cast<ACharacter>(AI->GetPawn()) : nullptr;
    const UAnimInstance* Anim = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
    if (!Anim || !ActiveMontage.IsValid() || !Anim->Montage_IsActive(ActiveMontage.Get()))
        FinishLatentTask(OwnerComp, Anim ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}