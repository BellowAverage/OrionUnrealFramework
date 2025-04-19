// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "OrionAIController.generated.h"

/**
 * 
 */
UCLASS()
class ORION_API AOrionAIController : public AAIController
{
    GENERATED_BODY()

public:
    AOrionAIController();

protected:

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* AIPerceptionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAISenseConfig_Sight* SightConfig;

    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	// AIController Utiltiy Functions
	UFUNCTION(BlueprintCallable, Category = "AIController Utiltiy")
	FString GetControlledPawnName() const;
};
