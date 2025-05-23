#pragma once

// OrionChara.h

#include "AIController.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include <Perception/AIPerceptionStimuliSourceComponent.h>
#include "Orion/OrionWeaponProjectile/OrionWeapon.h"
#include "Orion/OrionActor/OrionActor.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Orion/OrionCppFunctionLibrary/OrionCppFunctionLibrary.h"
#include "Orion/OrionActor/OrionActorOre.h"
#include "Orion/OrionActor/OrionActorProduction.h"
#include "Orion/OrionInterface/OrionInterfaceActionable.h"
#include "Orion/OrionInterface/OrionInterfaceSelectable.h"
#include "OrionChara.generated.h"

DECLARE_DELEGATE_OneParam(FOnInteractWithInventory, AOrionActor*);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharaActionChange, FString, PrevActionName, FString, CurrActionName);

UENUM(BlueprintType)
enum class ECharaState : uint8
{
	Alive UMETA(DisplayName = "Alive"),
	Dead UMETA(DisplayName = "Dead"),
	Incapacitated UMETA(DisplayName = "Incapacitated"),
	Ragdoll UMETA(DisplayName = "Ragdoll")
};

UENUM(BlueprintType)
enum class EInteractCategory : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable"),
	Mining UMETA(DisplayName = "Mining"),
	CraftingBullets UMETA(DisplayName = "Crafting Bullets"),
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


UENUM(BlueprintType)
enum class EOrionAction : uint8
{
	AttackOnChara UMETA(DisplayName = "Attack On Character"),
	InteractWithActor UMETA(DisplayName = "Interact With Actor"),
	InteractWithProduction UMETA(DisplayName = "Interact With Production"),
	InteractWithStorage UMETA(DisplayName = "Interact With Storage"),
};

UENUM(BlueprintType)
enum class EActionExecution : uint8
{
	Procedural,
	RealTime,
};

class OrionAction
{
public:
	FString Name;

	TFunction<bool(float)> ExecuteFunction;

	OrionAction(const FString& ActionName, TFunction<bool(float)> Func)
		: Name(ActionName)
		  , ExecuteFunction(MoveTemp(Func))
	{
	}
};

class ActionQueue
{
public:
	//std::deque<Action> Actions;
	TObservableArray<OrionAction> Actions;

	FORCEINLINE OrionAction* GetFrontAction() { return Actions.IsEmpty() ? nullptr : &Actions[0]; }

	FORCEINLINE void PopFrontAction()
	{
		if (!Actions.IsEmpty())
		{
			Actions.RemoveAt(0);
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
	DropOff UMETA(DisplayName = "Dropoff")
};

UCLASS()
class ORION_API AOrionChara : public ACharacter, public IOrionInterfaceSelectable, public IOrionInterfaceActionable
{
	GENERATED_BODY()

public:
	virtual FString GetUnifiedActionName() const override;

	virtual bool GetIsCharaProcedural() override
	{
		return bIsCharaProcedural;
	}

	virtual bool SetIsCharaProcedural(bool bInIsCharaProcedural) override
	{
		bIsCharaProcedural = bInIsCharaProcedural;
		return bIsCharaProcedural;
	}

	virtual void InsertOrionActionToQueue(
		const OrionAction& OrionActionInstance,
		const EActionExecution ActionExecutionType,
		const int32 Index) override
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

	virtual OrionAction InitActionMoveToLocation(const FString& ActionName, const FVector& TargetLocation) override
	{
		return OrionAction(
			ActionName,
			[charaPtr = this, targetLocation = TargetLocation](float DeltaTime) -> bool
			{
				return charaPtr->MoveToLocation(targetLocation);
			}
		);
	}

	virtual OrionAction InitActionAttackOnChara(const FString& ActionName,
	                                            AActor* TargetChara, const FVector& HitOffset) override
	{
		return OrionAction(
			ActionName,
			[charaPtr = this, targetChara = TargetChara, inHitOffset = HitOffset](float DeltaTime) -> bool
			{
				return charaPtr->AttackOnChara(DeltaTime, targetChara, inHitOffset);
			}
		);
	}

	virtual OrionAction InitActionInteractWithActor(const FString& ActionName,
	                                                AOrionActor* TargetActor) override
	{
		return OrionAction(
			ActionName,
			[charaPtr = this, targetActor = TargetActor](float DeltaTime) -> bool
			{
				return charaPtr->InteractWithActor(DeltaTime, targetActor);
			}
		);
	}

	virtual OrionAction InitActionInteractWithProduction(const FString& ActionName,
	                                                     AOrionActorProduction* TargetActor) override
	{
		return OrionAction(
			ActionName,
			[charaPtr = this, targetActor = TargetActor](float DeltaTime) -> bool
			{
				return charaPtr->InteractWithProduction(DeltaTime, targetActor);
			}
		);
	}

	virtual OrionAction InitActionCollectCargo(const FString& ActionName, AOrionActorStorage* TargetActor) override
	{
		return OrionAction(
			ActionName,
			[charaPtr = this, targetActor = TargetActor](float DeltaTime) -> bool
			{
				return charaPtr->CollectingCargo(targetActor);
			}
		);
	}

	virtual OrionAction InitActionCollectBullets(const FString& ActionName) override
	{
		return OrionAction(
			ActionName,
			[charaPtr = this](float DeltaTime) -> bool
			{
				return charaPtr->CollectBullets();
			}
		);
	}

	AOrionChara();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Interface Override */

	virtual ESelectable GetSelectableType() const override
	{
		return ESelectable::OrionChara;
	}

	virtual void OnSelected(APlayerController* PlayerController) override
	{
		bIsOrionAIControlled = true;
	}

	virtual void OnRemoveFromSelection(APlayerController* PlayerController) override
	{
		bIsOrionAIControlled = false;
	}

	/* Other */


	bool bIsInteractWithInventory = false;


	FOnInteractWithInventory OnInteractWithInventory;

	FOnCharaActionChange OnCharaActionChange;

	UPROPERTY()
	AAIController* AIController;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	bool InteractWithInventory(AOrionActor* OrionActor);


	/* Armed */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool bIsCharaArmed = false;

	/** Movement input amount for BlendSpace */
	UPROPERTY(BlueprintReadOnly, Category = "CombatMove")
	float MoveForwardScale;

	UPROPERTY(BlueprintReadOnly, Category = "CombatMove")
	float MoveRightScale;

	/** True while we're in the process of fetching bullets */
	bool bIsCollectingBullets = false;

	bool CollectBullets();

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

	/** The production building we're currently drawing bullets from */
	AOrionActor* BulletSource = nullptr;

	/* Character Movement */
	void InitOrionCharaMovement();


	UFUNCTION(BlueprintCallable, Category = "Basics")
	void SynchronizeCapsuleCompLocation() const;

	void OnForceExceeded(const FVector& VelocityChange);

	UPROPERTY(EditDefaultsOnly, Category = "Physics")
	float ForceThreshold = 2.0f;

	FVector PreviousVelocity;
	float VelocityChangeThreshold = 1000.0f;

	void ForceDetectionOnVelocityChange();

	UFUNCTION(BlueprintImplementableEvent, Category = "Basics")
	void BlueprintNativeVelocityExceeded();

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
	OrionAction* CurrentAction;
	OrionAction* PreviousAction;

	ActionQueue CharacterProcActionQueue;
	OrionAction* CurrentProcAction;
	OrionAction* PreviousProcAction;

	void DistributeCharaAction(float DeltaTime);
	void DistributeRealTimeAction(float DeltaTime);
	void DistributeProceduralAction(float DeltaTime);
	void SwitchingStateHandle(const FString& Prev, const FString& Curr);
	virtual void RemoveAllActions(const FString& Except = FString()) override;

	/** last non-empty action name, persists across frames when current action is interrupted */
	FString LastActionName;

	bool bCachedEmptyAction = false;


	/* AI: Character Procedural Action Distributor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	bool bIsCharaProcedural;

	/* Trading Cargo */
	bool TradingCargo(const TMap<AActor*, TMap<int32, int32>>& TradeRoute);

	//enum class ETradeStep : uint8 { ToSource, Pickup, ToDest, Dropoff };

	AOrionActor* FindClosetAvailableCargoContainer(int32 ItemId) const;
	TArray<AOrionActor*> FindAvailableCargoContainersByDistance(int32 ItemId) const;

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

	bool bSelfDeliveryDone = false;

	AOrionActorStorage* LastStorageActor = nullptr;


	UFUNCTION(BlueprintCallable, Category = "Basics")
	bool CollectingCargo(AOrionActorStorage* OrionStorageActor);

	void CollectingCargoStop();

	bool bIsCollectingCargo = false;

	TArray<AOrionActor*> AvailableCargoSources;
	int32 CurrentCargoIndex = 0;

	enum class ECollectStep : uint8 { ToSource, Pickup, ToStorage, Dropoff };

	ECollectStep CollectStep = ECollectStep::ToSource;
	TWeakObjectPtr<AOrionActorOre> CollectSource;

	/* MoveToLocation */
	bool MoveToLocation(const FVector& InTargetLocation);
	void MoveToLocationStop();

	/** Current pathfinding path point list */
	TArray<FVector> NavPathPoints;
	/** Current target path point index */
	int32 CurrentNavPointIndex = 0;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void ReRouteMoveToLocation();

	FVector LastMoveDestination;

	/** Flag indicating whether there is a valid MoveToLocation target */
	bool bHasMoveDestination = false;


	/* InteractWithActor */
	bool InteractWithActor(float DeltaTime, AOrionActor* InTarget);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
	bool IsInteractWithActor = false;
	void InteractWithActorStop();

	bool bIsMovingToInteraction = false;

	TWeakObjectPtr<AOrionActor> MoveTargetActor;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
	EInteractCategory InteractType = EInteractCategory::Unavailable;

	UPROPERTY()
	AOrionActor* CurrentInteractActor = nullptr;

	bool DoOnceInteractWithActor = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void SpawnAxeOnChara();

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void RemoveAxeOnChara();

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void SpawnHammerOnChara();

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void RemoveHammerOnChara();

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
