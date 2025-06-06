#include "OrionInventoryComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Orion/OrionGlobals/OrionDataItem.h"

TArray<FOrionDataItem> UOrionInventoryComponent::ItemInfoTable = {
	{1, FName("Log"), FText::FromString("Log"), FText::FromString(TEXT("原木")), 1.f, 30.f},
	{2, FName("StoneOre"), FText::FromString("Stone Ore"), FText::FromString(TEXT("石矿")), 2.f, 30.f},
	{3, FName("Bullet"), FText::FromString("Bullet"), FText::FromString(TEXT("子弹")), 2.f, 30.f},
};

UOrionInventoryComponent::UOrionInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	/* 设定一个“安全默认值”，至少保证 Find 不会崩溃 */
	AvailableInventoryMap = {
		{1, 20}, // Log
		{2, 20}, // Stone Ore
		{3, 300}, // Bullet
		{4, 300}, // 预留
	};

	static const FString Path = TEXT("/Game/_Orion/UI/UI_ResourceFloat/WB_ResourceFloat.WB_ResourceFloat_C");
	FloatWidgetClass = LoadClass<UOrionUserWidgetResourceFloat>(nullptr, *Path);
	checkf(FloatWidgetClass, TEXT("Cannot load resource floating ui from %s"), *Path);
}

void UOrionInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	/*/* 默认上限 #1#
	AvailableInventoryMap = {
		{1, 20}, // Log
		{2, 20}, // Stone Ore
		{3, 300}, // Bullet
		{4, 300}, // 预留
	};

	/* 如果持有者是角色，再覆盖成角色专属容量 #1#
	const FString Name = GetOwner() ? GetOwner()->GetName() : TEXT("");
	if (Name.Contains(TEXT("OrionChara")))
	{
		AvailableInventoryMap = {
			{1, 10},
			{2, 10},
			{3, 300},
			{4, 300},
		};
	}*/
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

void UOrionInventoryComponent::SpawnResourceFloatUI(const int32 ItemId, const int32 Quantity) const
{
	const FOrionDataItem Info = GetItemInfo(ItemId);
	const FString Name = Info.DisplayName.ToString();
	const FString Prefix = (Quantity > 0 ? TEXT("+") : TEXT("-"));
	const FString Text = FString::Printf(TEXT("%s%d %s"), *Prefix, FMath::Abs(Quantity), *Name);

	TArray<UTextRenderComponent*> Comps;
	if (const AActor* Owner = GetOwner())
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
			if (auto* Tr = Cast<UTextRenderComponent>(C))
			{
				InventoryChangeTextComp.Add(Tr);
			}
		}

		if (InventoryChangeTextComp.Num() > 0)
		{
			UTextRenderComponent* ChangeComp = InventoryChangeTextComp[0];
			ChangeComp->SetText(FText::FromString(Text));
			ChangeComp->SetHiddenInGame(false);

			// 延迟隐藏
			constexpr float Duration = 1.0f;
			FTimerHandle Handle;
			GetWorld()->GetTimerManager().SetTimer(Handle, [ChangeComp]()
			{
				ChangeComp->SetHiddenInGame(true);
			}, Duration, false);
		}
	}
}

bool UOrionInventoryComponent::ModifyItemQuantity(const int32 ItemId, const int32 Quantity)
{
	if (Quantity == 0)
	{
		return true;
	}

	/* --------- 安全检查 --------- */
	if (!AvailableInventoryMap.Contains(ItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Inventory] ItemId %d not in AvailableInventoryMap"), ItemId);
		return false;
	}
	const int32 MaxAllowed = AvailableInventoryMap[ItemId];

	// 2) 计算新数量并检查范围
	int32& CurrQ = InventoryMap.FindOrAdd(ItemId);
	const int32 NewQ = CurrQ + Quantity;
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

	/*SpawnResourceFloatUI(ItemId, Quantity);*/
	SpawnNewResourceFloatUI(ItemId, Quantity);

	OnInventoryChange();

	return true;
}

void UOrionInventoryComponent::SpawnNewResourceFloatUI(const int32 ItemId, const int32 Quantity) const
{
	if (!FloatWidgetClass)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	UOrionUserWidgetResourceFloat* W = CreateWidget<UOrionUserWidgetResourceFloat>(PC, FloatWidgetClass);
	if (!W)
	{
		return;
	}

	if (const TSoftObjectPtr<UTexture2D>* TexturePtr = ItemIDToTextureMap.Find(ItemId))
	{
		W->SetIcon(TexturePtr->LoadSynchronous());
	}
	W->DeltaText->SetText(FText::FromString(FString::Printf(TEXT("%s%d %s"), (Quantity > 0 ? TEXT("+") : TEXT("-")),
	                                                        FMath::Abs(Quantity),
	                                                        *GetItemInfo(ItemId).DisplayName.ToString())));

	// 计算世界坐标到屏幕坐标
	FVector Origin, BoxExtent;
	GetOwner()->GetActorBounds(true, Origin, BoxExtent);

	const FVector WorldPos = Origin + FVector(0.f, 0.f, BoxExtent.Z + 100.f);
	FVector2D ScreenPos;


	PC->ProjectWorldLocationToScreen(WorldPos, ScreenPos);


	W->AddToViewport();
	W->SetPositionInViewport(ScreenPos, true);

	W->PlayFloat();
	/*FWidgetAnimationDynamicEvent EndEvent; // ① 声明委托实例
	EndEvent.BindLambda( // ② 绑定 Lambda
		[W](UUserWidget*, const UWidgetAnimation*)
		{
			W->RemoveFromParent();
		});

	W->BindToAnimationFinished(W->FloatUp, EndEvent);*/
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
