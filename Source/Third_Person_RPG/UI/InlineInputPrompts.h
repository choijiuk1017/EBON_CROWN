#pragma once
#include "CoreMinimal.h"
#include "InputPromptIcons.h"

class UUserWidget;
class UTextBlock;
class UImage;
class USizeBox;

// Keeps the original text widget (and Blueprint bindings) inside an icon/text row.
class FInlineInputPrompts
{
public:
    void Update(UUserWidget* Owner, UTextBlock* Text, bool bGamepad);
    void Prune();
private:
    struct FState
    {
        FText Source;
        FText Label;
        FString Keyboard;
        FString Pad;
        TWeakObjectPtr<UImage> Image;
        TArray<TWeakObjectPtr<USizeBox>> ExtraKeyBoxes;
        bool bWasGamepad = false;
    };
    FInputPromptIcons Icons;
    TMap<TWeakObjectPtr<UTextBlock>, FState> States;
};
