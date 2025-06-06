// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Orion/OrionGlobals/OrionDataItem.h"
#include "OrionUserWidgetCargoItem.generated.h"

/**
 * 
 */

DECLARE_DELEGATE(FOnCargoItemClicked);

UCLASS(Blueprintable)
class ORION_API UOrionUserWidgetCargoItem : public UUserWidget
{
	GENERATED_BODY()

public:
	FOnCargoItemClicked OnCargoItemClicked;

	/*TMap<int32, TSoftObjectPtr<UTexture2D>> ItemIDToTextureMap = {
		{
			1,
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/_Orion/UI/Images/GPTLogIcon.GPTLogIcon")))
		},
		{
			2,
			TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/_Orion/UI/Images/GPTStoneOreIcon.GPTStoneOreIcon")))
		},
		{3, TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/_Orion/UI/Images/GPTBullet.GPTBullet")))},
	};*/

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CargoName = nullptr;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CargoQuantity = nullptr;

	UPROPERTY(meta = (BindWidget))
	UImage* ImageCargo = nullptr;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override
	{
		UE_LOG(LogTemp, Warning, TEXT("CargoItem Clicked!"));

		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			return FReply::Handled();
		}
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			// 弹起时再执行原来的业务：触发委托，最终由 CharaDetails 做 Clear/Update
			OnCargoItemClicked.ExecuteIfBound();
			return FReply::Handled();
		}
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	UFUNCTION()
	void InitCargoItemPanelParams(UOrionInventoryComponent* InventoryComponent, int32 ItemId, const FText& DisplayName,
	                              int32 ItemQuantity)
	{
		if (CargoQuantity)
		{
			CargoQuantity->SetText(FText::AsNumber(ItemQuantity));
		}

		if (CargoName)
		{
			CargoName->SetText(DisplayName);
		}

		if (ImageCargo)
		{
			if (const TSoftObjectPtr<UTexture2D>* TexturePtr = ItemIDToTextureMap.Find(ItemId))
			{
				UTexture2D* Texture = TexturePtr->Get();
				if (!Texture)
				{
					// 同步加载
					Texture = TexturePtr->LoadSynchronous();
				}
				if (Texture)
				{
					ImageCargo->SetBrushFromTexture(Texture);
				}
			}
		}
	}
};
