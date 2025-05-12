// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Orion/OrionChara/OrionChara.h"
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"
#include "WheeledVehiclePawn.h"
#include "Orion/OrionActor/OrionActorStorage.h"
#include "Orion/OrionActor/OrionActorProduction.h"
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
	//UFUNCTION(BlueprintCallable, Category = "Gameplay Requests")
	void ApproveCharaAttackOnActor(std::vector<AOrionChara*> OrionCharasRequested, AActor* TargetActor,
	                               FVector HitOffset, CommandType inCommandType);

	//UFUNCTION(BlueprintCallable, Category = "Gameplay Requests")
	bool ApproveCharaMoveToLocation(std::vector<AOrionChara*> OrionCharasRequested, FVector TargetLocation,
	                                CommandType inCommandType);

	//UFUNCTION(BlueprintCallable, Category = "Gameplay Requests")
	bool ApprovePawnMoveToLocation(std::vector<AWheeledVehiclePawn*> OrionPawnsRequested, FVector TargetLocation,
	                               CommandType inCommandType);

	//UFUNCTION(BlueprintCallable, Category = "Gameplay Requests")
	void ApproveInteractWithActor(std::vector<AOrionChara*> OrionCharasRequested, AOrionActor* TargetActor,
	                              CommandType inCommandType);

	void ApproveCollectingCargo(std::vector<AOrionChara*> OrionCharasRequested, AOrionActorStorage* TargetActor,
	                            CommandType inCommandType);

	void ApproveInteractWithProduction(std::vector<AOrionChara*> OrionCharasRequested,
	                                   AOrionActorProduction* TargetActor,
	                                   CommandType inCommandType);

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
