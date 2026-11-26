#include "CombatBalance.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
    FString FindProfile(const UObject* Actor)
    {
        if (!Actor || !GConfig) return FString();
        for (const UClass* Class = Actor->GetClass(); Class; Class = Class->GetSuperClass())
        {
            const FString Section = TEXT("EbonCrown.Balance.") + Class->GetPathName();
            int32 HP = 0;
            if (GConfig->GetInt(*Section, TEXT("MaxHP"), HP, GGameIni) && HP > 0)
                return Section;
        }
        return FString();
    }
}

bool CombatBalance::Apply(const UObject* Actor, int32& MaxHP, int32& CurrentHP,
    int32& AttackPower, int32& Defense, int32& Reward)
{
    const FString Section = FindProfile(Actor);
    if (Section.IsEmpty()) return false;
    GConfig->GetInt(*Section, TEXT("MaxHP"), MaxHP, GGameIni);
    GConfig->GetInt(*Section, TEXT("AttackPower"), AttackPower, GGameIni);
    GConfig->GetInt(*Section, TEXT("Defense"), Defense, GGameIni);
    GConfig->GetInt(*Section, TEXT("CurrencyReward"), Reward, GGameIni);
    MaxHP = FMath::Max(1, MaxHP);
    CurrentHP = MaxHP;
    AttackPower = FMath::Max(0, AttackPower);
    Defense = FMath::Max(0, Defense);
    Reward = FMath::Max(0, Reward);
    return true;
}

int32 CombatBalance::PatternDamage(const UObject* Actor, float AuthoredDamage)
{
    if (AuthoredDamage <= 0.f) return 0;
    const FString Section = FindProfile(Actor);
    if (Section.IsEmpty()) return FMath::RoundToInt(AuthoredDamage);
    int32 Minimum = 0, Maximum = INT32_MAX;
    float Multiplier = 1.f;
    GConfig->GetInt(*Section, TEXT("PatternDamageMin"), Minimum, GGameIni);
    GConfig->GetInt(*Section, TEXT("PatternDamageMax"), Maximum, GGameIni);
    GConfig->GetFloat(*Section, TEXT("PatternDamageMultiplier"), Multiplier, GGameIni);
    return FMath::RoundToInt(FMath::Clamp(AuthoredDamage * Multiplier,
        static_cast<float>(FMath::Max(0, Minimum)), static_cast<float>(FMath::Max(Minimum, Maximum))));
}

int32 CombatBalance::LevelUpCost(int32 TargetLevel)
{
    // L1 -> L2: 300. Each subsequent level grows faster than the previous one.
    const double Steps = FMath::Max(0.0, static_cast<double>(TargetLevel) - 2.0);
    return static_cast<int32>(FMath::Min(300.0 + 100.0 * Steps + 15.0 * Steps * Steps,
        static_cast<double>(INT32_MAX)));
}
