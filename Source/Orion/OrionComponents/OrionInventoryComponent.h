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

	/** 真正存储数量映射：<ItemId, Quantity> */
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	TMap<int32, int32> InventoryMap;

	/** 当库存中任意物品数量变化时广播 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void OnInventoryChange();

	/** 添加物品 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool ModifyItemQuantity(int32 ItemId, int32 DeltaQuantity);

	void SpawnFloatingText(const FString& InText);

	/** 获取某物品的当前数量 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetItemQuantity(int32 ItemId) const;

	/** 清空所有库存 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearInventory();

	/** 返回 (ItemId, Quantity) 数组 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<FIntPoint> GetAllItems() const;

	/** 根据 ItemId 查询静态物品信息，找不到返回 nullptr */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FOrionDataItem GetItemInfo(int32 ItemId) const;

	/** 返回所有预设的物品信息列表 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FOrionDataItem>& GetAllItemInfos() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshInventoryText();


	virtual void BeginPlay() override;

	static TArray<FOrionDataItem> ItemInfoTable;
};
