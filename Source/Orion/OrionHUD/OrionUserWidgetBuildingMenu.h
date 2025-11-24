// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionUserWidgetBuildingOption.h"
#include "Blueprint/UserWidget.h"
#include "Components/HorizontalBox.h"
#include "OrionUserWidgetBuildingMenu.generated.h"

struct FOrionDataBuilding;

DECLARE_DELEGATE_TwoParams(FOnBuildingOptionSelected, int32, bool);
DECLARE_DELEGATE_OneParam(FOnToggleDemolishMode, bool);


UCLASS(Blueprintable)
class ORION_API UOrionUserWidgetBuildingMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UHorizontalBox* HorizontalBoxBuildings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	TSubclassOf<UOrionUserWidgetBuildingOption> BuildingOptionClass;
	static inline TArray<UOrionUserWidgetBuildingOption*> CachedBuildingOptions;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckBoxDemolish;

	FOnToggleDemolishMode OnToggleDemolishMode;

	FOnBuildingOptionSelected OnBuildingOptionSelected;

	virtual void NativeConstruct() override
	{
		Super::NativeConstruct();
		if (CheckBoxDemolish)
		{
			CheckBoxDemolish->OnCheckStateChanged.AddDynamic(
				this, &UOrionUserWidgetBuildingMenu::OnDemolishModeChanged);
		}
	}

	UFUNCTION()
	void OnDemolishModeChanged(const bool bIsChecked)
	{
		if (CheckBoxDemolish && CheckBoxDemolish->IsHovered())
		{
			OnToggleDemolishMode.ExecuteIfBound(bIsChecked);

			for (const auto& EachOption : CachedBuildingOptions)
			{
				if (EachOption)
				{
					EachOption->BuildingOptionCheckBox->SetCheckedState(ECheckBoxState::Unchecked);
				}
			}
		}
	}


	void InitBuildingMenu(const TArray<FOrionDataBuilding>& BuildingMenuOptions)
	{
		if (!BuildingOptionClass)
		{
			return;
		}

		CachedBuildingOptions.Empty();

		for (auto& EachBuildingOption : BuildingMenuOptions)
		{
			if (UOrionUserWidgetBuildingOption* BuildingOption = CreateWidget<UOrionUserWidgetBuildingOption>(
				this, BuildingOptionClass))
			{
				BuildingOption->BuildingOptionInit(EachBuildingOption);

				CachedBuildingOptions.Add(BuildingOption);

				BuildingOption->OnBuildingOptionSelected.BindLambda(
					[&](int32 InBuildingId, bool bIsChecked)
					{
						//UE_LOG(LogTemp, Warning, TEXT("Building Id: %d, Checked: %s"), InBuildingId, bIsChecked ? TEXT("True") : TEXT("False"));

						if (CachedBuildingOptions.Num() == BuildingMenuOptions.Num() && bIsChecked)
						{
							for (const auto& EachOption : CachedBuildingOptions)
							{
								if (EachOption && EachOption->InBuildingId != InBuildingId && EachOption->
									BuildingOptionCheckBox->IsChecked())
								{
									EachOption->BuildingOptionCheckBox->SetCheckedState(ECheckBoxState::Unchecked);
								}
							}
						}

						OnBuildingOptionSelected.ExecuteIfBound(InBuildingId, bIsChecked);
					});

				HorizontalBoxBuildings->AddChild(BuildingOption);
			}
		}
	}
};
