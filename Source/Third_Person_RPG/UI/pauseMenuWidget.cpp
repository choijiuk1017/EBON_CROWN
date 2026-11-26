#include "Third_Person_RPG/UI/pauseMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

void UpauseMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	if (Button_ReturnToTitle)
	{
		Button_ReturnToTitle->OnClicked.AddUniqueDynamic(this, &UpauseMenuWidget::OnReturnToTitleClicked);
	}

	if (Button_QuitGame)
	{
		Button_QuitGame->OnClicked.AddUniqueDynamic(this, &UpauseMenuWidget::OnQuitGameClicked);
	}
}

void UpauseMenuWidget::Open(APlayerController* InPC)
{
	if (!IsValid(InPC) || IsInViewport()) return;
	OwningPC = InPC;

	AddToViewport(9999);
	PadSelection = 0;
	if (Button_ReturnToTitle) Button_ReturnToTitle->SetBackgroundColor(FLinearColor::Yellow);
	if (Button_QuitGame) Button_QuitGame->SetBackgroundColor(FLinearColor::White);

	if (OwningPC)
	{
		OwningPC->SetPause(true);
		OwningPC->bShowMouseCursor = true;

		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		OwningPC->SetInputMode(Mode);
	}

	// ★ 포커스 줘야 ESC가 위젯으로 들어옵니다
	SetKeyboardFocus();
}

void UpauseMenuWidget::Close()
{
	if (!IsInViewport()) return;
	RemoveFromParent();

	if (OwningPC)
	{
		OwningPC->SetPause(false);
		OwningPC->bShowMouseCursor = false;

		FInputModeGameOnly Mode;
		OwningPC->SetInputMode(Mode);
	}
}

FReply UpauseMenuWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // Handle navigation before a focused child button consumes it.
    const FReply Reply = HandleMenuKey(Event.GetKey(), Event.IsRepeat());
    return Reply.IsEventHandled() ? Reply : Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UpauseMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    const FReply Reply = HandleMenuKey(Event.GetKey(), Event.IsRepeat());
    return Reply.IsEventHandled() ? Reply : Super::NativeOnKeyDown(Geometry, Event);
}

FReply UpauseMenuWidget::HandleMenuKey(FKey Key, bool bRepeat)
{
    if (!IsInViewport()) return FReply::Unhandled();
    const bool bPrevious = Key == EKeys::Up || Key == EKeys::Left ||
        Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Left;
    const bool bNext = Key == EKeys::Down || Key == EKeys::Right ||
        Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_DPad_Right;
    if (bPrevious || bNext)
    {
        if (!bRepeat) PadSelection = 1 - PadSelection;
        if (Button_ReturnToTitle) Button_ReturnToTitle->SetBackgroundColor(PadSelection == 0 ? FLinearColor::Yellow : FLinearColor::White);
        if (Button_QuitGame) Button_QuitGame->SetBackgroundColor(PadSelection == 1 ? FLinearColor::Yellow : FLinearColor::White);
        return FReply::Handled();
    }
    if (Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (!bRepeat)
        {
            if (PadSelection == 0) OnReturnToTitleClicked(); else OnQuitGameClicked();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::Escape || Key == EKeys::X || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
    {
        if (!bRepeat) Close();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}

void UpauseMenuWidget::OnReturnToTitleClicked()
{
	if (OwningPC) OwningPC->SetPause(false);
	UGameplayStatics::OpenLevel(GetWorld(), TitleLevelName);
}

void UpauseMenuWidget::OnQuitGameClicked()
{
	if (!OwningPC)
	{
		OwningPC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	}

	UKismetSystemLibrary::QuitGame(GetWorld(), OwningPC, EQuitPreference::Quit, false);
}