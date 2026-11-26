#include "GamepadSupportSubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "Styling/CoreStyle.h"
#include "UI/pauseMenuWidget.h"
#include "UI/TitleScreenWidget.h"
#include "UI/Tutorial/TutorialWidget.h"
#include "UI/DialogueChoiceWidget.h"
#include "UI/WeaponUpgradeWidget.h"
#include "UI/SavePointUI/LevelUpMenu.h"
#include "UI/SavePointUI/WorldTravelMenu.h"
#include "UI/SavePointUI/SavePointMenu.h"
#include "UI/InventoryUI/InventoryWidget.h"
#include "InputCoreTypes.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "UI/InteractionWidget.h"
#include "UI/DialogueWidget.h"
#include "UI/InputPromptIcons.h"
#include "UI/InlineInputPrompts.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"

class FGamepadUIProcessor final : public IInputProcessor
{
public:
    explicit FGamepadUIProcessor(UWorld* InWorld) : World(InWorld) {}
    bool bUsingGamepad = false;
    UUserWidget* FindMenu() const
    {
        if (!World.IsValid()) return nullptr;
        UGameViewportClient* Viewport = World->GetGameViewport();
        if (!Viewport || !Viewport->GetGameViewportWidget().IsValid() || !FSlateApplication::IsInitialized()) return nullptr;
        const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Viewport->GetGameViewportWidget().ToSharedRef());
        if (!Window.IsValid() || !Window->IsActive()) return nullptr;
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UUserWidget::StaticClass(), true);
        const UClass* Classes[] = { UpauseMenuWidget::StaticClass(), UTutorialWidget::StaticClass(),
            UDialogueChoiceWidget::StaticClass(), UWeaponUpgradeWidget::StaticClass(), ULevelUpMenu::StaticClass(),
            UWorldTravelMenu::StaticClass(), USavePointMenu::StaticClass(), UInventoryWidget::StaticClass(),
            UTitleScreenWidget::StaticClass() };
        for (const UClass* Class : Classes)
            for (UUserWidget* Widget : Widgets)
                if (IsValid(Widget) && Widget->IsInViewport() && Widget->IsVisible() && Widget->IsA(Class)) return Widget;
        return nullptr;
    }
    void Send(UUserWidget* Menu, FKey Key)
    {
        if (UpauseMenuWidget* Pause = Cast<UpauseMenuWidget>(Menu))
        {
            Pause->HandleMenuKey(Key);
            return;
        }
        // Route to the menu itself: a focused child button must not steal custom navigation.
        Menu->TakeWidget()->OnKeyDown(Menu->GetCachedGeometry(),
            FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
    }
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        if (!Event.GetKey().IsGamepadKey()) { bUsingGamepad = false; return false; }
        bUsingGamepad = true;
        UUserWidget* Menu = FindMenu();
        if (!Menu) return false;
        const FKey Key = Event.GetKey();
        Consumed.Add(Key);
        if (UpauseMenuWidget* Pause = Cast<UpauseMenuWidget>(Menu))
        {
            Pause->HandleMenuKey(Key, Event.IsRepeat());
            return true;
        }
        FKey Mapped;
        if (Key == EKeys::Gamepad_DPad_Up) Mapped = EKeys::Up;
        if (Key == EKeys::Gamepad_DPad_Down) Mapped = EKeys::Down;
        if (Key == EKeys::Gamepad_DPad_Left) Mapped = EKeys::Left;
        if (Key == EKeys::Gamepad_DPad_Right) Mapped = EKeys::Right;
        if (Event.IsRepeat() && !Mapped.IsValid()) return true;
        if (Key == EKeys::Gamepad_FaceButton_Bottom) Mapped = EKeys::Enter;
        if (Key == EKeys::Gamepad_FaceButton_Right) Mapped = EKeys::X;
        if (Key == EKeys::Gamepad_Special_Right && Menu->IsA<UpauseMenuWidget>()) Mapped = EKeys::X;
        if (Menu->IsA<UInventoryWidget>())
        {
            if (Key == EKeys::Gamepad_Special_Left) Mapped = EKeys::X;
            if (Key == EKeys::Gamepad_LeftShoulder) Mapped = EKeys::PageUp;
            if (Key == EKeys::Gamepad_RightShoulder) Mapped = EKeys::PageDown;
            if (Key == EKeys::Gamepad_FaceButton_Top) Mapped = EKeys::Delete;
        }
        if (Mapped.IsValid()) Send(Menu, Mapped);
        return true;
    }
    virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        return Consumed.Remove(Event.GetKey()) > 0;
    }
    virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& Event) override
    {
        if (!Event.GetKey().IsGamepadKey()) return false;
        if (FMath::Abs(Event.GetAnalogValue()) > 0.25f) bUsingGamepad = true;
        if (!FindMenu()) { Axis = FVector2D::ZeroVector; return false; }
        if (Event.GetKey() == EKeys::Gamepad_LeftX) Axis.X = Event.GetAnalogValue();
        if (Event.GetKey() == EKeys::Gamepad_LeftY) Axis.Y = Event.GetAnalogValue();
        return true;
    }
    virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& Event) override
    {
        if (!Event.GetCursorDelta().IsNearlyZero()) bUsingGamepad = false;
        return false;
    }
    virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override
    {
        RefreshPrompts();
        RefreshIconHint();
        UUserWidget* Menu = FindMenu();
        if (!Menu) { Axis = FVector2D::ZeroVector; HeldDirection = FKey(); return; }
        FKey Direction;
        if (FMath::Abs(Axis.Y) > 0.55f && FMath::Abs(Axis.Y) >= FMath::Abs(Axis.X))
            Direction = Axis.Y > 0 ? EKeys::Up : EKeys::Down;
        else if (FMath::Abs(Axis.X) > 0.55f) Direction = Axis.X > 0 ? EKeys::Right : EKeys::Left;
        const double Now = FPlatformTime::Seconds();
        if (Direction.IsValid() && (Direction != HeldDirection || Now >= RepeatAt))
        {
            RepeatAt = Now + (Direction != HeldDirection ? 0.35 : 0.13);
            Send(Menu, Direction);
        }
        HeldDirection = Direction;
    }
private:

    FInlineInputPrompts InlinePrompts;
    struct FKeyImageState { FString OriginalName; TWeakObjectPtr<UObject> AppliedResource; FString AppliedIcon; };
    TMap<TWeakObjectPtr<UWidget>, FKeyImageState> KeyImages;
    FInputPromptIcons Icons;
    TSharedPtr<SHorizontalBox> IconHint;
    FString HintContext;
    bool bHintWasGamepad = false;
    void UpdateKeyImage(UWidget* Image, const FSlateBrush& CurrentBrush, const UUserWidget* Owner)
    {
        FKeyImageState* State = KeyImages.Find(Image);
        UObject* Resource = CurrentBrush.GetResourceObject();
        if (!State || State->AppliedResource.Get() != Resource)
        {
            if (!Resource) { KeyImages.Remove(Image); return; }
            const FString Path = Resource->GetPathName();
            if (!Path.StartsWith(TEXT("/Game/ImageResource/UI/Key/")) &&
                !Path.StartsWith(TEXT("/Game/UI/InputPrompts/")))
            {
                KeyImages.Remove(Image);
                return;
            }
            State = &KeyImages.Add(Image, { Resource->GetName(), Resource, FString() });
        }
        struct FMapping { const TCHAR* Original; const TCHAR* Keyboard; const TCHAR* Pad; };
        static const FMapping Mapping[] = {
            { TEXT("keyboard_arrows"), TEXT("keyboard_arrows"), TEXT("xbox_dpad") },
            { TEXT("keyboard_escape"), TEXT("keyboard_escape"), TEXT("xbox_button_color_b") },
            { TEXT("keyboard_space"), TEXT("keyboard_space"), TEXT("xbox_button_menu") },
            { TEXT("keyboard_delete"), TEXT("keyboard_delete"), TEXT("xbox_button_color_y") },
            { TEXT("keyboard_page_up"), TEXT("keyboard_page_up"), TEXT("xbox_lb") },
            { TEXT("keyboard_page_down"), TEXT("keyboard_page_down"), TEXT("xbox_rb") },
            { TEXT("Interaction_F_Key"), TEXT("keyboard_f"), TEXT("xbox_button_color_y") },
            { TEXT("Tutorial_Enter_Key"), TEXT("keyboard_enter"), TEXT("xbox_button_color_a") },
            { TEXT("Tutorial_Mouse_Active"), TEXT("mouse_left"), TEXT("xbox_rb") },
            { TEXT("Tutorial_Mouse"), TEXT("mouse"), TEXT("xbox_stick_r") },
            { TEXT("Tutorial_A_Key"), TEXT("keyboard_a"), TEXT("xbox_stick_l_left") },
            { TEXT("Tutorial_D_Key"), TEXT("keyboard_d"), TEXT("xbox_stick_l_right") },
            { TEXT("Tutorial_W_Key"), TEXT("keyboard_w"), TEXT("xbox_stick_l_up") },
            { TEXT("Tutorial_S_Key"), TEXT("keyboard_s"), TEXT("xbox_stick_l_down") },
            { TEXT("Tutorial_C_Key"), TEXT("keyboard_c"), TEXT("xbox_button_color_b") },
            { TEXT("Tutorial_E_Key"), TEXT("keyboard_e"), TEXT("xbox_button_view") },
            { TEXT("Tutorial_Q_Key"), TEXT("keyboard_q"), TEXT("xbox_rt") },
            { TEXT("Tutorial_X_Key"), TEXT("keyboard_x"), TEXT("xbox_button_color_x") },
            { TEXT("Tutorial_Z_Key"), TEXT("keyboard_z"), TEXT("xbox_dpad_down") },
        };
        for (const FMapping& Entry : Mapping)
        {
            if (State->OriginalName != Entry.Original && State->OriginalName != Entry.Keyboard) continue;
            FString Icon = bUsingGamepad ? Entry.Pad : Entry.Keyboard;
            if (FCString::Strcmp(Entry.Keyboard, TEXT("keyboard_enter")) == 0 && Owner->IsA<UDialogueWidget>())
                Icon = bUsingGamepad ? TEXT("xbox_button_color_y") : TEXT("keyboard_f");
            if (bUsingGamepad && Owner->IsA<UInventoryWidget>() && FCString::Strcmp(Entry.Keyboard, TEXT("keyboard_e")) == 0)
                Icon = TEXT("xbox_button_color_b");
            if (State->AppliedIcon == Icon) return;
            if (const FSlateBrush* Brush = Icons.Get(Icon))
            {
                FSlateBrush Replacement = CurrentBrush;
                Replacement.SetResourceObject(Brush->GetResourceObject());
                if (UImage* IconImage = Cast<UImage>(Image))
                    IconImage->SetBrush(Replacement);
                else if (UBorder* IconBorder = Cast<UBorder>(Image))
                    IconBorder->SetBrush(Replacement);
                State->AppliedResource = Replacement.GetResourceObject();
                State->AppliedIcon = Icon;
            }
            return;
        }
    }
public:
    TSharedRef<SWidget> MakeIconHint()
    {
        SAssignNew(IconHint, SHorizontalBox);
        RefreshIconHint();
        return IconHint.ToSharedRef();
    }
private:
    void RefreshIconHint()
    {
        if (!IconHint) return;
        UUserWidget* Menu = FindMenu();
        const FString Context = Menu ? Menu->GetClass()->GetName() : TEXT("Gameplay");
        if (Context == HintContext && bUsingGamepad == bHintWasGamepad) return;
        HintContext = Context;
        bHintWasGamepad = bUsingGamepad;
        IconHint->ClearChildren();
        auto Add = [&](const TCHAR* Keyboard, const TCHAR* Pad, const TCHAR* Label)
        {
            const FSlateBrush* Brush = Icons.Get(bUsingGamepad ? Pad : Keyboard);
            if (Brush)
                IconHint->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4.f, 0.f)
                [SNew(SBox).WidthOverride(32.f).HeightOverride(32.f)[SNew(SImage).Image(Brush)]];
            IconHint->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
                [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                .ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor::Black).Text(FText::FromString(Label))];
        };
        if (Menu)
        {
            if (Menu->IsA<UTutorialWidget>()) { Add(TEXT("keyboard_enter"), TEXT("xbox_button_color_a"), TEXT("\ud655\uc778")); return; }
            Add(TEXT("keyboard_arrows"), TEXT("xbox_dpad"), TEXT("\uc120\ud0dd"));
            Add(TEXT("keyboard_enter"), TEXT("xbox_button_color_a"), Menu->IsA<UInventoryWidget>() ? TEXT("\uc7a5\ucc29") : TEXT("\ud655\uc778"));
            if (Menu->IsA<UInventoryWidget>())
            {
                Add(TEXT("keyboard_delete"), TEXT("xbox_button_color_y"), TEXT("\ud574\uc81c"));
                Add(TEXT("keyboard_page_up"), TEXT("xbox_lb"), TEXT("\uc774\uc804 \ud0ed"));
                Add(TEXT("keyboard_page_down"), TEXT("xbox_rb"), TEXT("\ub2e4\uc74c \ud0ed"));
            }
            if (!Menu->IsA<UTitleScreenWidget>())
                Add(Menu->IsA<UpauseMenuWidget>() ? TEXT("keyboard_escape") : Menu->IsA<UInventoryWidget>() ? TEXT("keyboard_e") : TEXT("keyboard_x"), TEXT("xbox_button_color_b"), TEXT("\ub2eb\uae30"));
            return;
        }
        Add(TEXT("keyboard_f"), TEXT("xbox_button_color_y"), TEXT("\uc0c1\ud638\uc791\uc6a9"));
        Add(TEXT("mouse_left"), TEXT("xbox_rb"), TEXT("\uacf5\uaca9"));
        Add(TEXT("keyboard_q"), TEXT("xbox_rt"), TEXT("\uc2a4\ud0ac"));
        Add(TEXT("keyboard_c"), TEXT("xbox_button_color_b"), TEXT("\uad6c\ub974\uae30"));
        Add(TEXT("keyboard_x"), TEXT("xbox_button_color_x"), TEXT("\ud3ec\uc158"));
        Add(TEXT("keyboard_z"), TEXT("xbox_dpad_down"), TEXT("\ud3ec\uc158 \ubcc0\uacbd"));
        Add(TEXT("keyboard_e"), TEXT("xbox_button_view"), TEXT("\uc778\ubca4\ud1a0\ub9ac"));
    }

    double NextPromptRefresh = 0;
    void RefreshPrompts()
    {
        const double Now = FPlatformTime::Seconds();
        if (Now < NextPromptRefresh || !World.IsValid()) return;
        NextPromptRefresh = Now + 0.1;
        InlinePrompts.Prune();
        for (auto It = KeyImages.CreateIterator(); It; ++It)
            if (!It.Key().IsValid()) It.RemoveCurrent();
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UUserWidget::StaticClass(), false);
        for (UUserWidget* Widget : Widgets)
        {
            if (!IsValid(Widget) || !Widget->WidgetTree) continue;
            const bool Tutorial = Widget->IsA<UTutorialWidget>();
            const bool Interaction = Widget->IsA<UInteractionWidget>();
            const bool Dialogue = Widget->IsA<UDialogueWidget>();
            if (!Tutorial && !Interaction && !Dialogue && !Widget->GetClass()->GetPathName().StartsWith(TEXT("/Game/BluePrint/UI_Widget/"))) continue;
            TArray<UWidget*> Children;
            Widget->WidgetTree->GetAllWidgets(Children);
            for (UWidget* Child : Children)
            {
                if (UImage* Image = Cast<UImage>(Child))
                {
                    UpdateKeyImage(Image, Image->GetBrush(), Widget);
                }
                else if (UBorder* Border = Cast<UBorder>(Child))
                    UpdateKeyImage(Border, Border->Background, Widget);
                if (UTextBlock* Text = Cast<UTextBlock>(Child))
                    InlinePrompts.Update(Widget, Text, bUsingGamepad);
            }
        }
    }

    TWeakObjectPtr<UWorld> World;
    TSet<FKey> Consumed;
    FVector2D Axis = FVector2D::ZeroVector;
    FKey HeldDirection;
    double RepeatAt = 0;
};

bool UGamepadSupportSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}
TStatId UGamepadSupportSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UGamepadSupportSubsystem, STATGROUP_Tickables);
}
void UGamepadSupportSubsystem::Tick(float)
{
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport || !FSlateApplication::IsInitialized()) return;
    if (!Processor.IsValid())
    {
        Processor = MakeShared<FGamepadUIProcessor>(GetWorld());
        FSlateApplication::Get().RegisterInputPreProcessor(Processor, 1);
    }
    if (!Hint.IsValid())
    {
        Hint = SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(20.f)
            .Visibility(EVisibility::HitTestInvisible)
            [Processor->MakeIconHint()];
        Viewport->AddViewportWidgetContent(Hint.ToSharedRef(), 19000);
    }
}
void UGamepadSupportSubsystem::Deinitialize()
{
    if (Processor.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
    if (Hint.IsValid() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(Hint.ToSharedRef());
    Hint.Reset(); Processor.Reset();
    Super::Deinitialize();
}

bool UGamepadSupportSubsystem::IsUsingGamepad() const
{
    return Processor.IsValid() && Processor->bUsingGamepad;
}
