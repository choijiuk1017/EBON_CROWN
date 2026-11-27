// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"

#include "TPRPlayerController.generated.h"


class UInputMappingContext;
class UInputAction;
class APlayerCharacter;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
/**
 * 
 */
UCLASS()
class THIRD_PERSON_RPG_API ATPRPlayerController : public APlayerController
{
	GENERATED_BODY()

	ATPRPlayerController();


public:
	void PulseRumble(float Intensity, float Duration);
	void PlayCombatHitShake(bool bPlayerWasHit);
	virtual void PlayerTick(float DeltaTime) override;
	virtual void FlushPressedKeys() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Force Feedback")
	bool bCombatForceFeedbackEnabled = true;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Force Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackHitRumbleIntensity = 0.70f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Force Feedback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReceivedHitRumbleIntensity = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Force Feedback", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float AttackHitRumbleDuration = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Force Feedback", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float ReceivedHitRumbleDuration = 0.14f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Camera Shake", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float AttackHitShakeScale = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Camera Shake", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float ReceivedHitShakeScale = 1.0f;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> IMC_Basic;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_BasicMove;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_BasicLook;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Roll;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Attack;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Skill;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_UnEquipWeapon_Test;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Interaction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Inventory;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_DrinkPotion;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_ChangePotion;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_Enter;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> IA_ESC;

private:
	FDynamicForceFeedbackHandle CombatRumbleHandle = 0;
	void StopCombatRumble();
	TWeakObjectPtr<ACharacter> LockTarget;
	float OcclusionTime = 0.f;
	void ToggleLockOn();
	void TrackLockTarget(float DeltaTime);
	void DrawLockMarker(class AHUD* HUD, class UCanvas* Canvas);
	bool bPadBDown = false;
	bool bPadSprinting = false;
	double PadBStart = 0.0;
	bool CanUsePadGameplay() const;
	void PadBPressed();
	void PadBReleased();
	void PadInteract();
	void PadMoveForward(float Value);
	void PadMoveRight(float Value);
	void PadLookYaw(float Value);
	void PadLookPitch(float Value);
	double LastCombatShakeTime = -1.0;
	double LastReceivedShakeTime = -1.0;
	UPROPERTY()
	APlayerCharacter* CachedCharacter;

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Sprint_Start();
	void Input_Sprint_End();
	void Input_Attack();
	void Input_Roll();
	void Input_Skill();
	void Input_Interaction();
	void Input_Inventory();
	void Input_DrinkPotion();
	void Input_ChangePotion();

	void Input_Enter();

	void Input_ESC();
};
