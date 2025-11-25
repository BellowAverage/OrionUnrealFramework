#include "OrionLogisticsComponent.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionComponents/OrionMovementComponent.h"
#include "EngineUtils.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"

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

	AOrionActor* ClosestAvailableActor = nullptr;
	FVector MyLoc = GetOwner()->GetActorLocation();

	for (TActorIterator<AOrionActor> It(World); It; ++It)
	{
		AOrionActor* OrionActor = *It;
		if (!OrionActor || !OrionActor->InventoryComp) continue;

		int32 Quantity = OrionActor->InventoryComp->GetItemQuantity(ItemId);

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

TArray<AOrionActor*> UOrionLogisticsComponent::FindAvailableCargoContainersByDistance(int32 ItemId) const
{
	TArray<AOrionActor*> AvailableContainers;
	UWorld* World = GetWorld();
	if (!World) return AvailableContainers;

	for (TActorIterator<AOrionActor> It(World); It; ++It)
	{
		AOrionActor* OrionActor = *It;
		if (!OrionActor || !OrionActor->InventoryComp) continue;

		// Exclude Storage or Production
		if (OrionActor->IsA<AOrionActorStorage>() || OrionActor->IsA<AOrionActorProduction>())
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
	if (!OwnerChara || !OwnerChara->InventoryComp) return true;

	constexpr int32 StoneItemId = 2;

	// 1) Validate target storage
	if (!IsValid(StorageActor) || StorageActor->StorageCategory != EStorageCategory::StoneStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: invalid storage or not StoneStorage"));
		return true;
	}

	if (OwnerChara->InventoryComp->GetItemQuantity(StoneItemId) > 0 && !bSelfDeliveryDone)
	{
		int32 Have = OwnerChara->InventoryComp->GetItemQuantity(StoneItemId);

		if (Have > 0)
		{
			// Build this -> Storage route
			TMap<AActor*, TMap<int32, int32>> SelfRoute;
			SelfRoute.Add(OwnerChara, {{StoneItemId, Have}});
			SelfRoute.Add(StorageActor, {});

			bool bDone = TradingCargo(SelfRoute);
			if (!bDone) return false;
		}
		bSelfDeliveryDone = true;
	}

	// 3) Re-select next source
	if (!BIsTrading)
	{
		TArray<AOrionActor*> Sources = FindAvailableCargoContainersByDistance(StoneItemId);

		if (Sources.Num() == 0)
		{
			bSelfDeliveryDone = false; // Reset for next time
			return true; // Done
		}

		AOrionActor* NextSource = Sources[0];
		int32 Qty = NextSource->InventoryComp->GetItemQuantity(StoneItemId);

		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(NextSource, {{StoneItemId, Qty}});
		Route.Add(StorageActor, {});

		TradingCargo(Route);
		return false;
	}

	// 4) Continue advancing existing trade
	TradingCargo(TMap<AActor*, TMap<int32, int32>>());
	return false;
}

void UOrionLogisticsComponent::CollectingCargoStop()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Pickup);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Dropoff);
	}

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
	if (!OwnerChara || !OwnerChara->InventoryComp) return true;

	/* Initialize */
	if (!BIsTrading)
	{
		TradeSegments.Empty();
		CurrentSegIndex = 0;
		TradeStep = ETradingCargoState::ToSource;
		BIsTrading = true;
		BIsPickupAnimPlaying = false;
		BIsDropoffAnimPlaying = false;

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

	/* Check Completion */
	if (CurrentSegIndex >= TradeSegments.Num())
	{
		BIsTrading = false;
		return true;
	}

	FTradeSeg& Seg = TradeSegments[CurrentSegIndex];
	if (!IsValid(Seg.Source) || !IsValid(Seg.Destination))
	{
		BIsTrading = false;
		return true;
	}

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

	/* State Machine */
	switch (TradeStep)
	{
	case ETradingCargoState::ToSource:
		if (bAtNode)
		{
			if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocationStop();
			TradeStep = ETradingCargoState::Pickup;
		}
		else
		{
			if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocation(Seg.Source->GetActorLocation());
		}
		return false;

	case ETradingCargoState::Pickup:
		{
			if (bSourceIsSelf)
			{
				int32 Have = OwnerChara->InventoryComp->GetItemQuantity(Seg.ItemId);
				int32 ToMove = FMath::Min(Have, Seg.Quantity);
				Seg.Moved = ToMove;
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
				// Timer
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Pickup,
					this, &UOrionLogisticsComponent::OnPickupAnimFinished,
					OwnerChara->PickupDuration, false);
			}
			return false;
		}

	case ETradingCargoState::ToDestination:
		if (bAtNode)
		{
			if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocationStop();
			TradeStep = ETradingCargoState::DropOff;
		}
		else
		{
			if (OwnerChara->MovementComp) OwnerChara->MovementComp->MoveToLocation(Seg.Destination->GetActorLocation());
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
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Dropoff,
					this, &UOrionLogisticsComponent::OnDropOffAnimFinished,
					OwnerChara->DropoffDuration, false);
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
	// If not finished moving, may need to update logic here, currently directly jump to next segment as per original code
	if (TradeSegments.IsValidIndex(CurrentSegIndex))
	{
		// ... (Optional cleanup)
	}

	CurrentSegIndex++;
	TradeStep = ETradingCargoState::ToSource;
	BIsDropoffAnimPlaying = false;
}
