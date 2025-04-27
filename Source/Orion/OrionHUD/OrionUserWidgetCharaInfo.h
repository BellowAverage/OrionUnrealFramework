// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../OrionChara.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
//#include "Components/ProgressBar.h"
#include "OrionUserWidgetProceduralAction.h"
#include "Components/Border.h"
#include "Components/Slider.h"
#include "Components/VerticalBox.h"
#include "OrionUserWidgetCharaInfo.generated.h"

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class ORION_API UOrionUserWidgetCharaInfo : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UPROPERTY()
	AOrionChara* CharaRef = nullptr;

	/* Widget Bindings */

	UFUNCTION(BlueprintCallable)
	void InitCharaInfoPanelParams(AOrionChara* InChara);

	UFUNCTION(BlueprintCallable)
	void UpdateCharaInfo(AOrionChara* InChara);

	UFUNCTION(BlueprintCallable)
	void UpdateProcActionQueue(AOrionChara* InChara);

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextCharaName = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextCurrHealth = nullptr;

	UPROPERTY(meta = (BindWidget))
	USlider* SliderSpeed = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextActionQueue = nullptr;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckDefensive = nullptr;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckAggressive = nullptr;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckUnavailable = nullptr;

	UPROPERTY(meta = (BindWidget))
	UCheckBox* CheckbIsCharaProcedural = nullptr;

	UPROPERTY(meta = (BindWidget))
	UVerticalBox* ProceduralActionBox = nullptr;

	UPROPERTY(meta = (BindWidget))
	UBorder* ProceduralActionBorder = nullptr;

	// 上/下/删 三个按钮
	UPROPERTY(meta = (BindWidget))
	UButton* ButtonMoveUp = nullptr;
	UPROPERTY(meta = (BindWidget))
	UButton* ButtonMoveDown = nullptr;
	UPROPERTY(meta = (BindWidget))
	UButton* ButtonDelete = nullptr;

	/* Call Back */

	UFUNCTION()
	void OnSliderChange(float InValue);

	UFUNCTION()
	void OnDefensiveChanged(bool bIsChecked);

	UFUNCTION()
	void OnAggressiveChanged(bool bIsChecked);

	UFUNCTION()
	void OnUnavailableChanged(bool bIsChecked);

	bool bSuppressCheckEvents = false;

	UFUNCTION()
	void OnCheckbIsCharaProceduralChanged(bool bIsChecked);

	UPROPERTY(EditDefaultsOnly, Category = "Procedural")
	TSubclassOf<UOrionUserWidgetProceduralAction> ProceduralActionItemClass;

	int32 SelectedIndex = INDEX_NONE;

	UFUNCTION()
	void OnProcSelected(int32 ItemIndex);

	// 回调：上移/下移/删除 按钮
	UFUNCTION()
	void OnMoveUpClicked();
	UFUNCTION()
	void OnMoveDownClicked();
	UFUNCTION()
	void OnDeleteClicked();

	// 高亮选中条目
	void RefreshHighlight();
};
