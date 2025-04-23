// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionWeapon.h"
#include "OrionProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include <Kismet/GameplayStatics.h>
#include "Kismet/KismetMathLibrary.h"

AOrionWeapon::AOrionWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AOrionWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AOrionWeapon::SpawnOrionBulletActor(
	const FVector& SpawnLocation,
	const FVector& ForwardDirection)
{
	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[Weapon] ProjectileClass is null. Cannot spawn projectile."));
		return;
	}
	if (!ProjectileClass->IsChildOf(AOrionProjectile::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("[Weapon] ProjectileClass is not a subclass of AOrionProjectile."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRotator SpawnRot = ForwardDirection.Rotation();

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.Instigator = Cast<APawn>(GetOwner());

	AOrionProjectile* Proj = World->SpawnActor<AOrionProjectile>(
		ProjectileClass,
		SpawnLocation,
		SpawnRot,
		Params
	);

	if (Proj && Proj->ProjectileMovement)
	{
		Proj->ProjectileMovement->Velocity =
			ForwardDirection * Proj->ProjectileMovement->InitialSpeed;
	}
	else
	{
		//UE_LOG(LogTemp, Warning, TEXT("[Weapon] Failed to spawn or setup projectile."));
	}

	if (MuzzleFlashEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			MuzzleFlashEffect,
			SpawnLocation + FVector(0.0f, 0.0f, 0.0f),
			SpawnRot
		);
		//UE_LOG(LogTemp, Verbose, TEXT("[Weapon] Spawned Niagara MuzzleFlash at %s"), *SpawnLocation.ToString());
	}

	if (MuzzleParticleSystem)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World,
			MuzzleParticleSystem,
			SpawnLocation,
			SpawnRot,
			true
		);
		//UE_LOG(LogTemp, Verbose, TEXT("[Weapon] Spawned Particle MuzzleFlash at %s"), *SpawnLocation.ToString());
	}
}
