// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionStructure.h"
#include "OrionStructureFoundation.generated.h"

/**
 * 
 */

class AOrionStructureFoundation;

USTRUCT()
struct FFoundationSocket
{
	GENERATED_BODY()

	UPROPERTY()
	AOrionStructureFoundation* SocketStructurePtr;

	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	FVector SocketLocation;

	UPROPERTY()
	FRotator SocketRotation;

	FFoundationSocket()
		: SocketStructurePtr(nullptr)
		  , bIsOccupied(false)
		  , SocketLocation(FVector::ZeroVector)
		  , SocketRotation(FRotator::ZeroRotator)
	{
	}

	FFoundationSocket(AOrionStructureFoundation* InSocketStructurePtr,
	                  bool InbIsOccupied,
	                  FVector InSocketLocation,
	                  FRotator InSocketRotator)
		: SocketStructurePtr(InSocketStructurePtr)
		  , bIsOccupied(InbIsOccupied)
		  , SocketLocation(InSocketLocation)
		  , SocketRotation(InSocketRotator)
	{
	}
};


UCLASS()
class ORION_API AOrionStructureFoundation : public AOrionStructure
{
	GENERATED_BODY()

public:
	AOrionStructureFoundation();

	virtual void BeginPlay() override;

	/* Sockets */
	TArray<FFoundationSocket> FoundationSockets;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void PlaceFoundationStructure(int32 InSocketIndex, FRotator& InRotationOffset);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	TSubclassOf<AOrionStructureFoundation> BlueprintFoundationInstance;
};
