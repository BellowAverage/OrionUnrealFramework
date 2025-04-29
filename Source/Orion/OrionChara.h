#pragma once

// OrionChara.h

#include "AIController.h"
#include <unordered_map>
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include <Perception/AIPerceptionStimuliSourceComponent.h>
#include <vector>
#include <functional>
#include "OrionWeapon.h"
#include "OrionActor.h"
#include "OrionActor/OrionActorStorage.h"
#include "deque"
#include "OrionActor/OrionActorOre.h"
#include "OrionActor/OrionActorProduction.h"
#include "OrionChara.generated.h"

UENUM(BlueprintType)
enum class ECharaState : uint8
{
	Alive UMETA(DisplayName = "Alive"),
	Dead UMETA(DisplayName = "Dead"),
	Incapacitated UMETA(DisplayName = "Incapacitated"),
	Ragdoll UMETA(DisplayName = "Ragdoll")
};

UENUM(BlueprintType)
enum class EInteractType : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable"),
	Mining UMETA(DisplayName = "Mining"),
};

UENUM(BlueprintType)
enum class EActionType : uint8
{
	MoveToLocation UMETA(DisplayName = "Move To Location"),
	AttackOnCharaLongRange UMETA(DisplayName = "Attack On Character Long Range"),
	WorkingTest UMETA(DisplayName = "Working Test")
};

UENUM(BlueprintType)
enum class EAIState : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable"),
	Defensive UMETA(DisplayName = "Defensive"),

	Aggressive UMETA(DisplayName = "Aggressive"),
};


class Action
{
public:
	FString Name;

	TFunction<bool(float)> ExecuteFunction;

	Action(const FString& ActionName, TFunction<bool(float)> Func)
		: Name(ActionName)
		  , ExecuteFunction(MoveTemp(Func))
	{
	}
};

class ActionQueue
{
public:
	std::deque<Action> Actions;

	bool IsEmpty() const
	{
		return Actions.empty();
	}

	Action* GetFrontAction()
	{
		return IsEmpty() ? nullptr : &Actions.front();
	}

	void PopFrontAction()
	{
		if (!IsEmpty())
		{
			Actions.pop_front();
		}
	}
};

// Add this declaration to the AOrionChara class in OrionChara.h

UENUM(BlueprintType)
enum class ETradeStep : uint8
{
	ToSource UMETA(DisplayName = "To Source"),
	Pickup UMETA(DisplayName = "Pickup"),
	ToDest UMETA(DisplayName = "To Destination"),
	Dropoff UMETA(DisplayName = "Dropoff")
};

UCLASS()
class ORION_API AOrionChara : public ACharacter
{
	GENERATED_BODY()

public:
	AOrionChara();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	AAIController* AIController;

	/** True while we’re in the process of fetching bullets */
	bool bIsCollectingBullets = false;

	bool CollectBullets(float DeltaTime);

	/** Montage to play when picking up bullets */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* BulletPickupMontage;

	/** How long (in seconds) that montage should last */
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float BulletPickupDuration = 1.0f;

	bool bBulletPickupAnimPlaying = false;
	FTimerHandle TimerHandle_BulletPickup;

	// Callback when the montage finishes
	UFUNCTION()
	void OnBulletPickupFinished();

	/** The production building we’re currently drawing bullets from */
	AOrionActorProduction* BulletSource = nullptr;

	/* Character Movement */
	void InitOrionCharaMovement();

	void SynchronizeCapsuleCompLocation() const;

	void OnForceExceeded(const FVector& VelocityChange);

	UPROPERTY(EditDefaultsOnly, Category = "Physics")
	float ForceThreshold = 2.0f;

	FVector PreviousVelocity;
	float VelocityChangeThreshold = 1000.0f;

	void ForceDetectionOnVelocityChange();

	void RegisterCharaRagdoll(float DeltaTime);

	void RagdollWakeup();
	float RagdollWakeupAccumulatedTime = 0;
	float RagdollWakeupThreshold = 5.0f;

	FVector DefaultMeshRelativeLocation;
	FRotator DefaultMeshRelativeRotation;
	FVector DefaultCapsuleMeshOffset;

	float CapsuleDefaultMass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	float OrionCharaSpeed = 500.0f;

	/* Character Action Queue */
	ActionQueue CharacterActionQueue;
	Action* CurrentAction;
	Action* PreviousAction;

	ActionQueue CharacterProcActionQueue;
	Action* CurrentProcAction;
	Action* PreviousProcAction;

	void DistributeCharaAction(float DeltaTime);
	void DistributeRealTimeAction(float DeltaTime);
	void DistributeProceduralAction(float DeltaTime);
	void SwitchingStateHandle(const FString& Prev, const FString& Curr);
	void RemoveAllActions(const FString& Except = FString());
	FString GetUnifiedActionName() const;
	/** last non-empty action name, persists across frames when current action is interrupted */
	FString LastNonEmptyActionName;

	/* AI: Character Procedural Action Distributor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	bool bIsCharaProcedural;

	/* Trading Cargo */
	bool TradingCargo(const TMap<AActor*, TMap<int32, int32>>& TradeRoute);
	bool TradeMoveToLocation(const FVector& Dest, float AcceptanceRadius = 20.f);

	//enum class ETradeStep : uint8 { ToSource, Pickup, ToDest, Dropoff };

	bool bIsTrading = false;
	ETradeStep TradeStep = ETradeStep::ToSource;

	struct FTradeSeg
	{
		AActor* Source;
		AActor* Destination;
		int32 ItemId;
		int32 Quantity;
		int32 Moved = 0;
	};


	TArray<FTradeSeg> TradeSegments;
	int32 CurrentSegIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	UAnimMontage* PickupMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	float PickupDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	UAnimMontage* DropoffMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	float DropoffDuration = 3.0f;

	bool bPickupAnimPlaying = false;
	bool bDropoffAnimPlaying = false;
	FTimerHandle TimerHandle_Pickup, TimerHandle_Dropoff;

	void OnPickupAnimFinished();
	void OnDropoffAnimFinished();

	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	UOrionInventoryComponent* InventoryComp;

	/* Interact With Production */
	bool InteractWithProduction(float DeltaTime, AOrionActorProduction* InTargetProduction);

	FTimerHandle ProductionTimerHandle;
	bool bIsInteractProd = false;
	bool bPreparingInteractProd = false;
	TWeakObjectPtr<AOrionActorProduction> CurrentInteractProduction;
	void InteractWithProductionStop();

	/* Collecting Cargo */

	UFUNCTION(BlueprintCallable, Category = "Basics")
	bool CollectingCargo(AOrionActorStorage* OrionStorageActor);
	bool bIsCollectingCargo = false;
	void ResetCollectingState();
	TWeakObjectPtr<AOrionActorStorage> CollectStorageTarget;
	int32 CollectItemId;
	int32 CollectRemaining;

	enum class ECollectStep : uint8 { ToSource, Pickup, ToStorage, Dropoff };

	ECollectStep CollectStep = ECollectStep::ToSource;
	TWeakObjectPtr<AOrionActorOre> CollectSource;

	/* MoveToLocation */
	bool MoveToLocation(FVector InTargetLocation);
	void MoveToLocationStop();

	/* InteractWithActor */
	bool InteractWithActor(float DeltaTime, AOrionActor* InTarget);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
	bool IsInteractWithActor = false;
	void InteractWithActorStop();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
	EInteractType InteractType = EInteractType::Unavailable;

	UPROPERTY()
	AOrionActor* CurrentInteractActor = nullptr;

	bool DoOnceInteractWithActor = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void SpawnAxeOnChara();

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void RemoveAxeOnChara();

	/* Inventory */
	std::unordered_map<int, int> CharaInventoryMap;
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItemToInventory(int ItemID, int Quantity);
	int TempCharaInventory;

	/* Equipment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | AttackOnChara")
	TSubclassOf<AOrionWeapon> PrimaryWeaponClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | AttackOnChara")
	TSubclassOf<AOrionWeapon> SecondaryWeaponClass;

	/* AttackOnChara */
	bool AttackOnChara(float DeltaTime, AActor* InTarget, FVector HitOffset);
	float AttackOnCharaAccumulatedTime = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | AttackOnChara")
	bool IsAttackOnCharaLongRange = false;
	bool AttackOnCharaLongRange(float DeltaTime, AActor* InTarget, FVector HitOffset);
	void AttackOnCharaLongRangeStop();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | AttackOnChara")
	float FireRange = 2000.0f;

	void RefreshAttackFrequency();

	UFUNCTION(BlueprintCallable, Category = "Action | AttackOnChara")
	void SpawnBulletActor(const FVector& SpawnLoc, const FVector& DirToFire);

	float SpawnBulletActorAccumulatedTime;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | AttackOnChara")
	float AttackFrequencyLongRange = 2.3111f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | AttackOnChara")
	float AttackTriggerTimeLongRange = 1.60f;

	UFUNCTION(BlueprintImplementableEvent)
	void SpawnWeaponActor();
	UFUNCTION(BlueprintImplementableEvent)
	void RemoveWeaponActor();
	UFUNCTION(BlueprintImplementableEvent)
	void SpawnOrionBulletActor(const FVector& SpawnLocation, const FVector& ForwardDirection);


	void SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh);

	UPROPERTY()
	TArray<UStaticMeshComponent*> AttachedArrowComponents;

	/* Gameplay Mechanism */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayMechanism")
	ECharaState CharaState = ECharaState::Alive;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayMechanism")
	float CurrHealth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayMechanism")
	float MaxHealth = 10.0f;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;
	void Die();
	void Incapacitate();
	void Ragdoll();

	/* AI Vision */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionStimuliSourceComponent* StimuliSourceComp;
	bool bIsOrionAIControlled = true;

	/* Optimization */
	bool PreviousTickIsAttackOnCharaLongRange = false;


	/* Physics */


	/* AI Information */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	EAIState CharaAIState = EAIState::Unavailable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	int CharaSide = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	TArray<int> HostileGroupsIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	TArray<int> FriendlyGroupsIndex;

	UFUNCTION(BlueprintCallable, Category = "OrionChara Utility")
	bool bIsLineOfSightBlocked(AActor* InTargetActor) const;

	/* Interface */
	UFUNCTION(BlueprintCallable, Category = "Interface")
	void ChangeMaxWalkSpeed(float InValue);
	UFUNCTION()
	void ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex);
	UFUNCTION()
	void RemoveProceduralActionAt(int32 Index);

	/* Animation Optimization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* AM_EquipWeapon;

	void PlayEquipWeaponMontage();

	UFUNCTION()
	void OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	FTimerHandle EquipMontageTriggerHandle;
	FTimerHandle EquipMontageSpawnHandle;
	bool bIsPlayingMontage = false;
	void OnEquipWeaponMontageInterrupted();

	bool bIsEquipedWithWeapon = false;
	bool bMontageReady2Attack = false;

	std::vector<AOrionChara*> GetOtherCharasByProximity() const;

	/* Developer Only */
};
