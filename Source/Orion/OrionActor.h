// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrionActor.generated.h"


UENUM(BlueprintType)
enum class EActorStatus : uint8
{
	Interactable UMETA(DisplayName = "Interactable"),
	NotInteractable UMETA(DisplayName = "NotInteractable"),
};

UCLASS()
class ORION_API AOrionActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AOrionActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EActorStatus ActorStatus;

	/* Basic Properties API */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	FString InteractType;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	FString GetInteractType();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int OutItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int InItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int ProductionTimeCost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int MaxWorkers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int CurrWorkers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	float ProductionProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int CurrInventory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int MaxInventory;

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

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void ProductionProgressUpdate(float DeltaTime);

	/* Inventory Utility */

	void OnInventoryUpdate(float DeltaTime);
	void OnInventoryExceeded();
	int PreviousInventory;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
};
