#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OrionInventoryComponent.generated.h"


USTRUCT(BlueprintType)
struct FOrionItemInfo
{
	GENERATED_BODY()

	/** 唯一 ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 ItemId = 0;

	/** 内部名称 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName Name;

	/** 英文显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** 中文显示名 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText ChineseDisplayName;

	/** 标准价格 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float PriceSTD = 0.f;

	/** 标准生产/处理时间（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float ProductionTimeCostSTD = 0.f;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ORION_API UOrionInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOrionInventoryComponent();

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
	FOrionItemInfo GetItemInfo(int32 ItemId) const;

	/** 返回所有预设的物品信息列表 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FOrionItemInfo>& GetAllItemInfos() const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RefreshInventoryText();

protected:
	virtual void BeginPlay() override;

private:
	/** 静态全局物品信息表 */
	static TArray<FOrionItemInfo> ItemInfoTable;
};
