﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Orion/OrionInterface/OrionInterfaceActionable.h"
#include "OrionAIController.generated.h"

UENUM(BlueprintType)
enum class ERelation : uint8
{
	Hostile,
	Friendly,
	Neutral,
};


UCLASS()
class ORION_API AOrionAIController : public AAIController
{
	GENERATED_BODY()

public:
	AOrionAIController();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void OnPossess(APawn* InPawn) override;

	/* Deprecated */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAIPerceptionComponent* AIPerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UAISenseConfig_Sight* SightConfig;

	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// AIController Utility Functions

	IOrionInterfaceActionable* ControlledPawnActionableInterface;

	UPROPERTY()
	AOrionChara* ControlledPawn;

	/* Basic AI Logics */

	AOrionChara* GetClosestOrionCharaByRelation(
		ERelation Relation,
		const TArray<int>& HostileGroups,
		const TArray<int>& FriendlyGroups
	) const;

	void RegisterDefensiveAIActon();
	void RegisterFetchingAmmoEvent();

	EAIState CachedAIState;
};
