// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Orion/OrionChara/OrionChara.h"
#include "OrionUserWidgetProceduralAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnProcActionSelected, int32, ItemIndex
);


UCLASS()
class ORION_API UOrionUserWidgetProceduralAction : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* TextActionName = nullptr;

	// Save reference to owner character
	UPROPERTY(BlueprintReadOnly)
	AOrionChara* OwnerChara = nullptr;

	UFUNCTION(BlueprintCallable)
	void SetupActionItem(const FString& InName, int32 InIndex, AOrionChara* InChara)
	{
		if (TextActionName)
		{
			TextActionName->SetText(FText::FromString(InName));
		}
		ItemIndex = InIndex;
		OwnerChara = InChara;
	}

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override
	{
		Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
		UpdateValidityTooltip();
	}

	void UpdateValidityTooltip()
	{
		if (!OwnerChara || ItemIndex == INDEX_NONE)
		{
			SetToolTipText(FText::GetEmpty());
			return;
		}
		// Call C++ interface to get extended status (Executing, Blocked, or Condition Not Met)
		FString StatusStr = OwnerChara->GetActionStatusString(ItemIndex, true);
		SetToolTipText(FText::FromString(StatusStr));
	}


	UPROPERTY(meta = (BindWidget))
	UButton* ButtonSelect = nullptr;

	UPROPERTY(BlueprintAssignable, Category = "Procedural")
	FOnProcActionSelected OnActionSelected;


	virtual void NativeConstruct() override
	{
		Super::NativeConstruct();

		TextActionName->SetAutoWrapText(true);
		TextActionName->SetWrapTextAt(210.0f);

		if (ButtonSelect)
		{
			ButtonSelect->OnClicked.AddDynamic(this, &UOrionUserWidgetProceduralAction::HandleClick);
			ButtonSelect->SetIsEnabled(true);
			ButtonSelect->SetVisibility(ESlateVisibility::Visible);
		}

		SetIsEnabled(true);
		SetIsFocusable(true);
	}

	void SetSelected(bool bSelected)
	{
		if (ButtonSelect)
		{
			FButtonStyle Style = ButtonSelect->GetStyle();
			Style.Normal.TintColor =
				bSelected
					? FLinearColor(0, 0.5f, 1, 0.8f)
					: FLinearColor::Transparent;
			ButtonSelect->SetStyle(Style);
		}
	}

private:
	int32 ItemIndex = INDEX_NONE;
	friend class UOrionUserWidgetCharaInfo;

	UFUNCTION()
	void HandleClick()
	{
		OnActionSelected.Broadcast(ItemIndex);
	}
};
