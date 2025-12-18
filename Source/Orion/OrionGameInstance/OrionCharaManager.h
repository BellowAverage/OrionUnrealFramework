// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionComponents/OrionActionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Serialization/BufferArchive.h"
#include "Orion/OrionComponents/OrionCombatComponent.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "OrionCharaManager.generated.h"

USTRUCT(BlueprintType)
struct FOrionCharaSpawnParams
{
	GENERATED_BODY()

	/** Maximum health of the character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Params")
	float MaxHealth = 10.0f;

	/** Weapon Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Params")
	EOrionWeaponType WeaponType = EOrionWeaponType::Rifle;

	/** Character side/team identifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Params")
	int32 CharaSide = 1;

	/** Hostile group indices */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Params")
	TArray<int32> HostileGroupsIndex = {0};

	/** Character AI state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Params")
	EAIState CharaAIState = EAIState::Passive;

	/** Spawn rotation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Params")
	FRotator SpawnRotation = FRotator::ZeroRotator;

	/** Initial RealTime actions to add to the character's queue */
	TArray<FOrionActionParams> InitialRealTimeActions;

	/** Initial Procedural actions to add to the character's queue */
	TArray<FOrionActionParams> InitialProceduralActions;

	/* Initial Speed */
	float InitialSpeed = 0.f;
};

/**
 * 
 */
UCLASS()
class ORION_API UOrionCharaManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	UPROPERTY()
	TSubclassOf<AOrionChara> CharacterBPClass;

	static TArray<FOrionCharaSerializable> TestCharactersSet;

	void CollectCharacterRecords(TArray<FOrionCharaSerializable>& OutRecords)
	{
		OutRecords.Empty();

		for (const TPair<FGuid, TWeakObjectPtr<AOrionChara>>& Pair : GlobalCharaMap)
		{
			if (AOrionChara* Chara = Pair.Value.Get())
			{
				FBufferArchive MemWriter;
				FObjectAndNameAsStringProxyArchive Ar(MemWriter, /*bLoadIn=*/false);
				Ar.ArIsSaveGame = true;

				Chara->Serialize(Ar);

				FOrionCharaSerializable S;

				S.CharaGameId = Chara->GameSerializable.GameId;
				S.CharaLocation = Chara->GetActorLocation();
				S.CharaRotation = Chara->GetActorRotation();
				S.SerializedBytes = MemWriter;

				if (Chara->ActionComp)
				{
					for (const FOrionAction& Act : Chara->ActionComp->ProceduralActionQueue.Actions)
					{
						S.SerializedProcActions.Add(Act.Params);
					}
				}

				OutRecords.Add(MoveTemp(S));
			}
		}
	}

	void RemoveAllCharacters(UWorld* World)
	{
		for (const TPair<FGuid, TWeakObjectPtr<AOrionChara>>& Pair : GlobalCharaMap)
		{
			if (AOrionChara* Chara = Pair.Value.Get())
			{
				Chara->Destroy();
			}
		}
		GlobalCharaMap.Empty();
	}

	void LoadAllCharacters(UWorld* World,
	                       const TArray<FOrionCharaSerializable>& Saved)
	{
		if (!World)
		{
			return;
		}

		GlobalCharaMap.Empty();

		/* ----------① Spawn & Register（暂不恢复动作）---------- */
		TMap<FGuid, AOrionChara*> SpawnedMap;

		for (const FOrionCharaSerializable& S : Saved)
		{
			if (AOrionChara* Chara = SpawnOrionChara(S.CharaLocation, S.CharaRotation))
			{
				/* ① 基本标识 */
				Chara->GameSerializable.GameId = S.CharaGameId;

				/* ★② 反序列化 SaveGame 字节数组 —— 把存档属性写回对象 ★ */
				{
					FMemoryReader MemReader(S.SerializedBytes);
					FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bLoadIn=*/true);
					Ar.ArIsSaveGame = true; // 只恢复带 SaveGame 标记的属性
					Chara->Serialize(Ar); // ← 这里会把 IsCharaProceduralInInit 等还原
				}

				/* ③ 注册到全局表 */
				RegisterChara(Chara);
				SpawnedMap.Add(S.CharaGameId, Chara);
			}
		}

		/* ----------② 恢复动作 ---------- */
		for (const FOrionCharaSerializable& S : Saved)
		{
			if (AOrionChara** CharaPtr = SpawnedMap.Find(S.CharaGameId))
			{
				RecoverProcActions(*CharaPtr, S);
			}
		}
	}

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);

		CharacterBPClass = LoadObject<UClass>(
			nullptr,
			TEXT("/Game/_Orion/Blueprints/OrionCharacter.OrionCharacter_C"));

		if (!CharacterBPClass)
		{
			UE_LOG(LogTemp, Error,
			       TEXT("[OrionCharaManager] Failed to load CharacterBPClass!"));
		}
	}

	FORCEINLINE AOrionChara* FindCharaById(const FGuid& Id) const
	{
		const TWeakObjectPtr<AOrionChara>* Ptr = GlobalCharaMap.Find(Id);
		return Ptr && Ptr->IsValid() ? Ptr->Get() : nullptr;
	}

	/** Spawn a character instance with deferred construction and optional initial actions */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Spawn")
	AOrionChara* SpawnCharaInstance(const FVector& SpawnLocation, 
	                                const FOrionCharaSpawnParams& SpawnParams = FOrionCharaSpawnParams(),
	                                TSubclassOf<AOrionChara> CharacterClass = nullptr);

	TMap<FGuid, TWeakObjectPtr<AOrionChara>> GlobalCharaMap;

private:

	void SpawnAndRegisterOrionChara(const FOrionCharaSerializable& Serializable)
	{
		if (AOrionChara* Chara =
			SpawnOrionChara(Serializable.CharaLocation, Serializable.CharaRotation))
		{
			Chara->GameSerializable.GameId = Serializable.CharaGameId;

			FMemoryReader MemReader(Serializable.SerializedBytes);
			FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bLoadIn=*/true);
			Ar.ArIsSaveGame = true;
			Chara->Serialize(Ar);


			RegisterChara(Chara);
			RecoverProcActions(Chara, Serializable);
		}
	}


	AOrionChara* SpawnOrionChara(const FVector& Location,
	                             const FRotator& Rotation) const
	{
		if (!CharacterBPClass)
		{
			return nullptr;
		}

		UWorld* World = GetWorld();
		if (!World)
		{
			return nullptr;
		}

		/* 投射到地面 */
		const FVector TraceStart(Location.X, Location.Y, Location.Z + 10000.f);
		const FVector TraceEnd(Location.X, Location.Y, Location.Z - 10000.f);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpawnOrionChara), true);

		FVector SpawnLoc = Location;
		if (FHitResult Hit; World->LineTraceSingleByChannel(
			Hit, TraceStart, TraceEnd, ECC_GameTraceChannel1, QueryParams))
		{
			float HalfH =
				CharacterBPClass->GetDefaultObject<AOrionChara>()
				                ->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			SpawnLoc = Hit.Location + FVector(0.f, 0.f, HalfH);
		}

		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AOrionChara* SpawningChara = World->SpawnActor<AOrionChara>(CharacterBPClass, SpawnLoc, Rotation, P);

		return SpawningChara;
	}


	void RegisterChara(AOrionChara* Chara)
	{
		if (Chara)
		{
			GlobalCharaMap.Add(Chara->GameSerializable.GameId, Chara);
		}
	}

	void RecoverProcActions(AOrionChara* Chara,
	                        const FOrionCharaSerializable& S);

	/** Helper function to add initial actions to character's queue */
	void AddInitialActionsToChara(AOrionChara* Chara, const FOrionCharaSpawnParams& SpawnParams);

	/** 
	 * 向指定角色添加移动到位置的动作
	 * @param Chara 目标角色指针
	 * @param TargetLocation 目标位置
	 * @param ExecutionType 动作执行类型（Procedural 或 RealTime），默认为 Procedural
	 * @param Index 插入位置索引，-1 表示添加到队列末尾
	 * @return 是否成功添加动作
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool AddMoveToLocationAction(AOrionChara* Chara, const FVector& TargetLocation,
	                             EActionExecution ExecutionType = EActionExecution::Procedural,
	                             int32 Index = -1);

	/** 
	 * 向指定角色添加攻击角色的动作
	 * @param Chara 目标角色指针
	 * @param TargetChara 要攻击的目标角色
	 * @param HitOffset 命中偏移量
	 * @param ExecutionType 动作执行类型（Procedural 或 RealTime），默认为 Procedural
	 * @param Index 插入位置索引，-1 表示添加到队列末尾
	 * @return 是否成功添加动作
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool AddAttackOnCharaAction(AOrionChara* Chara, AOrionChara* TargetChara, const FVector& HitOffset = FVector::ZeroVector,
	                            EActionExecution ExecutionType = EActionExecution::Procedural,
	                            int32 Index = -1);

	/** 
	 * 向指定角色添加与Actor交互的动作
	 * @param Chara 目标角色指针
	 * @param TargetActor 要交互的目标Actor
	 * @param ExecutionType 动作执行类型（Procedural 或 RealTime），默认为 Procedural
	 * @param Index 插入位置索引，-1 表示添加到队列末尾
	 * @return 是否成功添加动作
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool AddInteractWithActorAction(AOrionChara* Chara, AOrionActor* TargetActor,
	                                EActionExecution ExecutionType = EActionExecution::Procedural,
	                                int32 Index = -1);

	/** 
	 * 向指定角色添加与生产Actor交互的动作
	 * @param Chara 目标角色指针
	 * @param TargetActor 要交互的生产Actor
	 * @param ExecutionType 动作执行类型（Procedural 或 RealTime），默认为 Procedural
	 * @param Index 插入位置索引，-1 表示添加到队列末尾
	 * @return 是否成功添加动作
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool AddInteractWithProductionAction(AOrionChara* Chara, AOrionActorProduction* TargetActor,
	                                      EActionExecution ExecutionType = EActionExecution::Procedural,
	                                      int32 Index = -1);

	/** 
	 * 向指定角色添加收集货物的动作
	 * @param Chara 目标角色指针
	 * @param TargetActor 要收集的存储Actor
	 * @param ExecutionType 动作执行类型（Procedural 或 RealTime），默认为 Procedural
	 * @param Index 插入位置索引，-1 表示添加到队列末尾
	 * @return 是否成功添加动作
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool AddCollectCargoAction(AOrionChara* Chara, AOrionActorStorage* TargetActor,
	                            EActionExecution ExecutionType = EActionExecution::Procedural,
	                            int32 Index = -1);

	/** 
	 * 向指定角色添加收集子弹的动作
	 * @param Chara 目标角色指针
	 * @param ExecutionType 动作执行类型（Procedural 或 RealTime），默认为 Procedural
	 * @param Index 插入位置索引，-1 表示添加到队列末尾
	 * @return 是否成功添加动作
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool AddCollectBulletsAction(AOrionChara* Chara,
	                             EActionExecution ExecutionType = EActionExecution::Procedural,
	                             int32 Index = -1);

	/** 
	 * 移除指定角色的所有动作
	 * @param Chara 目标角色指针
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool RemoveAllActionsFromChara(AOrionChara* Chara);

	/** 
	 * 移除指定角色的特定索引的 Procedural 动作
	 * @param Chara 目标角色指针
	 * @param Index 动作在 Procedural 队列中的索引
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool RemoveProceduralActionAtFromChara(AOrionChara* Chara, int32 Index);

	/** 
	 * 重新排序指定角色的 Procedural 动作
	 * @param Chara 目标角色指针
	 * @param DraggedIndex 被拖拽动作的原始索引
	 * @param DropIndex 目标位置索引
	 * @return 是否成功重新排序
	 */
	UFUNCTION(BlueprintCallable, Category = "Orion|Character Action")
	bool ReorderProceduralActionInChara(AOrionChara* Chara, int32 DraggedIndex, int32 DropIndex);

	/** 统一的通过参数添加动作的函数 */
	bool AddActionByParams(AOrionChara* Chara, const FOrionActionParams& P,
	                       EActionExecution ExecutionType = EActionExecution::Procedural,
	                       int32 Index = -1);

private:
	/** 内部统一的添加动作出口 */
	bool Internal_AddAction(AOrionChara* Chara, const FOrionAction& Action,
	                        EActionExecution ExecutionType, int32 Index);
};
