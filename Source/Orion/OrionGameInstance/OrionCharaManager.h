// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Components/CapsuleComponent.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "OrionCharaManager.generated.h"

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

				for (const FOrionAction& Act : Chara->CharacterProcActionQueue.Actions)
				{
					S.SerializedProcActions.Add(Act.Params);
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
					Chara->Serialize(Ar); // ← 这里会把 bIsCharaProcedural 等还原
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

	static void RecoverProcActions(AOrionChara* Chara,
	                               const FOrionCharaSerializable& S)
	{
		if (!Chara)
		{
			return;
		}
		UWorld* World = Chara->GetWorld();
		if (!World)
		{
			return;
		}

		auto FindActorById = [World](const FGuid& Id) -> AActor*
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (const IOrionInterfaceSerializable* Serial = Cast<IOrionInterfaceSerializable>(*It))
				{
					if (Serial->GetSerializable().GameId == Id)
					{
						return *It;
					}
				}
			}
			return nullptr;
		};

		for (const FOrionActionParams& P : S.SerializedProcActions)
		{
			switch (P.OrionActionType)
			{
			case EOrionAction::MoveToLocation:
				{
					auto Act = Chara->InitActionMoveToLocation(TEXT("MoveToLocation"), P.TargetLocation);
					Chara->InsertOrionActionToQueue(Act, EActionExecution::Procedural, INDEX_NONE);
					break;
				}
			case EOrionAction::AttackOnChara:
				{
					if (auto* Target = Cast<AOrionChara>(FindActorById(P.TargetActorId)))
					{
						auto Act = Chara->InitActionAttackOnChara(TEXT("AttackOnChara"), Target, P.HitOffset);
						Chara->InsertOrionActionToQueue(Act, EActionExecution::Procedural, INDEX_NONE);
					}
					break;
				}
			case EOrionAction::InteractWithActor:
				{
					if (auto* Target = Cast<AOrionActor>(FindActorById(P.TargetActorId)))
					{
						auto Act = Chara->InitActionInteractWithActor(TEXT("InteractWithActor"), Target);
						Chara->InsertOrionActionToQueue(Act, EActionExecution::Procedural, INDEX_NONE);
					}
					break;
				}
			case EOrionAction::InteractWithProduction:
				{
					if (auto* Target = Cast<AOrionActorProduction>(FindActorById(P.TargetActorId)))
					{
						auto Act = Chara->InitActionInteractWithProduction(TEXT("InteractWithProduction"), Target);
						Chara->InsertOrionActionToQueue(Act, EActionExecution::Procedural, INDEX_NONE);
					}
					break;
				}
			case EOrionAction::CollectCargo:
				{
					if (auto* Target = Cast<AOrionActorStorage>(FindActorById(P.TargetActorId)))
					{
						auto Act = Chara->InitActionCollectCargo(TEXT("CollectCargo"), Target);
						Chara->InsertOrionActionToQueue(Act, EActionExecution::Procedural, INDEX_NONE);
					}
					break;
				}
			case EOrionAction::CollectBullets:
				{
					auto Act = Chara->InitActionCollectBullets(TEXT("CollectBullets"));
					Chara->InsertOrionActionToQueue(Act, EActionExecution::Procedural, INDEX_NONE);
					break;
				}
			default:
				break;
			}
		}
	}
};
