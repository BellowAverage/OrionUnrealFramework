#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionGlobals/OrionDataItem.h"
#include "OrionInventoryComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ORION_API UOrionInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOrionInventoryComponent();

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	/* Full Status */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TMap<int32, bool> FullInventoryStatus;

	/* Mapping available item type to store and their max storage */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	TMap<int32, int32> AvailableInventoryMap;

	/** Actual storage quantity mapping: <ItemId, Quantity> */
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TMap<int32, int32> InventoryMap;

	/** Broadcast when any item quantity in inventory changes */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void OnInventoryChange();

	/** Add item */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ModifyItemQuantity(int32 ItemId, int32 DeltaQuantity);

	void SpawnFloatingText(const FString& InText);

	/** Get current quantity of a specific item */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetItemQuantity(int32 ItemId) const;

	/** Clear all inventory */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearInventory();

	/** Return (ItemId, Quantity) array */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<FIntPoint> GetAllItems() const;

	/** Query static item information by ItemId, return nullptr if not found */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FOrionDataItem GetItemInfo(int32 ItemId) const;

	/** Return all preset item information list */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FOrionDataItem>& GetAllItemInfos() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshInventoryText();


	virtual void BeginPlay() override;

	static TArray<FOrionDataItem> ItemInfoTable;
};
