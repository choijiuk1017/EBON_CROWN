#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GamepadSupportSubsystem.generated.h"
class FGamepadUIProcessor;
class SWidget;
UCLASS()
class THIRD_PERSON_RPG_API UGamepadSupportSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    bool IsUsingGamepad() const;
    virtual void Tick(float DeltaTime) override;
    virtual void Deinitialize() override;
    virtual TStatId GetStatId() const override;
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
    TSharedPtr<FGamepadUIProcessor> Processor;
    TSharedPtr<SWidget> Hint;
};
