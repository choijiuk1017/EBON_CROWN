#include "InlineInputPrompts.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/PanelSlot.h"
#include "UObject/UnrealType.h"
#include "UI/DialogueWidget.h"
#include "UI/Tutorial/TutorialWidget.h"
#include "UI/SavePointUI/LevelUpMenu.h"
#include "UI/SavePointUI/WorldTravelMenu.h"
#include "UI/SavePointUI/SavePointMenu.h"
#include "UI/WeaponUpgradeWidget.h"
#include "UI/InventoryUI/InventoryWidget.h"
#include "UI/TitleScreenWidget.h"
#include "UI/pauseMenuWidget.h"

namespace
{
    FString Normalize(FString Value)
    {
        Value.ReplaceInline(TEXT("\r"), TEXT(" "));
        Value.ReplaceInline(TEXT("\n"), TEXT(" "));
        Value.ReplaceInline(TEXT("\t"), TEXT(" "));
        while (Value.Contains(TEXT("  "))) Value.ReplaceInline(TEXT("  "), TEXT(" "));
        return Value.TrimStartAndEnd();
    }
    void CopyLayout(UPanelSlot* From, UPanelSlot* To)
    {
        for (TFieldIterator<FProperty> It(From->GetClass()); It; ++It)
            if (It->HasAnyPropertyFlags(CPF_Edit)) It->CopyCompleteValue_InContainer(To, From);
        To->SynchronizeProperties();
    }
}

void FInlineInputPrompts::Prune()
{
    for (auto It = States.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !It.Value().Image.IsValid()) It.RemoveCurrent();
}

void FInlineInputPrompts::Update(UUserWidget* Owner, UTextBlock* Text, bool bGamepad)
{
    if (!Owner || !Text || !Owner->WidgetTree) return;
    // Designer-authored icons retain their layout and follow the animated hint.
    if (UImage* Authored = Cast<UImage>(Owner->WidgetTree->FindWidget(
        FName(*(FString(TEXT("InputPrompt_")) + Text->GetName())))))
    {
        if (Owner->IsA<UTitleScreenWidget>())
        {
            const bool bConfirm = Text->GetFName() == TEXT("TextBlock_1");
            const bool bNavigate = Text->GetFName() == TEXT("TextBlock");
            if (bConfirm || bNavigate)
            {
                const TCHAR* Key = bConfirm
                    ? (bGamepad ? TEXT("xbox_button_color_a") : TEXT("keyboard_enter"))
                    : (bGamepad ? TEXT("xbox_dpad") : TEXT("keyboard_arrows"));
                if (const FSlateBrush* Brush = Icons.Get(Key)) Authored->SetBrush(*Brush);
                Text->SetText(FText::FromString(bConfirm ? TEXT("\uD655\uC778") : TEXT("\uBCC0\uACBD")));
                Authored->SetVisibility(Text->GetVisibility());
                FLinearColor Color = FLinearColor::White;
                Color.A = Text->GetColorAndOpacity().GetSpecifiedColor().A;
                Authored->SetColorAndOpacity(Color);
                Authored->SetRenderOpacity(Text->GetRenderOpacity());
                return;
            }
        }
    }
    if (FState* State = States.Find(Text))
    {
        if (!State->Image.IsValid()) return;
        const bool bKnown = Text->GetText().EqualTo(State->Source) || Text->GetText().EqualTo(State->Label);
        State->Image->SetVisibility(bKnown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        for (const TWeakObjectPtr<USizeBox>& Box : State->ExtraKeyBoxes)
            if (Box.IsValid()) Box->SetVisibility(bKnown && !bGamepad ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        if (!bKnown) return; // A Blueprint has replaced this hint with other content.
        if (!Text->GetText().EqualTo(State->Label)) Text->SetText(State->Label);
        if (State->bWasGamepad != bGamepad)
        {
            if (const FSlateBrush* Brush = Icons.Get(bGamepad ? State->Pad : State->Keyboard))
                State->Image->SetBrush(*Brush);
            State->bWasGamepad = bGamepad;
        }
        return;
    }
    // Never turn spoken dialogue or numeric/stat labels into input prompts.
    if (Owner->IsA<UDialogueWidget>() && Text->GetFName() == TEXT("DialogueText")) return;
    const FString Source = Normalize(Text->GetText().ToString());
    struct FRule { const TCHAR* Source; const TCHAR* Keyboard; const TCHAR* Pad; const TCHAR* Label; };
    static const FRule Rules[] = {
        { TEXT("'E'\ud0a4\ub97c \ub20c\ub7ec \uc778\ubca4\ud1a0\ub9ac \ub2eb\uae30"), TEXT("keyboard_e"), TEXT("xbox_button_color_b"), TEXT("\ub2eb\uae30") },
        { TEXT("'X' \ud0a4\ub97c \ub20c\ub7ec \ub098\uac00\uae30"), TEXT("keyboard_x"), TEXT("xbox_button_color_b"), TEXT("\ub2eb\uae30") },
        { TEXT("'Enter'\ub85c \uacb0\uc815"), TEXT("keyboard_enter"), TEXT("xbox_button_color_a"), TEXT("\ud655\uc778") },
        { TEXT("\uac15\ud654\ud558\ub824\uba74 \ubb34\uae30\ub97c \uc120\ud0dd \ud6c4 'ENTER' \ud0a4\ub97c \ub204\ub974\uc2dc\uc624"), TEXT("keyboard_enter"), TEXT("xbox_button_color_a"), TEXT("\uac15\ud654") },
        { TEXT("\ubb34\uae30 \uc120\ud0dd: \ubc29\ud5a5\ud0a4 \ubc0f 'W', 'S' \ub610\ub294 \ub9c8\uc6b0\uc2a4 \ud074\ub9ad"), TEXT("keyboard_arrows"), TEXT("xbox_dpad"), TEXT("\uc120\ud0dd") },
        { TEXT("\ubc29\ud5a5\ud0a4\ub85c \uc120\ud0dd\uc9c0 \ubcc0\uacbd \ubc0f \uc218\uce58 \ubcc0\uacbd"), TEXT("keyboard_arrows"), TEXT("xbox_dpad"), TEXT("\ubcc0\uacbd") },
        { TEXT("\ubc29\ud5a5\ud0a4\ub85c \uc120\ud0dd\uc9c0 \ubcc0\uacbd"), TEXT("keyboard_arrows"), TEXT("xbox_dpad"), TEXT("\ubcc0\uacbd") },
        { TEXT("\ubc29\ud5a5\ud0a4\ub85c \uba54\ub274 \ubcc0\uacbd"), TEXT("keyboard_arrows"), TEXT("xbox_dpad"), TEXT("\ubcc0\uacbd") },
        { TEXT("\ubc29\ud5a5\ud0a4\ub85c \uba54\ub274 \ubcc0\uacbd \uac00\ub2a5"), TEXT("keyboard_arrows"), TEXT("xbox_dpad"), TEXT("\ubcc0\uacbd") },
        { TEXT("Enter\ub85c \ud655\uc815"), TEXT("keyboard_enter"), TEXT("xbox_button_color_a"), TEXT("\ud655\uc778") },
        { TEXT("\uc5d4\ud130\ub85c \ud655\uc815"), TEXT("keyboard_enter"), TEXT("xbox_button_color_a"), TEXT("\ud655\uc778") },
        { TEXT("WASD\ub97c \uc774\uc6a9\ud574 \uce90\ub9ad\ud130\ub97c \uc774\ub3d9"), TEXT("keyboard_w"), TEXT("xbox_stick_l"), TEXT("\uc774\ub3d9") },
        { TEXT("\ub9c8\uc6b0\uc2a4\ub97c \uc774\uc6a9\ud574 \uce74\uba54\ub77c \ud68c\uc804"), TEXT("mouse"), TEXT("xbox_stick_r"), TEXT("\uc2dc\uc810 \uc870\uc808") },
        { TEXT("X \ud0a4\ub97c \ub20c\ub7ec \ud3ec\uc158 \uc0ac\uc6a9"), TEXT("keyboard_x"), TEXT("xbox_button_color_x"), TEXT("\uc0ac\uc6a9") },
        { TEXT("Z \ud0a4\ub97c \ub20c\ub7ec \ud3ec\uc158 \ubcc0\uacbd"), TEXT("keyboard_z"), TEXT("xbox_dpad_down"), TEXT("\ubcc0\uacbd") },
    };
    FString Keyboard, Pad, Label;
    for (const FRule& Rule : Rules)
        if (Source == Rule.Source) { Keyboard = Rule.Keyboard; Pad = Rule.Pad; Label = Rule.Label; break; }
    // Authored widgets may already contain the shortened labels, rather than the old instructions.
    const bool bMenu = Owner->IsA<ULevelUpMenu>() || Owner->IsA<UWorldTravelMenu>() ||
        Owner->IsA<USavePointMenu>() || Owner->IsA<UWeaponUpgradeWidget>() ||
        Owner->IsA<UInventoryWidget>() || Owner->IsA<UTitleScreenWidget>() || Owner->IsA<UpauseMenuWidget>();
    if (Keyboard.IsEmpty() && Owner->IsA<UWeaponUpgradeWidget>() && Source == TEXT("\uac15\ud654"))
    {
        Keyboard = TEXT("keyboard_enter"); Pad = TEXT("xbox_button_color_a"); Label = Source;
    }
    if (Keyboard.IsEmpty() && bMenu)
    {
        if (Source == TEXT("\ubcc0\uacbd") || Source == TEXT("\uc120\ud0dd") || Source == TEXT("\uc218\uce58 \ubcc0\uacbd"))
        {
            Keyboard = TEXT("keyboard_arrows"); Pad = TEXT("xbox_dpad"); Label = TEXT("\ubcc0\uacbd");
        }
        else if (Source == TEXT("\ub098\uac00\uae30") || Source == TEXT("\ub2eb\uae30"))
        {
            Keyboard = Owner->IsA<UInventoryWidget>() ? TEXT("keyboard_e") : Owner->IsA<UpauseMenuWidget>() ? TEXT("keyboard_escape") : TEXT("keyboard_x");
            Pad = TEXT("xbox_button_color_b"); Label = TEXT("\ub2eb\uae30");
        }
    }
    if (Keyboard.IsEmpty() && !Owner->IsA<UTutorialWidget>() && (Source == TEXT("\ud655\uc778") || Source == TEXT("\uacb0\uc815")))
    {
        Keyboard = TEXT("keyboard_enter"); Pad = TEXT("xbox_button_color_a"); Label = TEXT("\ud655\uc778");
        if (Owner->IsA<UDialogueWidget>()) { Keyboard = TEXT("keyboard_f"); Pad = TEXT("xbox_button_color_y"); }
    }
    if (Keyboard.IsEmpty()) return;
    UPanelWidget* Parent = Text->GetParent();
    const FSlateBrush* Brush = Icons.Get(bGamepad ? Pad : Keyboard);
    if (!Parent || !Brush) return;

    // Rebuild only this panel's slots in the same order, preserving their authored layout.
    // Keep Slate references alive so sibling user widgets do not destruct during reparenting.
    TArray<UWidget*> Children;
    TArray<TStrongObjectPtr<UPanelSlot>> Layouts;
    TArray<TSharedRef<SWidget>> SlateChildren;
    for (int32 I = 0; I < Parent->GetChildrenCount(); ++I)
    {
        UWidget* Child = Parent->GetChildAt(I);
        Children.Add(Child);
        SlateChildren.Add(Child->TakeWidget());
        UPanelSlot* Layout = NewObject<UPanelSlot>(GetTransientPackage(), Child->Slot->GetClass());
        CopyLayout(Child->Slot, Layout);
        Layouts.Emplace(Layout);
    }
    UHorizontalBox* Row = Owner->WidgetTree->ConstructWidget<UHorizontalBox>();
    UImage* Image = Owner->WidgetTree->ConstructWidget<UImage>();
    Image->SetBrush(*Brush);
    USizeBox* IconBox = Owner->WidgetTree->ConstructWidget<USizeBox>();
    const float Size = FMath::Clamp(float(Text->GetFont().Size) * 1.65f, 40.f, 160.f);
    IconBox->SetWidthOverride(Size); IconBox->SetHeightOverride(Size); IconBox->AddChild(Image);
    UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconBox);
    IconSlot->SetVerticalAlignment(VAlign_Center); IconSlot->SetPadding(FMargin(0.f, 0.f, Size * 0.2f, 0.f));
    TArray<TWeakObjectPtr<USizeBox>> ExtraKeyBoxes;
    if (Keyboard == TEXT("keyboard_w"))
    {
        for (const TCHAR* Key : { TEXT("keyboard_a"), TEXT("keyboard_s"), TEXT("keyboard_d") })
        {
            const FSlateBrush* ExtraBrush = Icons.Get(Key);
            if (!ExtraBrush) continue;
            UImage* Extra = Owner->WidgetTree->ConstructWidget<UImage>();
            Extra->SetBrush(*ExtraBrush);
            USizeBox* Box = Owner->WidgetTree->ConstructWidget<USizeBox>();
            Box->SetWidthOverride(Size); Box->SetHeightOverride(Size); Box->AddChild(Extra);
            Box->SetVisibility(bGamepad ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
            UHorizontalBoxSlot* ExtraSlot = Row->AddChildToHorizontalBox(Box);
            ExtraSlot->SetVerticalAlignment(VAlign_Center); ExtraSlot->SetPadding(FMargin(0.f, 0.f, Size * 0.2f, 0.f));
            ExtraKeyBoxes.Add(Box);
        }
    }
    Parent->ClearChildren();
    Row->AddChildToHorizontalBox(Text)->SetVerticalAlignment(VAlign_Center);
    for (int32 I = 0; I < Children.Num(); ++I)
    {
        UPanelSlot* Slot = Parent->AddChild(Children[I] == Text ? Row : Children[I]);
        if (Slot) CopyLayout(Layouts[I].Get(), Slot);
    }
    FState State;
    State.ExtraKeyBoxes = MoveTemp(ExtraKeyBoxes);
    State.Source = Text->GetText(); State.Label = FText::FromString(Label);
    State.Keyboard = Keyboard; State.Pad = Pad; State.Image = Image; State.bWasGamepad = bGamepad;
    Text->SetText(State.Label);
    States.Add(Text, MoveTemp(State));
    UE_LOG(LogTemp, Log, TEXT("[InputPrompt] %s.%s: %s -> %s (icon %.0f)"),
        *Owner->GetClass()->GetName(), *Text->GetName(), *Source, *Label, Size);
}
