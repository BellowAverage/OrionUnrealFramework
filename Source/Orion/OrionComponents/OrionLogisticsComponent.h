#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionActor/OrionActor.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Orion/OrionActor/OrionActorOre.h"
#include "OrionLogisticsComponent.generated.h"

// Forward declaration to avoid circular dependency
class AOrionChara;

// Move Enum from OrionChara.h
UENUM(BlueprintType)
enum class ETradingCargoState : uint8
{
	ToSource UMETA(DisplayName = "To Source"),
	Pickup UMETA(DisplayName = "Pickup"),
	ToDestination UMETA(DisplayName = "To Destination"),
	DropOff UMETA(DisplayName = "Dropoff")
};

UENUM(BlueprintType)
enum class ECollectStep : uint8
{
	ToSource,
	Pickup,
	ToStorage,
	Dropoff
};

// Move TradeSeg struct from OrionChara.h
USTRUCT(BlueprintType)
struct FTradeSeg
{
	GENERATED_BODY()

	UPROPERTY()
	AActor* Source = nullptr;

	UPROPERTY()
	AActor* Destination = nullptr;

	int32 ItemId = 0;
	int32 Quantity = 0;
	int32 Moved = 0;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ORION_API UOrionLogisticsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOrionLogisticsComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// --- Core Logic Functions ---

	/* Find nearest available cargo container */
	AOrionActor* FindClosetAvailableCargoContainer(int32 ItemId) const;

	/* Find all available cargo containers by distance */
	TArray<AOrionActor*> FindAvailableCargoContainersByDistance(int32 ItemId) const;

	/* Execute trading logic (state machine) */
	bool TradingCargo(const TMap<AActor*, TMap<int32, int32>>& TradeRoute);

	/* Execute collecting logic (state machine) */
	bool CollectingCargo(AOrionActorStorage* OrionStorageActor);
	void CollectingCargoStop();

	// --- State Variables (moved from Chara) ---

	// Trading Cargo State
	bool BIsTrading = false;
	ETradingCargoState TradeStep = ETradingCargoState::ToSource;
	UPROPERTY()
	TArray<FTradeSeg> TradeSegments;
	int32 CurrentSegIndex = 0;

	// Animation State (logistics-related animation logic)
	bool BIsPickupAnimPlaying = false;
	bool BIsDropoffAnimPlaying = false;
	FTimerHandle TimerHandle_Pickup;
	FTimerHandle TimerHandle_Dropoff;

	// Collecting Cargo State
	bool bIsCollectingCargo = false;
	// Flag for first delivery of self inventory
	bool bSelfDeliveryDone = false;
	UPROPERTY()
	TArray<AOrionActor*> AvailableCargoSources;
	int32 CurrentCargoIndex = 0;
	ECollectStep CollectStep = ECollectStep::ToSource;
	// Use WeakPtr to prevent crashes
	TWeakObjectPtr<AOrionActorOre> CollectSource;
	UPROPERTY()
	AOrionActorStorage* LastStorageActor = nullptr;

private:
	// --- Helper Functions ---
	// Get Owner cast to OrionChara
	AOrionChara* GetOrionOwner() const;

	// Animation callbacks
	UFUNCTION()
	void OnPickupAnimFinished();

	UFUNCTION()
	void OnDropOffAnimFinished();
};
