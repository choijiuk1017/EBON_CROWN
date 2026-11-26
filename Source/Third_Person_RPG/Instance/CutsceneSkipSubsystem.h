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
private:
    void RemoveHint();
    TWeakObjectPtr<ULevelSequencePlayer> ActivePlayer;
    TWeakObjectPtr<ULevelSequencePlayer> PendingSkip;
    TSharedPtr<IInputProcessor> InputProcessor;
    TSharedPtr<SWidget> Hint;
};
