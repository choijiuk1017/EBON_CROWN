#include "Third_Person_RPG/Camera/CombatHitCameraShake.h"

UCombatHitCameraShake::UCombatHitCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(CreateDefaultSubobject<UCombatHitShakePattern>(TEXT("HitPattern")));
}

void UCombatHitShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(0.14f);
}

void UCombatHitShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
	Duration = Params.bOverrideDuration ? FMath::Max(0.001f, Params.DurationOverride) : 0.14f;
}

void UCombatHitShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed = FMath::Min(Elapsed + Params.DeltaTime, Duration);
	const float Alpha = Elapsed / Duration;
	// Start and finish at zero, with a short damped impulse. No FOV change.
	const float Envelope = FMath::Sin(PI * Alpha) * (1.f - Alpha);
	const float Wave = FMath::Sin(Elapsed * 2.f * PI * 24.f) * Envelope;
	OutResult.Location = FVector(0.f, Wave * 0.7f, Wave * 1.1f);
	OutResult.Rotation = FRotator(Wave * 0.65f, Wave * 0.3f, 0.f);
}
