// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../OrionActor.h"
#include "OrionActorOre.generated.h"

/**
 * 
 */
UCLASS()
class ORION_API AOrionActorOre : public AOrionActor
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int ProductionTimeCost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	float ProductionProgress;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void ProductionProgressUpdate(float DeltaTime);
};
