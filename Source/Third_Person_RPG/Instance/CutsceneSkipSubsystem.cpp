#include "CutsceneSkipSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"
#include "InputCoreTypes.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

namespace
{
    class FSkipInputProcessor final : public IInputProcessor
    {
    public:
        explicit FSkipInputProcessor(UCutsceneSkipSubsystem* InOwner) : Owner(InOwner) {}
        virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
        virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            if ((Event.GetKey() != EKeys::SpaceBar && Event.GetKey() != EKeys::Gamepad_Special_Right)) return false;
            if (bConsumedPress) return true;
            if (Event.IsRepeat()) return false;
            bConsumedPress = Owner.IsValid() && Owner->RequestSkip();
            return bConsumedPress;
        }
        virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& Event) override
        {
            if ((Event.GetKey() != EKeys::SpaceBar && Event.GetKey() != EKeys::Gamepad_Special_Right) || !bConsumedPress) return false;
            bConsumedPress = false;
            return true;
        }
    private:
        TWeakObjectPtr<UCutsceneSkipSubsystem> Owner;
        bool bConsumedPress = false;
    };
}

bool UCutsceneSkipSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void UCutsceneSkipSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
}

TStatId UCutsceneSkipSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UCutsceneSkipSubsystem, STATGROUP_Tickables);
}

void UCutsceneSkipSubsystem::RemoveHint()
{
    if (Hint) Hint->RemoveFromParent();
    Hint = nullptr;
}

void UCutsceneSkipSubsystem::Deinitialize()
{
    if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
    InputProcessor.Reset();
    RemoveHint();
    ActivePlayer.Reset();
    PendingSkip.Reset();
    Super::Deinitialize();
}

bool UCutsceneSkipSubsystem::RequestSkip()
{
    if (!ActivePlayer.IsValid() || !ActivePlayer->IsPlaying() || PendingSkip.IsValid()) return false;
    UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
    if (!Viewport || !Viewport->GetGameViewportWidget().IsValid()) return false;
    FSlateApplication& Slate = FSlateApplication::Get();
    const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Viewport->GetGameViewportWidget().ToSharedRef());
    if (!Window.IsValid() || !Window->IsActive()) return false;
    PendingSkip = ActivePlayer;
    return true;
}

void UCutsceneSkipSubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World || !World->GetGameViewport() || !FSlateApplication::IsInitialized()) return;
    if (!InputProcessor.IsValid())
    {
        InputProcessor = MakeShared<FSkipInputProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
    }

    if (ULevelSequencePlayer* Player = PendingSkip.Get())
    {
        PendingSkip.Reset();
        ActivePlayer.Reset();
        RemoveHint();
        if (Player->IsPlaying())
        {
            // Play evaluates remaining event tracks and lets the engine invoke OnFinished.
            // Stop/GoToEndAndStop alone would omit boss spawning and map transition callbacks.
            Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(
                Player->GetEndTime().Time, EUpdatePositionMethod::Play));
        }
        return;
    }

    if (!ActivePlayer.IsValid() || !ActivePlayer->IsPlaying())
    {
        ActivePlayer.Reset();
        for (TActorIterator<ALevelSequenceActor> It(World); It; ++It)
        {
            ULevelSequencePlayer* Player = It->GetSequencePlayer();
            if (!Player || !Player->IsPlaying() || !Player->GetSequence()) continue;
            // Project cinematics only: do not skip gameplay/environment sequences.
            if (Player->GetSequence()->GetPathName().StartsWith(TEXT("/Game/Sequence/")))
            {
                ActivePlayer = Player;
                break;
            }
        }
    }

    if (!ActivePlayer.IsValid())
    {
        RemoveHint();
        return;
    }
    if (!Hint && World->GetFirstPlayerController())
    {
        if (!bTriedHintClass)
        {
            bTriedHintClass = true;
            HintClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/BluePrint/UI_Widget/Input/WBP_CutsceneSkip.WBP_CutsceneSkip_C"));
        }
        if (HintClass)
        {
            Hint = CreateWidget<UUserWidget>(World->GetFirstPlayerController(), HintClass);
            if (Hint)
            {
                Hint->AddToViewport(20000);
                Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
        }
    }
}
