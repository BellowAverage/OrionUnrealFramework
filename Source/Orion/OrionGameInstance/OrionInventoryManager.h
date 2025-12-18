// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
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

	virtual void Initialize(FSubsystemCollectionBase&) override;
	virtual void OnWorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams);

	TMap<int32, int32> GetPlayerFactionInventoryMap() const;

	UPROPERTY() TArray<UOrionInventoryComponent*> AllInventoryComponents;

	void RegisterInventoryComponent(UOrionInventoryComponent* InventoryComp);

	void CollectInventoryRecords(TArray<FOrionInventorySerializable>& SavingInventoryRecord) const;

	void ApplyInventoryRecords(const TArray<FOrionInventorySerializable>& Saved) const;



private:

	static AActor* FindOwnerById(const UWorld* World, const FGuid& Id);
};
