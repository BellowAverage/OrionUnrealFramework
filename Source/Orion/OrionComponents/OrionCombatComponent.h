// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionWeaponProjectile/OrionWeapon.h"
#include "TimerManager.h"
#include "OrionCombatComponent.generated.h"

class AOrionChara;
class UAnimMontage;
class UStaticMeshComponent;
class UStaticMesh;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class ORION_API UOrionCombatComponent : public UActorComponent
{
	GENERATED_BODY()

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
	void SpawnBulletActor(const FVector& SpawnLoc, const FVector& DirToFire);
	void SpawnArrowPenetrationEffect(const FVector& HitLocation, const FVector& HitNormal, UStaticMesh* ArrowMesh);

	/* Configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	TSubclassOf<AOrionWeapon> PrimaryWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<AOrionWeapon> SecondaryWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float FireRange = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackFrequencyLongRange = 2.3111f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackTriggerTimeLongRange = 1.60f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config (Non-null)")
	UAnimMontage* AM_EquipWeapon = nullptr;

	/* Runtime state */
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool IsAttackOnCharaLongRange = false;
	bool PreviousTickIsAttackOnCharaLongRange = false;
	float SpawnBulletActorAccumulatedTime = 0.f;
	bool bIsEquipedWithWeapon = false;
	bool bMontageReady2Attack = false;
	bool bIsPlayingMontage = false;

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
};
