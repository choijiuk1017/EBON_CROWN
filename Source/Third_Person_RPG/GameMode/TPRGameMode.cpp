// Fill out your copyright notice in the Description page of Project Settings.


#include "Third_Person_RPG/GameMode/TPRGameMode.h"

#include "Third_Person_RPG/Instance/TPRGameInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Engine/PostProcessVolume.h"



ATPRGameMode::ATPRGameMode()
{
	//DefaultPawn 설정
	//_C는 해당 블루프린트 클래스의 인스턴스를 구별하기 위한 언리얼의 네이밍 컨벤션
	//_C는 해당 블루프린트 클래스의 인스턴스를 가리킴
	//블루프린트 클래스를 경로로 찾아올 때는 "_C"를 붙여줘야 함
	static ConstructorHelpers::FClassFinder<APawn> PawnClassRef(TEXT("/Script/Engine.Blueprint'/Game/BluePrint/BP_PlayerCharacter.BP_PlayerCharacter_C'"));
	if (PawnClassRef.Succeeded())
	{
		DefaultPawnClass = PawnClassRef.Class;
	}

	//PlayerControllerClass 설정
	static ConstructorHelpers::FClassFinder<APlayerController> ControllerClassRef(
		TEXT("/Game/BluePrint/Player_Controller.Player_Controller_C")
	);
	if (ControllerClassRef.Succeeded())
	{
		PlayerControllerClass = ControllerClassRef.Class;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load Player_Controller class"));
	}
}

void ATPRGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Dungeon2 uses baked virtual lightmaps. Apply the same GI to gameplay and cinematic cameras.
	if (UGameplayStatics::GetCurrentLevelName(this) == TEXT("Dungeon2"))
	{
		if (auto* Lighting = GetWorld()->SpawnActor<APostProcessVolume>())
		{
			Lighting->bUnbound = true;
			Lighting->Priority = 10000.f;
			Lighting->Settings.bOverride_DynamicGlobalIlluminationMethod = true;
			Lighting->Settings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
		}
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		// 뷰포트에 추가된 위젯을 추적하고 있다면 이걸 제거
		if (PC->GetHUD())
		{
			// 방법 1: 로딩 위젯을 GameInstance 또는 PlayerController에서 직접 추적한 뒤 제거
			if (UTPRGameInstance* GI = Cast<UTPRGameInstance>(UGameplayStatics::GetGameInstance(GetWorld())))
			{
				if (GI->LoadingScreenWidget)
				{
					GI->LoadingScreenWidget->RemoveFromParent();
					GI->LoadingScreenWidget = nullptr;
				}
			}
		}

		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->bShowMouseCursor = false;  
	}



}