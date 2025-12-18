// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionCharaManager.h"
#include "Misc/Guid.h"
#include "Orion/OrionComponents/OrionActionComponent.h"
#include "Orion/OrionAIController/OrionAIController.h"
#include "GameFramework/Controller.h"
#include "Orion/OrionComponents/OrionMovementComponent.h"

#define ORION_CHARA_HALF_HEIGHT 88.f

TArray<FOrionCharaSerializable> UOrionCharaManager::TestCharactersSet = {
	{
		FGuid::NewGuid(),
		FVector(0.0f, 0.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
	{
		FGuid::NewGuid(),
		FVector(200.f, 200.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
	{
		FGuid::NewGuid(),
		FVector(400.f, 200.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
	{
		FGuid::NewGuid(),
		FVector(600.f, 100.0f, 150.f),
		FRotator(0.0f, 0.0f, 0.0f),
		{}
	},
};

AOrionChara* UOrionCharaManager::SpawnCharaInstance(const FVector& SpawnLocation, 
                                                     const FOrionCharaSpawnParams& SpawnParams,
                                                     TSubclassOf<AOrionChara> CharacterClass)
{
	// Use provided class or fall back to default CharacterBPClass
	TSubclassOf<AOrionChara> CharaClassToUse = CharacterClass ? CharacterClass : CharacterBPClass;
	
	if (!CharaClassToUse)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager] Character class is not set."));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager] World is null."));
		return nullptr;
	}

	constexpr float CapsuleHalfHeight = ORION_CHARA_HALF_HEIGHT;
	const FVector SpawnLocationWithOffset = SpawnLocation + FVector(0.f, 0.f, CapsuleHalfHeight);

	// Spawn actor with deferred construction
	AOrionChara* OrionChara = World->SpawnActorDeferred<AOrionChara>(
		CharaClassToUse,
		FTransform(SpawnParams.SpawnRotation, SpawnLocationWithOffset),
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn
	);

	if (OrionChara)
	{
		// Set properties before construction (ensures C++ changes take priority over Blueprint defaults)
		OrionChara->MaxHealth = SpawnParams.MaxHealth;
		// Deprecated: OrionChara->CharaSide = SpawnParams.CharaSide;
		// Deprecated: OrionChara->HostileGroupsIndex = SpawnParams.HostileGroupsIndex;
		OrionChara->CharaAIState = SpawnParams.CharaAIState;

		// Set combat component properties if available
		if (OrionChara->CombatComp)
		{
			// OrionChara->CombatComp->FireRange = SpawnParams.FireRange;
			
			// Set weapon type from spawn params
			OrionChara->CombatComp->CurrentWeaponType = SpawnParams.WeaponType;
		}

		if (OrionChara->MovementComp)
		{
			OrionChara->MovementComp->OrionCharaSpeed = SpawnParams.InitialSpeed;
		}

		// Finish spawning (this triggers construction and BeginPlay)
		OrionChara->FinishSpawning(FTransform(SpawnParams.SpawnRotation, SpawnLocationWithOffset));

		UE_LOG(LogTemp, Log, TEXT("[OrionCharaManager] Successfully spawned AOrionChara at location: %s"), *SpawnLocation.ToString());

		// Ensure AIControllerClass is set
		if (OrionChara->AIControllerClass)
		{
			// Use SpawnDefaultController to handle AI controller assignment
			OrionChara->SpawnDefaultController();
			AController* Controller = OrionChara->GetController();
			if (AOrionAIController* AIController = Cast<AOrionAIController>(Controller))
			{
				UE_LOG(LogTemp, Log, TEXT("[OrionCharaManager] OrionAIController possessed the character successfully."));
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager] Failed to assign AOrionAIController."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager] OrionChara's AIControllerClass is not set."));
		}

		// Add initial actions to the character's queue
		AddInitialActionsToChara(OrionChara, SpawnParams);

		// Register the character
		RegisterChara(OrionChara);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager] Failed to spawn AOrionChara."));
	}

	return OrionChara;
}

void UOrionCharaManager::AddInitialActionsToChara(AOrionChara* Chara, const FOrionCharaSpawnParams& SpawnParams)
{
	if (!Chara || !Chara->ActionComp)
	{
		return;
	}

	// Add RealTime actions
	for (const FOrionActionParams& ActionParams : SpawnParams.InitialRealTimeActions)
	{
		AddActionByParams(Chara, ActionParams, EActionExecution::RealTime, INDEX_NONE);
	}

	// Add Procedural actions
	for (const FOrionActionParams& ActionParams : SpawnParams.InitialProceduralActions)
	{
		AddActionByParams(Chara, ActionParams, EActionExecution::Procedural, INDEX_NONE);
	}
}

void UOrionCharaManager::RecoverProcActions(AOrionChara* Chara, const FOrionCharaSerializable& S)
{
	if (!Chara) return;

	for (const FOrionActionParams& P : S.SerializedProcActions)
	{
		AddActionByParams(Chara, P, EActionExecution::Procedural, INDEX_NONE);
	}
}

bool UOrionCharaManager::AddActionByParams(AOrionChara* Chara, const FOrionActionParams& P,
                                             EActionExecution ExecutionType, int32 Index)
{
	if (!Chara) return false;

	UWorld* World = Chara->GetWorld();
	if (!World) return false;

	// Helper lambda to find actor by ID
	auto FindActorById = [World](const FGuid& Id) -> AActor*
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (const IOrionInterfaceSerializable* Serial = Cast<IOrionInterfaceSerializable>(*It))
			{
				if (Serial->GetSerializable().GameId == Id)
				{
					return *It;
				}
			}
		}
		return nullptr;
	};

	FOrionAction Action;
	switch (P.OrionActionType)
	{
	case EOrionAction::MoveToLocation:
		Action = Chara->InitActionMoveToLocation(TEXT("MoveToLocation"), P.TargetLocation);
		break;
			
	case EOrionAction::AttackOnChara:
		if (auto* Target = Cast<AOrionChara>(FindActorById(P.TargetActorId)))
		{
			Action = Chara->InitActionAttackOnChara(TEXT("AttackOnChara"), Target, P.HitOffset);
		}
		break;
			
	case EOrionAction::InteractWithActor:
		if (auto* Target = Cast<AOrionActor>(FindActorById(P.TargetActorId)))
		{
			Action = Chara->InitActionInteractWithActor(TEXT("InteractWithActor"), Target);
		}
		break;
			
	case EOrionAction::InteractWithProduction:
		if (auto* Target = Cast<AOrionActorProduction>(FindActorById(P.TargetActorId)))
		{
			Action = Chara->InitActionInteractWithProduction(TEXT("InteractWithProduction"), Target);
		}
		break;
			
	case EOrionAction::CollectCargo:
		if (auto* Target = Cast<AOrionActorStorage>(FindActorById(P.TargetActorId)))
		{
			Action = Chara->InitActionCollectCargo(TEXT("CollectCargo"), Target);
		}
		break;
			
	case EOrionAction::CollectBullets:
		Action = Chara->InitActionCollectBullets(TEXT("CollectBullets"));
		break;
			
	default:
		break;
	}

	if (Action.ExecuteFunction)
	{
		return Internal_AddAction(Chara, Action, ExecutionType, Index);
	}

	return false;
}

bool UOrionCharaManager::Internal_AddAction(AOrionChara* Chara, const FOrionAction& Action,
                                            EActionExecution ExecutionType, int32 Index)
{
	if (!Chara || !Chara->ActionComp) return false;

	bool bIsProcedural = (ExecutionType == EActionExecution::Procedural);
	Chara->ActionComp->InsertAction(Action, bIsProcedural, Index);
	return true;
}

bool UOrionCharaManager::AddMoveToLocationAction(AOrionChara* Chara, const FVector& TargetLocation,
                                                   EActionExecution ExecutionType, int32 Index)
{
	if (!Chara) return false;
	FOrionAction Action = Chara->InitActionMoveToLocation(TEXT("MoveToLocation"), TargetLocation);
	return Internal_AddAction(Chara, Action, ExecutionType, Index);
}

bool UOrionCharaManager::AddAttackOnCharaAction(AOrionChara* Chara, AOrionChara* TargetChara, const FVector& HitOffset,
                                                 EActionExecution ExecutionType, int32 Index)
{
	if (!Chara || !TargetChara) return false;
	FOrionAction Action = Chara->InitActionAttackOnChara(TEXT("AttackOnChara"), TargetChara, HitOffset);
	return Internal_AddAction(Chara, Action, ExecutionType, Index);
}

bool UOrionCharaManager::AddInteractWithActorAction(AOrionChara* Chara, AOrionActor* TargetActor,
                                                     EActionExecution ExecutionType, int32 Index)
{
	if (!Chara || !TargetActor) return false;
	FOrionAction Action = Chara->InitActionInteractWithActor(TEXT("InteractWithActor"), TargetActor);
	return Internal_AddAction(Chara, Action, ExecutionType, Index);
}

bool UOrionCharaManager::AddInteractWithProductionAction(AOrionChara* Chara, AOrionActorProduction* TargetActor,
                                                           EActionExecution ExecutionType, int32 Index)
{
	if (!Chara || !TargetActor) return false;
	FOrionAction Action = Chara->InitActionInteractWithProduction(TEXT("InteractWithProduction"), TargetActor);
	return Internal_AddAction(Chara, Action, ExecutionType, Index);
}

bool UOrionCharaManager::AddCollectCargoAction(AOrionChara* Chara, AOrionActorStorage* TargetActor,
                                                 EActionExecution ExecutionType, int32 Index)
{
	if (!Chara || !TargetActor) return false;
	FOrionAction Action = Chara->InitActionCollectCargo(TEXT("CollectCargo"), TargetActor);
	return Internal_AddAction(Chara, Action, ExecutionType, Index);
}

bool UOrionCharaManager::AddCollectBulletsAction(AOrionChara* Chara, EActionExecution ExecutionType, int32 Index)
{
	if (!Chara) return false;
	FOrionAction Action = Chara->InitActionCollectBullets(TEXT("CollectBullets"));
	return Internal_AddAction(Chara, Action, ExecutionType, Index);
}

bool UOrionCharaManager::RemoveAllActionsFromChara(AOrionChara* Chara)
{
	if (!Chara || !Chara->ActionComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager::RemoveAllActionsFromChara] Character or ActionComp is null."));
		return false;
	}

	Chara->ActionComp->RemoveAllActions();
	return true;
}

bool UOrionCharaManager::RemoveProceduralActionAtFromChara(AOrionChara* Chara, int32 Index)
{
	if (!Chara || !Chara->ActionComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager::RemoveProceduralActionAtFromChara] Character or ActionComp is null."));
		return false;
	}

	Chara->ActionComp->RemoveProceduralActionAt(Index);
	return true;
}

bool UOrionCharaManager::ReorderProceduralActionInChara(AOrionChara* Chara, int32 DraggedIndex, int32 DropIndex)
{
	if (!Chara || !Chara->ActionComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OrionCharaManager::ReorderProceduralActionInChara] Character or ActionComp is null."));
		return false;
	}

	Chara->ActionComp->ReorderProceduralAction(DraggedIndex, DropIndex);
	return true;
}
