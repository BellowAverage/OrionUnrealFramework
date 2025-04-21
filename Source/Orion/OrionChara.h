#pragma once

// OrionChara.h

#include "AIController.h"
#include <unordered_map>
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include <Perception/AIPerceptionStimuliSourceComponent.h>
#include "Perception/AISense_Sight.h"
#include <vector>
#include <functional>
#include "OrionWeapon.h"
#include "OrionActor.h"
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
	Mining UMETA(DisplayName = "Mining")
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
	AttackingEnemies UMETA(DisplayName = "Attacking Enemies")
};

class Action
{
public:
    FString Name;
    std::function<bool(float)> ExecuteFunction;

    Action(const FString& ActionName, const std::function<bool(float)>& Func)
        : Name(ActionName), ExecuteFunction(Func) {
    }
};

class ActionQueue
{
private:


public:

    std::vector<Action> Actions;

    bool IsEmpty() const
    {
        return Actions.empty();
    }

    Action* GetNextAction()
    {
        if (!IsEmpty())
        {
            return &Actions.front();
        }
        return nullptr;
    }

    void RemoveCurrentAction()
    {
        if (!IsEmpty())
        {
            Actions.erase(Actions.begin());
        }
    }
};

UCLASS()
class ORION_API AOrionChara : public ACharacter
{
    GENERATED_BODY()

public:
    // Sets default values for this character's properties
    AOrionChara();


protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    AAIController* AIController;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	float OrionCharaSpeed = 500.0f;

    /* CharacterActionQueue */
    ActionQueue CharacterActionQueue;
    Action* CurrentAction;
    void RemoveAllActions(const FString& Except = FString());

    /* MoveToLocation */
    bool MoveToLocation(FVector InTargetLocation);
    void MoveToLocationStop();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | MoveToLocation")
    FVector TargetLocation1;

    /* InteractWithActor */
    bool InteractWithActor(float DeltaTime, AOrionActor* InTarget);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
    bool IsInteractWithActor = false;
    void InteractWithActorStop();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action | InteractWithActor")
	EInteractType InteractType = EInteractType::Unavailable;

	AOrionActor* CurrentInteractActor = nullptr;

    bool DoOnceInteractWithActor = false;


    /* Inventory */
    std::unordered_map<int, int> CharaInventoryMap;
    UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItemToInventory(int ItemID, int Quantity);

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

    UFUNCTION(BlueprintCallable, Category = "Action | AttackOnChara")
    void SpawnBulletActor(const FVector& TargetLocation, float DeltaTime);

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
    void SpawnOrionBulletActor(const FVector& TargetLocation, const FVector& MuzzleLocation);


    void SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh);
	TArray <UStaticMeshComponent*> AttachedArrowComponents;

    /* Gameplay Mechanism */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayMechanism")
	ECharaState CharaState = ECharaState::Alive;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayMechanism")
	float CurrHealth;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayMechanism")
    float MaxHealth = 10.0f;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	void Die();
	void Incapacitate();
	void Ragdoll();

    /* AI Vision */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionStimuliSourceComponent* StimuliSourceComp;

    /* Optimization */
    bool PreviousTickIsAttackOnCharaLongRange = false;


    /* Physics */

    void OnForceExceeded(const FVector& VelocityChange);

    UPROPERTY(EditDefaultsOnly, Category = "Physics")
    float ForceThreshold = 2.0f;

	FVector PreviousVelocity;
	float VelocityChangeThreshold = 1000.0f;

	void ForceDetectionOnVelocityChange();
	void RagdollWakeup();
	float RagdollWakeupAccumulatedTime = 0;
	float RagdollWakeupThreshold = 5.0f;

    FVector DefaultMeshRelativeLocation;
    FRotator DefaultMeshRelativeRotation;
    FVector DefaultCapsuleMeshOffset;
    float CapsuleDefaultMass;

    /* AI Information */

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	EAIState CharaAIState = EAIState::Unavailable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Information")
	int CharaSide = 0;

    UFUNCTION(BlueprintCallable, Category = "Optimization")
    AOrionChara* GetClosestOrionChara();

    /* Interface */
	UFUNCTION(BlueprintCallable, Category = "Interface")
	void ChangeMaxWalkSpeed(float InValue);

    /* Animation Optimization */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	UAnimMontage* AM_EquipWeapon;

    void PlayEquipWeaponMontage();

    UFUNCTION()
    void OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    AOrionChara* MontageCallbackChara = nullptr;
    AActor* MontageCallbackTargetActor = nullptr;
    FVector MontageCallbackHitOffset = FVector::ZeroVector;
    FString MontageCallbackActionName = TEXT("EquipWeaponMontage");
    FTimerHandle EquipMontageTriggerHandle;
    FTimerHandle EquipMontageSpawnHandle;
	bool bIsPlayingMontage = false;
	void OnEquipWeaponMontageInterrupted();
};