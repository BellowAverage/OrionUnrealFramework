// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionActorManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Orion/OrionInterface/OrionInterfaceSerializable.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "OrionInventoryManager.generated.h"

/**
 * 
 */
UCLASS()
class ORION_API UOrionInventoryManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/* ========== 采集 ========== */
	void CollectInventoryRecords(TArray<FOrionInventorySerializable>& SavingInventoryRecord) const
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

	/* ========== 恢复 ========== */
	void ApplyInventoryRecords(const TArray<FOrionInventorySerializable>& Saved) const
	{
		UWorld* World = GetWorld();
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

private:
	static AActor* FindOwnerById(const UWorld* World, const FGuid& Id)
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
};
