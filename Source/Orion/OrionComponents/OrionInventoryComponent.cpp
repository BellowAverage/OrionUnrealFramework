#include "OrionInventoryComponent.h"

TArray<FOrionItemInfo> UOrionInventoryComponent::ItemInfoTable = {
	{1, FName("Log"), FText::FromString("Log"), FText::FromString(TEXT("原木")), 1.f, 30.f},
	{2, FName("StoneOre"), FText::FromString("Stone Ore"), FText::FromString(TEXT("石矿")), 2.f, 30.f},
};

UOrionInventoryComponent::UOrionInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOrionInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UOrionInventoryComponent::ModifyItemQuantity(int32 ItemId, int32 DeltaQuantity)
{
	const int32* StorageLimit = AvailableInventoryMap.Find(ItemId);

	if (!StorageLimit)
	{
		return false;
	}

	const int32 MaxCapacity = *StorageLimit;

	int32 CurrentQty = InventoryMap.FindRef(ItemId);
	int32 NewQty = CurrentQty + DeltaQuantity;

	if (NewQty < 0 || NewQty > MaxCapacity)
	{
		return false;
	}

	if (NewQty == 0)
	{
		InventoryMap.Remove(ItemId);
	}
	else
	{
		InventoryMap.Add(ItemId, NewQty);
	}

	OnInventoryChange();
	return true;
}

int32 UOrionInventoryComponent::GetItemQuantity(int32 ItemId) const
{
	const int32* QPtr = InventoryMap.Find(ItemId);
	return QPtr ? *QPtr : 0;
}

void UOrionInventoryComponent::ClearInventory()
{
	InventoryMap.Empty();
	OnInventoryChange();
}

TArray<FIntPoint> UOrionInventoryComponent::GetAllItems() const
{
	TArray<FIntPoint> Out;
	Out.Reserve(InventoryMap.Num());
	for (auto& Pair : InventoryMap)
	{
		// FIntPoint 用作 (X=ItemId, Y=Quantity)
		Out.Emplace(Pair.Key, Pair.Value);
	}
	return Out;
}

FOrionItemInfo UOrionInventoryComponent::GetItemInfo(int32 ItemId) const
{
	for (const auto& Info : ItemInfoTable)
	{
		if (Info.ItemId == ItemId)
		{
			return Info;
		}
	}

	return FOrionItemInfo{};
}

const TArray<FOrionItemInfo>& UOrionInventoryComponent::GetAllItemInfos() const
{
	return ItemInfoTable;
}

void UOrionInventoryComponent::OnInventoryChange()
{
	FullInventoryStatus.Empty();

	for (auto& Pair : AvailableInventoryMap)
	{
		int32 ItemId = Pair.Key;
		int32 MaxAmount = Pair.Value;

		int32 CurrQty = InventoryMap.FindRef(ItemId);

		FullInventoryStatus.Add(ItemId, CurrQty >= MaxAmount);
	}
}
