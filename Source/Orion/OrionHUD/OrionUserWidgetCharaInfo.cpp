// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionUserWidgetCharaInfo.h"
#include "OrionUserWidgetProceduralAction.h"
#include "Components/VerticalBox.h"
#include "../OrionChara.h"
#include "OrionUserWidgetCharaDetails.h"


void UOrionUserWidgetCharaInfo::NativeConstruct()
{
	Super::NativeConstruct();

	if (ImageTradeIcon)
	{
		ImageTradeIcon->SetVisibility(ESlateVisibility::Hidden);
	}

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

	if (ButtonBag)
	{
		ButtonBag->OnClicked.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnBagClicked);
	}
}

void UOrionUserWidgetCharaInfo::BindInventoryEvents(AOrionChara* NewChara)
{
	if (CharaRef && CharaRef->InventoryComp)
	{
		CharaRef->InventoryComp->OnInventoryChanged.RemoveDynamic(
			this, &UOrionUserWidgetCharaInfo::BroadcastInventoryChanged);

		CharaRef->OnCharaActionChange.RemoveDynamic(
			this, &UOrionUserWidgetCharaInfo::BroadcastCharaActionChange);
	}

	CharaRef = NewChara;

	if (CharaRef && CharaRef->InventoryComp)
	{
		CharaRef->InventoryComp->OnInventoryChanged.AddDynamic(
			this, &UOrionUserWidgetCharaInfo::BroadcastInventoryChanged);


		CharaRef->OnInteractWithInventory.BindUObject(
			this, &UOrionUserWidgetCharaInfo::BroadcastOnInteractWithInventory);

		CharaRef->OnCharaActionChange.AddDynamic(
			this, &UOrionUserWidgetCharaInfo::BroadcastCharaActionChange);
	}

	if (!CharaRef || !CharaDetailsClass || !BorderCharaDetails)
	{
		return;
	}

	if (bCharaDetailShow)
	{
		BorderCharaDetails->ClearChildren();

		if (auto* W = CreateWidget<UOrionUserWidgetCharaDetails>(this, CharaDetailsClass))
		{
			if (bActorDetailShow && InventoryInteractActorRef && InventoryInteractActorRef->InventoryComp)
			{
				W->InitCargoItemPanelParams(CharaRef->InventoryComp, InventoryInteractActorRef->InventoryComp);
			}
			else if (!InventoryInteractActorRef)
			{
				W->InitCargoItemPanelParams(CharaRef->InventoryComp, nullptr);
			}
			BorderCharaDetails->AddChild(W);
			bCharaDetailShow = true;
		}
	}
}

void UOrionUserWidgetCharaInfo::BroadcastCharaActionChange(FString PrevActionName, FString CurrActionName)
{
	UE_LOG(LogTemp, Log, TEXT("bActorDetailShow: %s"), bActorDetailShow ? TEXT("true") : TEXT("false"));
	if (CharaRef)
	{
		if (bActorDetailShow && PrevActionName.IsEmpty() && !CurrActionName.IsEmpty())
		{
			BorderActorDetails->ClearChildren();
			ImageTradeIcon->SetVisibility(ESlateVisibility::Hidden);
			InventoryInteractActorRef = nullptr;
			bActorDetailShow = false;
		}
	}
}


void UOrionUserWidgetCharaInfo::BroadcastInventoryChanged()
{
	if (CharaRef && bCharaDetailShow && CharaDetailsClass && BorderCharaDetails)
	{
		UE_LOG(LogTemp, Log, TEXT("CN1M"));
		BorderCharaDetails->ClearChildren();

		if (auto* W = CreateWidget<UOrionUserWidgetCharaDetails>(this, CharaDetailsClass))
		{
			if (bActorDetailShow && InventoryInteractActorRef && InventoryInteractActorRef->InventoryComp)
			{
				W->InitCargoItemPanelParams(CharaRef->InventoryComp, InventoryInteractActorRef->InventoryComp);
			}
			else if (!InventoryInteractActorRef)
			{
				W->InitCargoItemPanelParams(CharaRef->InventoryComp, nullptr);
			}
			BorderCharaDetails->AddChild(W);
			bCharaDetailShow = true;
		}
	}

	if (bActorDetailShow && BorderActorDetails)
	{
		UE_LOG(LogTemp, Log, TEXT("CNM"));

		BorderActorDetails->ClearChildren();
		ImageTradeIcon->SetVisibility(ESlateVisibility::Hidden);

		if (auto* W = CreateWidget<UOrionUserWidgetCharaDetails>(this, CharaDetailsClass))
		{
			if (CharaRef && CharaRef->InventoryComp)
			{
				W->InitCargoItemPanelParams(InventoryInteractActorRef->InventoryComp, CharaRef->InventoryComp);
			}
			BorderActorDetails->AddChild(W);
			ImageTradeIcon->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void UOrionUserWidgetCharaInfo::UpdateProcActionQueue(AOrionChara* InChara)
{
	if (!ProceduralActionBox || !InChara || !ProceduralActionItemClass || !CharaDetailsClass)
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

	BindInventoryEvents(InChara); // ★ 改动：在这里做绑定

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

void UOrionUserWidgetCharaInfo::NativeDestruct()
{
	BindInventoryEvents(nullptr); // 自动解绑
	Super::NativeDestruct();
}

void UOrionUserWidgetCharaInfo::UpdateCharaInfo(AOrionChara* InChara)
{
	if (!InChara)
	{
		return;
	}

	if (InChara != CharaRef) // 角色切换时也要重新绑定
	{
		BindInventoryEvents(InChara);
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

void UOrionUserWidgetCharaInfo::OnBagClicked()
{
	if (!CharaRef || !CharaDetailsClass || !BorderCharaDetails)
	{
		return;
	}

	if (!bCharaDetailShow)
	{
		// 打开：先清空，再新建
		BorderCharaDetails->ClearChildren();

		if (auto* W = CreateWidget<UOrionUserWidgetCharaDetails>(this, CharaDetailsClass))
		{
			if (bActorDetailShow && InventoryInteractActorRef && InventoryInteractActorRef->InventoryComp)
			{
				W->InitCargoItemPanelParams(CharaRef->InventoryComp, InventoryInteractActorRef->InventoryComp);
			}
			else if (!InventoryInteractActorRef)
			{
				W->InitCargoItemPanelParams(CharaRef->InventoryComp, nullptr);
			}
			BorderCharaDetails->AddChild(W);
			bCharaDetailShow = true;
		}
	}
	else
	{
		// 关闭
		BorderCharaDetails->ClearChildren();
		bCharaDetailShow = false;
	}
}

void UOrionUserWidgetCharaInfo::BroadcastOnInteractWithInventory(AOrionActor* OrionActor)
{
	UE_LOG(LogTemp, Log, TEXT("WHATTHE FKJ Interact with inventory: %s"), *OrionActor->GetName());

	if (!CharaRef || !CharaDetailsClass || !BorderActorDetails)
	{
		return;
	}

	if (OrionActor && OrionActor->InventoryComp)
	{
		InventoryInteractActorRef = OrionActor;

		OrionActor->InventoryComp->OnInventoryChanged.RemoveDynamic(
			this, &UOrionUserWidgetCharaInfo::BroadcastInventoryChanged);
		OrionActor->InventoryComp->OnInventoryChanged.AddDynamic(
			this, &UOrionUserWidgetCharaInfo::BroadcastInventoryChanged);
	}

	if (!bActorDetailShow)
	{
		BorderActorDetails->ClearChildren();
		ImageTradeIcon->SetVisibility(ESlateVisibility::Hidden);
		if (auto* W = CreateWidget<UOrionUserWidgetCharaDetails>(this, CharaDetailsClass))
		{
			if (CharaRef && CharaRef->InventoryComp)
			{
				W->InitCargoItemPanelParams(InventoryInteractActorRef->InventoryComp, CharaRef->InventoryComp);
			}
			BorderActorDetails->AddChild(W);
			ImageTradeIcon->SetVisibility(ESlateVisibility::Visible);

			if (!bCharaDetailShow)
			{
				BorderCharaDetails->ClearChildren();

				if (auto* W2 = CreateWidget<UOrionUserWidgetCharaDetails>(this, CharaDetailsClass))
				{
					if (bActorDetailShow && InventoryInteractActorRef && InventoryInteractActorRef->InventoryComp)
					{
						W2->InitCargoItemPanelParams(CharaRef->InventoryComp, InventoryInteractActorRef->InventoryComp);
					}
					else if (!InventoryInteractActorRef)
					{
						W2->InitCargoItemPanelParams(CharaRef->InventoryComp, nullptr);
					}
					BorderCharaDetails->AddChild(W2);
					bCharaDetailShow = true;
				}
			}

			bActorDetailShow = true;
		}
	}
	else
	{
		// 关闭
		BorderActorDetails->ClearChildren();
		ImageTradeIcon->SetVisibility(ESlateVisibility::Hidden);
		bActorDetailShow = false;
	}
}
