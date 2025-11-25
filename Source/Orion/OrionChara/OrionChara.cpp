// OrionChara.cpp

#include "OrionChara.h"
#include "Orion/OrionAIController/OrionAIController.h"
#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "DrawDebugHelpers.h"
#include "Orion/OrionHUD/OrionUserWidgetCharaInfo.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
#include <algorithm>
#include "TimerManager.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include <Components/SphereComponent.h>
#include "Orion/OrionActor/OrionActorOre.h"
#include "Orion/OrionComponents/OrionLogisticsComponent.h"
#include "Orion/OrionComponents/OrionMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/PlayerController.h"

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
	CharaActionComp = CreateDefaultSubobject<UOrionCharaActionComponent>(TEXT("CharaActionComp"));
	LogisticsComp = CreateDefaultSubobject<UOrionLogisticsComponent>(TEXT("LogisticsComp"));
	CombatComp = CreateDefaultSubobject<UOrionCombatComponent>(TEXT("CombatComp"));
	MovementComp = CreateDefaultSubobject<UOrionMovementComponent>(TEXT("MovementComp"));

	/* Init Static Value */
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

void AOrionChara::SerializeCharaStats()
{
	UE_LOG(LogTemp, Log, TEXT("Saving Chara: %s"), *GetName());

	/* Serialize Procedural Actions */
	for (const auto& ProcAction : CharacterProcActionQueue.Actions)
	{
		CharaSerializable.CharaGameId = GameSerializable.GameId;
		CharaSerializable.CharaLocation = GetActorLocation();
		CharaSerializable.CharaRotation = GetActorRotation();
		CharaSerializable.SerializedProcActions.Add(ProcAction.Params);
	}
}

void AOrionChara::BeginPlay()
{
	Super::BeginPlay();

	InitSerializable(GameSerializable);

	AIController = Cast<AOrionAIController>(GetController());

	InitOrionCharaMovement();

	CurrHealth = MaxHealth;

	this->SetCanBeDamaged(true);
}

void AOrionChara::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);


	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}

	/* Physics Handle - See Movement Module */
	// RegisterCharaRagdoll(DeltaTime);

	// ForceDetectionOnVelocityChange();

	/* AI Controlling */

	const FString PrevName = LastActionName;
	const EOrionAction PrevType = LastActionType;

	DistributeCharaAction(DeltaTime);

	const FString CurrName = GetUnifiedActionName();
	const EOrionAction CurrType = GetUnifiedActionType();

	if (PrevType != CurrType)
	{
		SwitchingStateHandle(PrevType, CurrType);
	}

	if (PrevName != CurrName)
	{
		OnCharaActionChange.Broadcast(PrevName, CurrName);
	}

	LastActionName = CurrName;
	LastActionType = CurrType;


	/* Refresh Attack Frequency */

	if (CombatComp)
	{
		CombatComp->RefreshAttackFrequency();
	}
	IsInteractWithActor = InteractWithActorState == EInteractWithActorState::Interacting;
}

FString AOrionChara::GetUnifiedActionName() const
{
	// choose between procedural vs real-time
	if (bIsCharaProcedural)
	{
		return CurrentProcAction ? CurrentProcAction->Name : TEXT("");
	}
	return CurrentAction ? CurrentAction->Name : TEXT("");
}

EOrionAction AOrionChara::GetUnifiedActionType() const
{
	if (bIsCharaProcedural)
	{
		return CurrentProcAction ? CurrentProcAction->GetActionType() : EOrionAction::Undefined;
	}
	return CurrentAction ? CurrentAction->GetActionType() : EOrionAction::Undefined;
}

bool AOrionChara::GetIsCharaProcedural()
{
	return bIsCharaProcedural;
}

bool AOrionChara::SetIsCharaProcedural(bool bInIsCharaProcedural)
{
	bIsCharaProcedural = bInIsCharaProcedural;
	return bIsCharaProcedural;
}

void AOrionChara::InsertOrionActionToQueue(
	const FOrionAction& OrionActionInstance,
	const EActionExecution ActionExecutionType,
	const int32 Index)
{
	auto& Actions = (ActionExecutionType == EActionExecution::Procedural)
		                ? CharacterProcActionQueue.Actions
		                : CharacterActionQueue.Actions;

	if (Index == INDEX_NONE || Index < 0 || Index > Actions.Num())
	{
		Actions.Add(OrionActionInstance);
	}
	else
	{
		Actions.Insert(OrionActionInstance, Index);
	}
}

void AOrionChara::DistributeCharaAction(float DeltaTime)
{
	if (bIsCharaProcedural)
	{
		DistributeProceduralAction(DeltaTime);
	}
	else
	{
		DistributeRealTimeAction(DeltaTime);
	}
}

void AOrionChara::DistributeRealTimeAction(float DeltaTime)
{
	if (CurrentAction)
	{
		PreviousAction = CurrentAction;
	}

	if (!CharacterActionQueue.Actions.IsEmpty())
	{
		CurrentAction = CharacterActionQueue.GetFrontAction();
	}
	else
	{
		CurrentAction = nullptr;
	}

	if (CurrentAction && CurrentAction->ExecuteFunction(DeltaTime))
	{
		CharacterActionQueue.PopFrontAction();
		CurrentAction = nullptr;
	}
}

void AOrionChara::DistributeProceduralAction(float DeltaTime)
{
	if (CurrentProcAction)
	{
		PreviousProcAction = CurrentProcAction;
	}

	for (auto& ProcAction : CharacterProcActionQueue.Actions)
	{
		bool bInterrupted = ProcAction.ExecuteFunction(DeltaTime);

		if (!bInterrupted)
		{
			CurrentProcAction = &ProcAction;
			break;
		}
	}
}

void AOrionChara::SwitchingStateHandle(EOrionAction PrevType, EOrionAction CurrType)
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

FOrionAction AOrionChara::InitActionMoveToLocation(const FString& ActionName, const FVector& TargetLocation)
{
	// Use TWeakObjectPtr to capture this safely
	TWeakObjectPtr<AOrionChara> WeakChara(this);
	// Capture TargetLocation by value
	FVector CapturedLocation = TargetLocation;

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::MoveToLocation,
		[WeakChara, CapturedLocation](float DeltaTime) -> bool
		{
			// Check if pointer is valid before execution
			if (AOrionChara* Chara = WeakChara.Get())
			{
				if (Chara->MovementComp) // Delegate to component
				{
					return Chara->MovementComp->MoveToLocation(CapturedLocation);
				}
			}
			// If pointer is invalid (character is dead/destroyed), return true to indicate action should be removed
			return true;
		}
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

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::AttackOnChara,
		[WeakChara, WeakTarget, inHitOffset = HitOffset](float DeltaTime) -> bool
		{
			AOrionChara* CharaPtr = WeakChara.Get();
			AActor* TargetPtr = WeakTarget.Get();

			if (CharaPtr && TargetPtr && CharaPtr->CombatComp)
			{
				return CharaPtr->CombatComp->AttackOnChara(DeltaTime, TargetPtr, inHitOffset);
			}
			return true;
		}
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

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::InteractWithActor,
		[WeakChara, WeakTarget](float DeltaTime) -> bool
		{
			if (AOrionChara* CharaPtr = WeakChara.Get())
			{
				// If target is invalid, tell Character to stop interaction logic (clean up state)
				if (!WeakTarget.IsValid())
				{
					// Can choose to call a cleanup function here, or directly return true
					return true;
				}
				return CharaPtr->InteractWithActor(DeltaTime, WeakTarget.Get());
			}
			return true;
		}
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

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::InteractWithProduction,
		[WeakChara, WeakTarget](float DeltaTime) -> bool
		{
			AOrionChara* CharaPtr = WeakChara.Get();
			AOrionActorProduction* TargetPtr = WeakTarget.Get();

			if (CharaPtr && TargetPtr)
			{
				return CharaPtr->InteractWithProduction(DeltaTime, TargetPtr);
			}
			return true;
		}
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

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::CollectCargo,
		[WeakChara, WeakTarget](float DeltaTime) -> bool
		{
			if (AOrionChara* CharaPtr = WeakChara.Get())
			{
				if (CharaPtr->LogisticsComp)
				{
					return CharaPtr->LogisticsComp->CollectingCargo(WeakTarget.Get());
				}
			}
			return true;
		}
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

	FOrionAction AddingAction = FOrionAction(
		ActionName,
		EOrionAction::CollectBullets,
		[WeakChara](float DeltaTime) -> bool
		{
			if (AOrionChara* CharaPtr = WeakChara.Get())
			{
				return CharaPtr->CollectBullets();
			}
			return true;
		}
	);

	AddingAction.Params.OrionActionType = EOrionAction::CollectBullets;

	return AddingAction;
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
	if (!bIsCollectingBullets)
	{
		AOrionActor* Best = LogisticsComp ? LogisticsComp->FindClosetAvailableCargoContainer(BulletItemId) : nullptr;

		if (!Best)
		{
			return true;
		}

		BulletSource = Best;
		bIsCollectingBullets = true;
	}

	// 2) Abort if source destroyed
	if (!IsValid(BulletSource))
	{
		bIsCollectingBullets = false;
		return true;
	}

	// 3) Already full?
	{
		int32 Have = InventoryComp->GetItemQuantity(BulletItemId);
		if (Have >= MaxCarry)
		{
			bIsCollectingBullets = false;
			return true;
		}
	}

	// 4) Are we at the source?
	bool bAtSource = false;
	if (auto* Sphere = BulletSource->FindComponentByClass<USphereComponent>())
	{
		bAtSource = Sphere->IsOverlappingActor(this);
	}

	if (bAtSource)
	{
		if (MovementComp) MovementComp->MoveToLocationStop();

		// if montage not yet playing, start it
		if (!bBulletPickupAnimPlaying && BulletPickupMontage)
		{
			bBulletPickupAnimPlaying = true;

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
	if (MovementComp) MovementComp->MoveToLocation(BulletSource->GetActorLocation());
	return false;
}

void AOrionChara::OnBulletPickupFinished()
{
	constexpr int32 BulletItemId = 3;
	constexpr int32 MaxCarry = 300;

	bBulletPickupAnimPlaying = false;

	// actually transfer as many as we can
	if (!IsValid(BulletSource) || !InventoryComp)
	{
		bIsCollectingBullets = false;
		return;
	}

	auto* SrcInv = BulletSource->InventoryComp;
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
	bIsCollectingBullets = false;
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

		// If prioritize Storage
		if (bPreferStorageFirst)
		{
			// Check Storage
			for (TActorIterator<AOrionActorStorage> It(GetWorld()); It; ++It)
			{
				auto* S = *It;
				if (!IsValid(S))
				{
					continue;
				}
				auto* Inv = S->InventoryComp;
				if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
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
				for (TActorIterator<AOrionActorOre> It(GetWorld()); It; ++It)
				{
					auto* O = *It;
					if (!IsValid(O))
					{
						continue;
					}
					auto* Inv = O->InventoryComp;
					if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
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
			for (TActorIterator<AOrionActorOre> It(GetWorld()); It; ++It)
			{
				auto* O = *It;
				if (!IsValid(O))
				{
					continue;
				}
				auto* Inv = O->InventoryComp;
				if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
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
				for (TActorIterator<AOrionActorStorage> It(GetWorld()); It; ++It)
				{
					auto* S = *It;
					if (!IsValid(S))
					{
						continue;
					}
					auto* Inv = S->InventoryComp;
					if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
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
	UE_LOG(LogTemp, Warning, TEXT("[IP] 231331"));
	if (IsInteractWithActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithProductionStop: bIsInteractProd = true"));
		InteractWithActorStop(InteractWithActorState);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithProductionStop: bIsInteractProd = false"));
		if (MovementComp) MovementComp->MoveToLocationStop();
	}

	// Stop "formal interaction" state
	IsInteractWithActor = false;


	// Reset "preparing interaction" mark
	bPreparingInteractProd = false;

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
			if (!bOverlapping)
			{
				InteractWithActorState = EInteractWithActorState::MovingToTarget;
				return false;
			}
			/* Overlapped */


			CurrentInteractActor = InTarget;
			const bool InitActionRes = InteractWithActorStart(InteractWithActorState);
			InteractWithActorState = EInteractWithActorState::Interacting;
			return InitActionRes;
		}
		break;
	case EInteractWithActorState::MovingToTarget:
		{
			if (!bOverlapping)
			{
				if (MovementComp) MovementComp->MoveToLocation(InTarget->GetActorLocation());
				return false;
			}
			/* Overlapped */
			CurrentInteractActor = InTarget;
			const bool InitActionRes = InteractWithActorStart(InteractWithActorState);
			InteractWithActorState = EInteractWithActorState::Interacting;
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

	return false;
}

void AOrionChara::InteractWithActorStop(EInteractWithActorState& State)
{
	if (State == EInteractWithActorState::Interacting)
	{
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
	if (CurrentAction)
	{
		FString OngoingActionName = CurrentAction->Name;

		if (OngoingActionName.Contains("ForceMoveToLocation") || OngoingActionName.Contains("MoveToLocation"))
		{
			if (MovementComp) MovementComp->MoveToLocationStop();
		}

		if (OngoingActionName.Contains("ForceAttackOnCharaLongRange") || OngoingActionName.Contains(
			"AttackOnCharaLongRange"))
		{
			if (CombatComp)
			{
				CombatComp->AttackOnCharaLongRangeStop();
			}
		}

		if (OngoingActionName.Contains("ForceInteractWithActor") || OngoingActionName.Contains("InteractWithActor"))
		{
			InteractWithActorStop(InteractWithActorState);
		}

		CurrentAction = nullptr;
	}

	CharacterActionQueue.Actions.Empty();
}

void AOrionChara::Die()
{
	CharaState = ECharaState::Dead;

	UE_LOG(LogTemp, Log, TEXT("AOrionChara::Die() called."));

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
		UE_LOG(LogTemp, Warning, TEXT("StimuliSourceComp is null in Die()."));
	}

	// 3. StopAI control
	if (AIController)
	{
		AIController->StopMovement();
		AIController->UnPossess();
		UE_LOG(LogTemp, Log, TEXT("AIController stopped and unpossessed."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIController is null in Die()."));
	}

	Destroy();
	UE_LOG(LogTemp, Log, TEXT("AOrionChara destroyed."));
}

void AOrionChara::Incapacitate()
{
	CharaState = ECharaState::Incapacitated;

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
		UE_LOG(LogTemp, Warning, TEXT("StimuliSourceComp is null in Die()."));
	}

	if (AIController)
	{
		AIController->StopMovement();
		AIController->UnPossess();
		UE_LOG(LogTemp, Log, TEXT("AIController stopped and unpossessed."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIController is null in Die()."));
	}

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
			if (Other->CharaSide != CharaSide)
			{
				Enemies.push_back(Other);
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
	auto& CharaProcQueueActionsRef = CharacterProcActionQueue.Actions;

	int32 Count = CharaProcQueueActionsRef.Num();
	if (DraggedIndex < 0 || DraggedIndex >= Count)
	{
		return;
	}

	FOrionAction ActionRef = MoveTemp(CharaProcQueueActionsRef[DraggedIndex]);
	CharaProcQueueActionsRef.RemoveAt(DraggedIndex);

	int32 NewIndex = DropIndex;
	if (DropIndex > DraggedIndex)
	{
		NewIndex = DropIndex - 1;
	}

	NewIndex = FMath::Clamp(NewIndex, 0, CharaProcQueueActionsRef.Num());

	CharaProcQueueActionsRef.Insert(MoveTemp(ActionRef), NewIndex);
}

void AOrionChara::RemoveProceduralActionAt(int32 Index)
{
	auto& CharaProcQueueActionsRef = CharacterProcActionQueue.Actions;
	if (Index >= 0 && Index < CharaProcQueueActionsRef.Num())
	{
		CharaProcQueueActionsRef.RemoveAt(Index);
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

		bIsInteractWithInventory = true;

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

	CurrHealth -= DamageApplied;

	//UE_LOG(LogTemp, Log, TEXT("AOrionChara took %f damage. Current Health: %f"), DamageApplied, CurrHealth);

	/*
	// 检查 DamageEvent 是否为径向伤害事件
	if (DamageEvent.GetTypeID() == FRadialDamageEvent::ClassID)
	{
		UE_LOG(LogTemp, Log, TEXT("DamageEvent is a radial damage event."));
		// 将 DamageEvent 转换为 FRadialDamageEvent
		const FRadialDamageEvent& RadialDamageEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);

		// 计算角色与爆炸点的距离
		float Distance = FVector::Dist(RadialDamageEvent.Origin, GetActorLocation());

		// 计算力的大小
		float MaxForce = DamageAmount; // 这里假设 DamageAmount 与最大力成正比
		float ForceMagnitude = FMath::Max(0.f, MaxForce * (1 - Distance / RadialDamageEvent.Params.OuterRadius));

		UE_LOG(LogTemp, Warning, TEXT("Calculated ForceMagnitude: %f at Distance: %f"), ForceMagnitude, Distance);

		// 如果力超过阈值，调用 OnForceExceeded
		if (ForceMagnitude > ForceThreshold)
		{
			// 计算力的方向
			FVector ForceDirection = GetActorLocation() - RadialDamageEvent.Origin;
			ForceDirection = ForceDirection.GetSafeNormal();

			// 调用 OnForceExceeded，传递力向量
			OnForceExceeded(ForceDirection * ForceMagnitude);
		}
	}
	*/


	if (CurrHealth <= 0.0f)
	{
		Incapacitate();
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
		MovementComponent->RotationRate = FRotator(0.f, 270.f * 1.2f, 0.f);
		MovementComponent->bConstrainToPlane = true;
		MovementComponent->bSnapToPlaneAtStart = true;

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

void AOrionChara::SynchronizeCapsuleCompLocation() const
{
	const FName RootBone = GetMesh()->GetBoneName(0);
	const FVector MeshRootLocation = GetMesh()->GetBoneLocation(RootBone);

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		const FVector NewCapsuleLocation = MeshRootLocation + DefaultCapsuleMeshOffset;
		CapsuleComp->SetWorldLocation(NewCapsuleLocation);
	}
}

void AOrionChara::ForceDetectionOnVelocityChange()
{
	const FVector CurrentVelocity = GetVelocity();

	if (const float VelocityChange = (CurrentVelocity - PreviousVelocity).Size() > VelocityChangeThreshold)
	{
		//OnForceExceeded(CurrentVelocity - PreviousVelocity);
		BlueprintNativeVelocityExceeded();
	}

	PreviousVelocity = CurrentVelocity;
}

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

void AOrionChara::Ragdoll()
{
	RemoveAllActions();

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
	CharaState = ECharaState::Alive;

	UE_LOG(LogTemp, Warning, TEXT("AOrionChara::RagdollWakeup() called."));

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(false);
	}

	FRotator CurrentRot = GetActorRotation();
	FRotator UprightRot(0.0f, CurrentRot.Yaw, 0.0f);
	SetActorRotation(UprightRot);

	if (GetMesh())
	{
		GetMesh()->SetRelativeLocation(DefaultMeshRelativeLocation);
		GetMesh()->SetRelativeRotation(DefaultMeshRelativeRotation);

		GetMesh()->RefreshBoneTransforms();

		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->InitializeAnimation();
		}
	}
}

void AOrionChara::ChangeMaxWalkSpeed(float InValue)
{
	if (MovementComp)
	{
		MovementComp->ChangeMaxWalkSpeed(InValue);
	}
}
