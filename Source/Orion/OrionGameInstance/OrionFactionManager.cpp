// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionFactionManager.h"
#include "Orion/OrionHUD/OrionHUD.h"
#include "OrionInventoryManager.h"
#include "Kismet/GameplayStatics.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"

const TMap<int32, TArray<TPair<int32, int32>>> UOrionFactionManager::BuildingCostMap = {
	// BuildingId, <ItemId, Amount>
	{1, {{2, 1}}},   // Square Foundation: 1 StoneOre
	{2, {{2, 1}}},   // Triangle Foundation: 1 StoneOre
	{3, {{2, 2}}},   // Wall: 2 StoneOre
	{4, {{2, 4}}},   // Double Wall: 4 StoneOre
	{6, {{2, 10}}},  // Ore: 10 StoneOre
	{7, {{2, 15}}},  // Production: 10 StoneOre
	{8, {{2, 20}}},  // Storage: 10 StoneOre
	{9, {{2, 1}}},   // Inclined Roof: 1 StoneOre
	{10, {{2, 1}}},   // Inclined Roof: 1 StoneOre
};

void UOrionFactionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UOrionFactionManager::OnWorldInitializedActors);
}

bool UOrionFactionManager::ModifyFactionGold(const EFaction Faction, const int Gold)
{
	if (FactionsDataMap.Find(Faction) && FactionsDataMap[Faction].Gold + Gold >= 0)
	{
		FactionsDataMap[Faction].Gold += Gold;
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("OrionFactionManager::ModifyFactionGold: Faction doesn't exist or not enough gold. "));
	return false;
}


bool UOrionFactionManager::AffordBuildingCost(int32 BuildingId)
{
	if (!BuildingCostMap.Find(BuildingId))
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionFactionManager::AffordBuildingCost: BuildingId %d not found in BuildingCostMap."), BuildingId);
		return false;
	}

	const TArray<TPair<int32, int32>>& CostArray = BuildingCostMap[BuildingId];

	for (const auto& EachCost : CostArray)
	{
		int32 ItemId = EachCost.Key;
		int32 Amount = EachCost.Value;

		if (!CheckFactionResources(EFaction::PlayerFaction, ItemId, Amount))
		{
			UE_LOG(LogTemp, Warning, TEXT("OrionFactionManager::AffordBuildingCost: Not enough resources for building id %d (ItemId: %d, Amount: %d)."), BuildingId, ItemId, Amount);
			return false;
		}
	}

	for (const auto& EachCost : CostArray)
	{
		int32 ItemId = EachCost.Key;
		int32 Amount = EachCost.Value;

		if (!DeduceFactionResources(EFaction::PlayerFaction, ItemId, Amount))
		{
			UE_LOG(LogTemp, Error, TEXT("OrionFactionManager::AffordBuildingCost: Failed to deduce resources for building id %d (ItemId: %d, Amount: %d). This should not happen!"), BuildingId, ItemId, Amount);
			return false;
		}
	}
	
	return true;
}

bool UOrionFactionManager::CheckFactionResources(const EFaction Faction, const int32 ItemId, const int32 Amount) const
{
	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionFactionManager::CheckFactionResources: Amount needs to be positive."));
		return false;
	}

	TMap<int32, int32> PlayerFactionInventoryMap = InventoryManager->GetPlayerFactionInventoryMap();
	if (!PlayerFactionInventoryMap.Find(ItemId) || PlayerFactionInventoryMap[ItemId] < Amount)
	{
		return false;
	}

	int32 TotalAvailable = 0;
	TArray<AActor*> OrionActorStorages;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionActorStorage::StaticClass(), OrionActorStorages);

	for (const auto& Each : OrionActorStorages)
	{
		if (const AOrionActorStorage* EachStorage = Cast<AOrionActorStorage>(Each))
		{
			TotalAvailable += EachStorage->InventoryComp->GetItemQuantity(ItemId);
			if (TotalAvailable >= Amount)
			{
				return true;
			}
		}
	}

	return false;
}

bool UOrionFactionManager::DeduceFactionResources(const EFaction Faction, const int32 ItemId, int32 Amount)
{
	PlayerController = Cast<AOrionPlayerController>(GetWorld()->GetFirstPlayerController());

	checkf(PlayerController, TEXT("OrionFactionManager::DeduceFactionResources: PlayerController not found."));

	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionFactionManager::DeduceFactionResources: Amount needs to be positive."));
		return false;
	}

	// 先检查资源是否充足（不扣减）
	if (!CheckFactionResources(Faction, ItemId, Amount))
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionFactionManager::DeduceFactionResources: Not enough item (ItemId: %d, Amount: %d)."), ItemId, Amount);
		return false;
	}

	// 资源充足，执行扣减
	TArray<AActor*> OrionActorStorages;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionActorStorage::StaticClass(), OrionActorStorages);

	for (const auto& Each : OrionActorStorages)
	{
		if (Amount <= 0) break;

		if (AOrionActorStorage* EachStorage = Cast<AOrionActorStorage>(Each))
		{
			int32 StorageQuantity = EachStorage->InventoryComp->GetItemQuantity(ItemId);
			if (StorageQuantity > 0)
			{
				int32 DeductAmount = FMath::Min(Amount, StorageQuantity);
				EachStorage->InventoryComp->ModifyItemQuantity(ItemId, -DeductAmount);
				Amount -= DeductAmount;
			}
		}
	}

	// 验证扣减是否完成
	if (Amount == 0)
	{
		Cast<AOrionHUD>(PlayerController->GetHUD())->UpdatePlayerFactionResourceDisplay();
		return true;
	}

	// 如果还有剩余，说明扣减失败（理论上不应该发生，因为已经检查过了）
	UE_LOG(LogTemp, Error, TEXT("OrionFactionManager::DeduceFactionResources: Failed to deduct all resources (ItemId: %d, Remaining: %d). This should not happen!"), ItemId, Amount);
	return false;
}




bool UOrionFactionManager::IsHostile(const EFaction FactionA, const EFaction FactionB) const
{
	// Currently simple logic: different factions are hostile
	return FactionA != FactionB;
}

void UOrionFactionManager::InitFactions()
{
	FactionsDataMap = {
		{
			EFaction::PlayerFaction,
			FFaction(EFaction::PlayerFaction, FString(TEXT("The faction representing the player.")), 1000)
		},
		{
			EFaction::Vagrants,
			FFaction(EFaction::Vagrants, FString(TEXT("A nomadic faction with no fixed home.")), 500)
		},
	};
}

void UOrionFactionManager::OnWorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams)
{
	// Implementation for initializing economic systems when world actors are initialized

	checkf(GetWorld(), TEXT("OrionEconomicsManager::OnWorldInitializedActors: world not initialized. "));

	InventoryManager = GetGameInstance()->GetSubsystem<UOrionInventoryManager>();

	checkf(InventoryManager, TEXT("OrionFactionManager::OnWorldInitializedActors:: InventoryManager not found"));

	InitFactions();


}
