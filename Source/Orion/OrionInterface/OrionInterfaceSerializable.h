// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Misc/Guid.h"
#include "OrionInterfaceSerializable.generated.h"

USTRUCT(BlueprintType)
struct FSerializable
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid GameId;
};

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UOrionInterfaceSerializable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ORION_API IOrionInterfaceSerializable
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION()
	virtual void InitSerializable(const FSerializable& InSerializable) = 0;

	UFUNCTION()
	virtual FSerializable GetSerializable() const = 0;
};
