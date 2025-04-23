// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionUserWidgetCharaInfo.h"

void UOrionUserWidgetCharaInfo::NativeConstruct()
{
	Super::NativeConstruct();
	if (SliderSpeed)
	{
		SliderSpeed->OnValueChanged.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnSliderChange);
	}

	if (CheckDefensive)
	{
		CheckDefensive->OnCheckStateChanged.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnDefensiveChanged);
	}

	if (CheckAggressive)
	{
		CheckAggressive->OnCheckStateChanged.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnAggressiveChanged);
	}

	if (CheckUnavailable)
	{
		CheckUnavailable->OnCheckStateChanged.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnUnavailableChanged);
	}
}

void UOrionUserWidgetCharaInfo::InitCharaInfoPanelParams(AOrionChara* InChara)
{
	if (!InChara)
	{
		return;
	}

	CharaRef = InChara;
	bSuppressCheckEvents = true;
	if (CheckDefensive)
	{
		CheckDefensive->SetIsChecked(InChara->CharaAIState == EAIState::Defensive);
	}
	if (CheckAggressive)
	{
		CheckAggressive->SetIsChecked(InChara->CharaAIState == EAIState::Aggressive);
	}
	if (CheckUnavailable)
	{
		CheckUnavailable->SetIsChecked(InChara->CharaAIState == EAIState::Unavailable);
	}
	bSuppressCheckEvents = false;

	if (SliderSpeed)
	{
		SliderSpeed->SetValue(InChara->OrionCharaSpeed);
	}
}

void UOrionUserWidgetCharaInfo::UpdateCharaInfo(AOrionChara* InChara)
{
	if (!InChara)
	{
		return;
	}

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
	if (CharaRef)
	{
		CharaRef->ChangeMaxWalkSpeed(InValue);
	}
}

void UOrionUserWidgetCharaInfo::OnDefensiveChanged(bool bIsChecked)
{
	if (bSuppressCheckEvents)
	{
		return;
	}

	bSuppressCheckEvents = true;
	if (CheckAggressive)
	{
		CheckAggressive->SetIsChecked(false);
	}
	if (CheckUnavailable)
	{
		CheckUnavailable->SetIsChecked(false);
	}
	bSuppressCheckEvents = false;

	if (bIsChecked && CharaRef)
	{
		CharaRef->CharaAIState = EAIState::Defensive;
	}
}

void UOrionUserWidgetCharaInfo::OnAggressiveChanged(bool bIsChecked)
{
	if (bSuppressCheckEvents)
	{
		return;
	}

	bSuppressCheckEvents = true;
	if (CheckDefensive)
	{
		CheckDefensive->SetIsChecked(false);
	}
	if (CheckUnavailable)
	{
		CheckUnavailable->SetIsChecked(false);
	}
	bSuppressCheckEvents = false;

	if (bIsChecked && CharaRef)
	{
		CharaRef->CharaAIState = EAIState::Aggressive;
	}
}

void UOrionUserWidgetCharaInfo::OnUnavailableChanged(bool bIsChecked)
{
	if (bSuppressCheckEvents)
	{
		return;
	}

	bSuppressCheckEvents = true;
	if (CheckDefensive)
	{
		CheckDefensive->SetIsChecked(false);
	}
	if (CheckAggressive)
	{
		CheckAggressive->SetIsChecked(false);
	}
	bSuppressCheckEvents = false;

	if (bIsChecked && CharaRef)
	{
		CharaRef->CharaAIState = EAIState::Unavailable;
	}
}
