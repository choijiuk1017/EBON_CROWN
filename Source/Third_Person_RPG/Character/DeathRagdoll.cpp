#include "DeathRagdoll.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimStateMachineTypes.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

static void StartDeathRagdoll(ACharacter& Character)
{
    UCharacterMovementComponent* Movement = Character.GetCharacterMovement();
    Movement->StopMovementImmediately();
    Movement->DisableMovement();

    USkeletalMeshComponent* Body = Character.GetMesh();
    TInlineComponentArray<UPrimitiveComponent*> Components(&Character);
    for (UPrimitiveComponent* Part : Components)
    {
        if (Part != Body) Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (!Body) return;
    const UPhysicsAsset* Physics = Body->GetPhysicsAsset();
    if (!Physics || Physics->SkeletalBodySetups.IsEmpty())
    {
        Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        UE_LOG(LogTemp, Warning, TEXT("Death ragdoll has no physics bodies: %s"), *Character.GetName());
        return;
    }

    Body->SetCollisionProfileName(TEXT("Ragdoll"));
    Body->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    Body->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
    Body->SetEnableGravity(true);
    Body->SetSimulatePhysics(true);
    Body->SetAllBodiesSimulatePhysics(true);
    Body->SetAllBodiesPhysicsBlendWeight(1.f);
    Body->WakeAllRigidBodies();
}

void PlayDeathThenRagdoll(ACharacter& Character)
{
    // Keep floor collision for the death animation, but release the player's path immediately.
    TInlineComponentArray<UPrimitiveComponent*> Parts(&Character);
    for (UPrimitiveComponent* Part : Parts)
    {
        Part->SetCollisionResponseToChannel(ECC_GameTraceChannel3, ECR_Ignore); // Player
        Part->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    }

    USkeletalMeshComponent* Mesh = Character.GetMesh();
    UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
    const IAnimClassInterface* Class = Anim ? IAnimClassInterface::GetFromClass(Anim->GetClass()) : nullptr;
    static const FName Dead(TEXT("Dead"));
    static const FName FlyDead(TEXT("FlyDead"));
    int32 MachineIndex = INDEX_NONE;
    if (Class)
    {
        for (const FBakedAnimationStateMachine& Machine : Class->GetBakedStateMachines())
        {
            if (Machine.States.ContainsByPredicate([](const FBakedAnimationState& State) { return State.StateName == Dead; }))
            {
                MachineIndex = Anim->GetStateMachineIndex(Machine.MachineName);
                break;
            }
        }
    }
    if (MachineIndex == INDEX_NONE)
    {
        StartDeathRagdoll(Character);
        return;
    }
    const auto PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
    Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    auto Handle = MakeShared<FTimerHandle>();
    const double Start = Character.GetWorld()->GetTimeSeconds();
    Character.GetWorldTimerManager().SetTimer(*Handle, FTimerDelegate::CreateWeakLambda(&Character,
        [&Character, MachineIndex, Handle, Start, PreviousTickOption, LastPosition = -1.f, LastState = FName()]() mutable
        {
            UAnimInstance* Instance = Character.GetMesh()->GetAnimInstance();
            const auto* Machine = Instance ? Instance->GetStateMachineInstance(MachineIndex) : nullptr;
            if (Machine)
            {
                const FName State = Instance->GetCurrentStateName(MachineIndex);
                if (State == Dead || State == FlyDead)
                {
                    const int32 Index = Machine->GetCurrentState();
                    const float Length = Instance->GetRelevantAnimLength(MachineIndex, Index);
                    if (Length > KINDA_SMALL_NUMBER)
                    {
                        const float Position = Instance->GetRelevantAnimTime(MachineIndex, Index);
                        const bool bWrapped = State == LastState && Position < LastPosition;
                        LastState = State;
                        LastPosition = Position;
                        if (!bWrapped && Position + KINDA_SMALL_NUMBER < Length) return;
                    }
                    else if (Character.GetWorld()->GetTimeSeconds() - Start < 2.) return;
                }
                else if (Character.GetWorld()->GetTimeSeconds() - Start < 2.) return;
            }
            Character.GetMesh()->VisibilityBasedAnimTickOption = PreviousTickOption;
            Character.GetWorldTimerManager().ClearTimer(*Handle);
            StartDeathRagdoll(Character);
        }), 1.f / 60.f, true);
}