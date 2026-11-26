#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"

// PNGs are staged as NonUFS files. Cache textures and brushes for the owning UI lifetime.
class FInputPromptIcons
{
public:
    const FSlateBrush* Get(const FString& Name);
private:
    TMap<FString, TStrongObjectPtr<UTexture2D>> Textures;
    TMap<FString, TSharedPtr<FSlateBrush>> Brushes;
};
