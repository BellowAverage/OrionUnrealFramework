// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrionActor.generated.h"

UCLASS()
class ORION_API AOrionActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOrionActor();

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

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void ProductionProgressUpdate(float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Basics")
	int GetOutItemId() const;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	int GetInItemId() const;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	int GetProductionTimeCost() const;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	int GetMaxWorkers() const;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	int GetCurrWorkers() const;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	float GetProductionProgress() const;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	int GetCurrInventory() const;

protected:
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

};
