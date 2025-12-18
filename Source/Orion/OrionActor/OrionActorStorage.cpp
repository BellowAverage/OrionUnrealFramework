// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActorStorage.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"

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


TArray<FString> AOrionActorStorage::TickShowHoveringInfo()
{
	TArray<FString> Lines;

	Lines.Add(FString::Printf(TEXT("Name: %s"), *GetName()));

	float CurrentHealth = AttributeComp ? AttributeComp->Health : 0.0f;
	Lines.Add(FString::Printf(TEXT("CurrHealth: %.0f"), CurrentHealth));

	return Lines;
}