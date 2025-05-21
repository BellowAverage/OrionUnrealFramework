// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "OrionUserWidgetUIBase.generated.h"

DECLARE_DELEGATE(FOnViewLevelUp);
DECLARE_DELEGATE(FOnViewLevelDown);

UCLASS()
class ORION_API UOrionUserWidgetUIBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UButton* ViewLevelUp;

	UPROPERTY(meta = (BindWidget))
	UButton* ViewLevelDown;

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
	}
};
