// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Character/BT/BTTask_PlayBossPattern.h"
#include "AIController.h"
#include "Data/SkillData.h"
#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Third_Person_RPG/Character/BossCharacter.h"

UBTTask_PlayBossPattern::UBTTask_PlayBossPattern()
{
	NodeName = TEXT("Play Boss Pattern");
	bNotifyTick = true; 
}

EBTNodeResult::Type UBTTask_PlayBossPattern::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!AICon) return EBTNodeResult::Failed;

	ABossCharacter* Boss = Cast<ABossCharacter>(AICon->GetPawn());
	const UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	const auto* Player = Blackboard ? Cast<APlayerCharacter>(Blackboard->GetValueAsObject(TEXT("Target"))) : nullptr;
	if (!Boss || Boss->bIsDead || !IsValid(Player) || Player->bIsDead) return EBTNodeResult::Failed;

	if (!Boss->PatternDatas.IsValidIndex(PatternIndex) || !IsValid(Boss->PatternDatas[PatternIndex])) return EBTNodeResult::Failed;
	const auto* Montage = Boss->PatternDatas[PatternIndex]->SkillMontage.Get();
	const auto* Anim = Boss->GetMesh()->GetAnimInstance();
	if (!Montage || !Anim) return EBTNodeResult::Failed;
	Boss->PlayPatternMontage(PatternIndex);
	return Anim->Montage_IsActive(Montage) ? EBTNodeResult::InProgress : EBTNodeResult::Failed;
}

void UBTTask_PlayBossPattern::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	const auto* AI = OwnerComp.GetAIOwner();
	const auto* Boss = AI ? Cast<ABossCharacter>(AI->GetPawn()) : nullptr;
	if (!Boss || Boss->bIsDead || !Boss->PatternDatas.IsValidIndex(PatternIndex) || !IsValid(Boss->PatternDatas[PatternIndex]))
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}
	const auto* Anim = Boss->GetMesh()->GetAnimInstance();
	const auto* Montage = Boss->PatternDatas[PatternIndex]->SkillMontage.Get();
	if (!Anim || !Montage || !Anim->Montage_IsActive(Montage))
		FinishLatentTask(OwnerComp, Anim && Montage ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
}