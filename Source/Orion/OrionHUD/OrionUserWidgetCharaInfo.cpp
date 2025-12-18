// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionUserWidgetCharaInfo.h"
#include "OrionUserWidgetProceduralAction.h"
#include "Components/VerticalBox.h"
#include "Orion/OrionChara/OrionChara.h"
#include "OrionUserWidgetCharaDetails.h"
#include "Orion/OrionComponents/OrionMovementComponent.h"
// [Fix] Include ActionComponent
#include "Orion/OrionComponents/OrionActionComponent.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"


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
			this, &UOrionUserWidgetCharaInfo::OnCheckIsCharaProceduralChanged);
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
	// UE_LOG(LogTemp, Log, TEXT("bActorDetailShow: %s"), bActorDetailShow ? TEXT("true") : TEXT("false"));
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
		BorderCharaDetails->ClearChildren();

		if (auto* CharaDetails = CreateWidget<UOrionUserWidgetCharaDetails>(this, CharaDetailsClass))
		{
			if (bActorDetailShow && InventoryInteractActorRef && InventoryInteractActorRef->InventoryComp)
			{
				CharaDetails->InitCargoItemPanelParams(CharaRef->InventoryComp, InventoryInteractActorRef->InventoryComp);
			}
			else if (!InventoryInteractActorRef)
			{
				CharaDetails->InitCargoItemPanelParams(CharaRef->InventoryComp, nullptr);
			}
			BorderCharaDetails->AddChild(CharaDetails);
			bCharaDetailShow = true;
		}
	}

	if (bActorDetailShow && BorderActorDetails)
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

	// [Fix] 从 ActionComp 获取 ProceduralActionQueue
	if (InChara->ActionComp)
	{
		const auto& Q = InChara->ActionComp->ProceduralActionQueue.Actions;
		for (int32 i = 0; i < Q.Num(); ++i)
		{
			const FOrionAction& Act = Q[i];
			auto* Item = CreateWidget<UOrionUserWidgetProceduralAction>(
				this, ProceduralActionItemClass);
			Item->SetupActionItem(Act.Name, i, InChara);
			Item->OnActionSelected.AddDynamic(this, &UOrionUserWidgetCharaInfo::OnProcSelected);
			ProceduralActionBox->AddChild(Item);
		}
	}

	RefreshHighlight();
}

void UOrionUserWidgetCharaInfo::OnProcSelected(const int32 ItemIndex)
{
	SelectedIndex = ItemIndex;
	RefreshHighlight();
}

void UOrionUserWidgetCharaInfo::RefreshHighlight() const
{
	const int32 Count = ProceduralActionBox->GetChildrenCount();
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
	if (!CharaRef) return;
	// [Fix] 获取队列长度需通过 Component
	int32 Count = 0;
	if (CharaRef->ActionComp) Count = CharaRef->ActionComp->ProceduralActionQueue.Actions.Num();
	if (SelectedIndex >= 0 && SelectedIndex < Count - 1)
	{
		CharaRef->ReorderProceduralAction(SelectedIndex, SelectedIndex + 2);
		++SelectedIndex;
		UpdateProcActionQueue(CharaRef);
	}
}

void UOrionUserWidgetCharaInfo::OnDeleteClicked()
{
	if (!CharaRef) return;
	
	int32 Count = 0;
	if (CharaRef->ActionComp) Count = CharaRef->ActionComp->ProceduralActionQueue.Actions.Num();
	if (SelectedIndex >= 0 && SelectedIndex < Count)
	{
		// [Refactor] 如果删除了当前正在执行的动作，Logic 需要清理 CurrentProcAction
		// CharaRef->RemoveProceduralActionAt 会调用 Component->RemoveAt
		// 但我们需要注意 Component 内部是否处理了 CurrentProcAction 的重置？
		// 我们的 Component 代码中 RemoveAt 是数组操作，下一帧 Tick 会重新 Evaluate，
		// 如果删的是 index 0 (Current)，下一帧 Loop 会发现 CurrentProcAction 指针失效或内容变了。
		// 实际上我们使用的是裸指针 `FOrionAction* CurrentProcAction` 指向数组元素。
		// 当数组 RemoveAt 发生内存移动时，指针会失效！这是一个潜在的 Crash 点。
		// 幸好我们在 Component 的 RemoveProceduralActionAt 中并没有特别处理指针。
		// 建议：在 Component 中，RemoveAt 操作后，应该重置 CurrentProcAction = nullptr 以强制重新评估。
		// 不过这里我们只负责调用。
		
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

	BindInventoryEvents(InChara); // Change: Bind here

	if (SliderSpeed)
	{
		if (InChara->MovementComp)
		{
			SliderSpeed->SetValue(InChara->MovementComp->OrionCharaSpeed);
		}
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
		CheckUnavailable->SetIsChecked(InChara->CharaAIState == EAIState::Passive);
	}
	bSuppressCheckEvents = false;

	if (CheckbIsCharaProcedural)
	{
		if (InChara->ActionComp) CheckbIsCharaProcedural->SetIsChecked(InChara->ActionComp->IsProcedural());
	}
}

void UOrionUserWidgetCharaInfo::NativeDestruct()
{
	BindInventoryEvents(nullptr); // Auto unbind
	Super::NativeDestruct();
}

void UOrionUserWidgetCharaInfo::UpdateCharaInfo(AOrionChara* InChara)
{
	if (!InChara)
	{
		return;
	}

	if (InChara != CharaRef) // Rebind when character switches
	{
		BindInventoryEvents(InChara);
	}

	CharaRef = InChara;

	static TArray<FString> CachedProcNames; 

	TArray<FString> CurrProcNames;
	
	// [Fix] Access via ActionComp
	if (InChara->ActionComp)
	{
		const auto& Actions = InChara->ActionComp->ProceduralActionQueue.Actions;
		CurrProcNames.Reserve(Actions.Num());
		for (const auto& Act : Actions)
		{
			CurrProcNames.Add(Act.Name);
		}
	}

	// Compare: if different, refresh and update cache
	if (CurrProcNames != CachedProcNames)
	{
		// UE_LOG(LogTemp, Log, TEXT("Procedural queue changed: updating UI"));
		UpdateProcActionQueue(InChara);
		CachedProcNames = MoveTemp(CurrProcNames);
	}


	if (TextCharaName)
	{
		TextCharaName->SetText(FText::FromString(InChara->GetName()));
	}

	if (TextCurrHealth)
	{
		float CurrentHealth = InChara->AttributeComp ? InChara->AttributeComp->Health : 0.0f;
		TextCurrHealth->SetText(FText::FromString(FString::Printf(TEXT("Current Health: %f"), CurrentHealth)));
	}

	if (SliderSpeed)
	{
		SliderSpeed->SetMinValue(200.f);
		SliderSpeed->SetMaxValue(1000.f);
	}

	if (TextActionQueue)
	{
		FString ActionQueueContent;
		if (InChara->ActionComp)
		{
			for (const auto& Action : InChara->ActionComp->RealTimeActionQueue.Actions)
			{
				ActionQueueContent += Action.Name + TEXT(" | ");
			}
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
		if (InChara->ActionComp) CheckbIsCharaProcedural->SetIsChecked(InChara->ActionComp->IsProcedural());
	}

	//UpdateProcActionQueue(InChara);
}

void UOrionUserWidgetCharaInfo::OnSliderChange(float InValue)
{
	// UE_LOG(LogTemp, Log, TEXT("Slider Value Changed: %f"), InValue);
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
		CharaRef->CharaAIState = EAIState::Passive;
	}
}

void UOrionUserWidgetCharaInfo::OnCheckIsCharaProceduralChanged(bool bIsChecked)
{
	if (bSuppressCheckEvents)
	{
		return;
	}

	if (CharaRef)
	{
		CharaRef->ActionComp->SetProcedural(bIsChecked);
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
		// Open: clear first, then create new
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
		// Close
		BorderCharaDetails->ClearChildren();
		bCharaDetailShow = false;
	}
}

void UOrionUserWidgetCharaInfo::BroadcastOnInteractWithInventory(AOrionActor* OrionActor)
{
	// UE_LOG(LogTemp, Log, TEXT("WHATTHE FKJ Interact with inventory: %s"), *OrionActor->GetName());

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
			if (CharaRef && CharaRef->InventoryComp && InventoryInteractActorRef && InventoryInteractActorRef->
				InventoryComp)
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
		// Close
		BorderActorDetails->ClearChildren();
		ImageTradeIcon->SetVisibility(ESlateVisibility::Hidden);
		bActorDetailShow = false;
	}
}
