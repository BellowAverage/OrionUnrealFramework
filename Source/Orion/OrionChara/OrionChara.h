#pragma once

// OrionChara.h
#include <vector>
#include "AIController.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include <Perception/AIPerceptionStimuliSourceComponent.h>
#include "Orion/OrionActor/OrionActor.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Orion/OrionCppFunctionLibrary/OrionCppFunctionLibrary.h"
#include "Orion/OrionActor/OrionActorProduction.h"
#include "Orion/OrionInterface/OrionInterfaceActionable.h"
#include "Orion/OrionInterface/OrionInterfaceSelectable.h"
#include "Orion/OrionInterface/OrionInterfaceSerializable.h"
#include "Misc/Guid.h"
#include "Orion/OrionGlobals/EOrionAction.h"
#include "Orion/OrionComponents/OrionCharaActionComponent.h"
#include "Orion/OrionComponents/OrionLogisticsComponent.h"
#include "Orion/OrionComponents/OrionCombatComponent.h"
#include "OrionChara.generated.h"

class UOrionMovementComponent;

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
enum class EAIState : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable"),
	Defensive UMETA(DisplayName = "Defensive"),
	Aggressive UMETA(DisplayName = "Aggressive"),
};

UENUM(BlueprintType)
enum class EActionExecution : uint8
{
	Procedural,
	RealTime,
};

UENUM(BlueprintType)
enum class EInteractWithActorState : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable"),
	MovingToTarget UMETA(DisplayName = "MovingToTarget"),
	Interacting UMETA(DisplayName = "Interacting"),
};

/*UENUM(BlueprintType)
enum class EInteractWithProductionState : uint8
{
	Unavailable UMETA(DisplayName = "Unavailable"),
	MovingToTarget UMETA(DisplayName = "MovingToTarget"),
	Interacting UMETA(DisplayName = "Interacting"),
};*/


class FOrionAction
{
public:
	FString Name;

	EOrionAction OrionActionType = EOrionAction::Undefined;

	FOrionActionParams Params;

	TFunction<bool(float)> ExecuteFunction;

	FOrionAction(const FString& ActionName, EOrionAction InActionType, TFunction<bool(float)> Func)
		: Name(ActionName)
		  , OrionActionType(InActionType)
		  , ExecuteFunction(MoveTemp(Func))
	{
	}

	EOrionAction GetActionType() const
	{
		return OrionActionType;
	}
};

class FActionQueue
{
public:
	//std::deque<Action> Actions;
	TObservableArray<FOrionAction> Actions;

	FORCEINLINE FOrionAction* GetFrontAction() { return Actions.IsEmpty() ? nullptr : &Actions[0]; }

	FORCEINLINE void PopFrontAction()
	{
		if (!Actions.IsEmpty())
		{
			Actions.RemoveAt(0, 1, EAllowShrinking::Yes);
		}
	}
};

// Add this declaration to the AOrionChara class in OrionChara.h


UCLASS()
class ORION_API AOrionChara : public ACharacter, public IOrionInterfaceSelectable, public IOrionInterfaceActionable,
                              public IOrionInterfaceSerializable
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOrionCharaActionComponent* CharaActionComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOrionLogisticsComponent* LogisticsComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOrionCombatComponent* CombatComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOrionMovementComponent* MovementComp = nullptr;


	/* ---------- 序列化标识 ---------- */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Basics")
	FSerializable GameSerializable; // C++ 内部 ID，不打 SaveGame

	UPROPERTY(SaveGame)
	FOrionCharaSerializable CharaSerializable; // 快照

	/* ---------- 组件指针（运行时创建 / 蓝图继承） ---------- */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOrionInventoryComponent* InventoryComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionStimuliSourceComponent* StimuliSourceComp = nullptr;

	/* ---------- 持久化 Gameplay 状态 ---------- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Gameplay")
	ECharaState CharaState = ECharaState::Alive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Gameplay")
	float CurrHealth = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Gameplay")
	float MaxHealth = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "AI")
	EAIState CharaAIState = EAIState::Unavailable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "AI")
	int32 CharaSide = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	TMap<int32, int32> AvailableInventoryMap;

	/* ---------- 非持久化运行时缓存 ---------- */
	UPROPERTY(BlueprintReadOnly, Category = "Move")
	float MoveForwardScale = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Move")
	float MoveRightScale = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	UAnimMontage* BulletPickupMontage = nullptr;
	UPROPERTY(EditDefaultsOnly, Category = "Animation")
	float BulletPickupDuration = 1.f;


	virtual void InitSerializable(const FSerializable& InSerializable) override;
	virtual FSerializable GetSerializable() const override;


	void SerializeCharaStats();


	virtual FString GetUnifiedActionName() const override;
	EOrionAction GetUnifiedActionType() const;

	virtual bool GetIsCharaProcedural() override;
	virtual bool SetIsCharaProcedural(bool bInIsCharaProcedural) override;

	virtual void InsertOrionActionToQueue(
		const FOrionAction& OrionActionInstance,
		const EActionExecution ActionExecutionType,
		const int32 Index) override;

	virtual FOrionAction InitActionMoveToLocation(const FString& ActionName, const FVector& TargetLocation) override;

	virtual FOrionAction InitActionAttackOnChara(const FString& ActionName,
	                                             AActor* TargetChara, const FVector& HitOffset) override;

	virtual FOrionAction InitActionInteractWithActor(const FString& ActionName,
	                                                 AOrionActor* TargetActor) override;

	virtual FOrionAction InitActionInteractWithProduction(const FString& ActionName,
	                                                      AOrionActorProduction* TargetActor) override;

	virtual FOrionAction InitActionCollectCargo(const FString& ActionName, AOrionActorStorage* TargetActor) override;


	/* Collecting Bullets is currently not serializable */
	virtual FOrionAction InitActionCollectBullets(const FString& ActionName) override;

	AOrionChara();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Interface Override */

	virtual ESelectable GetSelectableType() const override;
	virtual void OnSelected(APlayerController* PlayerController) override;
	virtual void OnRemoveFromSelection(APlayerController* PlayerController) override;

	/* Other */


	bool bIsInteractWithInventory = false;


	FOnInteractWithInventory OnInteractWithInventory;

	FOnCharaActionChange OnCharaActionChange;

	UPROPERTY()
	AAIController* AIController;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	bool InteractWithInventory(AOrionActor* OrionActor);


	/* Armed */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	bool bIsCharaArmed = false;

	/** Movement input amount for BlendSpace */


	/** True while we're in the process of fetching bullets */
	bool bIsCollectingBullets = false;

	bool CollectBullets();


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

	/* Character Action Queue */
	FActionQueue CharacterActionQueue;
	FOrionAction* CurrentAction;
	FOrionAction* PreviousAction;

	FActionQueue CharacterProcActionQueue;
	FOrionAction* CurrentProcAction;
	FOrionAction* PreviousProcAction;

	void DistributeCharaAction(float DeltaTime);
	void DistributeRealTimeAction(float DeltaTime);
	void DistributeProceduralAction(float DeltaTime);
	void SwitchingStateHandle(EOrionAction PrevType, EOrionAction CurrType);
	virtual void RemoveAllActions(const FString& Except = FString()) override;

	/** last non-empty action name, persists across frames when current action is interrupted */
	FString LastActionName;
	EOrionAction LastActionType = EOrionAction::Undefined;


	/* AI: Character Procedural Action Distributor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "AI Information")
	bool bIsCharaProcedural;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	UAnimMontage* PickupMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	float PickupDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	UAnimMontage* DropoffMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Trading|Animation")
	float DropoffDuration = 3.0f;

	/* Interact With Production */
	bool InteractWithProduction(float DeltaTime, AOrionActorProduction* InTargetProduction,
	                            bool bPreferStorageFirst = true);

	FTimerHandle ProductionTimerHandle;
	bool bIsInteractProd = false;
	bool bPreparingInteractProd = false;
	TWeakObjectPtr<AOrionActorProduction> CurrentInteractProduction;
	void InteractWithProductionStop();



	/* InteractWithActor */
	bool InteractWithActor(float DeltaTime, AOrionActor* InTarget);
	bool SetInteractingAnimation();
	bool InteractWithActorStart(const EInteractWithActorState& State);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
	bool IsInteractWithActor = false;
	void InteractWithActorStop(EInteractWithActorState& State);
	EInteractWithActorState InteractWithActorState = EInteractWithActorState::Unavailable;

	TWeakObjectPtr<AOrionActor> MoveTargetActor;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
	EInteractCategory InteractAnimationKind = EInteractCategory::Unavailable;

	UPROPERTY()
	AOrionActor* CurrentInteractActor = nullptr;

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void SpawnAxeOnChara();

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void RemoveAxeOnChara();

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void SpawnHammerOnChara();

	UFUNCTION(BlueprintImplementableEvent, Category = "OrionChara Utility")
	void RemoveHammerOnChara();

	UFUNCTION(BlueprintImplementableEvent)
	void SpawnWeaponActor();
	UFUNCTION(BlueprintImplementableEvent)
	void RemoveWeaponActor();
	UFUNCTION(BlueprintImplementableEvent)
	void SpawnOrionBulletActor(const FVector& SpawnLocation, const FVector& ForwardDirection);


	void SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh);

	/* Gameplay Mechanism */


	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;
	void Die();
	void Incapacitate();
	void Ragdoll();

	/* AI Vision */

	bool bIsOrionAIControlled = true;

	/* Physics */


	/* AI Information */


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	TArray<int> HostileGroupsIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	TArray<int> FriendlyGroupsIndex;

	/* Interface */
	UFUNCTION(BlueprintCallable, Category = "Interface")
	void ChangeMaxWalkSpeed(float InValue);
	UFUNCTION()
	void ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex);
	UFUNCTION()
	void RemoveProceduralActionAt(int32 Index);

	std::vector<AOrionChara*> GetOtherCharasByProximity() const;

	/* Developer Only */
};
