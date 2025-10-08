﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionGlobals/EOrionStructure.h"
#include "OrionStructureComponent.generated.h"



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ORION_API UOrionStructureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool BIsPreviewStructure = true;


	UOrionStructureComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoRegisterSockets = true;

	void RegisterSquareFoundationSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterTriangleFoundationSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterWallSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterDoubleWallSockets(const FVector& StructureLocation, const FRotator& StructureRotation) const;
	void RegisterAllSockets() const;

	/* Orion Structure Rule Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EOrionStructure OrionStructureType;

	UPROPERTY()
	UStaticMeshComponent* StructureMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool bForceSnapOnGrid = false;

	static const TMap<EOrionStructure, FVector> StructureBoundMap;

	static FVector GetStructureBounds(const EOrionStructure Type);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool bIsAdjustable = false;

private:
	UPROPERTY()
	UOrionBuildingManager* BuildingManager = nullptr;
};
