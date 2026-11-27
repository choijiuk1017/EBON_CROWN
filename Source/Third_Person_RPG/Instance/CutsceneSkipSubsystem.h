#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CutsceneSkipSubsystem.generated.h"

class IInputProcessor;
class SWidget;
class ULevelSequencePlayer;

UCLASS()
class THIRD_PERSON_RPG_API UCutsceneSkipSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
    bool RequestSkip();
    bool IsSkipHint(const UUserWidget* Widget) const { return Hint == Widget; }
private:
    void RemoveHint();
    void CacheShakeSections(ULevelSequencePlayer& Player);
    void PlayShakeRumble(ULevelSequencePlayer& Player);
    TMultiMap<FGuid, TWeakObjectPtr<class UMovieSceneCameraShakeSection>> ShakeSections;
    double LastPulseTime = -1.0;
    TWeakObjectPtr<ULevelSequencePlayer> ActivePlayer;
    TWeakObjectPtr<ULevelSequencePlayer> PendingSkip;
    TSharedPtr<IInputProcessor> InputProcessor;
    UPROPERTY(Transient) TObjectPtr<class UUserWidget> Hint;
    UPROPERTY(Transient) TSubclassOf<UUserWidget> HintClass;
    bool bTriedHintClass = false;
};
