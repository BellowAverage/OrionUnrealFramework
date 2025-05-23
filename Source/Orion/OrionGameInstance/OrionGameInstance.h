// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OrionGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class ORION_API UOrionGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SaveGame();
	UFUNCTION(BlueprintCallable)
	void LoadGame();

private:
	static constexpr const TCHAR* SlotName = TEXT("OrionSlot");
};
