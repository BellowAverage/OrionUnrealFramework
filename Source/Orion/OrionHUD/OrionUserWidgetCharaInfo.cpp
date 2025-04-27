// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionUserWidgetCharaInfo.h"
#include "OrionUserWidgetProceduralAction.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/VerticalBox.h"
#include "../OrionChara.h"


void UOrionUserWidgetCharaInfo::NativeConstruct()
{
	Super::NativeConstruct();

	if (CharaRef)
	{
		UpdateProcActionQueue(CharaRef);
	}

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

	if (CheckbIsCharaProcedural)
	{
		CheckbIsCharaProcedural->OnCheckStateChanged.AddDynamic(
			this, &UOrionUserWidgetCharaInfo::OnCheckbIsCharaProceduralChanged);
	}


	if (ButtonMoveUp)
	{
		ButtonMoveUp->OnClicked.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnMoveUpClicked);
	}

	if (ButtonMoveDown)
	{
		ButtonMoveDown->OnClicked.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnMoveDownClicked);
	}

	if (ButtonDelete)
	{
		ButtonDelete->OnClicked.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnDeleteClicked);
	}
}

void UOrionUserWidgetCharaInfo::UpdateProcActionQueue(AOrionChara* InChara)
{
	if (!ProceduralActionBox || !InChara || !ProceduralActionItemClass)
	{
		return;
	}

	CharaRef = InChara;
	SelectedIndex = INDEX_NONE;
	ProceduralActionBox->ClearChildren();

	const auto& Q = InChara->CharacterProcActionQueue.Actions;
	for (int32 i = 0; i < Q.size(); ++i)
	{
		const Action& Act = Q[i];
		auto* Item = CreateWidget<UOrionUserWidgetProceduralAction>(
			this, ProceduralActionItemClass);
		Item->SetupActionItem(Act.Name, i);
		Item->OnActionSelected.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnProcSelected);
		ProceduralActionBox->AddChild(Item);
	}

	RefreshHighlight();
}

void UOrionUserWidgetCharaInfo::OnProcSelected(int32 ItemIndex)
{
	SelectedIndex = ItemIndex;
	RefreshHighlight();
}

void UOrionUserWidgetCharaInfo::RefreshHighlight()
{
	int32 Count = ProceduralActionBox->GetChildrenCount();
	for (int32 i = 0; i < Count; ++i)
	{
		if (auto* W = Cast<UOrionUserWidgetProceduralAction>(
			ProceduralActionBox->GetChildAt(i)))
		{
			W->SetSelected(i == SelectedIndex);
		}
	}
}

void UOrionUserWidgetCharaInfo::OnMoveUpClicked()
{
	if (!CharaRef)
	{
		return;
	}
	if (SelectedIndex > 0)
	{
		CharaRef->ReorderProceduralAction(SelectedIndex, SelectedIndex - 1);
		--SelectedIndex;
		UpdateProcActionQueue(CharaRef);
	}
}

void UOrionUserWidgetCharaInfo::OnMoveDownClicked()
{
	if (!CharaRef)
	{
		return;
	}
	int32 Count = CharaRef->CharacterProcActionQueue.Actions.size();
	if (SelectedIndex >= 0 && SelectedIndex < Count - 1)
	{
		CharaRef->ReorderProceduralAction(SelectedIndex, SelectedIndex + 2);
		// 注意 Reorder proceduralAction(OldIdx, DropIdx) 的 DropIdx 逻辑
		++SelectedIndex;
		UpdateProcActionQueue(CharaRef);
	}
}

void UOrionUserWidgetCharaInfo::OnDeleteClicked()
{
	if (!CharaRef)
	{
		return;
	}
	if (SelectedIndex >= 0 &&
		SelectedIndex < CharaRef->CharacterProcActionQueue.Actions.size())
	{
		CharaRef->RemoveProceduralActionAt(SelectedIndex);
		UpdateProcActionQueue(CharaRef);
	}
}

void UOrionUserWidgetCharaInfo::InitCharaInfoPanelParams(AOrionChara* InChara)
{
	if (!InChara)
	{
		return;
	}

	if (SliderSpeed)
	{
		SliderSpeed->SetValue(InChara->OrionCharaSpeed);
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

	if (CheckbIsCharaProcedural)
	{
		CheckbIsCharaProcedural->SetIsChecked(InChara->bIsCharaProcedural);
	}
}

void UOrionUserWidgetCharaInfo::UpdateCharaInfo(AOrionChara* InChara)
{
	if (!InChara)
	{
		return;
	}

	CharaRef = InChara;

	static TArray<FString> CachedProcNames; // 上一次的名称列表缓存

	// 构造当前的名称列表
	TArray<FString> CurrProcNames;
	CurrProcNames.Reserve(InChara->CharacterProcActionQueue.Actions.size());
	for (const auto& Act : InChara->CharacterProcActionQueue.Actions)
	{
		CurrProcNames.Add(Act.Name);
	}

	// 对比：如果不一样，刷新并更新缓存
	if (CurrProcNames != CachedProcNames)
	{
		UE_LOG(LogTemp, Log, TEXT("Procedural queue changed: updating UI"));
		UpdateProcActionQueue(InChara);
		CachedProcNames = MoveTemp(CurrProcNames);
	}


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

	if (CheckbIsCharaProcedural)
	{
		CheckbIsCharaProcedural->SetIsChecked(InChara->bIsCharaProcedural);
	}

	//UpdateProcActionQueue(InChara);
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

void UOrionUserWidgetCharaInfo::OnCheckbIsCharaProceduralChanged(bool bIsChecked)
{
	if (bSuppressCheckEvents)
	{
		return;
	}

	if (CharaRef)
	{
		CharaRef->bIsCharaProcedural = bIsChecked;
	}
}
