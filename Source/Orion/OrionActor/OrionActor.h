// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Orion/OrionInterface/OrionInterfaceSerializable.h"
#include "Misc/Guid.h"
#include "OrionActor.generated.h"

USTRUCT(BlueprintType)
struct FOrionActorFullRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	FGuid ActorGameId;

	UPROPERTY(SaveGame)
	FTransform ActorTransform;

	UPROPERTY(SaveGame)
	FString ClassPath;

	/* 标记：SerializedBytes.Num() == 0 视为无效 */
	UPROPERTY(SaveGame)
	TArray<uint8> SerializedBytes;
};


UENUM(BlueprintType)
enum class EActorStatus : uint8
{
	Interactable UMETA(DisplayName = "Interactable"),
	NotInteractable UMETA(DisplayName = "NotInteractable"),
};

UENUM(BlueprintType)
enum class EActorCategory : uint8
{
	None UMETA(DisplayName = "None"),
	Ore UMETA(DisplayName = "Ore"),
	Storage UMETA(DisplayName = "Storage"),
	Production UMETA(DisplayName = "Production"),
};

UCLASS()
class ORION_API AOrionActor : public AActor, public IOrionInterfaceSerializable
{
	GENERATED_BODY()

public:
	AOrionActor();

	/* --- 序列化接口 --- */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Basics")
	FSerializable ActorSerializable;

	virtual void InitSerializable(const FSerializable& InSerializable) override;
	virtual FSerializable GetSerializable() const override { return ActorSerializable; }

	/* --- 交互 / 分类 --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	EActorStatus ActorStatus = EActorStatus::Interactable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	FString InteractType;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	FString GetInteractType() const { return InteractType; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	EActorCategory ActorCategory = EActorCategory::None;

	/* --- 物品 --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Inventory")
	TMap<int32, int32> AvailableInventoryMap;

	/* --- 组件（指针不存档） --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Components")
	UOrionInventoryComponent* InventoryComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Basics")
	UStaticMeshComponent* RootStaticMeshComp;

	/* --- 属性 --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	int32 MaxHealth = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	int32 CurrHealth = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	TSubclassOf<AActor> DeathEffectClass;

	/* --- 生产 --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	int32 MaxWorkers = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	int32 CurrWorkers = 0;

	/* --- 基本行为 --- */
	virtual float TakeDamage(float DamageAmount,
	                         const FDamageEvent& DamageEvent,
	                         AController* EventInstigator,
	                         AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	void Die();
	void HandleDelayedDestroy();

	UFUNCTION(BlueprintNativeEvent, Category = "Basics")
	void SpawnDeathEffect(FVector DeathLocation);
	virtual void SpawnDeathEffect_Implementation(FVector DeathLocation);

public:
	virtual void Tick(float DeltaTime) override;

private:
	FTimerHandle DeathDestroyTimerHandle;
};
