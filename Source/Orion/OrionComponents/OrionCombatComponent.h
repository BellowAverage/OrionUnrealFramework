// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionWeaponProjectile/OrionWeapon.h"
#include "TimerManager.h"
#include "Orion/OrionGlobals/OrionWeaponData.h"
#include "OrionCombatComponent.generated.h"

class AOrionChara;
class UAnimMontage;
class UStaticMeshComponent;
class UStaticMesh;
class UOrionCombatComponent;
class AActor;

/**
 * Base class for lightweight Gameplay Abilities in Orion.
 * Designed to separate logic from ActorComponents.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class ORION_API UOrionGameplayAbility : public UObject
{
	GENERATED_BODY()

public:
	UOrionGameplayAbility();

	// Initialize the ability with its owner component
	virtual void Initialize(UOrionCombatComponent* InComponent);

	// Attempt to activate the ability
	virtual bool CanActivate() const;
	virtual void Activate();
	
	// Tick logic for the ability (since Orion uses tick-based actions)
	virtual void Tick(float DeltaTime);

	// Clean up when ability ends or is interrupted
	virtual void EndAbility();

	// Set the target for this ability (context)
	void SetTarget(AActor* InTarget);

	// Helper to get World from Component
	virtual UWorld* GetWorld() const override;

protected:
	UPROPERTY(Transient)
	UOrionCombatComponent* Component;

	UPROPERTY(Transient)
	AActor* TargetActor;

	bool bIsActive;
};

/**
 * Specific implementation of the Auto Attack logic moved from OrionCombatComponent.
 */
UCLASS()
class ORION_API UOrionGameplayAbility_AutoAttack : public UOrionGameplayAbility
{
	GENERATED_BODY()

public:
	virtual void Initialize(UOrionCombatComponent* InComponent) override;
	virtual void Activate() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndAbility() override;

private:
	// Logic moved from CombatComponent
	bool HasClearLineOfSight(AActor* InTargetActor) const;
	void PlayEquipWeaponMontage();

	UFUNCTION()
	void OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void OnEquipWeaponMontageInterrupted();

	// Timer delegates needs to be adapted or handles stored here
	FTimerHandle EquipMontageTriggerHandle;
	FTimerHandle EquipMontageSpawnHandle;

	// Local state tracking specific to this ability
	float SpawnBulletActorAccumulatedTime = 0.f;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ORION_API UOrionCombatComponent : public UActorComponent
{
	GENERATED_BODY()
	
	friend class UOrionGameplayAbility;
	friend class UOrionGameplayAbility_AutoAttack;

public:
	UOrionCombatComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	/* Combat entry points */
	bool AttackOnChara(float DeltaTime, AActor* InTarget, FVector HitOffset);
	bool AttackOnCharaLongRange(float DeltaTime, AActor* InTarget, FVector HitOffset);
	void AttackOnCharaLongRangeStop();

	void RefreshAttackFrequency();
	void CalculateAndSpawnBulletActor(const FVector& TargetLocation) const;
	void SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh);

	/* New Ability System Integration */
	UPROPERTY(Transient)
	UOrionGameplayAbility* AutoAttackAbility;

	UPROPERTY(Transient)
	UOrionGameplayAbility* ActiveAbility;

	/* Configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	EOrionWeaponType CurrentWeaponType = EOrionWeaponType::Rifle;

	/* Static global weapon data storage */
	static TMap<EOrionWeaponType, FOrionWeaponParams> GlobalWeaponParamsMap;

	/* Initialize global weapon params (call once, e.g. in BeginPlay or static init) */
	static void InitializeGlobalWeaponParams();

	UFUNCTION(BlueprintPure, Category = "Combat")
	const FOrionWeaponParams& GetCurrentWeaponParams() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	TSubclassOf<AOrionWeapon> PrimaryWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<AOrionWeapon> SecondaryWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	UAnimMontage* AM_EquipWeapon = nullptr;

	/* Runtime state */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool IsAttackOnCharaLongRange = false;
	bool PreviousTickIsAttackOnCharaLongRange = false;
	float SpawnBulletActorAccumulatedTime = 0.f;
	bool IsEquippedWithWeapon = false;
	bool IsMontageReady2Attack = false;
	bool IsPlayingMontage = false;

	UPROPERTY()
	TArray<UStaticMeshComponent*> AttachedArrowComponents;

private:
	AOrionChara* GetOrionOwner() const;
	bool HasClearLineOfSight(AActor* InTargetActor) const;

	void PlayEquipWeaponMontage();

	UFUNCTION()
	void OnEquipWeaponMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	void OnEquipWeaponMontageInterrupted();

	FTimerHandle EquipMontageTriggerHandle;
	FTimerHandle EquipMontageSpawnHandle;
	FTimerHandle UnequipWeaponTimerHandle; // [New] For delayed weapon retraction
};
