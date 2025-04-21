// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionUserWidgetCharaInfo.h"

void UOrionUserWidgetCharaInfo::NativeConstruct()
{
	Super::NativeConstruct();
	if (SliderSpeed)
	{
		SliderSpeed->OnValueChanged.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnSliderChange);
	}
}

void UOrionUserWidgetCharaInfo::InitCharaInfoPanelParams(AOrionChara* InChara)
{
    if (SliderSpeed)
    {
        SliderSpeed->SetValue(InChara->OrionCharaSpeed);
    }
}

void UOrionUserWidgetCharaInfo::UpdateCharaInfo(AOrionChara* InChara)
{
    if (!InChara) return;

    CharaRef = InChara;

    if (TextCharaName)
    {
        TextCharaName->SetText(FText::FromString(InChara->GetName()));
    }

	if (TextCurrHealth)
	{
		TextCurrHealth->SetText(FText::FromString(FString::Printf(TEXT("Current Health: %f"), InChara->CurrHealth)));
	}

    if (SliderSpeed)
    {
        SliderSpeed->SetMinValue(200.f);
        SliderSpeed->SetMaxValue(1000.f);
    }

	if (TextActionQueue)
	{
		FString ActionQueueContent;
		for (const auto& Action : InChara->CharacterActionQueue.Actions)
		{
			ActionQueueContent += Action.Name + TEXT(" | ");
		}

		if (ActionQueueContent.Len() > 0)
		{
			ActionQueueContent.RemoveAt(ActionQueueContent.Len() - 3, 3); // Remove the last " | "
		}
		else
		{
			ActionQueueContent = TEXT("Chara Has No Awaiting Actions. ");
		}

		TextActionQueue->SetText(FText::FromString(ActionQueueContent));
	}
}

void UOrionUserWidgetCharaInfo::OnSliderChange(float InValue)
{
	UE_LOG(LogTemp, Log, TEXT("Slider Value Changed: %f"), InValue);
	if (CharaRef) CharaRef->ChangeMaxWalkSpeed(InValue);
}