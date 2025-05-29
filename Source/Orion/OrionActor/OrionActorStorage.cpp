// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActorStorage.h"

AOrionActorStorage::AOrionActorStorage()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionActorStorage::BeginPlay()
{
	Super::BeginPlay();

	if (!InventoryComp)
	{
		return;
	}

	/* 示例：给成品 3 预存 100 */
	if (StorageCategory == EStorageCategory::StoneStorage)
	{
		InventoryComp->ModifyItemQuantity(3, +100);
	}
}

void AOrionActorStorage::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (StorageCategory == EStorageCategory::None)
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] StorageCategory is None!"), *GetName());
	}
}
