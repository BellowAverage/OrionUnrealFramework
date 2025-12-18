// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionAttributeComponent.h"

// Sets default values for this component's properties
UOrionAttributeComponent::UOrionAttributeComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// Defaults
	MaxHealth = 100.0f;
	Health = 100.0f;
}


// Called when the game starts
void UOrionAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	// Ensure Health starts at MaxHealth
	Health = MaxHealth;
}


// Called every frame
void UOrionAttributeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UOrionAttributeComponent::ReceiveDamage(float DamageAmount, AActor* InstigatorActor)
{
	if (DamageAmount <= 0.0f || Health <= 0.0f)
	{
		return;
	}

	float OldHealth = Health;
	Health = FMath::Clamp(Health - DamageAmount, 0.0f, MaxHealth);
	float ActualDelta = OldHealth - Health;

	// Broadcast change
	OnHealthChanged.Broadcast(InstigatorActor, this, Health, ActualDelta);

	// Check for death
	if (Health <= 0.0f)
	{
		OnHealthZero.Broadcast(InstigatorActor);
	}
}

void UOrionAttributeComponent::SetHealth(float NewHealth)
{
	float OldHealth = Health;
	Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	
	// We treat manual set as "neutral" change or pass nullptr if we don't have instigator
	if (Health != OldHealth)
	{
		OnHealthChanged.Broadcast(nullptr, this, Health, OldHealth - Health);
		
		if (Health <= 0.0f)
		{
			OnHealthZero.Broadcast(nullptr);
		}
	}
}

bool UOrionAttributeComponent::IsAlive() const
{
	return Health > 0.0f;
}

float UOrionAttributeComponent::GetHealthPercent() const
{
	if (MaxHealth > 0.0f)
	{
		return Health / MaxHealth;
	}
	return 0.0f;
}
