#include "CutsceneSkipSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneCameraShakeTrack.h"
#include "Sections/MovieSceneCameraShakeSection.h"
#include "Camera/CameraComponent.h"

#include "Character/TPRPlayerController.h"
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
                CacheShakeSections(*Player);
                break;
            }
        }
    }

    if (!ActivePlayer.IsValid())
    {
        RemoveHint();
        return;
    }
    PlayShakeRumble(*ActivePlayer.Get());
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
                // Intro slides and story dialogue use ZOrder 999999.
                Hint->AddToViewport(MAX_int32 - 100);
                Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
            }
        }
    }
}

void UCutsceneSkipSubsystem::CacheShakeSections(ULevelSequencePlayer& Player)
{
    ShakeSections.Reset();
    LastPulseTime = -1.0;
    const UMovieScene* Scene = Player.GetSequence()->GetMovieScene();
    if (!Scene) return;
    for (const FMovieSceneBinding& Binding : Scene->GetBindings())
    {
        for (const UMovieSceneTrack* Track : Binding.GetTracks())
        {
            if (!Track || !Track->IsA<UMovieSceneCameraShakeTrack>() || Track->IsEvalDisabled()) continue;
            for (UMovieSceneSection* Section : Track->GetAllSections())
            {
                auto* Shake = Cast<UMovieSceneCameraShakeSection>(Section);
                if (Shake && Shake->IsActive() && Shake->ShakeData.ShakeClass && !Track->IsRowEvalDisabled(Shake->GetRowIndex()))
                    ShakeSections.Add(Binding.GetObjectGuid(), Shake);
            }
        }
    }
}

void UCutsceneSkipSubsystem::PlayShakeRumble(ULevelSequencePlayer& Player)
{
    if (ShakeSections.IsEmpty()) return;
    const FQualifiedFrameTime Time = Player.GetCurrentTime();
    const double Now = Time.AsSeconds();
    if (Now >= LastPulseTime && Now - LastPulseTime < 0.05) return;
    const UCameraComponent* Camera = Player.GetActiveCameraComponent();
    if (!Camera) return;
    const FFrameRate Rate = Player.GetSequence()->GetMovieScene()->GetTickResolution();
    const FFrameTime Frame = Time.ConvertTo(Rate);
    float Strength = 0.f;
    float Duration = 0.f;
    for (const auto& Entry : ShakeSections)
    {
        const auto* Shake = Entry.Value.Get();
        if (!Shake || !Shake->GetRange().Contains(Frame.FloorToFrame())) continue;
        const auto Objects = Player.GetBoundObjects(UE::MovieScene::FRelativeObjectBindingID(Entry.Key));
        if (!Objects.Contains(Camera) && !Objects.Contains(Camera->GetOwner())) continue;
        const float Base = Shake->ShakeData.ShakeClass->GetName().Contains(TEXT("Small")) ? 0.35f : 0.85f;
        Strength = FMath::Max(Strength, Base * Shake->ShakeData.PlayScale * Shake->EvaluateEasing(Frame));
        const float Remaining = Shake->HasEndFrame()
            ? Rate.AsSeconds(Shake->GetExclusiveEndFrame() - Frame) / FMath::Max(0.01f, FMath::Abs(Player.GetPlayRate()))
            : 0.06f;
        Duration = FMath::Max(Duration, FMath::Min(Remaining, 0.06f));
    }
    if (Strength <= 0.f || Duration <= 0.f) return;
    if (auto* PC = Cast<ATPRPlayerController>(GetWorld()->GetFirstPlayerController()))
    {
        PC->PulseRumble(Strength, Duration);
        LastPulseTime = Now;
    }
}
