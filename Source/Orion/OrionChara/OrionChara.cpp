// OrionChara.cpp

#include "OrionChara.h"
#include "Orion/OrionAIController/OrionAIController.h"
#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
#include <algorithm>
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include <Components/SphereComponent.h>
#include "Orion/OrionActor/OrionActorOre.h"
#include "Orion/OrionComponents/OrionLogisticsComponent.h"
#include "Orion/OrionComponents/OrionMovementComponent.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"
#include "Orion/OrionGameInstance/OrionInventoryManager.h"

class OrionActorStorage;

/*
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "OrionGameMode.h"
#include "OrionWeapon.h"
#include "Animation/SkeletalMeshActor.h"
#include "OrionBPFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
*/


AOrionChara::AOrionChara()
{
	PrimaryActorTick.bCanEverTick = true;

	/* Register AI Controller */
	AIControllerClass = AOrionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	/* Register AI Senses */
	StimuliSourceComp = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComp"));
	StimuliSourceComp->RegisterForSense(TSubclassOf<UAISense_Sight>());
	StimuliSourceComp->bAutoRegister = true;

	/* Register Inventory Component */
	InventoryComp = CreateDefaultSubobject<UOrionInventoryComponent>(TEXT("InventoryComp"));

	/* Register OrionChara Components */
	ActionComp = CreateDefaultSubobject<UOrionActionComponent>(TEXT("ActionComp"));
	LogisticsComp = CreateDefaultSubobject<UOrionLogisticsComponent>(TEXT("LogisticsComp"));
	CombatComp = CreateDefaultSubobject<UOrionCombatComponent>(TEXT("CombatComp"));
	MovementComp = CreateDefaultSubobject<UOrionMovementComponent>(TEXT("MovementComp"));
	AttributeComp = CreateDefaultSubobject<UOrionAttributeComponent>(TEXT("AttributeComp"));
}

void AOrionChara::InitSerializable(const FSerializable& InSerializable)
{
	// Initialize the character with the provided serializable data
	if (!GameSerializable.GameId.IsValid())
	{
		GameSerializable.GameId = FGuid::NewGuid();
	}
}

FSerializable AOrionChara::GetSerializable() const
{
	return GameSerializable;
}

void AOrionChara::BeginPlay()
{
	Super::BeginPlay();

	InitSerializable(GameSerializable);

	OrionAIControllerInstance = Cast<AOrionAIController>(GetController());

	InitOrionCharaMovement();

	this->SetCanBeDamaged(true);

	// Bind component delegates
	if (ActionComp)
	{
		ActionComp->OnActionTypeChanged.AddDynamic(this, &AOrionChara::OnActionTypeChangedHandler);
		ActionComp->OnActionNameChanged.AddDynamic(this, &AOrionChara::OnActionNameChangedHandler);

	}

	if (AttributeComp)
	{
		AttributeComp->MaxHealth = MaxHealth;
		AttributeComp->SetHealth(MaxHealth);
		AttributeComp->OnHealthZero.AddDynamic(this, &AOrionChara::HandleHealthZero);
	}
}

void AOrionChara::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}

	ForceDetectionOnVelocityChange();
	RegisterCharaRagdoll(DeltaTime);




	/* Refresh Attack Frequency */

	if (CombatComp)
	{
		CombatComp->RefreshAttackFrequency();
	}
	IsInteractWithActor = InteractWithActorState == EInteractWithActorState::Interacting;
}

FString AOrionChara::GetUnifiedActionName() const
{
	return ActionComp ? ActionComp->GetUnifiedActionName() : FString();
}

EOrionAction AOrionChara::GetUnifiedActionType() const
{
	return ActionComp ? ActionComp->GetUnifiedActionType() : EOrionAction::Undefined;
}

void AOrionChara::OnActionTypeChangedHandler(EOrionAction PrevType, EOrionAction CurrType)
{
	if (const UEnum* EnumPtr = StaticEnum<EOrionAction>())
	{
		UE_LOG(LogTemp, Verbose, TEXT("SwitchingStateHandle: %s -> %s"),
		       *EnumPtr->GetNameStringByValue(static_cast<int64>(PrevType)),
		       *EnumPtr->GetNameStringByValue(static_cast<int64>(CurrType)));
	}

	switch (PrevType)
	{
	case EOrionAction::InteractWithActor:
		InteractWithActorStop(InteractWithActorState);
		break;
	case EOrionAction::InteractWithProduction:
		InteractWithProductionStop();
		break;
	case EOrionAction::CollectCargo:
		if (LogisticsComp)
		{
			LogisticsComp->CollectingCargoStop();
		}
		break;
	case EOrionAction::AttackOnChara:
		if (CombatComp)
		{
			CombatComp->AttackOnCharaLongRangeStop();
		}
		break;
	case EOrionAction::InteractWithStorage:
		if (MovementComp) MovementComp->MoveToLocationStop();
		break;
	case EOrionAction::CollectBullets:
	case EOrionAction::MoveToLocation:
	case EOrionAction::Undefined:
	default:
		break;
	}
}

/* Logistics functions moved to UOrionLogisticsComponent */
/* Movement functions moved to UOrionMovementComponent */

FString AOrionChara::GetActionValidityReason(int32 ActionIndex, bool bIsProcedural)
{
	return ActionComp ? ActionComp->GetActionValidityReason(ActionIndex, bIsProcedural) : TEXT("Action Component Invalid");
}

FString AOrionChara::GetActionStatusString(int32 ActionIndex, bool bIsProcedural)
{
	return ActionComp ? ActionComp->GetActionStatusString(ActionIndex, bIsProcedural) : TEXT("Action Component Invalid");
}

FOrionAction AOrionChara::InitActionMoveToLocation(const FString& ActionName, const FVector& TargetLocation)
{
	// Use TWeakObjectPtr to capture this safely
	TWeakObjectPtr<AOrionChara> WeakChara(this);
	// Capture TargetLocation by value
	FVector CapturedLocation = TargetLocation;

	auto DescFunc = []() -> FString
	{
		return TEXT("Moving to Location");
	};
	
	// [Fix 9] CheckFunc for MoveToLocation (always valid if character exists)
	auto CheckFunc = [WeakChara](FString& OutReason) -> EActionValidity
	{
		if (!WeakChara.IsValid())
		{
			OutReason = TEXT("Character Invalid");
			return EActionValidity::PermanentInvalid;
		}
		return EActionValidity::Valid;
	};

	// [Fix] Add OnExit
	auto ExitFunc = [WeakChara](bool bInterrupted)
	{
		if (AOrionChara* Chara = WeakChara.Get())
		{
			if (Chara->MovementComp) Chara->MovementComp->MoveToLocationStop();
		}
	};

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::MoveToLocation,
		[WeakChara, CapturedLocation](float DeltaTime) -> EActionStatus
		{
			// Check if pointer is valid before execution
			if (AOrionChara* Chara = WeakChara.Get())
			{
				if (Chara->MovementComp) // Delegate to component
				{
					// MovementComp->MoveToLocation returns true if arrived/done
					bool bArrived = Chara->MovementComp->MoveToLocation(CapturedLocation);
					return bArrived ? EActionStatus::Finished : EActionStatus::Running;
				}
			}
			// If pointer is invalid (character is dead/destroyed), treat as finished
			return EActionStatus::Finished;
		},
		CheckFunc, // [Fix 9] Pass CheckFunc
		DescFunc,
		ExitFunc // Pass ExitFunc
	);

	AddingAction.Params.TargetLocation = TargetLocation;
	AddingAction.Params.OrionActionType = EOrionAction::MoveToLocation;

	return AddingAction;
}

FOrionAction AOrionChara::InitActionAttackOnChara(const FString& ActionName,
	                                             AActor* TargetChara, const FVector& HitOffset)
{
	TWeakObjectPtr<AOrionChara> WeakChara(this);
	// Target should also be weak reference to prevent crashes when target disappears
	TWeakObjectPtr<AActor> WeakTarget(TargetChara);

	auto ExecFunc = [WeakChara, WeakTarget, inHitOffset = HitOffset](float DeltaTime) -> EActionStatus
	{
		AOrionChara* CharaPtr = WeakChara.Get();
		AActor* TargetPtr = WeakTarget.Get();

		if (CharaPtr && TargetPtr && CharaPtr->CombatComp)
		{
			bool bFinished = CharaPtr->CombatComp->AttackOnChara(DeltaTime, TargetPtr, inHitOffset);
			return bFinished ? EActionStatus::Finished : EActionStatus::Running;
		}
		return EActionStatus::Finished;
	};

	// [Fix 9] Updated CheckFunc signature
	auto CheckFunc = [WeakChara, WeakTarget](FString& OutReason) -> EActionValidity
	{
		AOrionChara* Chara = WeakChara.Get();
		if (!Chara) 
		{
			OutReason = TEXT("Character Invalid");
			return EActionValidity::PermanentInvalid;
		}

		AActor* Target = WeakTarget.Get();
		if (!IsValid(Target))
		{
			OutReason = TEXT("Target Destroyed");
			return EActionValidity::PermanentInvalid;
		}

		// [Fix] Check if target is dead or incapacitated
		if (const AOrionChara* TargetChara = Cast<AOrionChara>(Target))
		{
			if (TargetChara->CharaState == ECharaState::Dead || TargetChara->CharaState == ECharaState::Incapacitated)
			{
				OutReason = TEXT("Target is Dead or Incapacitated");
				return EActionValidity::PermanentInvalid;
			}
		}

		// [Fix] Also check AttributeComponent for non-Chara targets (buildings, structures, etc.)
		if (const UOrionAttributeComponent* TargetAttr = Target->FindComponentByClass<UOrionAttributeComponent>())
		{
			if (!TargetAttr->IsAlive())
			{
				OutReason = TEXT("Target is Dead (AttributeComponent)");
				return EActionValidity::PermanentInvalid;
			}
		}

		return EActionValidity::Valid;
	};

	// [Fix] Add OnExit
	auto ExitFunc = [WeakChara](bool bInterrupted)
	{
		if (AOrionChara* Chara = WeakChara.Get())
		{
			if (Chara->CombatComp) Chara->CombatComp->AttackOnCharaLongRangeStop();
		}
	};

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::AttackOnChara,
		ExecFunc,
		CheckFunc,
		nullptr,
		ExitFunc // Pass ExitFunc
	);

	AddingAction.Params.HitOffset = HitOffset;
	AddingAction.Params.OrionActionType = EOrionAction::AttackOnChara;

	// Safely get ID
	IOrionInterfaceSerializable* TargetCharaSerializable = Cast<IOrionInterfaceSerializable>(TargetChara);

	if (TargetCharaSerializable)
	{
		AddingAction.Params.TargetActorId = TargetCharaSerializable->GetSerializable().GameId;
	}
	else if (TargetChara)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("InitActionAttackOnChara: TargetChara does not implement IOrionInterfaceSerializable!"));
	}

	return AddingAction;
}

FOrionAction AOrionChara::InitActionInteractWithActor(const FString& ActionName,
	                                                 AOrionActor* TargetActor)
{
	TWeakObjectPtr<AOrionChara> WeakChara(this);
	TWeakObjectPtr<AOrionActor> WeakTarget(TargetActor);

	auto ExecFunc = [WeakChara, WeakTarget](float DeltaTime) -> EActionStatus
	{
		if (AOrionChara* CharaPtr = WeakChara.Get())
		{
			// If target is invalid, tell Character to stop interaction logic (clean up state)
			if (!WeakTarget.IsValid())
			{
				// Can choose to call a cleanup function here, or directly return true
				return EActionStatus::Finished;
			}
			bool bFinished = CharaPtr->InteractWithActor(DeltaTime, WeakTarget.Get());
			
			// [Refactor] Return Skipped instead of Finished to persist in Procedural Queue
			// This ensures that "InteractWithActor" (e.g. mining) stays in the list until target is destroyed or user removes it.
			EActionStatus Result = bFinished ? EActionStatus::Skipped : EActionStatus::Running;
			
			// UE_LOG(LogTemp, Log, TEXT("[InteractWithActor] InteractWithActor returned: %s -> ActionStatus: %s"), 
			// 	bFinished ? TEXT("true (finished/idle)") : TEXT("false (running)"),
			// 	(Result == EActionStatus::Skipped) ? TEXT("Skipped (Persist)") : TEXT("Running"));
				
			return Result;
		}
		return EActionStatus::Finished;
	};

	// [Fix 9] Updated CheckFunc signature to return EActionValidity
	auto CheckFunc = [WeakChara, WeakTarget](FString& OutReason) -> EActionValidity
	{
		AOrionChara* Chara = WeakChara.Get();
		if (!Chara) 
		{
			OutReason = TEXT("Character Invalid");
			return EActionValidity::PermanentInvalid;
		}
		
		AOrionActor* Target = WeakTarget.Get();
		if (!IsValid(Target))
		{
			OutReason = TEXT("Target Destroyed");
			return EActionValidity::PermanentInvalid; // Target destroyed -> permanently remove
		}

		if (Target->ActorStatus == EActorStatus::NotInteractable)
		{
			OutReason = TEXT("Target Not Interactable");
			return EActionValidity::TemporarySkip; // Maybe can interact later? Usually suggest Skip.
		}

		return EActionValidity::Valid;
	};

	auto DescFunc = [WeakTarget]() -> FString
	{
		if (AOrionActor* Target = WeakTarget.Get())
		{
			return FString::Printf(TEXT("Interacting with %s"), *Target->GetName());
		}
		return TEXT("Interacting...");
	};

	// [Fix] Add OnExit
	auto ExitFunc = [WeakChara](bool bInterrupted)
	{
		if (AOrionChara* Chara = WeakChara.Get())
		{
			Chara->InteractWithActorStop(Chara->InteractWithActorState);
		}
	};

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::InteractWithActor,
		ExecFunc,
		CheckFunc,
		DescFunc,
		ExitFunc // Pass ExitFunc
	);

	if (TargetActor)
	{
		AddingAction.Params.TargetActorId = TargetActor->GetSerializable().GameId;
	}
	AddingAction.Params.OrionActionType = EOrionAction::InteractWithActor;

	return AddingAction;
}

FOrionAction AOrionChara::InitActionInteractWithProduction(const FString& ActionName,
	                                                      AOrionActorProduction* TargetActor)
{
	TWeakObjectPtr<AOrionChara> WeakChara(this);
	TWeakObjectPtr<AOrionActorProduction> WeakTarget(TargetActor);

	auto ExecFunc = [WeakChara, WeakTarget](float DeltaTime) -> EActionStatus
	{
		AOrionChara* CharaPtr = WeakChara.Get();
		AOrionActorProduction* TargetPtr = WeakTarget.Get();

		if (!CharaPtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[InteractProduction] CharaPtr is null -> Returning Finished"));
			return EActionStatus::Finished;
		}
		if (!TargetPtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[InteractProduction] TargetPtr is null -> Returning Finished"));
			return EActionStatus::Finished;
		}

		bool bFinished = CharaPtr->InteractWithProduction(DeltaTime, TargetPtr);
		// [Change] Return Skipped instead of Finished to persist in Procedural Queue
		// This allows the task to yield to lower priority tasks when done for now, but reactivate later.
		EActionStatus Result = bFinished ? EActionStatus::Skipped : EActionStatus::Running;
		// UE_LOG(LogTemp, Log, TEXT("[InteractProduction] InteractWithProduction returned: %s -> ActionStatus: %s"), 
		// 	bFinished ? TEXT("true (finished/idle)") : TEXT("false (running)"),
		// 	(Result == EActionStatus::Skipped) ? TEXT("Skipped (Persist)") : TEXT("Running"));
		return Result;
	};

	// [Fix 9] Updated CheckFunc signature
	auto CheckFunc = [WeakChara, WeakTarget](FString& OutReason) -> EActionValidity
	{
		AOrionChara* Chara = WeakChara.Get();
		if (!Chara) 
		{
			OutReason = TEXT("Character Invalid");
			return EActionValidity::PermanentInvalid;
		}

		AOrionActorProduction* Target = WeakTarget.Get();
		if (!IsValid(Target))
		{
			OutReason = TEXT("Target Destroyed");
			return EActionValidity::PermanentInvalid;
		}

		if (Target->ActorStatus == EActorStatus::NotInteractable)
		{
			OutReason = TEXT("Target Not Interactable");
			return EActionValidity::TemporarySkip;
		}

		return EActionValidity::Valid;
	};

	auto DescFunc = [WeakTarget]() -> FString
	{
		if (AOrionActorProduction* Target = WeakTarget.Get())
		{
			return FString::Printf(TEXT("Managing Production: %s"), *Target->GetName());
		}
		return TEXT("Production Active");
	};

	// [Fix] Add OnExit - CRITICAL for Double Drop issue
	auto ExitFunc = [WeakChara](bool bInterrupted)
	{
		if (AOrionChara* Chara = WeakChara.Get())
		{
			Chara->InteractWithProductionStop();
		}
	};

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::InteractWithProduction,
		ExecFunc,
		CheckFunc,
		DescFunc,
		ExitFunc // Pass ExitFunc
	);

	if (TargetActor)
	{
		AddingAction.Params.TargetActorId = TargetActor->GetSerializable().GameId;
	}
	AddingAction.Params.OrionActionType = EOrionAction::InteractWithProduction;

	return AddingAction;
}

FOrionAction AOrionChara::InitActionCollectCargo(const FString& ActionName, AOrionActorStorage* TargetActor)
{
	TWeakObjectPtr<AOrionChara> WeakChara(this);
	TWeakObjectPtr<AOrionActorStorage> WeakTarget(TargetActor);

	// 1. Define execution logic (keep original logic)
	auto ExecFunc = [WeakChara, WeakTarget](float DeltaTime) -> EActionStatus
	{
		AOrionChara* CharaPtr = WeakChara.Get();
		AOrionActorStorage* TargetPtr = WeakTarget.Get();

		if (!CharaPtr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[CollectCargo] CharaPtr is null -> Returning Finished"));
			return EActionStatus::Finished;
		}
		if (!IsValid(TargetPtr))
		{
			UE_LOG(LogTemp, Warning, TEXT("[CollectCargo] TargetPtr is invalid -> Returning Finished"));
			return EActionStatus::Finished;
		}

		if (CharaPtr->LogisticsComp)
		{
			bool bFinished = CharaPtr->LogisticsComp->CollectingCargo(TargetPtr);
			EActionStatus Result = bFinished ? EActionStatus::Skipped : EActionStatus::Running;
			// UE_LOG(LogTemp, Log, TEXT("[CollectCargo] CollectingCargo returned: %s -> ActionStatus: %s"), 
			// 	bFinished ? TEXT("true (done)") : TEXT("false (running)"),
			// 	(Result == EActionStatus::Skipped) ? TEXT("Skipped") : TEXT("Running"));
			return Result;
		}
		UE_LOG(LogTemp, Warning, TEXT("[CollectCargo] LogisticsComp is null -> Returning Finished"));
		return EActionStatus::Finished;
	};

	// 2. Define check logic (this is the reason shown to UI)
	// [Fix 9] Updated CheckFunc signature
	auto CheckFunc = [WeakChara, WeakTarget](FString& OutReason) -> EActionValidity
	{
		AOrionChara* Chara = WeakChara.Get();
		if (!Chara) 
		{
			OutReason = TEXT("Character Invalid");
			return EActionValidity::PermanentInvalid;
		}
		AOrionActorStorage* Target = WeakTarget.Get();

		// Check 1: Target exists
		if (!IsValid(Target))
		{
			OutReason = TEXT("Target Destroyed");
			return EActionValidity::PermanentInvalid;
		}

		// Check 2: Target interactable
		if (Target->ActorStatus == EActorStatus::NotInteractable)
		{
			OutReason = TEXT("Target Not Interactable");
			return EActionValidity::TemporarySkip;
		}

		// Empty storage is fine, we want to put items into it.
		// Full inventory is fine, we are going to unload at storage.

		return EActionValidity::Valid; // Condition met
	};

	// [New] Status Description Function
	auto DescFunc = [WeakChara]() -> FString
	{
		AOrionChara* Chara = WeakChara.Get();
		if (!Chara || !Chara->LogisticsComp) return TEXT("");
		
		UOrionLogisticsComponent* Log = Chara->LogisticsComp;
		if (Log->BIsTrading)
		{
			FString StepName = (Log->TradeStep == ETradingCargoState::ToSource) ? TEXT("Moving to Source") :
							   (Log->TradeStep == ETradingCargoState::Pickup) ? TEXT("Picking up") :
							   (Log->TradeStep == ETradingCargoState::ToDestination) ? TEXT("Moving to Dest") :
							   TEXT("Dropping off");
			
			if (Log->TradeSegments.IsValidIndex(Log->CurrentSegIndex))
			{
				const auto& Seg = Log->TradeSegments[Log->CurrentSegIndex];
				if (IsValid(Seg.Source) && IsValid(Seg.Destination))
				{
					if (Log->TradeStep == ETradingCargoState::ToSource || Log->TradeStep == ETradingCargoState::Pickup)
						return FString::Printf(TEXT("%s: %s"), *StepName, *Seg.Source->GetName());
					else
						return FString::Printf(TEXT("%s: %s"), *StepName, *Seg.Destination->GetName());
				}
			}
			return StepName;
		}
		return TEXT("Searching...");
	};

	// [Fix] Add OnExit - CRITICAL for Double Drop issue
	auto ExitFunc = [WeakChara](bool bInterrupted)
	{
		if (AOrionChara* Chara = WeakChara.Get())
		{
			if (Chara->LogisticsComp)
			{
				Chara->LogisticsComp->CollectingCargoStop();
			}
		}
	};

	// 3. Construct Action (pass CheckFunc, DescFunc, and ExitFunc)
	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::CollectCargo,
		ExecFunc,
		CheckFunc,
		DescFunc,
		ExitFunc // Pass ExitFunc
	);

	if (TargetActor)
	{
		AddingAction.Params.TargetActorId = TargetActor->GetSerializable().GameId;
	}
	AddingAction.Params.OrionActionType = EOrionAction::CollectCargo;

	return AddingAction;
}

FOrionAction AOrionChara::InitActionCollectBullets(const FString& ActionName)
{
	TWeakObjectPtr<AOrionChara> WeakChara(this);

	// [Fix 9] CheckFunc for CollectBullets
	auto CheckFunc = [WeakChara](FString& OutReason) -> EActionValidity
	{
		if (!WeakChara.IsValid())
		{
			OutReason = TEXT("Character Invalid");
			return EActionValidity::PermanentInvalid;
		}
		return EActionValidity::Valid;
	};

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::CollectBullets,
		[WeakChara](float DeltaTime) -> EActionStatus
		{
			if (AOrionChara* CharaPtr = WeakChara.Get())
			{
				bool bFinished = CharaPtr->CollectBullets();
				return bFinished ? EActionStatus::Finished : EActionStatus::Running;
			}
			return EActionStatus::Finished;
		},
		CheckFunc // [Fix 9] Pass CheckFunc
	);

	AddingAction.Params.OrionActionType = EOrionAction::CollectBullets;

	return AddingAction;
}

// [New] InitActionInteractWithInventory
FOrionAction AOrionChara::InitActionInteractWithInventory(const FString& ActionName, AOrionActor* TargetActor)
{
	FGuid TargetID;
	if (TargetActor) TargetID = TargetActor->ActorSerializable.GameId;

	FOrionAction Action(
		ActionName,
		EOrionAction::InteractWithStorage,
		[this, TargetActor](float DeltaTime) -> EActionStatus
		{
			bool bFinished = InteractWithInventory(TargetActor);
			return bFinished ? EActionStatus::Finished : EActionStatus::Running;
		}
	);

	Action.Params.OrionActionType = EOrionAction::InteractWithStorage;
	Action.Params.TargetActorId = TargetID;

	Action.ValidityCheckFunction = [this, TargetActor](FString& OutReason) -> EActionValidity
	{
		if (!TargetActor || !TargetActor->IsValidLowLevel())
		{
			OutReason = TEXT("Inventory Target Invalid");
			return EActionValidity::PermanentInvalid;
		}
		return EActionValidity::Valid;
	};

	return Action;
}

ESelectable AOrionChara::GetSelectableType() const
{
	return ESelectable::OrionChara;
}

void AOrionChara::OnSelected(APlayerController* PlayerController)
{
	bIsOrionAIControlled = true;
}

void AOrionChara::OnRemoveFromSelection(APlayerController* PlayerController)
{
	bIsOrionAIControlled = false;
}

bool AOrionChara::CollectBullets()
{
	constexpr int32 BulletItemId = 3;
	constexpr int32 MaxCarry = 300;

	// 0) Ensure inventory comp
	if (!InventoryComp)
	{
		InventoryComp = FindComponentByClass<UOrionInventoryComponent>();
		if (!InventoryComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("CollectBullets: no InventoryComp on chara"));
			return true;
		}
	}

	// 1) Initialization: pick nearest production actor that has bullets
	if (!IsCollectingBullets)
	{
		AOrionActor* Best = LogisticsComp ? LogisticsComp->FindClosetAvailableCargoContainer(BulletItemId) : nullptr;

		if (!Best)
		{
			return true;
		}

		BulletSource = Best; // [Fix 3] WeakPtr assignment
		IsCollectingBullets = true;
	}

	// 2) Abort if source destroyed
	if (!BulletSource.IsValid())
	{
		IsCollectingBullets = false;
		return true;
	}

	// 3) Already full?
	{
		int32 Have = InventoryComp->GetItemQuantity(BulletItemId);
		if (Have >= MaxCarry)
		{
			IsCollectingBullets = false;
			return true;
		}
	}

	// 4) Are we at the source?
	bool bAtSource = false;
	if (AOrionActor* SourcePtr = BulletSource.Get())
	{
		if (auto* Sphere = SourcePtr->FindComponentByClass<USphereComponent>())
		{
			bAtSource = Sphere->IsOverlappingActor(this);
		}
	}

	if (bAtSource)
	{
		if (MovementComp) MovementComp->MoveToLocationStop();

		// if montage not yet playing, start it
		if (!IsBulletPickupAnimPlaying && BulletPickupMontage)
		{
			IsBulletPickupAnimPlaying = true;

			// play at a rate so it finishes in BulletPickupDuration
			float RawLength = BulletPickupMontage->GetPlayLength();
			float PlayRate = RawLength > 0.f
				                 ? RawLength / BulletPickupDuration
				                 : 1.f;

			if (auto* AnimInst = GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_Play(BulletPickupMontage, PlayRate);
			}

			// schedule the actual ammo transfer for when the montage ends
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle_BulletPickup,
				this, &AOrionChara::OnBulletPickupFinished,
				BulletPickupDuration,
				false
			);
		}

		// while montage is playing, stay in this action
		return false;
	}

	// 5) Not at source yet → move toward it
	if (AOrionActor* SourcePtr = BulletSource.Get())
	{
		if (MovementComp) MovementComp->MoveToLocation(SourcePtr->GetActorLocation());
	}
	return false;
}

void AOrionChara::OnBulletPickupFinished()
{
	constexpr int32 BulletItemId = 3;
	constexpr int32 MaxCarry = 300;

	IsBulletPickupAnimPlaying = false;

	// actually transfer as many as we can
	AOrionActor* SourcePtr = BulletSource.Get();
	if (!IsValid(SourcePtr) || !InventoryComp)
	{
		IsCollectingBullets = false;
		return;
	}

	auto* SrcInv = SourcePtr->InventoryComp;
	int32 Available = SrcInv->GetItemQuantity(BulletItemId);
	int32 Have = InventoryComp->GetItemQuantity(BulletItemId);
	int32 SpaceLeft = MaxCarry - Have;
	int32 ToTake = FMath::Min(Available, SpaceLeft);

	if (ToTake > 0)
	{
		SrcInv->ModifyItemQuantity(BulletItemId, -ToTake);
		InventoryComp->ModifyItemQuantity(BulletItemId, ToTake);
	}

	// done!
	IsCollectingBullets = false;
}

bool AOrionChara::InteractWithProduction(float DeltaTime,
                                         AOrionActorProduction* InTargetProduction, bool bPreferStorageFirst)
{
	// === Configuration: true = First check Storage, then check Ore; false = First check Ore, then check Storage ===

	bPreparingInteractProd = true;

	// ① Invalid or un interactable when stopped
	if (!IsValid(InTargetProduction) ||
		InTargetProduction->ActorStatus == EActorStatus::NotInteractable)
	{
		if (bPreparingInteractProd) { InteractWithProductionStop(); }
		if (bIsInteractProd)
		{
			InteractWithActorStop(InteractWithActorState);
			bIsInteractProd = false;
		}
		return true;
	}

	// ② Check production point raw material
	constexpr int32 RawItemId = 2;
	constexpr int32 NeedPerCycle = 2;

	UOrionInventoryComponent* ProdInv = InTargetProduction->InventoryComp;
	int32 HaveProd = ProdInv ? ProdInv->GetItemQuantity(RawItemId) : 0;

	// ③ Raw material insufficient triggers transport
	if (HaveProd < NeedPerCycle)
	{
		// ③.1 Priority use from character's backpack
		if (InventoryComp &&
			InventoryComp->GetItemQuantity(RawItemId) >= NeedPerCycle)
		{
			if (bIsInteractProd) { InteractWithActorStop(InteractWithActorState); }

			TMap<AActor*, TMap<int32, int32>> Route;
			Route.Add(this, {{RawItemId, NeedPerCycle}});
			Route.Add(InTargetProduction, {{}});
		if (LogisticsComp && !LogisticsComp->TradingCargo(Route))
		{
			return false; // Transport not completed, continue this Action
		}
		}

		// ③.2 Field container transport
		AOrionActorOre* BestOre = nullptr;
		float BestOreDist = TNumericLimits<float>::Max();
		AOrionActorStorage* BestStore = nullptr;
		float BestStoreDist = TNumericLimits<float>::Max();

		// Get InventoryManager to access AllInventoryComponents
		UOrionInventoryManager* InvManager = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UOrionInventoryManager>() : nullptr;
		if (!InvManager)
		{
			return true;
		}

		// If prioritize Storage
		if (bPreferStorageFirst)
		{
			// Check Storage - Use InventoryManager instead of TActorIterator
			for (UOrionInventoryComponent* Inv : InvManager->AllInventoryComponents)
			{
				if (!IsValid(Inv)) continue;
				
				AOrionActorStorage* S = Cast<AOrionActorStorage>(Inv->GetOwner());
				// [Fix] Add IsHidden check to filter out preview objects
				if (!IsValid(S) || S->IsHidden()) continue;
				
				if (Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
				{
					continue;
				}
				float d2 = FVector::DistSquared(S->GetActorLocation(),
				                                InTargetProduction->GetActorLocation());
				if (d2 < BestStoreDist)
				{
					BestStoreDist = d2;
					BestStore = S;
				}
			}
			// If no Storage, then check Ore
			if (!BestStore)
			{
				for (UOrionInventoryComponent* Inv : InvManager->AllInventoryComponents)
				{
					if (!IsValid(Inv)) continue;
					
					AOrionActorOre* O = Cast<AOrionActorOre>(Inv->GetOwner());
					// [Fix] Add IsHidden check to filter out preview objects
					if (!IsValid(O) || O->IsHidden()) continue;
					
					if (Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
					{
						continue;
					}
					float d2 = FVector::DistSquared(O->GetActorLocation(),
					                                InTargetProduction->GetActorLocation());
					if (d2 < BestOreDist)
					{
						BestOreDist = d2;
						BestOre = O;
					}
				}
			}
		}
		else // Priority Ore
		{
			for (UOrionInventoryComponent* Inv : InvManager->AllInventoryComponents)
			{
				if (!IsValid(Inv)) continue;
				
				AOrionActorOre* O = Cast<AOrionActorOre>(Inv->GetOwner());
				// [Fix] Add IsHidden check to filter out preview objects
				if (!IsValid(O) || O->IsHidden()) continue;
				
				if (Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
				{
					continue;
				}
				float d2 = FVector::DistSquared(O->GetActorLocation(),
				                                InTargetProduction->GetActorLocation());
				if (d2 < BestOreDist)
				{
					BestOreDist = d2;
					BestOre = O;
				}
			}
			if (!BestOre)
			{
				for (UOrionInventoryComponent* Inv : InvManager->AllInventoryComponents)
				{
					if (!IsValid(Inv)) continue;
					
					AOrionActorStorage* S = Cast<AOrionActorStorage>(Inv->GetOwner());
					// [Fix] Add IsHidden check to filter out preview objects
					if (!IsValid(S) || S->IsHidden()) continue;
					
					if (Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
					{
						continue;
					}
					float d2 = FVector::DistSquared(S->GetActorLocation(),
					                                InTargetProduction->GetActorLocation());
					if (d2 < BestStoreDist)
					{
						BestStoreDist = d2;
						BestStore = S;
					}
				}
			}
		}

		// No source when exiting
		AActor* ChosenSource =
			BestStore
				? static_cast<AActor*>(BestStore)
				: static_cast<AActor*>(BestOre);

		if (!ChosenSource) { return true; }

		// Calculate transport amount
		auto* SrcInv = ChosenSource->FindComponentByClass<UOrionInventoryComponent>();
		int32 SrcHave = SrcInv ? SrcInv->GetItemQuantity(RawItemId) : 0;
		int32 MaxCanPut = ProdInv && ProdInv->AvailableInventoryMap.Contains(RawItemId)
			                  ? ProdInv->AvailableInventoryMap[RawItemId]
			                  : TNumericLimits<int32>::Max();
		int32 ToMove = FMath::Min(SrcHave, MaxCanPut);
		if (ToMove < NeedPerCycle) { return true; }

		if (bIsInteractProd) { InteractWithActorStop(InteractWithActorState); }

		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(ChosenSource, {{RawItemId, ToMove}});
		Route.Add(InTargetProduction, {{}});
		if (LogisticsComp && !LogisticsComp->TradingCargo(Route))
		{
			return false;
		}
	}

	// ④ Raw material preparation complete, start production interaction
	bIsInteractProd = true;
	return InteractWithActor(DeltaTime, InTargetProduction);
}

void AOrionChara::InteractWithProductionStop()
{
	// If already in "formal interaction" stage, need to reduce production point worker count
	if (IsInteractWithActor)
	{
		InteractWithActorStop(InteractWithActorState);
	}
	else
	{
		if (MovementComp) MovementComp->MoveToLocationStop();
	}

	// Stop "formal interaction" state
	IsInteractWithActor = false;


	// Reset "preparing interaction" mark
	bPreparingInteractProd = false;

	// Ensure Logistics is stopped
	if (LogisticsComp)
	{
		LogisticsComp->CollectingCargoStop();
	}

	// Clear current production target
	//CurrentInteractProduction = nullptr;
}

bool AOrionChara::InteractWithActor(float DeltaTime, AOrionActor* InTarget)
{
	// 1) Invalid target immediately terminate
	if (!IsValid(InTarget))
	{
		InteractWithActorStop(InteractWithActorState);
		return true;
	}

	// 2) not interactable immediately terminate
	if (InTarget->ActorStatus == EActorStatus::NotInteractable)
	{
		InteractWithActorStop(InteractWithActorState);
		return true;
	}

	// 3) check SphereComponent once
	const USphereComponent* CollisionSphere = InTarget->CollisionSphere;
	const bool bOverlapping = CollisionSphere && CollisionSphere->IsOverlappingActor(this);


	switch (InteractWithActorState)
	{
	case EInteractWithActorState::Unavailable:
		{
			// [Fix] Ensure IsInteractWithActor is false in Unavailable state
			IsInteractWithActor = false;
			if (!bOverlapping)
			{
				InteractWithActorState = EInteractWithActorState::MovingToTarget;
				// [Fix 8] Immediately start moving, no longer wait for next frame
				if (MovementComp) MovementComp->MoveToLocation(InTarget->GetActorLocation());
				return false;
			}
			/* Overlapped */


			CurrentInteractActor = InTarget;
			const bool InitActionRes = InteractWithActorStart(InteractWithActorState);
			InteractWithActorState = EInteractWithActorState::Interacting;
			// [Fix] Immediately update IsInteractWithActor when entering Interacting state
			// This ensures animation state machine gets the correct value even in procedural mode
			IsInteractWithActor = true;
			return InitActionRes;
		}
		break;
	case EInteractWithActorState::MovingToTarget:
		{
			// [Fix] Ensure IsInteractWithActor is false while moving
			IsInteractWithActor = false;
			if (!bOverlapping)
			{
				if (MovementComp) MovementComp->MoveToLocation(InTarget->GetActorLocation());
				return false;
			}
			/* Overlapped */
			CurrentInteractActor = InTarget;
			const bool InitActionRes = InteractWithActorStart(InteractWithActorState);
			InteractWithActorState = EInteractWithActorState::Interacting;
			// [Fix] Immediately update IsInteractWithActor when entering Interacting state
			// This ensures animation state machine gets the correct value even in procedural mode
			IsInteractWithActor = true;
			return InitActionRes;
		}
		break;
	case EInteractWithActorState::Interacting:
		{
			if (!bOverlapping)
			{
				InteractWithActorStop(InteractWithActorState);
				return true; // Interaction ended
			}
			/* Overlapped */
			// [Fix] Ensure IsInteractWithActor remains true while interacting
			// This is important for procedural mode where the function may not be called every frame
			IsInteractWithActor = true;
			return false;
		}
		break;
	}


	return false;
}

bool AOrionChara::SetInteractingAnimation()
{
	if (const FString& TargetType = CurrentInteractActor->GetInteractType(); TargetType == TEXT("Mining"))
	{
		InteractAnimationKind = EInteractCategory::Mining;
	}
	else if (TargetType == TEXT("CraftingBullets"))
	{
		InteractAnimationKind = EInteractCategory::CraftingBullets;
	}
	else
	{
		InteractAnimationKind = EInteractCategory::Unavailable;
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: Unavailable interact type."));
		InteractWithActorStop(InteractWithActorState);
		return true;
	}

	// First arrival, trigger start interaction

	if (InteractAnimationKind == EInteractCategory::Mining)
	{
		SpawnAxeOnChara();
	}
	else // CraftingBullets
	{
		SpawnHammerOnChara();
	}
	return false;
}

bool AOrionChara::InteractWithActorStart(const EInteractWithActorState& State)
{
	/* Set Interacting Animation */
	if (bool SetInteractingAnimationFailed = SetInteractingAnimation())
	{
		InteractWithActorStop(InteractWithActorState);
		return true; // Animation set failed, stop interaction
	}

	if (State == EInteractWithActorState::MovingToTarget)
	{
		if (MovementComp) MovementComp->MoveToLocationStop();
	}

	CurrentInteractActor->CurrWorkers += 1;

	FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
		GetActorLocation(),
		CurrentInteractActor->GetActorLocation()
	);
	LookAtRot.Pitch = 0;
	LookAtRot.Roll = 0;
	SetActorRotation(LookAtRot);

	/* 播放对应的动画蒙太奇 */
	UAnimMontage* MontageToPlay = nullptr;

	// 根据 SetInteractingAnimation 设置好的 Enum 决定播放哪个蒙太奇
	switch (InteractAnimationKind)
	{
	case EInteractCategory::Mining:
		MontageToPlay = Montage_Mining;
		break;
	case EInteractCategory::CraftingBullets:
		MontageToPlay = Montage_Crafting;
		break;
	default:
		break;
	}

	if (MontageToPlay)
	{
		// 播放蒙太奇 (如果蒙太奇设置了 Loop Section，它会自动循环直到被打断)
		PlayAnimMontage(MontageToPlay);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActorStart: No Montage assigned for InteractKind: %d"), (int32)InteractAnimationKind);
	}

	return false;
}

void AOrionChara::InteractWithActorStop(EInteractWithActorState& State)
{
	if (State == EInteractWithActorState::Interacting)
	{
		StopAnimMontage();

		if (CurrentInteractActor)
		{
			CurrentInteractActor->CurrWorkers -= 1;
		}

		if (InteractAnimationKind == EInteractCategory::Mining)
		{
			RemoveAxeOnChara();
		}
		else if (InteractAnimationKind == EInteractCategory::CraftingBullets)
		{
			RemoveHammerOnChara();
		}
	}
	else if (State == EInteractWithActorState::MovingToTarget)
	{
		if (MovementComp) MovementComp->MoveToLocationStop();
	}

	State = EInteractWithActorState::Unavailable;
	CurrentInteractActor = nullptr;
	InteractAnimationKind = EInteractCategory::Unavailable;
}

/* TradingCargo and animation callbacks moved to UOrionLogisticsComponent */

void AOrionChara::RemoveAllActions(const FString& Except)
{
	// Component will clear actions, and next frame Tick will detect Type change to Undefined,
	// triggering OnActionTypeChangedHandler which calls SwitchingStateHandle to stop components.
	if (ActionComp) ActionComp->RemoveAllActions(Except);
}

void AOrionChara::OnActionNameChangedHandler(FString PrevName, FString CurrName)
{
	OnCharaActionChange.Broadcast(PrevName, CurrName);
}

// [Fix 7] Unified cleanup function for Die and Incapacitate
void AOrionChara::CleanupState()
{
	RemoveAllActions();
	if (CombatComp)
	{
		CombatComp->AttackOnCharaLongRangeStop();
	}
	if (LogisticsComp)
	{
		LogisticsComp->CollectingCargoStop();
	}

	if (StimuliSourceComp)
	{
		if (UAISense_Sight::StaticClass())
		{
			StimuliSourceComp->UnregisterFromSense(UAISense_Sight::StaticClass());
			UE_LOG(LogTemp, Log, TEXT("Unregistered from UAISense_Sight."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAISense_Sight::StaticClass() returned nullptr."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("StimuliSourceComp is null in CleanupState()."));
	}

	if (OrionAIControllerInstance)
	{
		OrionAIControllerInstance->StopMovement();
		OrionAIControllerInstance->UnPossess();
		UE_LOG(LogTemp, Log, TEXT("OrionAIControllerInstance stopped and unpossessed."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionAIControllerInstance is null in CleanupState()."));
	}
}

void AOrionChara::HandleHealthZero(AActor* InstigatorActor)
{
	Incapacitate();
}

void AOrionChara::Die()
{
	// 终结时清理延迟死亡计时器
	GetWorldTimerManager().ClearTimer(TimerHandle_DieAfterIncapacitated);

	CharaState = ECharaState::Dead;

	UE_LOG(LogTemp, Log, TEXT("AOrionChara::Die() called."));

	CleanupState(); // Call unified cleanup

	CleanAffiliatedObjects();

	Destroy();
	UE_LOG(LogTemp, Log, TEXT("AOrionChara destroyed."));
}

void AOrionChara::Incapacitate()
{
	CharaState = ECharaState::Incapacitated;

	CleanupState(); // Call unified cleanup

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(true);
	}

	if (UCapsuleComponent* CharaCapsuleComponent = GetCapsuleComponent())
	{
		CharaCapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 进入失能状态后 2 秒自动死亡
	GetWorldTimerManager().ClearTimer(TimerHandle_DieAfterIncapacitated);
	GetWorldTimerManager().SetTimer(
		TimerHandle_DieAfterIncapacitated,
		this,
		&AOrionChara::Die,
		2.0f,
		false);
}


std::vector<AOrionChara*> AOrionChara::GetOtherCharasByProximity() const
{
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), StaticClass(), AllActors);

	std::vector<AOrionChara*> Enemies;
	Enemies.reserve(AllActors.Num());
	for (AActor* Actor : AllActors)
	{
		if (Actor == this)
		{
			continue;
		}

		if (AOrionChara* Other = Cast<AOrionChara>(Actor))
		{
			// [Refactor] Use Faction System via AttributeComp instead of CharaSide
			if (AttributeComp && Other->AttributeComp)
			{
				if (const UOrionFactionManager* FactionManager = GetGameInstance()->GetSubsystem<UOrionFactionManager>())
				{
					if (FactionManager->IsHostile(AttributeComp->ActorFaction, Other->AttributeComp->ActorFaction))
					{
						Enemies.push_back(Other);
					}
				}
			}
		}
	}

	const FVector MyLocation = GetActorLocation();
	std::sort(Enemies.begin(), Enemies.end(),
	          [MyLocation](AOrionChara* A, AOrionChara* B)
	          {
		          const float DA = FVector::DistSquared(MyLocation, A->GetActorLocation());
		          const float DB = FVector::DistSquared(MyLocation, B->GetActorLocation());
		          return DA < DB;
	          });

	return Enemies;
}

void AOrionChara::ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex)
{
	if (ActionComp)
	{
		ActionComp->ReorderProceduralAction(DraggedIndex, DropIndex);
	}
}

void AOrionChara::RemoveProceduralActionAt(int32 Index)
{
	if (ActionComp)
	{
		ActionComp->RemoveProceduralActionAt(Index);
	}
}

bool AOrionChara::InteractWithInventory(AOrionActor* OrionActor)
{
	if (!IsValid(OrionActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithInventory: invalid actor"));

		return true;
	}

	if (!bIsOrionAIControlled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithInventory: not controlled"));

		if (MovementComp) MovementComp->MoveToLocationStop();

		return true;
	}

	USphereComponent* CollisionSphere = OrionActor->CollisionSphere;
	const bool bOverlapping = CollisionSphere && CollisionSphere->IsOverlappingActor(this);

	if (bOverlapping)
	{
		if (MovementComp) MovementComp->MoveToLocationStop();

		OnInteractWithInventory.ExecuteIfBound(OrionActor);

		return true;
	}
	if (MovementComp) MovementComp->MoveToLocation(OrionActor->GetActorLocation());

	return false;
}

float AOrionChara::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	AActor* DamageCauser)
{
	const float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (AttributeComp)
	{
		AttributeComp->ReceiveDamage(DamageApplied, EventInstigator ? EventInstigator->GetPawn() : DamageCauser);
	}

	return DamageApplied;
}

void AOrionChara::SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal,
	UStaticMesh* ArrowMesh)
{
	if (CombatComp)
	{
		CombatComp->SpawnArrowPenetrationEffect(HitLocation, HitNormal, ArrowMesh);
	}
}

void AOrionChara::InitOrionCharaMovement()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		// Don't rotate character to camera direction
		bUseControllerRotationPitch = false;
		bUseControllerRotationYaw = false;
		bUseControllerRotationRoll = false;

		// Configure character movement
		MovementComponent->bOrientRotationToMovement = true;
		MovementComponent->RotationRate = FRotator(0.f, 270.f * 3.f, 0.f);
		MovementComponent->bConstrainToPlane = true;
		MovementComponent->bSnapToPlaneAtStart = true;

		MovementComponent->MaxAcceleration = 10000.0f;

		// Speed is now managed by MovementComp
		if (MovementComp)
		{
			MovementComponent->MaxWalkSpeed = MovementComp->OrionCharaSpeed;
		}
	}

	if (GetMesh())
	{
		DefaultMeshRelativeLocation = GetMesh()->GetRelativeLocation();
		DefaultMeshRelativeRotation = GetMesh()->GetRelativeRotation();
		DefaultCapsuleMeshOffset = GetCapsuleComponent()->GetComponentLocation() - GetMesh()->GetComponentLocation();
	}
}

void AOrionChara::RegisterCharaRagdoll(float DeltaTime)
{
	if (CharaState == ECharaState::Ragdoll)
	{
		SynchronizeCapsuleCompLocation();

		if (RagdollWakeupAccumulatedTime >= RagdollWakeupThreshold)
		{
			RagdollWakeupAccumulatedTime = 0;
			RagdollWakeup();
		}
		else
		{
			RagdollWakeupAccumulatedTime += DeltaTime;
		}
	}
}

void AOrionChara::SynchronizeCapsuleCompLocation()
{
	if (GetMesh() && GetMesh()->IsSimulatingPhysics())
	{
		// 1. 获取 Mesh 盆骨（核心位置）的世界坐标
		FVector PelvisLocation = GetMesh()->GetSocketLocation(TEXT("pelvis"));

		// 2. 调整胶囊体的高度
		// 因为胶囊体的中心点是在半个高度处，而躺在地上的盆骨非常低。
		// 如果直接把胶囊体设在盆骨位置，胶囊体会陷进地里一半。
		float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		// 我们希望胶囊体底部略低于盆骨，或者做一个射线检测找到地面（更严谨做法）
		// 这里用简易做法：假设盆骨在地面，胶囊体中心 = 盆骨位置 + (0, 0, HalfHeight)
		// 但为了防止穿墙，通常需要做一个向下的射线检测找到真正的地面
		FVector TargetLocation = PelvisLocation;

		// --- 进阶：防止穿地逻辑 ---
		FHitResult Hit;
		FVector TraceStart = PelvisLocation + FVector(0, 0, 50.0f);
		FVector TraceEnd = PelvisLocation - FVector(0, 0, 100.0f); // 向下探测
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this); // 忽略自己

		if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			// 如果探测到地面，将胶囊体中心放在地面之上 HalfHeight 的位置
			TargetLocation = Hit.ImpactPoint + FVector(0, 0, CapsuleHalfHeight);
		}
		else
		{
			// 没探测到地面（可能飞在空中），直接跟盆骨，但修正高度以免相机太低
			TargetLocation.Z += CapsuleHalfHeight * 0.5f;
		}

		// 3. 更新胶囊体位置
		// 注意：bSweep=true，这样如果撞墙了就会停下，防止Mesh穿墙导致胶囊体也卡进墙里
		SetActorLocation(TargetLocation, true);
	}
}

void AOrionChara::ForceDetectionOnVelocityChange()
{
	const FVector CurrentVelocity = GetVelocity();

	if (const float VelocityChange = (CurrentVelocity - PreviousVelocity).Size() > VelocityChangeThreshold)
	{
		OnForceExceeded(CurrentVelocity - PreviousVelocity);
		UE_LOG(LogTemp, Warning, TEXT("Velocity change exceeded threshold: %f"), VelocityChange);
		//BlueprintNativeVelocityExceeded();
	}

	PreviousVelocity = CurrentVelocity;
}

/*
void AOrionChara::OnForceExceeded(const FVector& DeltaVelocity)
{
	const float DeltaVSize = DeltaVelocity.Size();
	UE_LOG(LogTemp, Warning, TEXT("Force exceeded! Delta Velocity: %f"), DeltaVSize);

	const float Mass = GetMesh()->GetMass();

	constexpr float DeltaT = 0.0166f;

	// F = m * Δv / Δt  
	const float ApproximatedForce = Mass * DeltaVSize / DeltaT / 20.0f;
	UE_LOG(LogTemp, Warning, TEXT("Approximated Impulse Force: %f"), ApproximatedForce);

	const FVector CurrVel = GetVelocity();

	Ragdoll();

	GetMesh()->SetPhysicsLinearVelocity(CurrVel);

	const FVector ImpulseToAdd = DeltaVelocity.GetSafeNormal() * ApproximatedForce * DeltaT;

	// Log the physics simulation status of GetMesh()  
	if (GetMesh())
	{
		const bool bIsSimulatingPhysics = GetMesh()->IsSimulatingPhysics();
		UE_LOG(LogTemp, Warning, TEXT("Physics Simulation Status of GetMesh(): %s"),
		       bIsSimulatingPhysics ? TEXT("Enabled") : TEXT("Disabled"));
	}

	GetMesh()->AddImpulse(ImpulseToAdd, NAME_None, true);
}
*/

void AOrionChara::OnForceExceeded(const FVector& DeltaVelocity)
{
	// 1. 记录当前基础速度 (用于平滑过渡)
	const FVector CurrVel = GetVelocity();

	// 2. 开启物理模拟 (Ragdoll)
	Ragdoll();

	// 3. 确保 Mesh 已经开启物理
	if (GetMesh() && GetMesh()->IsSimulatingPhysics())
	{
		// --- 核心逻辑开始 ---

		// A. 继承胶囊体之前的速度，避免瞬间停顿
		GetMesh()->SetAllPhysicsLinearVelocity(CurrVel);

		// B. 计算虚拟撞击点 (Virtual Impact Point)
		// 逻辑：如果速度向右(DeltaVelocity)，说明撞击点在左边(-DeltaVelocity)

		// 获取力的方向
		FVector ImpulseDir = DeltaVelocity.GetSafeNormal();

		// 获取角色中心位置 (或者使用 GetMesh()->GetCenterOfMass())
		FVector CenterLocation = GetMesh()->GetComponentLocation();

		// 获取胶囊体半径，用于把点推到身体表面
		float CapsuleRadius = 35.0f;
		if (GetCapsuleComponent())
		{
			CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
		}

		// C. 制造随机性 (关键步骤)
		// 为了让布娃娃每次倒下的姿势不同，我们在高度上加一点随机偏移
		// 这样有时会打中头部(后仰)，有时打中腿部(前扑)
		float RandomHeightOffset = FMath::RandRange(-150.0f, 150.0f);
		FVector RandomOffset(0, 0, RandomHeightOffset);

		// 计算最终的虚拟撞击点位置：
		// 中心点 + (反方向 * 半径) + 随机高度
		FVector VirtualImpactPoint = CenterLocation - (ImpulseDir * CapsuleRadius) + RandomOffset;

		// D. 应用带位置的冲量 (Option B)
		// 注意：使用 bVelChange=true，直接把 DeltaVelocity 作为速度变化量施加，
		// 这样就不用关心质量(Mass)和时间(DeltaT)的换算了，手感最准。
		GetMesh()->AddImpulseAtLocation(DeltaVelocity, VirtualImpactPoint, NAME_None);

		// --- 核心逻辑结束 ---

		// Debug 线条：画出撞击点位置 (红色) 和 力的方向 (绿色)
		// DrawDebugSphere(GetWorld(), VirtualImpactPoint, 10.0f, 12, FColor::Red, false, 3.0f);
		// DrawDebugLine(GetWorld(), VirtualImpactPoint, VirtualImpactPoint + DeltaVelocity, FColor::Green, false, 3.0f, 0, 2.0f);

		UE_LOG(LogTemp, Warning, TEXT("Applied Ragdoll Impulse at Virtual Point: %s"), *VirtualImpactPoint.ToString());
	}
}

void AOrionChara::Ragdoll()
{
	RemoveAllActions();

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(true);
	}

	CharaState = ECharaState::Ragdoll;
}

void AOrionChara::RagdollWakeup()
{
	if (!GetMesh() || !GetCapsuleComponent()) return;

	UE_LOG(LogTemp, Log, TEXT("AOrionChara::RagdollWakeup() - Starting Recovery Sequence"));

	// =================================================================================
	// 步骤 A: 获取盆骨数据
	// =================================================================================
	FVector PelvisLoc = GetMesh()->GetSocketLocation(TEXT("pelvis"));
	FRotator PelvisRot = GetMesh()->GetSocketRotation(TEXT("pelvis"));

	// =================================================================================
	// 步骤 B: 判断朝向 (Face Up vs Face Down)
	// =================================================================================
	// 获取盆骨的 X轴 (Forward Vector，通常是从肚子指出去的方向)
	FVector PelvisForwardVec = FRotationMatrix(PelvisRot).GetUnitAxis(EAxis::X);

	// 如果 Z > 0，说明肚子指向上方 (仰卧)；反之则为俯卧
	bool bIsFaceUp = PelvisForwardVec.Z > 0.0f;

	UE_LOG(LogTemp, Log, TEXT("Ragdoll Orientation Check: Pelvis Z = %f, IsFaceUp = %s"),
		PelvisForwardVec.Z, bIsFaceUp ? TEXT("TRUE") : TEXT("FALSE"));

	// 选择对应的蒙太奇
	UAnimMontage* MontageToPlay = bIsFaceUp ? GetUpMontage_FaceUp : GetUpMontage_FaceDown;

	// =================================================================================
	// 步骤 C: 计算落地位置 (防止穿地)
	// =================================================================================
	FHitResult HitResult;
	FVector TraceStart = PelvisLoc + FVector(0.f, 0.f, 50.f);
	FVector TraceEnd = PelvisLoc - FVector(0.f, 0.f, 150.f);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FVector NewCapsuleLocation = PelvisLoc;
	float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
	{
		NewCapsuleLocation = HitResult.ImpactPoint + FVector(0.f, 0.f, CapsuleHalfHeight);
	}
	else
	{
		NewCapsuleLocation.Z += CapsuleHalfHeight;
	}

	// =================================================================================
	// 步骤 D: 同步胶囊体位置与旋转
	// =================================================================================
	FRotator NewCapsuleRotation = GetActorRotation();
	NewCapsuleRotation.Yaw = PelvisRot.Yaw; // 保持角色倒下的朝向

	// 特殊处理：如果是俯卧 (Face Down)，有时候需要根据动画资源反转180度
	// 这取决于你的动画制作方式。如果播放俯卧起身时发现角色朝向反了，可以取消下面这行的注释：
	// if (!bIsFaceUp) { NewCapsuleRotation.Yaw += 180.0f; }

	SetActorLocationAndRotation(NewCapsuleLocation, NewCapsuleRotation, false, nullptr, ETeleportType::TeleportPhysics);

	// =================================================================================
	// 步骤 E: 关闭物理并复位 Mesh
	// =================================================================================
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetIncludingScale);

	// 强制复位 Mesh 到胶囊体中心
	GetMesh()->SetRelativeLocation(DefaultMeshRelativeLocation);
	GetMesh()->SetRelativeRotation(DefaultMeshRelativeRotation);
	GetMesh()->RefreshBoneTransforms();

	// =================================================================================
	// 步骤 F: 播放动画
	// =================================================================================
	if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
	{
		if (MontageToPlay)
		{
			// 可选：在这里根据不同的起身速度调整 PlayRate
			// float PlayRate = 1.0f;
			AnimInst->Montage_Play(MontageToPlay, 1.0f);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("RagdollWakeup: No Montage assigned for %s state!"),
				bIsFaceUp ? TEXT("FaceUp") : TEXT("FaceDown"));
		}
	}

	// =================================================================================
	// 步骤 G: 恢复状态
	// =================================================================================
	CharaState = ECharaState::Alive;
	RagdollWakeupAccumulatedTime = 0.0f;
	PreviousVelocity = FVector::ZeroVector;
	GetVelocity();
}

void AOrionChara::ChangeMaxWalkSpeed(float InValue)
{
	if (MovementComp)
	{
		MovementComp->ChangeMaxWalkSpeed(InValue);
	}
}
