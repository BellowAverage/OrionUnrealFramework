// OrionChara.cpp

#include "OrionChara.h"
#include "OrionAIController.h"

#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Navigation/PathFollowingComponent.h"

#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

#include "DrawDebugHelpers.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
#include <algorithm>
#include "TimerManager.h"

//#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include <Components/SphereComponent.h>

/*
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "OrionGameMode.h"
#include "OrionWeapon.h"
#include "Animation/SkeletalMeshActor.h"
#include "OrionBPFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
*/


AOrionChara::AOrionChara()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AOrionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;


	StimuliSourceComp = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComp"));
	StimuliSourceComp->RegisterForSense(TSubclassOf<UAISense_Sight>());
	StimuliSourceComp->bAutoRegister = true;


	SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;

	//CharacterActionQueue.Actions.reserve(100);
}

void AOrionChara::BeginPlay()
{
	Super::BeginPlay();

	// UE_LOG(LogTemp, Log, TEXT("%s has been constructed."), *GetName());

	AIController = Cast<AOrionAIController>(GetController());

	/*
	if (AIController)
	{
		UE_LOG(LogTemp, Log, TEXT("OrionChara is now controlled by OrionAIController"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionChara is not controlled by OrionAIController"));

		if (AIControllerClass)
		{
			UE_LOG(LogTemp, Log, TEXT("AIControllerClass is set to: %s"), *AIControllerClass->GetName());

			if (UWorld* World = GetWorld())
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Instigator = GetInstigator();
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				UE_LOG(LogTemp, Log, TEXT("Attempting to spawn AIController of class: %s"),
				       *AIControllerClass->GetName());

				AIController = World->SpawnActor<AOrionAIController>(AIControllerClass, SpawnParams);
				if (AIController)
				{
					AIController->Possess(this);
					UE_LOG(LogTemp, Log, TEXT("OrionAIController has been spawned and possessed the OrionChara."));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to spawn OrionAIController. Skipping related logic."));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("World context is invalid. Skipping related logic."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT("AIControllerClass is not set. Please assign it in the constructor or editor."));
		}
	}
	*/

	InitOrionCharaMovement();

	CurrHealth = MaxHealth;
	this->SetCanBeDamaged(true);
}

void AOrionChara::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//UE_LOG(LogTemp, Log, TEXT("AOrionChara::Tick()"));

	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}

	/* Physics Handle - See Movement Module */

	// RegisterCharaRagdoll(DeltaTime);

	// ForceDetectionOnVelocityChange();

	/* AI Controlling */

	const FString PrevName = LastNonEmptyActionName;

	/*
	UE_LOG(LogTemp, Warning,
	       TEXT("[DEBUG] %s PrevAction='%s'"),
	       *GetName(),
	       PrevName.IsEmpty() ? TEXT("None") : *PrevName
	);
	*/

	DistributeCharaAction(DeltaTime);

	const FString CurrName = GetUnifiedActionName();

	/*
	UE_LOG(LogTemp, Warning,
	       TEXT("[DEBUG] %s CurrAction='%s'"),
	       *GetName(),
	       CurrName.IsEmpty() ? TEXT("None") : *CurrName
	);
	*/

	// if both non-empty and changed, invoke state transition
	if (!PrevName.IsEmpty() && !CurrName.IsEmpty() && PrevName != CurrName)
	{
		UE_LOG(LogTemp, Log,
		       TEXT("SwitchingStateHandle: %s -> %s"),
		       *PrevName, *CurrName
		);
		SwitchingStateHandle(PrevName, CurrName);
	}

	if (!CurrName.IsEmpty())
	{
		LastNonEmptyActionName = CurrName;
	}


	/* Refresh Attack Frequency */

	RefreshAttackFrequency();
}

FString AOrionChara::GetUnifiedActionName() const
{
	// choose between procedural vs real‐time
	if (bIsCharaProcedural)
	{
		return CurrentProcAction ? CurrentProcAction->Name : TEXT("");
	}
	return CurrentAction ? CurrentAction->Name : TEXT("");
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

	if (CurrentAction == nullptr && !CharacterActionQueue.IsEmpty())
	{
		CurrentAction = CharacterActionQueue.GetFrontAction();
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

void AOrionChara::SwitchingStateHandle(const FString& Prev, const FString& Curr)
{
	if (Prev.Contains(TEXT("InteractWithActor")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		InteractWithActorStop();
	}
}

bool AOrionChara::MoveToLocation(FVector InTargetLocation)
{
	/*
	static auto DoOnce = []() -> bool
	    {
	        static bool bHasExecuted = false;
	        if (!bHasExecuted)
	        {
	            bHasExecuted = true;
	            return true;
	        }
	        return false;
	    };
	*/

	if (bIsPlayingMontage)
	{
		OnEquipWeaponMontageInterrupted();
		bIsPlayingMontage = false;
	}

	if (AIController)
	{
		FNavAgentProperties AgentProperties = AIController->GetNavAgentPropertiesRef();

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		FNavLocation ProjectedLocation;
		if (NavSys && NavSys->ProjectPointToNavigation(InTargetLocation, ProjectedLocation))
		{
			// ProjectedLocation.Location 就是投影到可行走区域后的坐标，Z 值将是地面或 NavMesh 的真实高度
			InTargetLocation = ProjectedLocation.Location;
		}

		EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
			InTargetLocation, 5.0f, true, true, true, false, nullptr, true);

		/*
		    if (DoOnce())
		    {
		        // 根据请求结果处理逻辑
		        if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
		        {
		            UE_LOG(LogTemp, Log, TEXT("MoveToLocation called successfully with TargetLocation: %s"), *InTargetLocation.ToString());
		        }
		        else if (RequestResult == EPathFollowingRequestResult::Failed)
		        {
		            UE_LOG(LogTemp, Warning, TEXT("MoveToLocation failed for TargetLocation: %s"), *InTargetLocation.ToString());
		        }
		        else if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
		        {
		            UE_LOG(LogTemp, Log, TEXT("Already at TargetLocation: %s"), *InTargetLocation.ToString());
		        }
		    }
		*/
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIController is not assigned!"));
	}

	FVector AjustedActorLocation = GetActorLocation();
	AjustedActorLocation.Z -= 85.f - 1.548;

	return FVector::Dist(AjustedActorLocation, InTargetLocation) < 60.0f;
}

void AOrionChara::MoveToLocationStop()
{
	AOrionAIController* OrionAC = Cast<AOrionAIController>(GetController());
	if (OrionAC)
	{
		OrionAC->StopMovement();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveToLocationStop: OrionAC is nullptr or not of type AOrionAIController."));
	}
}

bool AOrionChara::InteractWithActor(float DeltaTime, AOrionActor* InTarget)
{
	// UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is being interacted."));

	if (!IsValid(InTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not valid."));

		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}

		IsInteractWithActor = false;
		return true;
	}

	if (InTarget->ActorStatus == EActorStatus::NotInteractable)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not Interactable."));

		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}

		IsInteractWithActor = false;
		return true;
	}

	USphereComponent* CollisionSphere = InTarget->FindComponentByClass<USphereComponent>();

	if (CollisionSphere && CollisionSphere->IsOverlappingActor(this))
	{
		// Determine the type of interaction

		FString TargetInteractType = InTarget->GetInteractType();
		if (TargetInteractType == "Mining")
		{
			InteractType = EInteractType::Mining;
		}
		else
		{
			InteractType = EInteractType::Unavailable;
			UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: Unavailable interact type."));

			if (IsInteractWithActor)
			{
				InteractWithActorStop();
			}

			IsInteractWithActor = false;
			return true;
		}

		if (!IsInteractWithActor)
		{
			IsInteractWithActor = true;
			CurrentInteractActor = InTarget;
			InTarget->CurrWorkers += 1;
			//InTarget->ArrInteractingCharas.Add(this);
		}

		if (AIController)
		{
			AIController->StopMovement();
		}

		FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			GetActorLocation(),
			InTarget->GetActorLocation()
		);

		LookAtRot.Pitch = 0.0f;
		LookAtRot.Roll = 0.0f;

		SetActorRotation(LookAtRot);

		return false;
	}

	if (AIController)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (NavSys)
		{
			FNavLocation ProjectedLocation;

			FVector SearchExtent(500.0f, 500.0f, 500.0f);

			if (NavSys->ProjectPointToNavigation(
				InTarget->GetActorLocation(),
				ProjectedLocation,
				SearchExtent
			))
			{
				EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
					ProjectedLocation.Location,
					20.0f,
					true,
					true,
					true,
					false,
					nullptr,
					true
				);
			}
		}
	}

	if (IsInteractWithActor)
	{
		InteractWithActorStop();
	}

	IsInteractWithActor = false;
	return false;
}

void AOrionChara::InteractWithActorStop()
{
	if (!CurrentInteractActor)
	{
		return;
	}

	IsInteractWithActor = false;
	DoOnceInteractWithActor = false;
	CurrentInteractActor->CurrWorkers -= 1;
	//CurrentInteractActor->ArrInteractingCharas.Remove(this);
	CurrentInteractActor = nullptr;
	InteractType = EInteractType::Unavailable;
}

bool AOrionChara::TradeMoveToLocation(const FVector& Dest, float AcceptanceRadius)
{
	UE_LOG(LogTemp, Log, TEXT("[TradeMoveToLocation] Dest=%s, Radius=%.1f"), *Dest.ToString(), AcceptanceRadius);

	if (AIController)
	{
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Nav)
		{
			FNavLocation Projected;
			// 加上一个搜索范围，让它在 Dest 附近（比如上下各 500，高宽各 500）找 NavMesh
			const FVector SearchExtent(500.f, 500.f, 500.f);
			if (Nav->ProjectPointToNavigation(Dest, Projected, SearchExtent))
			{
				UE_LOG(LogTemp, Log, TEXT("[TradeMoveToLocation] Projected to NavMesh at %s"),
				       *Projected.Location.ToString());
				AIController->MoveToLocation(Projected.Location, AcceptanceRadius, /*StopOnOverlap=*/true);
				return false; // 还在移动
			}
			UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] Projection to NavMesh failed even with extent"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] No NavigationSystem found"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] No AIController available"));
	}

	// 如果连搜索都失败，就强制告诉它已经到位，让流程往下走
	UE_LOG(LogTemp, Error, TEXT("[TradeMoveToLocation] Fallback: treat as arrived"));
	return true;
}


bool AOrionChara::TradingCargo(const TMap<AOrionActor*, TMap<int32, int32>>& TradeRoute)
{
	UE_LOG(LogTemp, Log, TEXT("========== TradingCargo Tick =========="));
	UE_LOG(LogTemp, Log, TEXT("  bIsTrading=%s, CurrentSegIndex=%d"),
	       bIsTrading ? TEXT("true") : TEXT("false"), CurrentSegIndex);

	// 1) Initialization
	if (!bIsTrading)
	{
		UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Initializing trade route"));
		TradeSegments.Empty();
		CurrentSegIndex = 0;
		TradeStep = ETradeStep::ToSource;
		bIsTrading = true;

		TArray<AOrionActor*> Nodes;
		Nodes.Reserve(TradeRoute.Num());
		for (auto& P : TradeRoute)
		{
			Nodes.Add(P.Key);
			UE_LOG(LogTemp, Log, TEXT("  Route node: %s"), *P.Key->GetName());
		}

		int32 Num = Nodes.Num();
		if (Num < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TradingCargo] Not enough nodes to form a loop"));
			bIsTrading = false;
			return true;
		}

		// expand into segments
		for (int32 i = 0; i < Num; ++i)
		{
			AOrionActor* Src = Nodes[i];
			AOrionActor* Dst = Nodes[(i + 1) % Num];
			const auto& Cargo = TradeRoute[Src];
			for (auto& C : Cargo)
			{
				FTradeSeg Seg;
				Seg.Source = Src;
				Seg.Dest = Dst;
				Seg.ItemId = C.Key;
				Seg.Quantity = C.Value;
				Seg.Moved = 0;
				TradeSegments.Add(Seg);
				UE_LOG(LogTemp, Log, TEXT("  Added segment: %s -> %s, Item=%d x%d"),
				       *Src->GetName(), *Dst->GetName(), Seg.ItemId, Seg.Quantity);
			}
		}
	}

	// 2) All done?
	if (CurrentSegIndex >= TradeSegments.Num())
	{
		UE_LOG(LogTemp, Log, TEXT("[TradingCargo] All segments completed"));
		bIsTrading = false;
		return true;
	}

	FTradeSeg& Seg = TradeSegments[CurrentSegIndex];
	UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Segment %d/%d, Step=%d"),
	       CurrentSegIndex, TradeSegments.Num(), int32(TradeStep));

	// validate actors
	if (!IsValid(Seg.Source) || !IsValid(Seg.Dest))
	{
		UE_LOG(LogTemp, Warning, TEXT("[TradingCargo] Source or Dest invalid, abort"));
		bIsTrading = false;
		return true;
	}

	// get sphere for overlap test
	USphereComponent* Sphere = nullptr;
	if (TradeStep == ETradeStep::ToSource || TradeStep == ETradeStep::Pickup)
	{
		Sphere = Seg.Source->FindComponentByClass<USphereComponent>();
	}
	else
	{
		Sphere = Seg.Dest->FindComponentByClass<USphereComponent>();
	}

	bool bAtNode = Sphere && Sphere->IsOverlappingActor(this);
	UE_LOG(LogTemp, Log, TEXT("[TradingCargo] bAtNode=%s"), bAtNode ? TEXT("true") : TEXT("false"));

	switch (TradeStep)
	{
	case ETradeStep::ToSource:
		{
			if (bAtNode)
			{
				UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Arrived at Source: %s"), *Seg.Source->GetName());
				TradeStep = ETradeStep::Pickup;
				if (AIController)
				{
					AIController->StopMovement();
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Moving to Source: %s"), *Seg.Source->GetName());
				TradeMoveToLocation(Seg.Source->GetActorLocation());
			}
			return false;
		}

	case ETradeStep::Pickup:
		{
			auto* Inv = Seg.Source->FindComponentByClass<UOrionInventoryComponent>();
			int32 Available = Inv ? Inv->GetItemQuantity(Seg.ItemId) : 0;
			int32 ToTake = FMath::Min(Available, Seg.Quantity);
			UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Pickup: Available=%d, ToTake=%d"),
			       Available, ToTake);

			if (ToTake <= 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("[TradingCargo] Nothing left to take, abort"));
				bIsTrading = false;
				return true;
			}

			Inv->ModifyItemQuantity(Seg.ItemId, -ToTake);
			Seg.Moved = ToTake;
			UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Took %d of Item %d"), ToTake, Seg.ItemId);

			TradeStep = ETradeStep::ToDest;
			return false;
		}

	case ETradeStep::ToDest:
		{
			if (bAtNode)
			{
				UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Arrived at Dest: %s"), *Seg.Dest->GetName());
				TradeStep = ETradeStep::Dropoff;
				if (AIController)
				{
					AIController->StopMovement();
				}
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Moving to Dest: %s"), *Seg.Dest->GetName());
				TradeMoveToLocation(Seg.Dest->GetActorLocation());
			}
			return false;
		}

	case ETradeStep::Dropoff:
		{
			auto* InvDst = Seg.Dest->FindComponentByClass<UOrionInventoryComponent>();
			InvDst->ModifyItemQuantity(Seg.ItemId, Seg.Moved);
			UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Dropped %d of Item %d at %s"),
			       Seg.Moved, Seg.ItemId, *Seg.Dest->GetName());

			// next segment
			CurrentSegIndex++;
			TradeStep = ETradeStep::ToSource;
			return false;
		}
	}

	// should never reach here, but just in case
	UE_LOG(LogTemp, Error, TEXT("[TradingCargo] Unexpected step, aborting"));
	bIsTrading = false;
	return true;
}


void AOrionChara::RemoveAllActions(const FString& Except)
{
	if (CurrentAction)
	{
		FString OngoingActionName = CurrentAction->Name;

		if (OngoingActionName.Contains("ForceMoveToLocation") || OngoingActionName.Contains("MoveToLocation"))
		{
			if (!Except.Contains("TempDoNotStopMovement"))
			{
				MoveToLocationStop();
			}
		}

		if (OngoingActionName.Contains("ForceAttackOnCharaLongRange") || OngoingActionName.Contains(
			"AttackOnCharaLongRange"))
		{
			if (Except.Contains("SwitchAttackingTarget"))
			{
				IsAttackOnCharaLongRange = false;
				SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
			}
			else
			{
				AttackOnCharaLongRangeStop();
			}
		}

		if (OngoingActionName.Contains("ForceInteractWithActor") || OngoingActionName.Contains("InteractWithActor"))
		{
			InteractWithActorStop();
		}

		CurrentAction = nullptr;
	}

	CharacterActionQueue.Actions.clear();
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
	GetMesh()->AddImpulse(ImpulseToAdd, NAME_None, true);
}

void AOrionChara::Die()
{
	CharaState = ECharaState::Dead;

	UE_LOG(LogTemp, Log, TEXT("AOrionChara::Die() called."));

	RemoveAllActions();

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

	// 3. 停止AI控制
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

	if (AttachedArrowComponents.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("No attached arrow components to destroy."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Cleaning up %d attached arrow components."), AttachedArrowComponents.Num());

	for (auto& ArrowComp : AttachedArrowComponents)
	{
		if (ArrowComp && ArrowComp->IsValidLowLevel())
		{
			ArrowComp->DestroyComponent();
			UE_LOG(LogTemp, Log, TEXT("Destroyed an attached arrow component."));
		}
	}

	// Clear the array after destruction
	AttachedArrowComponents.Empty();

	Destroy();
	UE_LOG(LogTemp, Log, TEXT("AOrionChara destroyed."));
}

void AOrionChara::Incapacitate()
{
	CharaState = ECharaState::Incapacitated;

	RemoveAllActions();

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

void AOrionChara::AddItemToInventory(int ItemID, int Quantity)
{
	if (CharaInventoryMap.contains(ItemID))
	{
		CharaInventoryMap[ItemID] += Quantity;
	}
	else
	{
		CharaInventoryMap[ItemID] = Quantity;
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

bool AOrionChara::bIsLineOfSightBlocked(AActor* InTargetActor) const
{
	FHitResult Hit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AttackTrace), true);
	TraceParams.AddIgnoredActor(this);
	TraceParams.AddIgnoredActor(InTargetActor);

	/*
	for (TActorIterator<AOrionProjectile> It(GetWorld()); It; ++It)
	{
		TraceParams.AddIgnoredActor(*It);
	}
	*/

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		GetActorLocation(),
		InTargetActor->GetActorLocation(),
		ECC_Visibility,
		TraceParams
	);

	if (bHit)
	{
		if (Hit.GetActor())
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by %s."),
			       *Hit.GetActor()->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by an unknown object."));
		}
	}
	else
	{
		return true;
	}

	return false;
}

void AOrionChara::ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex)
{
	auto& Q = CharacterProcActionQueue.Actions;

	int32 Count = static_cast<int32>(Q.size());
	if (DraggedIndex < 0 || DraggedIndex >= Count)
	{
		return;
	}

	Action Temp = MoveTemp(Q[DraggedIndex]);
	Q.erase(Q.begin() + DraggedIndex);

	int32 NewIndex = DropIndex;
	if (DropIndex > DraggedIndex)
	{
		NewIndex = DropIndex - 1;
	}

	NewIndex = FMath::Clamp(NewIndex, 0, static_cast<int32>(Q.size()));

	Q.insert(Q.begin() + NewIndex, MoveTemp(Temp));
}

void AOrionChara::RemoveProceduralActionAt(int32 Index)
{
	auto& Q = CharacterProcActionQueue.Actions;
	if (Index >= 0 && Index < Q.size())
	{
		Q.erase(Q.begin() + Index);
	}
}
