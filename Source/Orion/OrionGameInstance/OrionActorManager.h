// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EngineUtils.h"
#include "Orion/OrionActor/OrionActor.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "OrionActorManager.generated.h"

/**
 * 
 */
UCLASS()
class ORION_API UOrionActorManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void CollectActorRecords(TArray<FOrionActorFullRecord>& Out) const
	{
		Out.Empty();
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		for (TActorIterator<AOrionActor> It(World); It; ++It)
		{
			AOrionActor* Act = *It;

			/* ① 创建内存 Writer，并声明 SaveGame 语义 */
			FBufferArchive MemWriter;
			FObjectAndNameAsStringProxyArchive Ar(MemWriter, /*bLoadIn=*/false);
			Ar.ArIsSaveGame = true;

			/* ② 把整个 Actor 写进字节流 */
			Act->Serialize(Ar);

			/* ③ 生成记录条 */
			FOrionActorFullRecord R;
			R.ActorGameId = Act->GetSerializable().GameId;
			R.ActorTransform = Act->GetActorTransform();
			R.ClassPath = Act->GetClass()->GetPathName();
			R.SerializedBytes = MemWriter;

			Out.Add(MoveTemp(R));
		}
	}

	/* ② 清空当前世界中的所有 OrionActor */
	static void RemoveAllActors(const UWorld* World)
	{
		for (TActorIterator<AOrionActor> It(World); It; ++It)
		{
			It->Destroy();
		}
	}

	void LoadAllActors(UWorld* World,
	                   const TArray<FOrionActorFullRecord>& Saved)
	{
		if (!World)
		{
			return;
		}

		RemoveAllActors(World);
		GlobalActorMap.Empty();

		for (const FOrionActorFullRecord& Rec : Saved)
		{
			if (AOrionActor* A = SpawnAndRegisterActor(World, Rec))
			{
				UE_LOG(LogTemp, Log, TEXT("Spawned %s"), *A->GetName());
			}
		}
	}

	/* ④ 根据记录 Spawn + 注册 */
	AOrionActor* SpawnAndRegisterActor(
		UWorld* World,
		const FOrionActorFullRecord& Rec)
	{
		UClass* ActorClass = LoadClass<AOrionActor>(nullptr, *Rec.ClassPath);
		if (!ActorClass) { return nullptr; }

		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AOrionActor* A = World->SpawnActor<AOrionActor>(ActorClass, Rec.ActorTransform, P);
		if (!A) { return nullptr; }

		/* 反序列化字节流覆盖默认值 */
		FMemoryReader MemReader(Rec.SerializedBytes);
		FObjectAndNameAsStringProxyArchive Ar(MemReader, /*bLoadIn=*/true);
		Ar.ArIsSaveGame = true;
		A->Serialize(Ar);

		/* 写回 Guid 并注册 */
		A->ActorSerializable.GameId = Rec.ActorGameId;
		GlobalActorMap.Add(Rec.ActorGameId, A);
		return A;
	}

	/* ⑤ 提供查询接口（可选） */
	AOrionActor* FindActorById(const FGuid& Id) const
	{
		const TWeakObjectPtr<AOrionActor>* Ptr = GlobalActorMap.Find(Id);
		return Ptr && Ptr->IsValid() ? Ptr->Get() : nullptr;
	}

private:
	TMap<FGuid, TWeakObjectPtr<AOrionActor>> GlobalActorMap;
};
