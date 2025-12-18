// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Orion/OrionInterface/OrionInterfaceHoverable.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"
#include "OrionStructure.generated.h"


UCLASS()
class ORION_API AOrionStructure : public AActor, public IOrionInterfaceHoverable
{
	GENERATED_BODY()

public:
	AOrionStructure();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Basics */

	bool bForceSnapOnGrid = false;

	UPROPERTY()
	UOrionBuildingManager* BuildingManager = nullptr;

	UPROPERTY()
	UOrionStructureComponent* StructureComponent = nullptr;

	/* Attributes */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UOrionAttributeComponent* AttributeComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MaxHealth = 1000.0f;

	UFUNCTION()
	virtual void HandleHealthZero(AActor* InstigatorActor);

	virtual void Die();

	/* Hover info for debug */
	virtual TArray<FString> TickShowHoveringInfo() override;


	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;
};
