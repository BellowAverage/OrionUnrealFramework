// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OrionInterfaceActionable.generated.h"

class AOrionActorStorage;
class AOrionActorProduction;
class AOrionActor;
enum class EActionExecution : uint8;
class FOrionAction;
class AOrionChara;


// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UOrionInterfaceActionable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ORION_API IOrionInterfaceActionable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.

public:
	virtual void InsertOrionActionToQueue(
		const FOrionAction& OrionActionInstance,
		const EActionExecution ActionExecutionType,
		const int32 Index = INDEX_NONE) = 0;

	virtual FOrionAction InitActionMoveToLocation(
		const FString& ActionName,
		const FVector& TargetLocation) = 0;

	virtual FOrionAction InitActionAttackOnChara(
		const FString& ActionName,
		AActor* TargetChara,
		const FVector& HitOffset) = 0;

	virtual FOrionAction InitActionInteractWithActor(
		const FString& ActionName,
		AOrionActor* TargetActor) = 0;

	virtual FOrionAction InitActionInteractWithProduction(
		const FString& ActionName,
		AOrionActorProduction* TargetActor) = 0;

	virtual FOrionAction InitActionCollectCargo(
		const FString& ActionName,
		AOrionActorStorage* TargetActor) = 0;

	virtual FOrionAction InitActionCollectBullets(
		const FString& ActionName) = 0;

	virtual bool GetIsCharaProcedural() = 0;
	virtual bool SetIsCharaProcedural(bool bInIsCharaProcedural) = 0;

	virtual void RemoveAllActions(const FString& Except = FString()) = 0;

	virtual FString GetUnifiedActionName() const = 0;
};
