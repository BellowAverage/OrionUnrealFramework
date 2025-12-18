// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OrionInterfaceInteractable.generated.h"

class AOrionActor;
class AOrionChara;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UOrionInterfaceInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ORION_API IOrionInterfaceInteractable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	virtual void ShowInteractOptions() = 0;
	virtual bool ApplyInteractionToCharas(TArray<AOrionChara*> InteractedCharas, AOrionActor* InteractingActor) = 0;
};
