#include "InputPromptIcons.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

const FSlateBrush* FInputPromptIcons::Get(const FString& Name)
{
    if (const TSharedPtr<FSlateBrush>* Found = Brushes.Find(Name)) return Found->Get();
    const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/InputPrompts"), Name + TEXT(".png"));
    UTexture2D* Texture = FImageUtils::ImportFileAsTexture2D(Path);
    if (!Texture)
    {
        UE_LOG(LogTemp, Warning, TEXT("Input prompt icon unavailable: %s"), *Name);
        Brushes.Add(Name, nullptr); // Do not retry or log every frame.
        return nullptr;
    }
    Textures.Add(Name, TStrongObjectPtr<UTexture2D>(Texture));
    TSharedPtr<FSlateBrush> Brush = MakeShared<FSlateBrush>();
    Brush->SetResourceObject(Texture);
    Brush->ImageSize = FVector2D(32.f, 32.f);
    Brush->DrawAs = ESlateBrushDrawType::Image;
    Brushes.Add(Name, Brush);
    return Brush.Get();
}
