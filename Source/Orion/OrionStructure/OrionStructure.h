// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrionStructure.generated.h"

UCLASS()
class ORION_API AOrionStructure : public AActor
{
	GENERATED_BODY()

public:
	AOrionStructure();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	UStaticMeshComponent* StructureMesh;
};
