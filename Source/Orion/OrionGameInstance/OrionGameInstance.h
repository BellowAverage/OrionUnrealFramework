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
	/** 手动绑定到 UI / 按键即可 */
	UFUNCTION(BlueprintCallable)
	void SaveGame();
	UFUNCTION(BlueprintCallable)
	void LoadGame();

private:
	static constexpr const TCHAR* SlotName = TEXT("OrionSlot");
};
