// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActorOre.h"
#include "Orion/OrionGameInstance/OrionCharaManager.h"


AOrionActorOre::AOrionActorOre()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionActorOre::BeginPlay()
{
	Super::BeginPlay();

	if (InventoryComp && OreCategory == EOreCategory::StoneOre)
	{
		AvailableInventoryMap = {{2, 10}};
		InventoryComp->AvailableInventoryMap = AvailableInventoryMap;
		InventoryComp->ModifyItemQuantity(2, 5);
	}
}


void AOrionActorOre::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ProductionProgressUpdate(DeltaTime);

	/* 满仓则不可交互 */
	if (OreCategory == EOreCategory::StoneOre &&
		InventoryComp && InventoryComp->FullInventoryStatus.FindRef(2))
	{
		ActorStatus = EActorStatus::NotInteractable;
	}
	else
	{
		ActorStatus = EActorStatus::Interactable;
	}
}

void AOrionActorOre::ShowInteractOptions()
{
	UE_LOG(LogTemp, Log, TEXT("ShowInteractOptions called on Ore Actor: %s"), *GetName());
}

bool AOrionActorOre::ApplyInteractionToCharas(TArray<AOrionChara*> InteractedCharas, AOrionActor* InteractingActor)
{
	for (const auto& Chara : InteractedCharas)
	{
		if (Chara && Chara->ActionComp)
		{
			if (Chara->ActionComp->IsProcedural())
			{
				Chara->ActionComp->SetProcedural(false);
			}

			if (UOrionCharaManager* Manager = GetGameInstance()->GetSubsystem<UOrionCharaManager>())
			{
				Manager->RemoveAllActionsFromChara(Chara);
				Manager->AddInteractWithActorAction(Chara, InteractingActor, EActionExecution::RealTime);
			}
		}
		else
		{
			return false;
		}
	}


	return true;
}

void AOrionActorOre::ProductionProgressUpdate(float DeltaTime)
{
	// Only update production if there's at least one worker.
	if (CurrWorkers < 1 || !InventoryComp) { return; }

	// Calculate the progress increment.
	// For one worker, the progress rate is 100 / ProductionTimeCost per second.
	// With multiple workers, the production speeds up proportionally.
	float ProgressIncrement = (100.0f / ProductionTimeCost) * CurrWorkers * DeltaTime;

	// Accumulate the progress.
	ProductionProgress += ProgressIncrement;

	// When production progress reaches or exceeds 100, consider production complete.
	while (ProductionProgress >= 100.0f)
	{
		// Production cycle complete, add one item to inventory.

		if (OreCategory == EOreCategory::StoneOre)
		{
			InventoryComp->ModifyItemQuantity(2, 1);
		}


		// Subtract 100 from the progress, retaining any leftover progress.
		ProductionProgress -= 100.0f;
	}
}
