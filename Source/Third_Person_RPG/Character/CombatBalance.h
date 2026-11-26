#pragma once

#include "CoreMinimal.h"

// Runtime tuning is applied after Blueprint BeginPlay. Original assets remain editable.
namespace CombatBalance
{
    bool Apply(const UObject* Actor, int32& MaxHP, int32& CurrentHP,
        int32& AttackPower, int32& Defense, int32& Reward);
    int32 PatternDamage(const UObject* Actor, float AuthoredDamage);
    int32 LevelUpCost(int32 TargetLevel);
}
