// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "OrionUserWidgetUIBase.generated.h"

DECLARE_DELEGATE(FOnViewLevelUp);
DECLARE_DELEGATE(FOnViewLevelDown);
DECLARE_DELEGATE(FOnSaveGame);
DECLARE_DELEGATE(FOnLoadGame);

UCLASS()
class ORION_API UOrionUserWidgetUIBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UButton* ViewLevelUp;

	UPROPERTY(meta = (BindWidget))
	UButton* ViewLevelDown;

	UPROPERTY(meta = (BindWidget))
	UButton* SaveGame;

	UPROPERTY(meta = (BindWidget))
	UButton* LoadGame;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* BulletNum = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StoneNum = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* GoldNum = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* GameClock = nullptr;

	FOnSaveGame OnSaveGame;
	FOnLoadGame OnLoadGame;

	FOnViewLevelUp OnViewLevelUp;
	FOnViewLevelUp OnViewLevelDown;

	UFUNCTION()
	void OnViewLevelUpClicked()
	{
		OnViewLevelUp.ExecuteIfBound();
	}

	UFUNCTION()
	void OnViewLevelDownClicked()
	{
		OnViewLevelDown.ExecuteIfBound();
	}

	UFUNCTION()
	void OnSaveGameClicked()
	{
		OnSaveGame.ExecuteIfBound();
	}

	UFUNCTION()
	void OnLoadGameClicked()
	{
		OnLoadGame.ExecuteIfBound();
	}

	virtual void NativeConstruct() override
	{
		Super::NativeConstruct();

		if (ViewLevelUp)
		{
			ViewLevelUp->OnClicked.AddDynamic(this, &UOrionUserWidgetUIBase::OnViewLevelUpClicked);
		}

		if (ViewLevelDown)
		{
			ViewLevelDown->OnClicked.AddDynamic(this, &UOrionUserWidgetUIBase::OnViewLevelDownClicked);
		}

		if (SaveGame)
		{
			SaveGame->OnClicked.AddDynamic(this, &UOrionUserWidgetUIBase::OnSaveGameClicked);
		}

		if (LoadGame)
		{
			LoadGame->OnClicked.AddDynamic(this, &UOrionUserWidgetUIBase::OnLoadGameClicked);
		}


		if (BulletNum) BulletNum->SetText(FText::AsNumber(1));
		if (StoneNum) StoneNum->SetText(FText::AsNumber(12));
		if (GoldNum) GoldNum->SetText(FText::AsNumber(100));
	}
};
