#pragma once

// OrionChara.h
#include <vector>
#include "AIController.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include <Perception/AIPerceptionStimuliSourceComponent.h>
#include "Orion/OrionActor/OrionActor.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Orion/OrionActor/OrionActorProduction.h"
#include "Orion/OrionInterface/OrionInterfaceSelectable.h"
#include "Orion/OrionInterface/OrionInterfaceSerializable.h"
#include "Misc/Guid.h"
#include "Orion/OrionComponents/OrionActionComponent.h"
#include "Orion/OrionComponents/OrionLogisticsComponent.h"
#include "Orion/OrionComponents/OrionCombatComponent.h"
#include "OrionChara.generated.h"

class UOrionMovementComponent;

USTRUCT(BlueprintType)
struct FOrionCharaSerializable
{
	GENERATED_BODY()
	/* Basics */

	UPROPERTY(SaveGame)
	FGuid CharaGameId;

	UPROPERTY(SaveGame)
	FVector CharaLocation;

	UPROPERTY(SaveGame)
	FRotator CharaRotation;

	/* Procedural Actions */
	UPROPERTY(SaveGame)
	TArray<FOrionActionParams> SerializedProcActions;

	UPROPERTY(SaveGame)
	TArray<uint8> SerializedBytes;
};


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
	Passive UMETA(DisplayName = "Passive"),
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

UCLASS()
class ORION_API AOrionChara : public ACharacter, public IOrionInterfaceSelectable,
                              public IOrionInterfaceSerializable
{
	GENERATED_BODY()

public:

	AOrionChara();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* 1. References to External Resources*/

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Components")
	UOrionAttributeComponent* AttributeComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Components")
	UOrionActionComponent* ActionComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Components")
	UOrionLogisticsComponent* LogisticsComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Components")
	UOrionCombatComponent* CombatComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Components")
	UOrionMovementComponent* MovementComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Components")
	UOrionInventoryComponent* InventoryComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Components")
	UAIPerceptionStimuliSourceComponent* StimuliSourceComp = nullptr;

	UPROPERTY() AAIController* OrionAIControllerInstance;


	/* 2. Serialization Identifier */

	virtual void InitSerializable(const FSerializable& InSerializable) override;
	virtual FSerializable GetSerializable() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Orion|Serialization")
	FSerializable GameSerializable; // C++ internal ID, not saved to SaveGame

	UPROPERTY(SaveGame)
	FOrionCharaSerializable CharaSerializable; // Structure Default Initialization in Cpp

	/* 3. Character States Initialization*/

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Config(Non-null)|Gameplay")
	ECharaState CharaState = ECharaState::Alive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Config(Non-null)|Gameplay")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Config(Non-null)|AI")
	EAIState CharaAIState = EAIState::Passive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Config(Non-null)|AI", meta = (ToolTip = "当子弹数量小于等于此值时，角色会自动寻找子弹"))
	int32 LowAmmoThreshold = 0;

	// Deprecated: int32 CharaSide = 0;

	void InitOrionCharaMovement();

	/* 4. Character Animation Configuration  */

	UPROPERTY(EditDefaultsOnly, Category = "Config(Non-null)|Animation")
	UAnimMontage* BulletPickupMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Config(Non-null)|Animation")
	float BulletPickupDuration = 1.f;


	/* 5. Character Action System */

	UFUNCTION() void OnActionTypeChangedHandler(EOrionAction PrevType, EOrionAction CurrType);

	UFUNCTION() void OnActionNameChangedHandler(FString PrevName, FString CurrName);

	void RemoveAllActions(const FString& Except = FString());

	FOnCharaActionChange OnCharaActionChange;

	/* Actionable Interface Override */
	FString GetUnifiedActionName() const;
	EOrionAction GetUnifiedActionType() const;

	/* UI interface to get specific action status */
	UFUNCTION(BlueprintCallable, Category = "Orion|ActionSystem")
	FString GetActionValidityReason(int32 ActionIndex, bool bIsProcedural);

	UFUNCTION(BlueprintCallable, Category = "Orion|ActionSystem")
	FString GetActionStatusString(int32 ActionIndex, bool bIsProcedural);

	/* Action Factory Functions */

	FOrionAction InitActionMoveToLocation(const FString& ActionName, const FVector& TargetLocation);
	FOrionAction InitActionAttackOnChara(const FString& ActionName,
	                                             AActor* TargetChara, const FVector& HitOffset);
	FOrionAction InitActionInteractWithActor(const FString& ActionName,
	                                                 AOrionActor* TargetActor);
	FOrionAction InitActionInteractWithProduction(const FString& ActionName,
	                                                      AOrionActorProduction* TargetActor);
	FOrionAction InitActionCollectCargo(const FString& ActionName, AOrionActorStorage* TargetActor);


	FOrionAction InitActionCollectBullets(const FString& ActionName);

	// [New] InitActionInteractWithInventory
	FOrionAction InitActionInteractWithInventory(const FString& ActionName, AOrionActor* TargetActor);


	/* 5. Character Selectable System */

	/* Interface Override */

	virtual ESelectable GetSelectableType() const override;
	virtual void OnSelected(APlayerController* PlayerController) override;
	virtual void OnRemoveFromSelection(APlayerController* PlayerController) override;


	/* 6. Action: Interact with Inventory */

	FOnInteractWithInventory OnInteractWithInventory;

	UFUNCTION(BlueprintCallable, Category = "Orion|ActionSystem")
	bool InteractWithInventory(AOrionActor* OrionActor);


	/* 7. Action: Collect Something */

	/** True while in the process of fetching bullets */
	bool IsCollectingBullets = false;

	bool CollectBullets();
	bool IsBulletPickupAnimPlaying = false;

	FTimerHandle TimerHandle_BulletPickup;
	UFUNCTION() void OnBulletPickupFinished();

	TWeakObjectPtr<AOrionActor> BulletSource = nullptr;

	/* Incapacitated -> delayed death */
	FTimerHandle TimerHandle_DieAfterIncapacitated;


	/* Physics & Animation Utils */

	UFUNCTION(BlueprintCallable, Category = "Orion|Physics")
	void SynchronizeCapsuleCompLocation();

	void OnForceExceeded(const FVector& VelocityChange);

	UPROPERTY(EditDefaultsOnly, Category = "Orion|Physics")
	float ForceThreshold = 2.0f;

	FVector PreviousVelocity;
	float VelocityChangeThreshold = 600.f;

	void ForceDetectionOnVelocityChange();

	UFUNCTION(BlueprintImplementableEvent, Category = "Basics")
	void BlueprintNativeVelocityExceeded();

	void RegisterCharaRagdoll(float DeltaTime);

	void RagdollWakeup();
	float RagdollWakeupAccumulatedTime = 0;
	float RagdollWakeupThreshold = 5.f;

	/** 仰卧起身蒙太奇 (躺在背上时播放) - Supine */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Ragdoll")
	UAnimMontage* GetUpMontage_FaceUp;

	/** 俯卧起身蒙太奇 (趴在地上时播放) - Prone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Ragdoll")
	UAnimMontage* GetUpMontage_FaceDown;

	FVector DefaultMeshRelativeLocation;
	FRotator DefaultMeshRelativeRotation;
	FVector DefaultCapsuleMeshOffset;

	float CapsuleDefaultMass;

	UPROPERTY(EditAnywhere, Category = "Config(Non-null)|Trading|Animation")
	UAnimMontage* PickupMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Config(Non-null)|Trading|Animation")
	float PickupDuration = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Config(Non-null)|Trading|Animation")
	UAnimMontage* DropoffMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Config(Non-null)|Trading|Animation")
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config(Non-null)|Action | InteractWithActor")
	bool IsInteractWithActor = false;
	void InteractWithActorStop(EInteractWithActorState& State);
	EInteractWithActorState InteractWithActorState = EInteractWithActorState::Unavailable;

	TWeakObjectPtr<AOrionActor> MoveTargetActor;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config(Non-null)|Action | InteractWithActor")
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
	void GetBulletSpawnParams(FVector& OutSpawnLocation, AOrionWeapon*& OutSpawnedWeapon);

	void SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh);

	/* Gameplay Mechanism */


	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;
	
	// [Fix 7] Unified cleanup function for Die and Incapacitate
	void CleanupState();
	
	UFUNCTION()
	void HandleHealthZero(AActor* InstigatorActor);

	void Die();

	UFUNCTION(BlueprintImplementableEvent, Category = "Orion|Gameplay")
	void CleanAffiliatedObjects();

	void Incapacitate();
	void Ragdoll();

	/* AI Vision */

	bool bIsOrionAIControlled = true;

	/* Physics */


	/* AI Information */


	// Deprecated: HostileGroupsIndex, FriendlyGroupsIndex

	/* Interface */
	UFUNCTION(BlueprintCallable, Category = "Interface")
	void ChangeMaxWalkSpeed(float InValue);
	UFUNCTION()
	void ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex);
	UFUNCTION()
	void RemoveProceduralActionAt(int32 Index);

	std::vector<AOrionChara*> GetOtherCharasByProximity() const;

	/* Developer Only */

	UPROPERTY(EditDefaultsOnly, Category = "Config(Non-null)|Animation")
	UAnimMontage* Montage_Mining;

	UPROPERTY(EditDefaultsOnly, Category = "Config(Non-null)|Animation")
	UAnimMontage* Montage_Crafting;
};
