#include "OrionInventoryComponent.h"
#include "Components/TextRenderComponent.h"

TArray<FOrionDataItem> UOrionInventoryComponent::ItemInfoTable = {
	{1, FName("Log"), FText::FromString("Log"), FText::FromString(TEXT("原木")), 1.f, 30.f},
	{2, FName("StoneOre"), FText::FromString("Stone Ore"), FText::FromString(TEXT("石矿")), 2.f, 30.f},
	{3, FName("Bullet"), FText::FromString("Bullet"), FText::FromString(TEXT("子弹")), 2.f, 30.f},
};

UOrionInventoryComponent::UOrionInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UOrionInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && (GetOwner()->GetName().Contains("OrionChara") || GetOwner()->GetName().Contains("OrionCChara")))
	{
		AvailableInventoryMap = {
			{1, 10}, // Log
			{2, 10}, // Stone Ore
			{3, 300}, // Stone Ore
		};
	}
}

void UOrionInventoryComponent::RefreshInventoryText()
{
	// 找到带有特定 Tag 的 TextRenderComponent
	// 这样即使 Actor 上有多个 TextRenderComponent，也只更新我们想要的那个

	TArray<UTextRenderComponent*> Comps;
	GetOwner()->GetComponents<UTextRenderComponent>(Comps);

	for (UTextRenderComponent* TextComp : Comps)
	{
		if (!TextComp->ComponentHasTag(TEXT("InventoryText")))
		{
			continue;
		}

		// 拼接库存字符串
		FString Out;
		for (auto& Pair : InventoryMap)
		{
			int32 ItemId = Pair.Key;
			int32 Quantity = Pair.Value;

			FString Name;
			switch (ItemId)
			{
			case 1: Name = TEXT("Log");
				break;
			case 2: Name = TEXT("StoneOre");
				break;
			case 3: Name = TEXT("Bullet");
				break;
			default: Name = FString::Printf(TEXT("Item%d"), ItemId);
				break;
			}

			Out += FString::Printf(TEXT("%s:%d  "), *Name, Quantity);
		}

		if (Out.IsEmpty())
		{
			Out = TEXT("Empty");
		}

		TextComp->SetText(FText::FromString(Out));

		return;
	}
}

bool UOrionInventoryComponent::ModifyItemQuantity(int32 ItemId, int32 Quantity)
{
	if (Quantity == 0)
	{
		return true;
	}

	// 1) 检查是否允许存/取，以及最大容量
	int32* MaxAllowedPtr = AvailableInventoryMap.Find(ItemId);
	if (!MaxAllowedPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemId %d not allowed here"), ItemId);
		return false;
	}
	int32 MaxAllowed = *MaxAllowedPtr;

	// 2) 计算新数量并检查范围
	int32& CurrQ = InventoryMap.FindOrAdd(ItemId);
	int32 NewQ = CurrQ + Quantity;
	if (NewQ < 0 || NewQ > MaxAllowed)
	{
		UE_LOG(LogTemp, Warning, TEXT("ModifyItemQuantity out of range for ItemId %d (new=%d)"), ItemId, NewQ);
		return false;
	}

	// 3) 应用变更
	CurrQ = NewQ;
	if (CurrQ == 0)
	{
		InventoryMap.Remove(ItemId);
	}

	FOrionDataItem Info = GetItemInfo(ItemId);
	FString Name = Info.DisplayName.ToString();
	FString Prefix = (Quantity > 0 ? TEXT("+") : TEXT("-"));
	FString Text = FString::Printf(TEXT("%s%d %s"), *Prefix, FMath::Abs(Quantity), *Name);

	TArray<UTextRenderComponent*> Comps;
	if (AActor* Owner = GetOwner())
	{
		// 1) 先拿到所有带 tag 的 UActorComponent*
		auto Raw = Owner->GetComponentsByTag(
			UTextRenderComponent::StaticClass(),
			FName(TEXT("OrionInventoryChangeTextComp"))
		);

		// 2) 再把它们 cast 到 UTextRenderComponent*
		TArray<UTextRenderComponent*> InventoryChangeTextComp;
		for (UActorComponent* C : Raw)
		{
			if (auto* TR = Cast<UTextRenderComponent>(C))
			{
				InventoryChangeTextComp.Add(TR);
			}
		}

		if (InventoryChangeTextComp.Num() > 0)
		{
			UTextRenderComponent* ChangeComp = InventoryChangeTextComp[0];
			ChangeComp->SetText(FText::FromString(Text));
			ChangeComp->SetHiddenInGame(false);

			// 延迟隐藏
			float Duration = 1.0f;
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle, [ChangeComp]()
			{
				ChangeComp->SetHiddenInGame(true);
			}, Duration, false);
		}
	}

	// 触发外部事件
	OnInventoryChange();

	return true;
}

void UOrionInventoryComponent::SpawnFloatingText(const FString& InText)
{
	if (AActor* Owner = GetOwner())
	{
		// 动态创建一个 TextRenderComponent
		UTextRenderComponent* TR = NewObject<UTextRenderComponent>(Owner);
		TR->RegisterComponent();
		TR->SetText(FText::FromString(InText));
		TR->SetHorizontalAlignment(EHTA_Center);
		TR->SetWorldSize(50.f);
		FVector Pos = Owner->GetActorLocation() + FVector(0, 0, 200);
		TR->SetWorldLocation(Pos);

		// 1 秒后自动销毁
		FTimerHandle Handle;
		Owner->GetWorldTimerManager().SetTimer(Handle, [TR]()
		{
			if (TR->IsValidLowLevel())
			{
				TR->DestroyComponent();
			}
		}, 1.0f, false);
	}
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

FOrionDataItem UOrionInventoryComponent::GetItemInfo(int32 ItemId) const
{
	for (const auto& Info : ItemInfoTable)
	{
		if (Info.ItemId == ItemId)
		{
			return Info;
		}
	}

	return FOrionDataItem{};
}

const TArray<FOrionDataItem>& UOrionInventoryComponent::GetAllItemInfos() const
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

	RefreshInventoryText();

	OnInventoryChanged.Broadcast();
}
