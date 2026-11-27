#include "DeathRagdoll.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"

void StartDeathRagdoll(ACharacter& Character)
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
