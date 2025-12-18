// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Orion/OrionGameInstance/OrionFactionManager.h"
#include "OrionAttributeComponent.generated.h"

// 声明委托
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHealthChanged, AActor*, InstigatorActor, UOrionAttributeComponent*, OwningComp, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHealthZero, AActor*, InstigatorActor);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ORION_API UOrionAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UOrionAttributeComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* --- 属性 --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attributes")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attributes")
	EFaction ActorFaction = EFaction::PlayerFaction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Attributes")
	bool IsBaseStorage = false;

	/* --- 委托 --- */
	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FOnHealthZero OnHealthZero;

	/* --- 功能 --- */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void ReceiveDamage(float DamageAmount, AActor* InstigatorActor);

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetHealth(float NewHealth);

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "Attributes")
	float GetHealthPercent() const;
};
