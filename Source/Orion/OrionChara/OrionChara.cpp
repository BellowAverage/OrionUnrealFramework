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

	/* Init Static Value */
	SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
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

	DistributeCharaAction(DeltaTime);

	const FString CurrName = GetUnifiedActionName();

	if (!bCachedEmptyAction && CurrName.IsEmpty())
	{
		bCachedEmptyAction = true;
	}

	else if (bCachedEmptyAction)
	{
		if (PrevName != CurrName)
		{
			SwitchingStateHandle(PrevName, CurrName);
		}

		bCachedEmptyAction = false;
		LastActionName = CurrName;
	}

	else
	{
		if (PrevName != CurrName)
		{
			SwitchingStateHandle(PrevName, CurrName);
		}

		LastActionName = CurrName;
	}


	/* Refresh Attack Frequency */

	RefreshAttackFrequency();
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

void AOrionChara::SwitchingStateHandle(const FString& Prev, const FString& Curr)
{
	UE_LOG(LogTemp, Log,
	       TEXT("SwitchingStateHandle: %s -> %s"),
	       *Prev, *Curr
	);

	OnCharaActionChange.Broadcast(Prev, Curr);

	if (Prev.Contains(TEXT("InteractWithActor")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		InteractWithActorStop(InteractWithActorState);
	}
	else if (Prev.Contains(TEXT("InteractWithActor")) && Curr.IsEmpty())
	{
		InteractWithActorStop(InteractWithActorState);
	}
	else if (Prev.Contains(TEXT("InteractWithActor")))
	{
		InteractWithActorStop(InteractWithActorState);
	}

	else if (Prev.Contains(TEXT("InteractWithProduction")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		UE_LOG(LogTemp, Log, TEXT("111111"));
		InteractWithProductionStop();
	}
	else if (Prev.Contains(TEXT("InteractWithProduction")) && Curr.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("222222"));
		InteractWithProductionStop();
	}
	else if (Prev.Contains(TEXT("InteractWithProduction")))
	{
		UE_LOG(LogTemp, Log, TEXT("33333"));
		InteractWithProductionStop();
	}

	else if (Prev.Contains(TEXT("CollectingCargo")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		CollectingCargoStop();
	}
	else if (Prev.Contains(TEXT("CollectingCargo")) && Curr.IsEmpty())
	{
		CollectingCargoStop();
	}
	else if (Prev.Contains(TEXT("CollectingCargo")))
	{
		CollectingCargoStop();
	}

	else if (Prev.Contains(TEXT("InteractWithInventory")))
	{
		MoveToLocationStop();
	}
}

AOrionActor* AOrionChara::FindClosetAvailableCargoContainer(int32 ItemId) const
{
	UWorld* World = GetWorld();

	AOrionActor* ClosestAvailableActor = nullptr;

	for (TActorIterator<AOrionActor> It(World); It; ++It)
	{
		AOrionActor* OrionActor = *It;
		if (!OrionActor)
		{
			continue;
		}

		int32 Quantity = OrionActor->InventoryComp->GetItemQuantity(ItemId);

		if (Quantity > 0)
		{
			float Distance = FVector::Dist(GetActorLocation(), OrionActor->GetActorLocation());
			if (!ClosestAvailableActor || Distance < FVector::Dist(GetActorLocation(),
			                                                       ClosestAvailableActor->GetActorLocation()))
			{
				ClosestAvailableActor = OrionActor;
			}
		}
	}

	if (!ClosestAvailableActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindClosetAvailableCargoContainer: No available cargo container found."));
		return nullptr;
	}

	return ClosestAvailableActor;
}

TArray<AOrionActor*> AOrionChara::FindAvailableCargoContainersByDistance(int32 ItemId) const
{
	TArray<AOrionActor*> AvailableContainers;
	UWorld* World = GetWorld();
	if (!World)
	{
		return AvailableContainers;
	}

	for (TActorIterator<AOrionActor> It(World); It; ++It)
	{
		AOrionActor* OrionActor = *It;
		if (!OrionActor || !OrionActor->InventoryComp)
		{
			continue;
		}

		// Exclude Storage or Production
		if (OrionActor->IsA<AOrionActorStorage>() ||
			OrionActor->IsA<AOrionActorProduction>())
		{
			continue;
		}

		if (OrionActor->InventoryComp->GetItemQuantity(ItemId) > 0)
		{
			AvailableContainers.Add(OrionActor);
		}
	}

	if (AvailableContainers.Num() > 1)
	{
		AOrionActor** DataPtr = AvailableContainers.GetData();
		int32 Count = AvailableContainers.Num();

		std::sort(
			DataPtr,
			DataPtr + Count,
			[this](AOrionActor* A, AOrionActor* B)
			{
				const float DistA = FVector::DistSquared(GetActorLocation(), A->GetActorLocation());
				const float DistB = FVector::DistSquared(GetActorLocation(), B->GetActorLocation());
				return DistA < DistB;
			}
		);
	}

	return AvailableContainers;
}

bool AOrionChara::CollectingCargo(AOrionActorStorage* StorageActor)
{
	constexpr int32 StoneItemId = 2;

	// 1) Validate target storage
	if (!IsValid(StorageActor) ||
		StorageActor->StorageCategory != EStorageCategory::StoneStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: invalid storage or not StoneStorage"));
		return true;
	}

	if (this->InventoryComp->GetItemQuantity(StoneItemId) > 0)
	{
		int32 Have = InventoryComp
			             ? InventoryComp->GetItemQuantity(StoneItemId)
			             : 0;

		if (Have > 0)
		{
			// Build this -> Storage route (pickup from self, deliver to storage)
			TMap<AActor*, TMap<int32, int32>> SelfRoute;
			SelfRoute.Add(this, {{StoneItemId, Have}});
			SelfRoute.Add(StorageActor, {});

			// Let TradingCargo handle this segment
			bool bDone = TradingCargo(SelfRoute);
			if (!bDone)
			{
				// Still delivering self inventory, continue next frame
				return false;
			}
			// Delivery complete, mark it so we can proceed to field container logic below
		}
		bSelfDeliveryDone = true;
	}

	// 3) Each time BIsTrading==false, reselect next source and start new transport segment
	if (!BIsTrading)
	{
		// 3.1) All available ore on site + (excluding self, self inventory already delivered here)
		TArray<AOrionActor*> Sources =
			FindAvailableCargoContainersByDistance(
				StoneItemId
			);

		if (Sources.Num() == 0)
		{
			// Not even one - end
			UE_LOG(LogTemp, Log, TEXT("CollectingCargo: no more sources, done"));
			// Reset so next call can re-run self delivery
			bSelfDeliveryDone = false;
			return true;
		}

		// 3.2) Select the nearest one, build one segment Source -> Storage
		AOrionActor* NextSource = Sources[0];
		int32 Qty = NextSource->InventoryComp->GetItemQuantity(StoneItemId);

		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(NextSource, {{StoneItemId, Qty}});
		Route.Add(StorageActor, {});

		// Start this transport segment
		TradingCargo(Route);

		// Always return false, wait for TradingCargo state machine to progress itself
		return false;
	}

	// 4) BIsTrading is true, meaning the previous segment is executing → continue advancing
	TradingCargo(TMap<AActor*, TMap<int32, int32>>());
	return false;
}

void AOrionChara::CollectingCargoStop()
{
	// 1) Stop any pending pickup/dropoff animation callbacks
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Pickup);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Dropoff);
	}

	// 2) Reset TradingCargo state machine
	BIsTrading = false;
	BIsPickupAnimPlaying = false;
	BIsDropoffAnimPlaying = false;
	TradeSegments.Empty();
	CurrentSegIndex = 0;
	TradeStep = ETradingCargoState::ToSource;

	// 3) Reset "first deliver self inventory" mark
	//bSelfDeliveryDone = false;

	// 4) Stop navigation and clear path
	MoveToLocationStop(); // Internal will clear NavPathPoints and CurrentNavPointIndex

	// 5) Clean up CollectingCargo itself state
	bIsCollectingCargo = false;
	AvailableCargoSources.Empty();
	CurrentCargoIndex = 0;
	CollectStep = ECollectStep::ToSource;
	CollectSource.Reset();
}

bool AOrionChara::MoveToLocation(const FVector& InTargetLocation)
{
	// Record target for "replan" use
	LastMoveDestination = InTargetLocation;
	bHasMoveDestination = true;

	constexpr float AcceptanceRadius = 50.f;
	const FVector SearchExtent(500.f, 500.f, 500.f);

	if (!AIController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No AIController → treat as arrived"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No NavigationSystem → treat as arrived"));
		return true;
	}

	// If path hasn't been generated yet, sync calculate once
	if (NavPathPoints.Num() == 0)
	{
		FNavLocation ProjectedNav;
		FVector DestLocation = InTargetLocation;
		if (NavSys->ProjectPointToNavigation(InTargetLocation, ProjectedNav, SearchExtent))
		{
			DestLocation = ProjectedNav.Location;
		}
		// Calculate path
		FPathFindingQuery Query;
		UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), DestLocation,
		                                                                this);
		if (!Path || Path->PathPoints.Num() <= 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] Failed to build path or already at goal"));
			return true;
		}
		NavPathPoints = Path->PathPoints;
		CurrentNavPointIndex = 1; // Skip start point

		for (int32 i = 0; i < NavPathPoints.Num(); ++i)
		{
			// Draw path point sphere
			DrawDebugSphere(
				GetWorld(),
				NavPathPoints[i],
				20.f, // Radius
				12, // Segment count
				FColor::Green,
				false, // Not persistent
				5.f // Display duration
			);
			// Draw line between points
			if (i > 0)
			{
				DrawDebugLine(
					GetWorld(),
					NavPathPoints[i - 1],
					NavPathPoints[i],
					FColor::Green,
					false, // Not persistent
					5.f, // Display duration
					0,
					2.f // Line width
				);
			}
		}
	}

	// Current target point
	FVector TargetPoint = NavPathPoints[CurrentNavPointIndex];
	float Dist2D = FVector::Dist2D(GetActorLocation(), TargetPoint);
	UE_LOG(LogTemp, Verbose, TEXT("[MoveToLocation] NextPt=%s, Dist2D=%.1f"), *TargetPoint.ToString(), Dist2D);

	// After reaching current path point, advance to next
	if (Dist2D <= AcceptanceRadius)
	{
		CurrentNavPointIndex++;
		if (CurrentNavPointIndex >= NavPathPoints.Num())
		{
			// All points reached, stop moving
			NavPathPoints.Empty();
			CurrentNavPointIndex = 0;
			UE_LOG(LogTemp, Log, TEXT("[MoveToLocation] Arrived at final destination"));
			return true;
		}
	}

	// Move by adding input: toward next path point
	FVector Dir = (TargetPoint - GetActorLocation()).GetSafeNormal2D();
	AddMovementInput(Dir, 1.0f, true);

	return false; // Still moving
}

void AOrionChara::MoveToLocationStop()
{
	if (AIController)
	{
		AIController->StopMovement();
	}
	bHasMoveDestination = false;
	NavPathPoints.Empty();
	CurrentNavPointIndex = 0;
}

void AOrionChara::ReRouteMoveToLocation()
{
	if (!bHasMoveDestination || !AIController)
	{
		return;
	}

	// 1) Clear old navigation points
	NavPathPoints.Empty();
	CurrentNavPointIndex = 0;

	// 2) Immediately send a new MoveToLocation request to AIController
	constexpr float AcceptanceRadius = 50.f;
	const FVector SearchExtent(500.f, 500.f, 500.f);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RefreshMoveToLocation] No NavSys"));
		return;
	}

	// Project to NavMesh
	FNavLocation ProjectedNav;
	FVector Dest = LastMoveDestination;
	if (NavSys->ProjectPointToNavigation(LastMoveDestination, ProjectedNav, SearchExtent))
	{
		Dest = ProjectedNav.Location;
	}

	// Start pathfinding
	AIController->MoveToLocation(
		Dest,
		AcceptanceRadius,
		/*StopOnOverlap=*/ true,
		/*UsePathfinding=*/ true,
		/*ProjectDestinationToNavigation=*/ false,
		/*bCanStrafe=*/ true,
		/*FilterClass=*/ nullptr,
		/*bAllowPartialPath=*/ true
	);

	UE_LOG(LogTemp, Log, TEXT("[RefreshMoveToLocation] Re-routed to %s"), *Dest.ToString());
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
		AOrionActor* Best = FindClosetAvailableCargoContainer(3);

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
		MoveToLocationStop();

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
	MoveToLocation(BulletSource->GetActorLocation());
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
			if (!TradingCargo(Route))
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
		if (!TradingCargo(Route))
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
		MoveToLocationStop();
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
				MoveToLocation(InTarget->GetActorLocation());
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
		MoveToLocationStop();
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
		MoveToLocationStop();
	}

	State = EInteractWithActorState::Unavailable;
	CurrentInteractActor = nullptr;
	InteractAnimationKind = EInteractCategory::Unavailable;
}

bool AOrionChara::TradingCargo(const TMap<AActor*, TMap<int32, int32>>& TradeRoute)
{
	/* First time to enter the state machine (Not trading -> trading) */

	if (!BIsTrading)
	{
		/* Init state machine */
		TradeSegments.Empty();
		CurrentSegIndex = 0;
		TradeStep = ETradingCargoState::ToSource;
		BIsTrading = true;
		BIsPickupAnimPlaying = false;
		BIsDropoffAnimPlaying = false;

		/* Init trade segment that describes one run of trade */
		TArray<AActor*> Nodes;
		TradeRoute.GetKeys(Nodes);

		if (Nodes.Num() < 2)
		{
			BIsTrading = false;
			return true;
		}

		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			AActor* Source = Nodes[i];
			AActor* Destination = Nodes[(i + 1) % Nodes.Num()];

			for (auto& CargoMap : TradeRoute[Source])
			{
				FTradeSeg TradeSegment;
				TradeSegment.Source = Source;
				TradeSegment.Destination = Destination;
				TradeSegment.ItemId = CargoMap.Key;
				TradeSegment.Quantity = CargoMap.Value;
				TradeSegment.Moved = 0;
				TradeSegments.Add(TradeSegment);
			}
		}
	}

	/*--------------------------------------------------------------------
	 * 1) All segments completed?
	 *------------------------------------------------------------------*/
	if (CurrentSegIndex >= TradeSegments.Num())
	{
		BIsTrading = false;
		return true;
	}

	FTradeSeg& Seg = TradeSegments[CurrentSegIndex];
	if (!IsValid(Seg.Source) || !IsValid(Seg.Destination))
	{
		UE_LOG(LogTemp, Warning, TEXT("TradingCargo: invalid actor, abort"));
		BIsTrading = false;
		return true;
	}

	/*--------------------------------------------------------------------
	 * 2) Check "whether arrived" at current node
	 *    —— Role itself as Source/Dest is naturally considered "arrived" ——
	 *------------------------------------------------------------------*/
	bool bSourceIsSelf = (Seg.Source == this);
	bool bDestIsSelf = (Seg.Destination == this);

	USphereComponent* Sphere = nullptr;
	if (TradeStep == ETradingCargoState::ToDestination && !bDestIsSelf)
	{
		Sphere = Seg.Destination->FindComponentByClass<USphereComponent>();
	}
	else if (TradeStep == ETradingCargoState::ToSource && !bSourceIsSelf)
	{
		Sphere = Seg.Source->FindComponentByClass<USphereComponent>();
	}

	bool bAtNode = (Sphere && Sphere->IsOverlappingActor(this))
		|| (TradeStep == ETradingCargoState::ToSource && bSourceIsSelf)
		|| (TradeStep == ETradingCargoState::ToDestination && bDestIsSelf);

	/*--------------------------------------------------------------------
	 * 3) State machine
	 *------------------------------------------------------------------*/
	switch (TradeStep)
	{
	/*==============================================================
	 * → Source
	 *============================================================*/
	case ETradingCargoState::ToSource:
		if (bAtNode)
		{
			MoveToLocationStop(); // Already arrived (or it's self), stop pathfinding
			TradeStep = ETradingCargoState::Pickup;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			MoveToLocation(Seg.Source->GetActorLocation());
		}
		return false;

	/*==============================================================
	 *  Pick‑up
	 *============================================================*/
	case ETradingCargoState::Pickup:
		{
			/* If Source is self: directly record quantity, no animation, immediately enter ToDestination */
			if (bSourceIsSelf)
			{
				// Already in self backpack, just record actual quantity to move
				if (InventoryComp)
				{
					int32 Have = InventoryComp->GetItemQuantity(Seg.ItemId);
					int32 ToMove = FMath::Min(Have, Seg.Quantity);
					Seg.Moved = ToMove;
				}

				// Skip animation, directly next step
				TradeStep = ETradingCargoState::ToDestination;
				BIsPickupAnimPlaying = false;
				return false;
			}

			// ---------- OLD Old logic ----------
			// if (!BIsPickupAnimPlaying) { … Play animation and pickup … }
			// --------------------------------

			if (!BIsPickupAnimPlaying)
			{
				BIsPickupAnimPlaying = true;

				if (auto* SrcInv = Seg.Source->FindComponentByClass<UOrionInventoryComponent>())
				{
					int32 Available = SrcInv->GetItemQuantity(Seg.ItemId);
					int32 ToTake = FMath::Min(Available, Seg.Quantity);

					if (ToTake > 0 && InventoryComp)
					{
						SrcInv->ModifyItemQuantity(Seg.ItemId, -ToTake);
						InventoryComp->ModifyItemQuantity(Seg.ItemId, ToTake);
						Seg.Moved = ToTake;
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("TradingCargo: nothing to pickup"));
						BIsTrading = false;
						return true;
					}
				}

				if (PickupMontage)
				{
					float Rate = PickupMontage->GetPlayLength() / PickupDuration;
					GetMesh()->GetAnimInstance()->Montage_Play(PickupMontage, Rate);
				}
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Pickup,
					this, &AOrionChara::OnPickupAnimFinished,
					PickupDuration, false);
			}
			return false;
		}


	/*==============================================================
	 * → Destination
	 *============================================================*/
	case ETradingCargoState::ToDestination:
		if (bAtNode)
		{
			MoveToLocationStop();
			TradeStep = ETradingCargoState::DropOff;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			MoveToLocation(Seg.Destination->GetActorLocation());
		}
		return false;

	/*==============================================================
	 *  Drop‑off
	 *============================================================*/
	case ETradingCargoState::DropOff:
		{
			/* If Destination is self: directly deduct backpack, no animation, immediately next segment */
			if (bDestIsSelf)
			{
				if (InventoryComp)
				{
					InventoryComp->ModifyItemQuantity(Seg.ItemId, -Seg.Moved);
				}

				CurrentSegIndex++;
				TradeStep = ETradingCargoState::ToSource;
				BIsDropoffAnimPlaying = false;
				return false;
			}

			if (!BIsDropoffAnimPlaying)
			{
				BIsDropoffAnimPlaying = true;

				if (auto* DstInv = Seg.Destination->FindComponentByClass<UOrionInventoryComponent>())
				{
					DstInv->ModifyItemQuantity(Seg.ItemId, Seg.Moved);
				}

				if (InventoryComp)
				{
					InventoryComp->ModifyItemQuantity(Seg.ItemId, -Seg.Moved);
				}

				if (DropoffMontage)
				{
					float Rate = DropoffMontage->GetPlayLength() / DropoffDuration;
					GetMesh()->GetAnimInstance()->Montage_Play(DropoffMontage, Rate);
				}
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Dropoff,
					this, &AOrionChara::OnDropOffAnimFinished,
					DropoffDuration, false);
			}
			return false;
		}
	}

	return true; // Won't reach here
}

void AOrionChara::OnPickupAnimFinished()
{
	TradeStep = ETradingCargoState::ToDestination;
	BIsPickupAnimPlaying = false;
}

void AOrionChara::OnDropOffAnimFinished()
{
	FTradeSeg& TradeSegment = TradeSegments[CurrentSegIndex];
	if (InventoryComp)
	{
		InventoryComp->ModifyItemQuantity(TradeSegment.ItemId, -TradeSegment.Moved);
	}

	CurrentSegIndex++;
	TradeStep = ETradingCargoState::ToSource;
	BIsDropoffAnimPlaying = false;
}

void AOrionChara::RemoveAllActions(const FString& Except)
{
	if (CurrentAction)
	{
		FString OngoingActionName = CurrentAction->Name;

		if (OngoingActionName.Contains("ForceMoveToLocation") || OngoingActionName.Contains("MoveToLocation"))
		{
			if (!Except.Contains(TEXT("TempDoNotStopMovement")))
			{
				MoveToLocationStop();
			}
			else
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

		MoveToLocationStop();

		return true;
	}

	USphereComponent* CollisionSphere = OrionActor->CollisionSphere;
	const bool bOverlapping = CollisionSphere && CollisionSphere->IsOverlappingActor(this);

	if (bOverlapping)
	{
		MoveToLocationStop();

		bIsInteractWithInventory = true;

		OnInteractWithInventory.ExecuteIfBound(OrionActor);

		return true;
	}
	MoveToLocation(OrionActor->GetActorLocation());

	return false;
}
