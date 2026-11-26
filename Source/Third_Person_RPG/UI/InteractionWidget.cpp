#include "Third_Person_RPG/UI/InteractionWidget.h"
#include "Components/TextBlock.h"

void UInteractionWidget::SetHelpText(FString HelpText)
{
    // Older Blueprint instances may still override the C++ default with a keyboard hint.
    HelpText.TrimStartAndEndInline();
    for (const TCHAR* Prefix : { TEXT("'F'\ud0a4\ub97c \ub20c\ub7ec "), TEXT("'F' \ud0a4\ub97c \ub20c\ub7ec "), TEXT("F \ud0a4\ub97c \ub20c\ub7ec ") })
        if (HelpText.RemoveFromStart(Prefix, ESearchCase::IgnoreCase)) break;
    if (TXT_HelpText) TXT_HelpText->SetText(FText::FromString(HelpText));
}
