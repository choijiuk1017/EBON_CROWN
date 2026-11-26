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
#include "UI/InteractionWidget.h"
#include "UI/DialogueWidget.h"

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
    FText HintText() const
    {
        if (UUserWidget* Menu = FindMenu())
        {
            if (Menu->IsA<UInventoryWidget>()) return FText::FromString(TEXT("D-pad: Select   A: Equip   Y: Unequip   LB/RB: Tab   B: Close"));
            if (Menu->IsA<UTutorialWidget>()) return FText::FromString(TEXT("A: Continue"));
            if (Menu->IsA<UTitleScreenWidget>()) return FText::FromString(TEXT("D-pad / LS: Navigate   A: Confirm"));
            return FText::FromString(TEXT("D-pad / LS: Navigate   A: Confirm   B: Back"));
        }
        return FText::FromString(TEXT("Y: Interact / Talk   RB: Attack   RT: Skill   B: Roll / Hold to sprint   X: Potion   View: Inventory"));
    }
private:

    struct FPromptState { FText Original; FText Applied; };
    TMap<TWeakObjectPtr<UTextBlock>, FPromptState> PromptStates;
    TMap<TWeakObjectPtr<UImage>, float> KeyImages;
    double NextPromptRefresh = 0;
    void RefreshPrompts()
    {
        const double Now = FPlatformTime::Seconds();
        if (Now < NextPromptRefresh || !World.IsValid()) return;
        NextPromptRefresh = Now + 0.1;
        for (auto It = PromptStates.CreateIterator(); It; ++It)
        {
            UTextBlock* Text = It.Key().Get();
            if (!Text) { It.RemoveCurrent(); continue; }
            if (!bUsingGamepad)
            {
                if (Text->GetText().EqualTo(It.Value().Applied)) Text->SetText(It.Value().Original);
                It.RemoveCurrent();
            }
        }
        for (auto It = KeyImages.CreateIterator(); It; ++It)
        {
            UImage* Image = It.Key().Get();
            if (!Image) { It.RemoveCurrent(); continue; }
            if (!bUsingGamepad) { Image->SetRenderOpacity(It.Value()); It.RemoveCurrent(); }
        }
        if (!bUsingGamepad) return;
        TArray<UUserWidget*> Widgets;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UUserWidget::StaticClass(), false);
        for (UUserWidget* Widget : Widgets)
        {
            if (!IsValid(Widget) || !Widget->WidgetTree) continue;
            const bool Tutorial = Widget->IsA<UTutorialWidget>();
            const bool Interaction = Widget->IsA<UInteractionWidget>();
            const bool Dialogue = Widget->IsA<UDialogueWidget>();
            if (!Tutorial && !Interaction && !Dialogue && !Widget->GetClass()->GetPathName().StartsWith(TEXT("/Game/BluePrint/UI_Widget/"))) continue;
            Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
            {
                if (UImage* Image = Cast<UImage>(Child))
                {
                    UObject* Resource = Image->GetBrush().GetResourceObject();
                    if ((Tutorial || Interaction || Dialogue) && Resource && Resource->GetPathName().StartsWith(TEXT("/Game/ImageResource/UI/Key/")))
                    {
                        if (!KeyImages.Contains(Image)) KeyImages.Add(Image, Image->GetRenderOpacity());
                        Image->SetRenderOpacity(0.f);
                    }
                }
                UTextBlock* Text = Cast<UTextBlock>(Child);
                if (!Text) return;
                FPromptState* Existing = PromptStates.Find(Text);
                if (Existing && Text->GetText().EqualTo(Existing->Applied)) return;
                const FText Original = Text->GetText();
                FString Value = Original.ToString();
                const FString Before = Value;
                // Exact authored instructions only; never substitute letters in item names or dialogue.
                const TPair<const TCHAR*, const TCHAR*> Replacements[] = {
                    { TEXT("'E'\ud0a4\ub97c \ub20c\ub7ec \uc778\ubca4\ud1a0\ub9ac \ub2eb\uae30"), TEXT("[B / View] \uc778\ubca4\ud1a0\ub9ac \ub2eb\uae30") },
                    { TEXT("'X' \ud0a4\ub97c \ub20c\ub7ec \ub098\uac00\uae30"), TEXT("[B] \ub098\uac00\uae30") },
                    { TEXT("'Enter'\ub85c \uacb0\uc815"), TEXT("[A] \uacb0\uc815") },
                    { TEXT("\uac15\ud654\ud558\ub824\uba74 \ubb34\uae30\ub97c \uc120\ud0dd \ud6c4 'ENTER' \ud0a4\ub97c \ub204\ub974\uc2dc\uc624"), TEXT("\ubb34\uae30 \uc120\ud0dd \ud6c4 [A] \uac15\ud654") },
                    { TEXT("\ubb34\uae30 \uc120\ud0dd: \ubc29\ud5a5\ud0a4 \ubc0f 'W', 'S' \ub610\ub294 \ub9c8\uc6b0\uc2a4 \ud074\ub9ad"), TEXT("\ubb34\uae30 \uc120\ud0dd: \ubc29\ud5a5 \ud328\ub4dc / LS") },
                    { TEXT("\ubc29\ud5a5\ud0a4\ub85c \uc120\ud0dd\uc9c0 \ubcc0\uacbd \ubc0f \uc218\uce58 \ubcc0\uacbd"), TEXT("\ubc29\ud5a5 \ud328\ub4dc / LS: \uc120\ud0dd \ubc0f \uc218\uce58 \ubcc0\uacbd") },
                    { TEXT("\ubc29\ud5a5\ud0a4\ub85c \uc120\ud0dd\uc9c0 \ubcc0\uacbd"), TEXT("\ubc29\ud5a5 \ud328\ub4dc / LS: \uc120\ud0dd") },
                    { TEXT("\ubc29\ud5a5\ud0a4\ub85c \uba54\ub274 \ubcc0\uacbd"), TEXT("\ubc29\ud5a5 \ud328\ub4dc / LS: \uba54\ub274 \ubcc0\uacbd") },
                    { TEXT("Enter\ub85c \ud655\uc815"), TEXT("[A] \ud655\uc815") },
                    { TEXT("\uc5d4\ud130\ub85c \ud655\uc815"), TEXT("[A] \ud655\uc815") },
                    { TEXT("WASD\ub97c \uc774\uc6a9\ud574 \uce90\ub9ad\ud130\ub97c \uc774\ub3d9"), TEXT("LS\ub85c \uce90\ub9ad\ud130 \uc774\ub3d9") },
                    { TEXT("\ub9c8\uc6b0\uc2a4\ub97c \uc774\uc6a9\ud574 \uce74\uba54\ub77c \ud68c\uc804"), TEXT("RS\ub85c \uce74\uba54\ub77c \ud68c\uc804") },
                    { TEXT("X \ud0a4\ub97c \ub20c\ub7ec \ud3ec\uc158 \uc0ac\uc6a9"), TEXT("[X] \ud3ec\uc158 \uc0ac\uc6a9") },
                    { TEXT("Z \ud0a4\ub97c \ub20c\ub7ec \ud3ec\uc158 \ubcc0\uacbd"), TEXT("[\ubc29\ud5a5 \ud328\ub4dc \uc544\ub798] \ud3ec\uc158 \ubcc0\uacbd") },
                };
                for (const auto& Pair : Replacements) Value.ReplaceInline(Pair.Key, Pair.Value, ESearchCase::CaseSensitive);
                if (Tutorial)
                {
                    const TPair<const TCHAR*, const TCHAR*> Labels[] = {
                    { TEXT("\ud655\uc778"), TEXT("[A] \ud655\uc778") },
                    { TEXT("\uad6c\ub974\uae30"), TEXT("[B] \uad6c\ub974\uae30 / \uae38\uac8c \ub20c\ub7ec \ub2ec\ub9ac\uae30") },
                    { TEXT("\ud3ec\uc158 \uc0ac\uc6a9"), TEXT("[X] \ud3ec\uc158 \uc0ac\uc6a9") },
                    { TEXT("\ud3ec\uc158 \ubcc0\uacbd"), TEXT("[\ubc29\ud5a5 \ud328\ub4dc \uc544\ub798] \ud3ec\uc158 \ubcc0\uacbd") },
                    { TEXT("\uae30\ubcf8 \uacf5\uaca9"), TEXT("[RB] \uae30\ubcf8 \uacf5\uaca9") },
                    { TEXT("\uc778\ubca4\ud1a0\ub9ac"), TEXT("[View] \uc778\ubca4\ud1a0\ub9ac / [A] \uc7a5\ucc29 / [Y] \ud574\uc81c") },
                    { TEXT("\ubb34\uae30 \ud68d\ub4dd"), TEXT("[Y] \ubb34\uae30 \ud68d\ub4dd") },
                    { TEXT("\uc0c1\ud638\uc791\uc6a9"), TEXT("[Y] \uc0c1\ud638\uc791\uc6a9") },
                    { TEXT("\uc2a4\ud0ac \ubc1c\ub3d9"), TEXT("[RT] \uc2a4\ud0ac \ubc1c\ub3d9") },
                    { TEXT("\uc774\ub3d9"), TEXT("[LS] \uc774\ub3d9") },
                    { TEXT("\uce74\uba54\ub77c \uc870\uc791"), TEXT("[RS] \uce74\uba54\ub77c \uc870\uc791") },
                    };
                    for (const auto& Pair : Labels) if (Value.TrimStartAndEnd() == Pair.Key) { Value = Pair.Value; break; }
                }
                if (Interaction && !Value.StartsWith(TEXT("[Y]"))) Value = TEXT("[Y] ") + Value;
                if (Dialogue && Value == TEXT("\uD655\uC778")) Value = TEXT("[Y] \uB2E4\uC74C");
                if (Value != Before)
                {
                    const FText Applied = FText::FromString(Value);
                    PromptStates.Add(Text, { Original, Applied });
                    Text->SetText(Applied);
                }
            });
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
        const TWeakPtr<FGamepadUIProcessor> Weak = Processor;
        Hint = SNew(SBox).HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(20.f)
            .Visibility_Lambda([Weak]() { auto P = Weak.Pin(); return P && P->bUsingGamepad ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                .ShadowOffset(FVector2D(1.f, 1.f)).ShadowColorAndOpacity(FLinearColor::Black)
                .Text_Lambda([Weak]() { auto P = Weak.Pin(); return P ? P->HintText() : FText::GetEmpty(); })];
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
