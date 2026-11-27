// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Character/TPRPlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Character/EnemyCharacter.h"
#include "Character/BossCharacter.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Third_Person_RPG/Camera/CombatHitCameraShake.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "Third_Person_RPG/UI/DialogueWidget.h"
#include "Third_Person_RPG/Character/PlayerCharacter.h"
#include "Third_Person_RPG/Component/PlayerCombatComponent.h"
#include "Kismet/GameplayStatics.h"

ATPRPlayerController::ATPRPlayerController()
{
	bShowMouseCursor = false;
	bForceFeedbackEnabled = true;


}

void ATPRPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (IsLocalController()) AHUD::OnHUDPostRender.AddUObject(this, &ATPRPlayerController::DrawLockMarker);

	CachedCharacter = Cast<APlayerCharacter>(GetPawn());

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (IMC_Basic)
		{
			Subsystem->AddMappingContext(IMC_Basic, 0);
		}
	}
}

void ATPRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	LockTarget.Reset();
	CachedCharacter = Cast<APlayerCharacter>(InPawn);
}


void ATPRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(InputComponent);
	InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &ATPRPlayerController::ToggleLockOn);
	InputComponent->BindKey(EKeys::Gamepad_RightThumbstick, IE_Pressed, this, &ATPRPlayerController::ToggleLockOn);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftY, this, &ATPRPlayerController::PadMoveForward);
	InputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &ATPRPlayerController::PadMoveRight);
	InputComponent->BindAxisKey(EKeys::Gamepad_RightX, this, &ATPRPlayerController::PadLookYaw);
	InputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &ATPRPlayerController::PadLookPitch);
	InputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &ATPRPlayerController::Input_Attack);
	InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &ATPRPlayerController::Input_Skill);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &ATPRPlayerController::PadInteract);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &ATPRPlayerController::Input_DrinkPotion);
	InputComponent->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &ATPRPlayerController::Input_ChangePotion);
	InputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &ATPRPlayerController::Input_Inventory);
	InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &ATPRPlayerController::Input_ESC);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &ATPRPlayerController::PadBPressed);
	InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Released, this, &ATPRPlayerController::PadBReleased);

	EIC->BindAction(IA_BasicMove, ETriggerEvent::Triggered, this, &ATPRPlayerController::Input_Move);
	EIC->BindAction(IA_BasicLook, ETriggerEvent::Triggered, this, &ATPRPlayerController::Input_Look);
	EIC->BindAction(IA_Sprint, ETriggerEvent::Triggered, this, &ATPRPlayerController::Input_Sprint_Start);
	EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ATPRPlayerController::Input_Sprint_End);
	EIC->BindAction(IA_Attack, ETriggerEvent::Started, this, &ATPRPlayerController::Input_Attack);
	EIC->BindAction(IA_Roll, ETriggerEvent::Triggered, this, &ATPRPlayerController::Input_Roll);
	EIC->BindAction(IA_DrinkPotion, ETriggerEvent::Started, this, &ATPRPlayerController::Input_DrinkPotion);
	EIC->BindAction(IA_ChangePotion, ETriggerEvent::Started, this, &ATPRPlayerController::Input_ChangePotion);
	EIC->BindAction(IA_Skill, ETriggerEvent::Started, this, &ATPRPlayerController::Input_Skill);
	EIC->BindAction(IA_Interaction, ETriggerEvent::Started, this, &ATPRPlayerController::Input_Interaction);
	EIC->BindAction(IA_Inventory, ETriggerEvent::Started, this, &ATPRPlayerController::Input_Inventory);
	EIC->BindAction(IA_Enter, ETriggerEvent::Started, this, &ATPRPlayerController::Input_Enter);
	EIC->BindAction(IA_ESC, ETriggerEvent::Started, this, &ATPRPlayerController::Input_ESC);
}


void ATPRPlayerController::Input_Move(const FInputActionValue& Value)
{
	if (!CachedCharacter || CachedCharacter->bIsDead) return;

	const FVector2D Movement = Value.Get<FVector2D>();
	const FRotator ControlRot = GetControlRotation();
	const FRotator YawRot(0, ControlRot.Yaw, 0);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	CachedCharacter->AddMovementInput(Forward, Movement.X);
	CachedCharacter->AddMovementInput(Right, Movement.Y);
}

void ATPRPlayerController::Input_Look(const FInputActionValue& Value)
{
	if (LockTarget.IsValid()) return;
	const FVector2D Look = Value.Get<FVector2D>();
	AddYawInput(Look.X);
	AddPitchInput(Look.Y);
}

void ATPRPlayerController::Input_Sprint_Start()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->BeginSprint();
}

void ATPRPlayerController::Input_Sprint_End()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->EndSprint();
}

void ATPRPlayerController::Input_Roll()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->RollStart();
}

void ATPRPlayerController::Input_Attack()
{
	if (!CachedCharacter || CachedCharacter->bIsDead) return;

	if (UPlayerCombatComponent* Combat = CachedCharacter->GetCombatComponent())
	{
		Combat->BasicAttack();
	}
}

void ATPRPlayerController::Input_Skill()
{
	if (!CachedCharacter || CachedCharacter->bIsDead) return;

	if (UPlayerCombatComponent* Combat = CachedCharacter->GetCombatComponent())
	{
		Combat->SkillStart();
	}
}


void ATPRPlayerController::Input_Interaction()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->Interact();
}

void ATPRPlayerController::Input_Inventory()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->ToggleInventory();
}

void ATPRPlayerController::Input_DrinkPotion()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->DrinkPotion();
}

void ATPRPlayerController::Input_ChangePotion()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->ChangePotion();
}

void ATPRPlayerController::Input_Enter()
{
	if (!CachedCharacter) return;

	if (!IA_Enter)
	{
		UE_LOG(LogTemp, Error, TEXT("[PC] IA_Enter is NULL"));
		return;
	}
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->OnAdvanceDialogue();
}

void ATPRPlayerController::Input_ESC()
{
	if (!CachedCharacter) return;
	if (CachedCharacter && !CachedCharacter->bIsDead) CachedCharacter->TogglePauseMenu();
}
void ATPRPlayerController::PlayCombatHitShake(bool bPlayerWasHit)
{
	if (!IsLocalController() || !PlayerCameraManager || !GetWorld()) return;
	const float Scale = bPlayerWasHit ? ReceivedHitShakeScale : AttackHitShakeScale;
	if (Scale <= 0.f) return;
	const double Now = GetWorld()->GetTimeSeconds();
	// Multiple enemies hit in one swing produce one impulse; receiving damage has priority.
	if (bPlayerWasHit)
	{
		if (Now - LastReceivedShakeTime < 0.06) return;
		LastReceivedShakeTime = Now;
	}
	else if (Now - LastCombatShakeTime < 0.06 || Now - LastReceivedShakeTime < 0.14)
	{
		return;
	}
	LastCombatShakeTime = Now;
	PlayerCameraManager->StartCameraShake(UCombatHitCameraShake::StaticClass(), Scale);
	// Share the camera shake's hit filtering, so one swing cannot stack rumble per target.
	PulseRumble(bPlayerWasHit ? ReceivedHitRumbleIntensity : AttackHitRumbleIntensity,
		bPlayerWasHit ? ReceivedHitRumbleDuration : AttackHitRumbleDuration);
}

void ATPRPlayerController::PulseRumble(float Intensity, float Duration)
{
	if (!IsLocalController() || !bCombatForceFeedbackEnabled || !bForceFeedbackEnabled || IsPaused()) return;
	StopCombatRumble();
	if (Intensity > 0.f && Duration > 0.f)
		CombatRumbleHandle = PlayDynamicForceFeedback(FMath::Clamp(Intensity, 0.f, 1.f), Duration, true, true, true, true);
}

namespace
{
	float PadAxis(float Value)
	{
		const float DeadZone = 0.2f;
		return FMath::Abs(Value) <= DeadZone ? 0.f :
			FMath::Sign(Value) * (FMath::Abs(Value) - DeadZone) / (1.f - DeadZone);
	}
}
bool ATPRPlayerController::CanUsePadGameplay() const
{
	return CachedCharacter && !CachedCharacter->bIsDead && !IsPaused() &&
		!IsMoveInputIgnored() && !bShowMouseCursor && !CachedCharacter->IsPadGameplayBlocked();
}
void ATPRPlayerController::PadMoveForward(float Value)
{
	if (CanUsePadGameplay()) Input_Move(FInputActionValue(FVector2D(PadAxis(Value), 0.f)));
}
void ATPRPlayerController::PadMoveRight(float Value)
{
	if (CanUsePadGameplay()) Input_Move(FInputActionValue(FVector2D(0.f, PadAxis(Value))));
}
void ATPRPlayerController::PadLookYaw(float Value)
{
	if (CanUsePadGameplay() && !LockTarget.IsValid() && !IsLookInputIgnored())
		AddYawInput(PadAxis(Value) * 120.f * GetWorld()->GetDeltaSeconds());
}
void ATPRPlayerController::PadLookPitch(float Value)
{
	if (CanUsePadGameplay() && !LockTarget.IsValid() && !IsLookInputIgnored())
		AddPitchInput(-PadAxis(Value) * 90.f * GetWorld()->GetDeltaSeconds());
}
void ATPRPlayerController::PadInteract()
{
	if (!CachedCharacter || CachedCharacter->bIsDead) return;
	if (CachedCharacter->IsDialogueOpen())
		CachedCharacter->OnAdvanceDialogue();
	else if (CanUsePadGameplay())
		Input_Interaction();
}
void ATPRPlayerController::PadBPressed()
{
	if (!CanUsePadGameplay()) return;
	bPadBDown = true;
	bPadSprinting = false;
	PadBStart = FPlatformTime::Seconds();
}
void ATPRPlayerController::PadBReleased()
{
	const bool bRoll = bPadBDown && !bPadSprinting && CanUsePadGameplay();
	if (bPadSprinting) Input_Sprint_End();
	bPadBDown = bPadSprinting = false;
	if (bRoll) Input_Roll();
}
void ATPRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (LockTarget.IsValid()) TrackLockTarget(DeltaTime);
	if (IsPaused() || bShowMouseCursor) StopCombatRumble();
	if (!bPadBDown) return;
	if (!CanUsePadGameplay())
	{
		if (bPadSprinting) Input_Sprint_End();
		bPadBDown = bPadSprinting = false;
		return;
	}
	if (!bPadSprinting && FPlatformTime::Seconds() - PadBStart >= 0.25)
	{
		bPadSprinting = true;
		Input_Sprint_Start();
	}
}
void ATPRPlayerController::FlushPressedKeys()
{
	if (bPadSprinting) Input_Sprint_End();
	bPadBDown = bPadSprinting = false;
	StopCombatRumble();
	Super::FlushPressedKeys();
}

void ATPRPlayerController::StopCombatRumble()
{
	if (CombatRumbleHandle != 0)
		CombatRumbleHandle = PlayDynamicForceFeedback(0.f, 0.f, true, true, true, true,
			EDynamicForceFeedbackAction::Stop, CombatRumbleHandle);
}

void ATPRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCombatRumble();
	AHUD::OnHUDPostRender.RemoveAll(this);
	Super::EndPlay(EndPlayReason);
}

namespace
{
	bool IsLivingEnemy(const ACharacter* Target)
	{
		if (!IsValid(Target) || Target->IsHidden()) return false;
		if (const auto* Enemy = Cast<AEnemyCharacter>(Target)) return !Enemy->bIsDead;
		if (const auto* Boss = Cast<ABossCharacter>(Target)) return !Boss->bIsDead;
		return false;
	}

	FVector LockPoint(const ACharacter* Target)
	{
		return Target->GetActorLocation() + FVector::UpVector * Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 0.45f;
	}
}

void ATPRPlayerController::ToggleLockOn()
{
	if (LockTarget.IsValid()) { LockTarget.Reset(); return; }
	if (!CanUsePadGameplay() || !PlayerCameraManager) return;
	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	float Best = -1.f;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		ACharacter* Candidate = *It;
		if (!IsLivingEnemy(Candidate)) continue;
		const float Range = FVector::Dist(GetPawn()->GetActorLocation(), Candidate->GetActorLocation());
		const float Facing = FVector::DotProduct(View.Vector(), (LockPoint(Candidate) - Eye).GetSafeNormal());
		if (Range > 1800.f || Facing < 0.55f) continue;
		const float Score = Facing - 0.3f * Range / 1800.f;
		if (Score > Best && LineOfSightTo(Candidate, Eye))
		{
			LockTarget = Candidate;
			Best = Score;
		}
	}
	OcclusionTime = 0.f;
}

void ATPRPlayerController::TrackLockTarget(float DeltaTime)
{
	ACharacter* Target = LockTarget.Get();
	if (!CanUsePadGameplay() || !IsLivingEnemy(Target) || GetViewTarget() != GetPawn() ||
		FVector::DistSquared(GetPawn()->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(2400.f))
	{
		LockTarget.Reset();
		return;
	}
	FVector Eye;
	FRotator View;
	GetPlayerViewPoint(Eye, View);
	OcclusionTime = LineOfSightTo(Target, Eye) ? 0.f : OcclusionTime + DeltaTime;
	if (OcclusionTime > 0.6f) { LockTarget.Reset(); return; }
	const FRotator Aim = (LockPoint(Target) - Eye).Rotation();
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), Aim, DeltaTime, 10.f));
}

void ATPRPlayerController::DrawLockMarker(AHUD* HUD, UCanvas* Canvas)
{
	if (!HUD || HUD != GetHUD() || !LockTarget.IsValid()) return;
	FVector2D Point;
	if (!ProjectWorldLocationToScreen(LockPoint(LockTarget.Get()), Point, true)) return;
	HUD->DrawRect(FLinearColor::Black, Point.X - 4, Point.Y - 4, 8, 8);
	HUD->DrawRect(FLinearColor::White, Point.X - 2, Point.Y - 2, 4, 4);
}
