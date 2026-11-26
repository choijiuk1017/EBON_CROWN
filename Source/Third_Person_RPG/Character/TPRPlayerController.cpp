// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/Character/TPRPlayerController.h"

#include "Camera/PlayerCameraManager.h"
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


}

void ATPRPlayerController::BeginPlay()
{
	Super::BeginPlay();

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
	CachedCharacter = Cast<APlayerCharacter>(InPawn);
}


void ATPRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = CastChecked<UEnhancedInputComponent>(InputComponent);
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
	if (CanUsePadGameplay() && !IsLookInputIgnored())
		AddYawInput(PadAxis(Value) * 120.f * GetWorld()->GetDeltaSeconds());
}
void ATPRPlayerController::PadLookPitch(float Value)
{
	if (CanUsePadGameplay() && !IsLookInputIgnored())
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
	Super::FlushPressedKeys();
}
