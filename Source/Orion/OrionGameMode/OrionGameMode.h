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

	void OnTestKey3Pressed();
	void OnTestKey4Pressed();
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
	bool GenerateExplosionSimulation();

	UFUNCTION(BlueprintCallable, Category = "Developer")
	void OnTestKey2Pressed();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer")
	TSubclassOf<AActor> ExplosionClass;

	/* Test Function Configuration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer|Test", meta = (ToolTip = "Random seed for test function. Set this to reproduce the same random task assignments."))
	int32 TestRandomSeed = 555;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer|Test", meta = (ToolTip = "Minimum number of tasks to assign to each character"))
	int32 MinTasksPerChara = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer|Test", meta = (ToolTip = "Maximum number of tasks to assign to each character"))
	int32 MaxTasksPerChara = 4;
};
