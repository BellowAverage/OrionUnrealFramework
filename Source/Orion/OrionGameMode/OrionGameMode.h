// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Orion/OrionChara/OrionChara.h"
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WheeledVehiclePawn.h"
#include "OrionGameMode.generated.h"


UCLASS(Blueprintable)
class ORION_API AOrionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOrionGameMode();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Gameplay Stats */

	/* Gameplay Requests Handle */

	UFUNCTION(BlueprintImplementableEvent)
	void OrionVehicleMoveToLocation(AWheeledVehiclePawn* WheeledVehiclePawn, FVector TargetLocation);

	/* Developer */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer")
	TSubclassOf<AOrionChara> SubclassOfOrionChara;

	UFUNCTION(BlueprintCallable, Category = "Developer")
	void SpawnCharaInstance(FVector SpawnLocation);

	void OnTestKey1Pressed();
	void OnTestKey2Pressed();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer")
	TSubclassOf<AActor> ExplosionClass;
};
