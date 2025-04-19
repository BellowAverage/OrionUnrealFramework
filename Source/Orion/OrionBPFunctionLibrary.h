// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OrionBPFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class ORION_API UOrionBPFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Default")
	static void OrionPrint(FString String2Print);

	UFUNCTION(BlueprintCallable, Category = "Default")
	static float OrionOscillation(float DeltaTime, float OscillatingSpeed = 1.f, float Amplitude = -1.f);
	static float TimeAccumulated;

	UFUNCTION(BlueprintCallable, Category = "Developer")
	static void FindSourceInURL(const FString& url);
	
	static void DisablePhysicsAndCollision(UActorComponent* Component);
	static void DisablePhysicsAndCollisionForAllComponents(AActor* TargetActor);
	UFUNCTION(BlueprintCallable, Category = "Developer")
	static void DisablePhysicsOnActor(AActor* TargetActor);

	static bool CheckActorPhysicsStatus(AActor* TargetActor);

};
