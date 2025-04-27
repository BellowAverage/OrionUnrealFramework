#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OrionProjectile.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundCue.h"
#include "OrionWeapon.generated.h"

UCLASS()
class ORION_API AOrionWeapon : public AActor
{
	GENERATED_BODY()

public:
	AOrionWeapon();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
	TSubclassOf<AOrionProjectile> ProjectileClass;
	UFUNCTION(BlueprintCallable, Category = "Bullet")
	void SpawnOrionBulletActor(
		const FVector& SpawnLocation,
		const FVector& ForwardDirection);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bullet")
	float SpawnZOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* MuzzleFlashEffect;

	UPROPERTY(EditAnywhere, Category = "Particles")
	UParticleSystem* MuzzleParticleSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound Effect")
	USoundCue* SC_AssaultRifleShot;
};
