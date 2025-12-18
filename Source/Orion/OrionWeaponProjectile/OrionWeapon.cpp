// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionWeapon.h"
#include "OrionProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include <Kismet/GameplayStatics.h>
#include "Sound/SoundCue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"

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

void AOrionWeapon::SpawnFireVisual(const FVector& SpawnLocation, const FVector& ForwardDirection) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FRotator SpawnRot = ForwardDirection.Rotation();

	if (SC_AssaultRifleShot)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			SC_AssaultRifleShot,
			SpawnLocation
		);

		UE_LOG(LogTemp, Log, TEXT("[Weapon] Played AssaultRifleShot sound at %s"), *SpawnLocation.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Weapon] SC_AssaultRifleShot is null, cannot play sound."));
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

void AOrionWeapon::SpawnOrionBulletActor(const FVector& SpawnLocation, const FVector& ForwardDirection) const
{
	// DrawDebugSphere(GetWorld(), SpawnLocation, 10.f, 12, FColor::Blue, false, 10.f);

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

	SpawnFireVisual(SpawnLocation, ForwardDirection);
}

void AOrionWeapon::Fire(const FVector& SpawnLocation, const FVector& Direction, EOrionWeaponType WeaponType, int32 PelletsCount, float SpreadAngle)
{
	if (WeaponType == EOrionWeaponType::Rifle)
	{
		// Rifle: Apply random spread based on SpreadAngle
		FVector FinalDir = Direction;
		
		if (SpreadAngle > 0.f)
		{
			// Apply random spread for Rifle
			FinalDir = UKismetMathLibrary::RandomUnitVectorInConeInDegrees(Direction, SpreadAngle);
		}
		
		SpawnOrionBulletActor(SpawnLocation, FinalDir);
	}
	else if (WeaponType == EOrionWeaponType::Shotgun)
	{
		// Shotgun: cone damage instead of pellets
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		const float ConeLength = 200.f;
		const float ConeRadius = 100.f; // diameter 200
		const float HalfAngleRad = FMath::Atan2(ConeRadius, ConeLength);
		const float DamageAmount = 10.f;

		const FVector Forward = Direction.GetSafeNormal();
		const float CosHalfAngle = FMath::Cos(HalfAngleRad);

		// Debug: visualize cone
		DrawDebugCone(World, SpawnLocation, Forward, ConeLength, HalfAngleRad, HalfAngleRad, 16, FColor::Red, false, 1.0f, 0, 2.0f);
		DrawDebugLine(World, SpawnLocation, SpawnLocation + Forward * ConeLength, FColor::Yellow, false, 1.0f, 0, 1.5f);

		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectQueryParams = FCollisionObjectQueryParams::AllDynamicObjects;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShotgunCone), false, GetOwner());
		const FCollisionShape SphereShape = FCollisionShape::MakeSphere(ConeLength);

		if (World->OverlapMultiByObjectType(Overlaps, SpawnLocation, FQuat::Identity, ObjectQueryParams, SphereShape, QueryParams))
		{
			for (const FOverlapResult& Result : Overlaps)
			{
				AActor* HitActor = Result.GetActor();
				if (!HitActor || HitActor == GetOwner())
				{
					continue;
				}

				const FVector ToActor = HitActor->GetActorLocation() - SpawnLocation;
				const float Distance = ToActor.Size();
				if (Distance > ConeLength || Distance <= KINDA_SMALL_NUMBER)
				{
					continue;
				}

				const float ForwardDot = FVector::DotProduct(Forward, ToActor.GetSafeNormal());
				if (ForwardDot < CosHalfAngle)
				{
					continue; // outside cone
				}

				if (UOrionAttributeComponent* AttributeComp = HitActor->FindComponentByClass<UOrionAttributeComponent>())
				{
					AttributeComp->ReceiveDamage(DamageAmount, GetOwner());
					DrawDebugSphere(World, HitActor->GetActorLocation(), 20.f, 12, FColor::Green, false, 1.0f);
					// High-contrast box on hit actors for quick visual check
					DrawDebugBox(World, HitActor->GetActorLocation(), FVector(10.f), FColor::Emerald, false, 1.0f, 0, 2.0f);
				}
			}
		}

		SpawnFireVisual(SpawnLocation, Direction);
	}
}
