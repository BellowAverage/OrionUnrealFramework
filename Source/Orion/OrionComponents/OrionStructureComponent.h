// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionGlobals/EOrionStructure.h"
#include "OrionStructureComponent.generated.h"

USTRUCT()
struct FRegisteredSocketHandle
{
	GENERATED_BODY()
	int32 RawIndex = INDEX_NONE;
};


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ORION_API UOrionStructureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOrionStructureComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bAutoRegisterSockets = true;

	void RegisterAllSockets() const;

	/* Orion Structure Rule Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EOrionStructure OrionStructureType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	UStaticMeshComponent* StructureMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	bool bForceSnapOnGrid = false;

private:
	UPROPERTY()
	UOrionBuildingManager* BuildingManager = nullptr;
	UPROPERTY()
	TArray<FRegisteredSocketHandle> Handles;
};
