#include "OrionLogisticsComponent.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionComponents/OrionMovementComponent.h"
#include "EngineUtils.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Orion/OrionGameInstance/OrionInventoryManager.h"

UOrionLogisticsComponent::UOrionLogisticsComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // Logic driven by Action, no need for Component's own Tick
}

void UOrionLogisticsComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UOrionLogisticsComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

AOrionChara* UOrionLogisticsComponent::GetOrionOwner() const
{
	return Cast<AOrionChara>(GetOwner());
}

AOrionActor* UOrionLogisticsComponent::FindClosetAvailableCargoContainer(int32 ItemId) const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	// Get InventoryManager to access AllInventoryComponents
	UOrionInventoryManager* InvManager = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UOrionInventoryManager>() : nullptr;
	if (!InvManager) return nullptr;

	AOrionActor* ClosestAvailableActor = nullptr;
	FVector MyLoc = GetOwner()->GetActorLocation();

	// Use InventoryManager instead of TActorIterator
	for (UOrionInventoryComponent* Inv : InvManager->AllInventoryComponents)
	{
		if (!IsValid(Inv)) continue;
		
		AOrionActor* OrionActor = Cast<AOrionActor>(Inv->GetOwner());
		if (!IsValid(OrionActor)) continue;
		
		// [Fix] Filter out hidden Actors (preview objects in object pool)
		if (OrionActor->IsHidden())
		{
			continue;
		}

		int32 Quantity = Inv->GetItemQuantity(ItemId);

		if (Quantity > 0)
		{
			float Distance = FVector::DistSquared(MyLoc, OrionActor->GetActorLocation());
			// Simple nearest search logic
			if (!ClosestAvailableActor ||
				Distance < FVector::DistSquared(MyLoc, ClosestAvailableActor->GetActorLocation()))
			{
				ClosestAvailableActor = OrionActor;
			}
		}
	}
	return ClosestAvailableActor;
}

TArray<AOrionActor*> UOrionLogisticsComponent::FindAvailableCargoContainersByDistance(int32 ItemId, AActor* IgnoredActor) const
{
	UE_LOG(LogTemp, Log, TEXT("[FindContainers] Called with ItemId: %d, IgnoredActor: %s"), ItemId, IgnoredActor ? *IgnoredActor->GetName() : TEXT("None"));

	TArray<AOrionActor*> AvailableContainers;
	UWorld* World = GetWorld();
	if (!World && GetOwner())
	{
		World = GetOwner()->GetWorld();
	}
	
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[FindContainers] GetWorld() returned nullptr! Cannot iterate actors."));
		return AvailableContainers;
	}

	// Get InventoryManager to access AllInventoryComponents
	UOrionInventoryManager* InvManager = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UOrionInventoryManager>() : nullptr;
	if (!InvManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[FindContainers] InventoryManager is null! Cannot iterate inventory components."));
		return AvailableContainers;
	}

	int32 TotalChecked = 0;
	int32 FoundWithItem = 0;

	// Use InventoryManager instead of TActorIterator
	for (UOrionInventoryComponent* Inv : InvManager->AllInventoryComponents)
	{
		if (!IsValid(Inv)) continue;
		
		TotalChecked++;
		
		AOrionActor* OrionActor = Cast<AOrionActor>(Inv->GetOwner());
		if (!IsValid(OrionActor))
		{
			UE_LOG(LogTemp, Warning, TEXT("[FindContainers] Skipping %s (Owner is not AOrionActor)"), *Inv->GetName());
			continue;
		}
		
		// [Fix] Filter out hidden Actors (preview objects in object pool)
		if (OrionActor->IsHidden())
		{
			// UE_LOG(LogTemp, Verbose, TEXT("[FindContainers] Skipping %s (Is Hidden/Preview)"), *OrionActor->GetName());
			continue;
		}
		
		if (OrionActor == IgnoredActor)
		{
			UE_LOG(LogTemp, Log, TEXT("[FindContainers] Skipping %s (Is IgnoredActor)"), *OrionActor->GetName());
			continue;
		}

		// Exclude Storage or Production (as requested, do not transport from other storage)
		if (OrionActor->IsA<AOrionActorStorage>())
		{
			UE_LOG(LogTemp, Log, TEXT("[FindContainers] Skipping %s (Is Storage)"), *OrionActor->GetName());
			continue;
		}
		if (OrionActor->IsA<AOrionActorProduction>())
		{
			UE_LOG(LogTemp, Log, TEXT("[FindContainers] Skipping %s (Is Production)"), *OrionActor->GetName());
			continue;
		}

		int32 Qty = Inv->GetItemQuantity(ItemId);
		if (Qty > 0)
		{
			AvailableContainers.Add(OrionActor);
			FoundWithItem++;
			UE_LOG(LogTemp, Log, TEXT("[FindContainers] Found valid source: %s, Quantity: %d"), *OrionActor->GetName(), Qty);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[FindContainers] Skipping %s (Item Quantity is 0)"), *OrionActor->GetName());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[FindContainers] Checked %d actors, Found %d valid sources with ItemId %d"), TotalChecked, FoundWithItem, ItemId);

	if (AvailableContainers.Num() > 1)
	{
		FVector MyLoc = GetOwner()->GetActorLocation();
		AvailableContainers.Sort([MyLoc](const AOrionActor& A, const AOrionActor& B)
		{
			return FVector::DistSquared(MyLoc, A.GetActorLocation()) <
				FVector::DistSquared(MyLoc, B.GetActorLocation());
		});
	}

	return AvailableContainers;
}

bool UOrionLogisticsComponent::CollectingCargo(AOrionActorStorage* StorageActor)
{
	AOrionChara* OwnerChara = GetOrionOwner();
	if (!OwnerChara || !OwnerChara->InventoryComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CollectingCargo] OwnerChara or InventoryComp is null -> Returning true"));
		return true;
	}

	constexpr int32 StoneItemId = 2;

	// 1) Validate target storage
	if (!IsValid(StorageActor) || StorageActor->StorageCategory != EStorageCategory::StoneStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CollectingCargo] Invalid storage or not StoneStorage -> Returning true"));
		return true;
	}

	int32 InventoryQty = OwnerChara->InventoryComp->GetItemQuantity(StoneItemId);

	if (InventoryQty > 0)
	{
		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Character has items (%d), starting self-delivery to storage"), InventoryQty);
		
		// Build this -> Storage route
		TMap<AActor*, TMap<int32, int32>> SelfRoute;
		SelfRoute.Add(OwnerChara, {{StoneItemId, InventoryQty}});
		SelfRoute.Add(StorageActor, {});

		bool bDone = TradingCargo(SelfRoute);
		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Self-delivery TradingCargo returned: %s"), bDone ? TEXT("true (done)") : TEXT("false (running)"));
		if (!bDone)
		{
			// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Self-delivery still running -> Returning false (Running)"));
			return false;
		}
		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Self-delivery completed, checking for external sources"));
	}

	// 2) If empty backpack, search for other sources
	if (!BIsTrading)
	{
		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] No active trade, searching for external cargo sources..."));
		// Pass StorageActor as IgnoredActor to prevent picking target as source
		TArray<AOrionActor*> Sources = FindAvailableCargoContainersByDistance(StoneItemId, StorageActor);

		if (Sources.Num() == 0)
		{
			// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] No external sources found -> Returning true (Skipped)"));
			// No external sources, task is 'done' for now (Skipped)
			return true; 
		}

		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Found %d external sources, using first one: %s"), Sources.Num(), *Sources[0]->GetName());
		AOrionActor* NextSource = Sources[0];
		int32 Qty = NextSource->InventoryComp->GetItemQuantity(StoneItemId);

		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(NextSource, {{StoneItemId, Qty}});
		Route.Add(StorageActor, {});

		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Starting new trade route: Source has %d items -> Returning false (Running)"), Qty);
		TradingCargo(Route);
		return false;
	}

	// 3) Continue advancing existing trade
	// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Continuing existing trade..."));
	bool bTradeDone = TradingCargo(TMap<AActor*, TMap<int32, int32>>());
	// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] TradingCargo returned: %s, BIsTrading: %s"), 
	// 	bTradeDone ? TEXT("true (done)") : TEXT("false (running)"),
	// 	BIsTrading ? TEXT("true") : TEXT("false"));
	
	if (bTradeDone)
	{
		// Trade completed, check if there are more external sources
		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Trade completed, checking for more external sources..."));
		// Pass StorageActor as IgnoredActor
		TArray<AOrionActor*> Sources = FindAvailableCargoContainersByDistance(StoneItemId, StorageActor);
		
		if (Sources.Num() == 0)
		{
			// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] No more external sources -> Returning true (Skipped)"));
			return true; // No more sources, task is done for now (Skipped)
		}
		
		// Start new trade with next source
		// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Found %d more sources, starting new trade with %s"), Sources.Num(), *Sources[0]->GetName());
		AOrionActor* NextSource = Sources[0];
		int32 Qty = NextSource->InventoryComp->GetItemQuantity(StoneItemId);
		
		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(NextSource, {{StoneItemId, Qty}});
		Route.Add(StorageActor, {});
		
		TradingCargo(Route);
		return false; // Running
	}
	
	// UE_LOG(LogTemp, Log, TEXT("[CollectingCargo] Trade still running -> Returning false (Running)"));
	return false;
}

void UOrionLogisticsComponent::CollectingCargoStop()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Pickup);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Dropoff);
	}

	// [Fix] 关键：自增会话 ID。任何持有旧 ID 的 Timer 回调现在都会失效。
	CurrentTradeID++;

	BIsTrading = false;
	BIsPickupAnimPlaying = false;
	BIsDropoffAnimPlaying = false;
	TradeSegments.Empty();
	CurrentSegIndex = 0;
	TradeStep = ETradingCargoState::ToSource;

	// Stop owner movement
	if (AOrionChara* Owner = GetOrionOwner())
	{
		if (Owner->MovementComp) Owner->MovementComp->MoveToLocationStop();
	}

	bIsCollectingCargo = false;
	AvailableCargoSources.Empty();
	CurrentCargoIndex = 0;
	CollectStep = ECollectStep::ToSource;
	CollectSource.Reset();
	bSelfDeliveryDone = false;
}

bool UOrionLogisticsComponent::TradingCargo(const TMap<AActor*, TMap<int32, int32>>& TradeRoute)
{
	AOrionChara* OwnerChara = GetOrionOwner();
	if (!OwnerChara || !OwnerChara->InventoryComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TradingCargo] OwnerChara or InventoryComp is null -> Returning true"));
		return true;
	}

	/* Initialize */
	if (!BIsTrading)
	{
		TradeSegments.Empty();
		CurrentSegIndex = 0;
		TradeStep = ETradingCargoState::ToSource;
		CurrentTradeID++; // 开始新交易时也自增
		BIsTrading = true;
		BIsPickupAnimPlaying = false;
		BIsDropoffAnimPlaying = false;
		TradeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Starting new trade, TradeStartTime: %f"), TradeStartTime);

		TArray<AActor*> Nodes;
		TradeRoute.GetKeys(Nodes);

		if (Nodes.Num() < 2)
		{
			BIsTrading = false;
			return true;
		}

		// [Fix 10] Calculate dynamic timeout based on distance
		float TotalDist = 0.f;
		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			AActor* A = Nodes[i];
			AActor* B = Nodes[(i + 1) % Nodes.Num()];
			if (A && B) 
			{
				TotalDist += FVector::Dist(A->GetActorLocation(), B->GetActorLocation());
			}
		}
		// Assume average speed 500 (OrionMovementComponent default), plus 30 seconds buffer (interaction animations, etc.)
		DynamicTimeout = (TotalDist / 500.0f) + 30.0f;

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

	/* Check Completion */
	if (CurrentSegIndex >= TradeSegments.Num())
	{
		// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] All segments completed (CurrentSegIndex=%d >= TradeSegments.Num()=%d) -> Setting BIsTrading=false, Returning true"), 
		// 	CurrentSegIndex, TradeSegments.Num());
		BIsTrading = false;
		TradeStartTime = 0.0f;
		return true;
	}

	/* Check Timeout - [Fix 10] Use dynamic timeout based on distance */
	if (GetWorld() && TradeStartTime > 0.0f)
	{
		float CurrentTime = GetWorld()->GetTimeSeconds();
		float ElapsedTime = CurrentTime - TradeStartTime;
		
		if (ElapsedTime > DynamicTimeout)
		{
			UE_LOG(LogTemp, Warning, TEXT("[TradingCargo] Trade TIMEOUT after %.1f seconds (Dynamic: %.1fs)! Force completing. CurrentSegIndex=%d/%d, TradeStep=%d"), 
				ElapsedTime, DynamicTimeout, CurrentSegIndex, TradeSegments.Num(), (int32)TradeStep);
			BIsTrading = false;
			TradeStartTime = 0.0f;
			CurrentSegIndex = TradeSegments.Num(); // Mark all segments as done
			return true;
		}
	}

	FTradeSeg& Seg = TradeSegments[CurrentSegIndex];
	if (!IsValid(Seg.Source) || !IsValid(Seg.Destination))
	{
		// UE_LOG(LogTemp, Warning, TEXT("[TradingCargo] Segment[%d] has invalid Source or Destination -> Setting BIsTrading=false, Returning true"), CurrentSegIndex);
		BIsTrading = false;
		return true;
	}
	
	FString StepName = (TradeStep == ETradingCargoState::ToSource) ? TEXT("ToSource") :
	                   (TradeStep == ETradingCargoState::Pickup) ? TEXT("Pickup") :
	                   (TradeStep == ETradingCargoState::ToDestination) ? TEXT("ToDestination") :
	                   TEXT("DropOff");
	// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Processing segment[%d/%d], TradeStep: %s, ItemId: %d, Quantity: %d, Moved: %d, BIsPickupAnimPlaying: %s, BIsDropoffAnimPlaying: %s"), 
	// 	CurrentSegIndex, TradeSegments.Num(), *StepName, Seg.ItemId, Seg.Quantity, Seg.Moved,
	// 	BIsPickupAnimPlaying ? TEXT("true") : TEXT("false"),
	// 	BIsDropoffAnimPlaying ? TEXT("true") : TEXT("false"));

	/* Check Arrived */
	bool bSourceIsSelf = (Seg.Source == OwnerChara);
	bool bDestIsSelf = (Seg.Destination == OwnerChara);

	// Reuse collision sphere logic from original code
	USphereComponent* Sphere = nullptr;
	if (TradeStep == ETradingCargoState::ToDestination && !bDestIsSelf)
	{
		Sphere = Seg.Destination->FindComponentByClass<USphereComponent>();
	}
	else if (TradeStep == ETradingCargoState::ToSource && !bSourceIsSelf)
	{
		Sphere = Seg.Source->FindComponentByClass<USphereComponent>();
	}

	bool bAtNode = (Sphere && Sphere->IsOverlappingActor(OwnerChara))
		|| (TradeStep == ETradingCargoState::ToSource && bSourceIsSelf)
		|| (TradeStep == ETradingCargoState::ToDestination && bDestIsSelf);

	// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] bAtNode: %s, bSourceIsSelf: %s, bDestIsSelf: %s"), 
	// 	bAtNode ? TEXT("true") : TEXT("false"),
	// 	bSourceIsSelf ? TEXT("true") : TEXT("false"),
	// 	bDestIsSelf ? TEXT("true") : TEXT("false"));

	/* State Machine */
	switch (TradeStep)
	{
	case ETradingCargoState::ToSource:
		// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] State: ToSource, bAtNode: %s"), bAtNode ? TEXT("true") : TEXT("false"));
		if (bAtNode)
		{
			// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Arrived at source (Overlap), transitioning to Pickup"));
			if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocationStop();
			TradeStep = ETradingCargoState::Pickup;
		}
		else
		{
			// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Moving to source location"));
			bool bMoveFinished = false;
			if (OwnerChara->MovementComp) 
			{
				bMoveFinished = OwnerChara->MovementComp->MoveToLocation(Seg.Source->GetActorLocation());
			}
			
			if (bMoveFinished)
			{
				// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] MovementComp reported arrival, forcing transition to Pickup"));
				if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocationStop();
				TradeStep = ETradingCargoState::Pickup;
			}
		}
		return false;

	case ETradingCargoState::Pickup:
		{
			// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] State: Pickup, bSourceIsSelf: %s, BIsPickupAnimPlaying: %s"), 
			// 	bSourceIsSelf ? TEXT("true") : TEXT("false"),
			// 	BIsPickupAnimPlaying ? TEXT("true") : TEXT("false"));
			
			if (bSourceIsSelf)
			{
				int32 Have = OwnerChara->InventoryComp->GetItemQuantity(Seg.ItemId);
				int32 ToMove = FMath::Min(Have, Seg.Quantity);
				Seg.Moved = ToMove;
				// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Source is self, moving %d items, transitioning to ToDestination"), ToMove);
				TradeStep = ETradingCargoState::ToDestination;
				BIsPickupAnimPlaying = false;
				return false;
			}

			if (!BIsPickupAnimPlaying)
			{
				BIsPickupAnimPlaying = true;

				if (auto* SrcInv = Seg.Source->FindComponentByClass<UOrionInventoryComponent>())
				{
					int32 Available = SrcInv->GetItemQuantity(Seg.ItemId);
					int32 ToTake = FMath::Min(Available, Seg.Quantity);
					// Transfer
					if (ToTake > 0)
					{
						SrcInv->ModifyItemQuantity(Seg.ItemId, -ToTake);
						OwnerChara->InventoryComp->ModifyItemQuantity(Seg.ItemId, ToTake);
						Seg.Moved = ToTake;
					}
				}

				// Play Animation via Owner
				if (OwnerChara->PickupMontage)
				{
					float Rate = OwnerChara->PickupMontage->GetPlayLength() / OwnerChara->PickupDuration;
					if (UAnimInstance* Anim = OwnerChara->GetMesh()->GetAnimInstance())
					{
						Anim->Montage_Play(OwnerChara->PickupMontage, Rate);
					}
				}
				// [Fix] 使用 Lambda 绑定 Timer，捕获当前的 TradeID
				// [Fix] Use WeakPtr to prevent dangling pointer
				TWeakObjectPtr<UOrionLogisticsComponent> WeakThis(this);
				TWeakObjectPtr<AOrionChara> WeakOwner(OwnerChara);
				int32 CapturedID = CurrentTradeID;
				float CapturedDuration = OwnerChara->PickupDuration;
				
				UWorld* World = GetWorld();
				if (!World)
				{
					UE_LOG(LogTemp, Error, TEXT("[TradingCargo] GetWorld() returned nullptr in Pickup state"));
					BIsPickupAnimPlaying = false;
					return false;
				}
				
				FTimerDelegate TimerDel;
				TimerDel.BindLambda([WeakThis, WeakOwner, CapturedID]()
				{
					// Check if component is still valid
					if (!WeakThis.IsValid()) return;
					UOrionLogisticsComponent* Comp = WeakThis.Get();
					
					// Validate ID
					if (Comp->CurrentTradeID != CapturedID) return;
					
					// Check if owner is still valid
					if (!WeakOwner.IsValid()) return;
					
					Comp->OnPickupAnimFinished();
				});
				World->GetTimerManager().SetTimer(
					TimerHandle_Pickup,
					TimerDel,
					CapturedDuration, 
					false);
			}
			return false;
		}

	case ETradingCargoState::ToDestination:
		// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] State: ToDestination, bAtNode: %s"), bAtNode ? TEXT("true") : TEXT("false"));
		if (bAtNode)
		{
			// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Arrived at destination (Overlap), transitioning to DropOff"));
			if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocationStop();
			TradeStep = ETradingCargoState::DropOff;
		}
		else
		{
			// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] Moving to destination location"));
			bool bMoveFinished = false;
			if (OwnerChara->MovementComp) 
			{
				bMoveFinished = OwnerChara->MovementComp->MoveToLocation(Seg.Destination->GetActorLocation());
			}

			if (bMoveFinished)
			{
				// UE_LOG(LogTemp, Log, TEXT("[TradingCargo] MovementComp reported arrival, forcing transition to DropOff"));
				if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocationStop();
				TradeStep = ETradingCargoState::DropOff;
			}
		}
		return false;

	case ETradingCargoState::DropOff:
		{
			if (bDestIsSelf)
			{
				OwnerChara->InventoryComp->ModifyItemQuantity(Seg.ItemId, -Seg.Moved);
				CurrentSegIndex++;
				TradeStep = ETradingCargoState::ToSource;
				BIsDropoffAnimPlaying = false;
				return false;
			}

			if (!BIsDropoffAnimPlaying)
			{
				BIsDropoffAnimPlaying = true;

				// [Confirm] 确保 Transfer 在这里执行（立即执行模式）
				// 如果您的代码是在 OnFinished 里执行 Transfer，请把它移回来，或者确保 OnFinished 的 ID 检查生效
				// 根据之前的讨论，建议在这里（Start）执行 Transfer 以保证数据原子性，
				// 只有动画完成的事件才需要等待 Timer。
				if (auto* DstInv = Seg.Destination->FindComponentByClass<UOrionInventoryComponent>())
				{
					DstInv->ModifyItemQuantity(Seg.ItemId, Seg.Moved);
				}
				OwnerChara->InventoryComp->ModifyItemQuantity(Seg.ItemId, -Seg.Moved);

				// Play Animation
				if (OwnerChara->DropoffMontage)
				{
					float Rate = OwnerChara->DropoffMontage->GetPlayLength() / OwnerChara->DropoffDuration;
					if (UAnimInstance* Anim = OwnerChara->GetMesh()->GetAnimInstance())
					{
						Anim->Montage_Play(OwnerChara->DropoffMontage, Rate);
					}
				}
				// [Fix] 使用 Lambda 绑定 Timer，捕获当前的 TradeID
				int32 CapturedID = CurrentTradeID;
				FTimerDelegate TimerDel;
				TimerDel.BindLambda([this, CapturedID]()
				{
					// [Critical Fix] 只有 ID 匹配才执行后续逻辑
					if (this->CurrentTradeID != CapturedID) 
					{
						UE_LOG(LogTemp, Warning, TEXT("[TradingCargo] DropOff Timer Expired but TradeID mismatch (Old: %d, Curr: %d). Ignoring."), CapturedID, this->CurrentTradeID);
						return;
					}
					this->OnDropOffAnimFinished();
				});
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Dropoff,
					TimerDel,
					OwnerChara->DropoffDuration, 
					false);
			}
			return false;
		}
	}
	return true;
}

void UOrionLogisticsComponent::OnPickupAnimFinished()
{
	TradeStep = ETradingCargoState::ToDestination;
	BIsPickupAnimPlaying = false;
}

void UOrionLogisticsComponent::OnDropOffAnimFinished()
{
	// 双重保险（ID 检查已在 Lambda 中完成）
	if (!BIsTrading) return;

	CurrentSegIndex++;
	TradeStep = ETradingCargoState::ToSource;
	BIsDropoffAnimPlaying = false;
	
	if (CurrentSegIndex >= TradeSegments.Num())
	{
		BIsTrading = false;
	}
}
