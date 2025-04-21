// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../OrionChara.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
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

	AOrionChara* CharaRef = nullptr;

	/* Widget Bindings */

	UFUNCTION(BlueprintCallable)
	void InitCharaInfoPanelParams(AOrionChara* InChara);

	UFUNCTION(BlueprintCallable)
	void UpdateCharaInfo(AOrionChara* InChara);

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextCharaName = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextCurrHealth = nullptr;

	UPROPERTY(meta = (BindWidget))
	USlider* SliderSpeed = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextActionQueue = nullptr;

	/* Call Back */

	UFUNCTION()
	void OnSliderChange(float InValue);


};
