// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionUserWidgetCargoItem.h"
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "OrionUserWidgetCharaDetails.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class ORION_API UOrionUserWidgetCharaDetails : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Basics")
	int32 CargoSwapMultiplier = 1;

	UFUNCTION()
	void InitCargoItemPanelParams(UOrionInventoryComponent* InventoryComponent,
	                              UOrionInventoryComponent* TargetInventoryComponent)
	{
		if (!InventoryComponent || !CargoItemClass || !VerticalBoxInventory)
		{
			return;
		}

		for (const auto& Pair : InventoryComponent->InventoryMap)
		{
			const int32 ItemId = Pair.Key;
			const int32 Quantity = Pair.Value;

			const FOrionItemInfo* Info = UOrionInventoryComponent::ItemInfoTable.FindByPredicate(
				[ItemId](const FOrionItemInfo& I) { return I.ItemId == ItemId; });

			const FText Name = Info
				                   ? Info->DisplayName
				                   : FText::FromString(TEXT("Unknown Item"));

			if (UOrionUserWidgetCargoItem* CargoItemWidget =
				CreateWidget<UOrionUserWidgetCargoItem>(this, CargoItemClass))
			{
				CargoItemWidget->OnCargoItemClicked.BindLambda(
					[this, ItemId, InventoryComponent, TargetInventoryComponent]()
					{
						if (!InventoryComponent || !TargetInventoryComponent)
						{
							UE_LOG(LogTemp, Warning, TEXT("InventoryComponent or TargetInventoryComponent is null!"));
							return;
						}

						UE_LOG(LogTemp, Warning,
						       TEXT(
							       "Cargo item clicked, ItemId = %d, InventoryComponent Owner = %s, TargetInventoryComponent Owner = %s"
						       ), ItemId, *InventoryComponent->GetOwner()->GetName(),
						       *TargetInventoryComponent->GetOwner()->GetName());


						if (InventoryComponent && TargetInventoryComponent)
						{
							InventoryComponent->ModifyItemQuantity(ItemId, -CargoSwapMultiplier);
							TargetInventoryComponent->ModifyItemQuantity(ItemId, CargoSwapMultiplier);
						}
					});

				CargoItemWidget->InitCargoItemPanelParams(InventoryComponent, ItemId, Name, Quantity);
				VerticalBoxInventory->AddChild(CargoItemWidget);
			}
		}
	}

	UPROPERTY(EditDefaultsOnly, Category = "CargoItem")
	TSubclassOf<UOrionUserWidgetCargoItem> CargoItemClass;

	UPROPERTY(meta=(BindWidget))
	UVerticalBox* VerticalBoxInventory = nullptr;
};
