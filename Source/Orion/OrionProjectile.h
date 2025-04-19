// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrionProjectile.generated.h"


UENUM(BlueprintType)
enum class EOrionProjectile : uint8
{
	Arrow,
	Bullet,
};

UCLASS()
class ORION_API AOrionProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AOrionProjectile();

protected:
	virtual void BeginPlay() override;

public:	
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
    class UStaticMeshComponent* ArrowMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Components")
    class UProjectileMovementComponent* ProjectileMovement;

    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
        FVector NormalImpulse, const FHitResult& Hit);

    bool bHasHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float ProjectileDamage = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OrionProjectileAttribute")
	EOrionProjectile OrionProjectileType;
};
