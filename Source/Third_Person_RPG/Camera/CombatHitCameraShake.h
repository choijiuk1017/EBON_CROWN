#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "CombatHitCameraShake.generated.h"

UCLASS()
class THIRD_PERSON_RPG_API UCombatHitShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()
protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override { return Elapsed >= Duration; }
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override { Elapsed = Duration; }
private:
	float Elapsed = 0.f;
	float Duration = 0.14f;
};

UCLASS()
class THIRD_PERSON_RPG_API UCombatHitCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UCombatHitCameraShake(const FObjectInitializer& ObjectInitializer);
};
