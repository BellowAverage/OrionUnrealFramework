// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionGameInstance/OrionCharaManager.h"
#include "OrionGameMode.generated.h"

class AOrionActorStorage;
class AOrionActorProduction;
class AOrionActorOre;

UCLASS(Blueprintable)
class ORION_API AOrionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOrionGameMode();

	void OnTestKey3Pressed();
	void OnTestKey4Pressed();

	UFUNCTION(BlueprintCallable, Category = "Developer")
	void OnTestKey2Pressed();

	UFUNCTION(BlueprintCallable, Category = "Developer")
	void OnTestKey5Pressed();

	UFUNCTION(BlueprintCallable, Category = "Developer")
	void OnTestKey6Pressed();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;
	/* Developer */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer")
	TSubclassOf<AOrionChara> SubclassOfOrionChara;

	UFUNCTION(BlueprintCallable, Category = "Developer")
	AOrionChara* SpawnCharaInstance(const FVector& SpawnLocation, const FOrionCharaSpawnParams& SpawnParams = FOrionCharaSpawnParams());

	void OnTestKey1Pressed();
	bool GenerateExplosionSimulation();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Developer")
	TSubclassOf<AActor> ExplosionClass;

	/* Enemy / Ally spawn settings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float EnemyWaveInterval = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float AllySpawnInterval = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float EnemySpawnInnerRadius = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float EnemySpawnOuterRadius = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float AllySpawnRadius = 5000.f;

	/* Game Time System */
	
	/** 现实时间与游戏时间的比例（现实时间:游戏时间），例如1.0表示1秒现实时间=1秒游戏时间，60.0表示1秒现实时间=60秒游戏时间 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Time")
	float TimeScale = 60.0f;

	/** 游戏开始时的世界时间（秒） */
	float GameStartTime = 0.0f;

	/** 当前游戏时间（秒，从游戏开始累计） */
	float CurrentGameTime = 0.0f;

	/** 更新游戏时间并刷新UI */
	void UpdateGameTime(float DeltaTime);

	/** 格式化游戏时间为 "Day X | HH:MM" 格式 */
	FString FormatGameTime(float GameTimeInSeconds) const;

	/* Debug Settings */
	
	/** 是否在屏幕上显示OrionChara的调试信息（名称、血量等） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bShowOrionCharaDebugInfo = false;

public:
	void InitSpawnTimers();
	void SpawnEnemyWave();
	void SpawnAllyReinforcement();
	int32 GetCurrentWaveEnemyCount() const;
	FVector GetRandomPointInAnnulus(float MinRadius, float MaxRadius) const;
	FVector GetRandomPointInRadius(float Radius) const;
	FVector FindValidSpawnLocationNearOrigin(float MaxOffsetRadius = 200.f) const;
	AOrionChara* SpawnEnemyCharaAt(const FVector& SpawnLocation);
	AOrionChara* SpawnFriendlyCharaAt(const FVector& SpawnLocation);
	void IssueMoveOrder(AOrionChara* Chara, const FVector& TargetLocation) const;

	FTimerHandle EnemyWaveTimerHandle;
	FTimerHandle AllySpawnTimerHandle;
	TArray<int32> EnemyWaveCounts = {1, 1, 1, 1, 5, 5, 5, 10, 10, 10, 10, 20, 20, 20, 30};
	int32 CurrentWaveIndex = 0;

	/* 简易消息队列，用于向玩家展示游戏事件 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Messaging")
	float MessageDisplayInterval = 2.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Messaging")
	TArray<FString> PendingMessages;

	float MessageCooldown = 0.0f;

	void EnqueueGameMessage(const FString& Message);
	void ProcessMessageQueue(float DeltaTime);
	void DisplayGameMessage(const FString& Message) const;
	FString GetWaveOrdinal(int32 WaveNumber) const;
};
