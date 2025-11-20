// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/CheckBox.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "OrionUserWidgetBuildingOption.generated.h"


DECLARE_DELEGATE_TwoParams(FOnBuildingOptionSelected, int32, bool);

UCLASS(Blueprintable)
class ORION_API UOrionUserWidgetBuildingOption : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UCheckBox* BuildingOptionCheckBox;


	FOnBuildingOptionSelected OnBuildingOptionSelected;


	virtual void NativeConstruct() override
	{
		Super::NativeConstruct();
		if (BuildingOptionCheckBox)
		{
			BuildingOptionCheckBox->OnCheckStateChanged.AddDynamic(
				this, &UOrionUserWidgetBuildingOption::OnBuildingOptionChanged);
		}
	}

	int32 InBuildingId = -1;

	UFUNCTION()
	void OnBuildingOptionChanged(const bool bIsChecked)
	{
		if (BuildingOptionCheckBox && BuildingOptionCheckBox->IsHovered())
		{
			OnBuildingOptionSelected.ExecuteIfBound(InBuildingId, bIsChecked);
		}
	}

	void BuildingOptionInit(const FOrionDataBuilding& InBuildingData)
	{
		InBuildingId = InBuildingData.BuildingId;

		if (!BuildingOptionCheckBox)
		{
			return;
		}

		// —— 1) 准备基础背景刷子 —— 
		FSlateBrush BaseBrush;
		if (UTexture2D* BackgroundTexture = LoadObject<UTexture2D>(nullptr, *InBuildingData.BuildingImageReference))
		{
			BaseBrush.SetResourceObject(BackgroundTexture);
		}
		BaseBrush.ImageSize = FVector2D(200.f, 200.f);

		// —— 2) 构造未选中态 —— 
		FSlateBrush Unchecked = BaseBrush;
		FSlateBrush UncheckedHover = BaseBrush;
		UncheckedHover.TintColor = FSlateColor{FLinearColor(1, 1, 1, 0.8f)};
		FSlateBrush UncheckedPressed = BaseBrush;
		UncheckedPressed.TintColor = FSlateColor{FLinearColor(1, 1, 1, 0.6f)};

		// —— 3) 构造选中态（带蓝色高亮）—— 
		FLinearColor CheckedTint(0.2f, 0.6f, 1.f, 1.f);
		FSlateBrush Checked = BaseBrush;
		Checked.TintColor = FSlateColor{CheckedTint};
		FSlateBrush CheckedHover = Checked;
		CheckedHover.TintColor = FSlateColor{CheckedTint * FLinearColor(1, 1, 1, 0.8f)};
		FSlateBrush CheckedPressed = Checked;
		CheckedPressed.TintColor = FSlateColor{CheckedTint * FLinearColor(1, 1, 1, 0.6f)};

		// —— 4) 应用到 CheckBoxStyle —— 
		FCheckBoxStyle NewStyle = BuildingOptionCheckBox->GetWidgetStyle();

		NewStyle.SetUncheckedImage(Unchecked);
		NewStyle.SetUncheckedHoveredImage(UncheckedHover);
		NewStyle.SetUncheckedPressedImage(UncheckedPressed);

		NewStyle.SetCheckedImage(Checked);
		NewStyle.SetCheckedHoveredImage(CheckedHover);
		NewStyle.SetCheckedPressedImage(CheckedPressed);

		BuildingOptionCheckBox->SetWidgetStyle(NewStyle);
		BuildingOptionCheckBox->SynchronizeProperties();
	}
};
