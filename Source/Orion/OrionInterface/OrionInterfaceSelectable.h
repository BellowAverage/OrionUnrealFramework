// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OrionInterfaceSelectable.generated.h"

UENUM(BlueprintType)
enum class ESelectable : uint8
{
	OrionActor,
	OrionStructure,
	OrionChara,
	VehiclePawn
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UOrionInterfaceSelectable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ORION_API IOrionInterfaceSelectable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	virtual ESelectable GetSelectableType() const = 0;
	virtual void OnSelected(APlayerController* PlayerController) = 0;
	virtual void OnRemoveFromSelection(APlayerController* PlayerController) = 0;
};
