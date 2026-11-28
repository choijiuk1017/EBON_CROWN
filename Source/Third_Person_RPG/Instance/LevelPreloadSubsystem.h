#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LevelPreloadSubsystem.generated.h"

struct FStreamableHandle;

// Retain the next map's dependencies across OpenLevel without creating a second world.
UCLASS()
class THIRD_PERSON_RPG_API ULevelPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	TSharedPtr<FStreamableHandle> PreloadLevel(FName LevelName);

	void OpenWithProgress(FName LevelName);

private:
	void RemoveProgress();
	TSharedPtr<class SWidget> ProgressOverlay;
	TWeakObjectPtr<class UGameViewportClient> ProgressViewport;
	FTimerHandle TravelTimer;
	bool bTravelPending = false;
	void ReleasePreload();
	void OnMapLoaded(UWorld* World);
	FDelegateHandle MapLoadedHandle;
	TSharedPtr<FStreamableHandle> PreloadHandle;
	FString RequestedMap;
};
