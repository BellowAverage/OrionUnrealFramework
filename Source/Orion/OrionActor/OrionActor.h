// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "OrionActor.generated.h"


UENUM(BlueprintType)
enum class EActorStatus : uint8
{
	Interactable UMETA(DisplayName = "Interactable"),
	NotInteractable UMETA(DisplayName = "NotInteractable"),
};

UENUM(BlueprintType)
enum class EActorCategory : uint8
{
	None UMETA(DisplayName = "None"),
	Ore UMETA(DisplayName = "Ore"),
	Storage UMETA(DisplayName = "Storage"),
	Production UMETA(DisplayName = "Production"),
};

UCLASS()
class ORION_API AOrionActor : public AActor
{
	GENERATED_BODY()

public:
	AOrionActor();


	/* Chara Interaction */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EActorStatus ActorStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	FString InteractType;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	FString GetInteractType();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EActorCategory ActorCategory;

	/* Inventory */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TMap<int32, int32> AvailableInventoryMap;

	/* Components */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Components")
	UOrionInventoryComponent* InventoryComp;

	/* Gameplay Basics */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int CurrHealth;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;
	void Die();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	TSubclassOf<AActor> DeathEffectClass;

	UFUNCTION(BlueprintNativeEvent, Category = "Basics")
	void SpawnDeathEffect(FVector DeathLocation);
	virtual void SpawnDeathEffect_Implementation(FVector DeathLocation);

	FTimerHandle DeathDestroyTimerHandle;
	void HandleDelayedDestroy();

	/* Working Progression */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int MaxWorkers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int CurrWorkers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	USphereComponent* CollisionSphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	UStaticMeshComponent* RootStaticMeshComp;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
};
