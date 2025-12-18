// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionInventoryManager.h"
#include "Kismet/GameplayStatics.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Orion/OrionHUD/OrionHUD.h"
#include "Orion/OrionInterface/OrionInterfaceSerializable.h"

void UOrionInventoryManager::Initialize(FSubsystemCollectionBase& SubsystemCollectionBase)
{
	Super::Initialize(SubsystemCollectionBase);

	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UOrionInventoryManager::OnWorldInitializedActors);
}

void UOrionInventoryManager::OnWorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams)
{

}

TMap<int32, int32> UOrionInventoryManager::GetPlayerFactionInventoryMap() const
{
	TMap<int32, int32> PlayerFactionInventoryMap;
	TArray<AActor*> OrionActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionActorStorage::StaticClass(), OrionActors);
	for (const auto& Each : OrionActors)
	{
		if (const AOrionActorStorage* EachStorage = Cast<AOrionActorStorage>(Each))
		{
			for (const auto& Pair : EachStorage->InventoryComp->GetInventoryMap())
			{
				int32 ItemId = Pair.Key;
				int32 Quantity = Pair.Value;
				if (PlayerFactionInventoryMap.Contains(ItemId))
				{
					PlayerFactionInventoryMap[ItemId] += Quantity;
				}
				else
				{
					PlayerFactionInventoryMap.Add(ItemId, Quantity);
				}
			}
		}
	}

	//OrionCppFunctionLibrary::OrionPrintTMap(PlayerFactionInventoryMap, ELogVerbosity::Error);
	return PlayerFactionInventoryMap;
}

void UOrionInventoryManager::RegisterInventoryComponent(UOrionInventoryComponent* InventoryComp)
{
	AllInventoryComponents.Add(InventoryComp);
}

void UOrionInventoryManager::CollectInventoryRecords(TArray<FOrionInventorySerializable>& SavingInventoryRecord) const
{
	SavingInventoryRecord.Empty();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Act = *It;
		if (const auto* FoundInventoryComp = Act->FindComponentByClass<UOrionInventoryComponent>())
		{
			if (const auto* Serial = Cast<IOrionInterfaceSerializable>(Act))
			{
				FOrionInventorySerializable InventoryRecord;
				InventoryRecord.OwnerGameId = Serial->GetSerializable().GameId;
				InventoryRecord.SerializedInventoryMap = FoundInventoryComp->GetInventoryMap();
				InventoryRecord.SerializedAvailableInventoryMap = FoundInventoryComp->GetAvailableInventoryMap();
				SavingInventoryRecord.Add(MoveTemp(InventoryRecord));
			}
		}
	}
}

void UOrionInventoryManager::ApplyInventoryRecords(const TArray<FOrionInventorySerializable>& Saved) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const FOrionInventorySerializable& InventorySerializable : Saved)
	{
		const AActor* Target = FindOwnerById(World, InventorySerializable.OwnerGameId);
		if (!Target)
		{
			continue;
		}

		if (auto* Inv = Target->FindComponentByClass<UOrionInventoryComponent>())
		{
			UE_LOG(LogTemp, Log, TEXT("[ApplyInventoryRecords] Found Inventory for %s"), *Target->GetName());
			Inv->ClearInventory();
			for (auto& Pair : InventorySerializable.SerializedInventoryMap)
			{
				Inv->ModifyItemQuantity(Pair.Key, Pair.Value);
				Inv->SetCapacityMap(InventorySerializable.SerializedAvailableInventoryMap);
				Inv->ForceSetInventory(InventorySerializable.SerializedInventoryMap);
			}
		}
	}
}

AActor* UOrionInventoryManager::FindOwnerById(const UWorld* World, const FGuid& Id)
{
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (auto* Serial = Cast<IOrionInterfaceSerializable>(*It))
		{
			if (Serial->GetSerializable().GameId == Id)
			{
				return *It;
			}
		}
	}
	return nullptr;
}