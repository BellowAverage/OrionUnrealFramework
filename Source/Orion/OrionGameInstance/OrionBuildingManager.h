// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OrionBuildingManager.generated.h"

USTRUCT()
struct FFoundationSocket
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	FVector SocketLocation;

	UPROPERTY()
	FRotator SocketRotation;

	FFoundationSocket()
		: bIsOccupied(false)
		  , SocketLocation(FVector::ZeroVector)
		  , SocketRotation(FRotator::ZeroRotator)
	{
	}

	FFoundationSocket(
		bool InbIsOccupied,
		FVector InSocketLocation,
		FRotator InSocketRotator)
		: bIsOccupied(InbIsOccupied)
		  , SocketLocation(InSocketLocation)
		  , SocketRotation(InSocketRotator)
	{
	}
};

USTRUCT()
struct FWallSocket
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	FVector SocketLocation;

	UPROPERTY()
	FRotator SocketRotation;

	FWallSocket()
		: bIsOccupied(false)
		  , SocketLocation(FVector::ZeroVector)
		  , SocketRotation(FRotator::ZeroRotator)
	{
	}

	FWallSocket(
		bool InbIsOccupied,
		FVector InSocketLocation,
		FRotator InSocketRotator)
		: bIsOccupied(InbIsOccupied)
		  , SocketLocation(InSocketLocation)
		  , SocketRotation(InSocketRotator)
	{
	}
};

UENUM()
enum class EOrionStructure : uint8
{
	Foundation,
	Wall
};

USTRUCT()
struct FOrionGlobalSocket
{
	GENERATED_BODY()

	FVector Location;
	FRotator Rotation;
	EOrionStructure Kind;
	TWeakObjectPtr<AActor> Owner; // 谁登记的

	FOrionGlobalSocket() = default;

	FOrionGlobalSocket(const FVector& InLoc,
	                   const FRotator& InRot,
	                   EOrionStructure InKind,
	                   AActor* InOwner)
		: Location(InLoc), Rotation(InRot), Kind(InKind), Owner(InOwner)
	{
	}
};

/**
 * 
 */
UCLASS()
class ORION_API UOrionBuildingManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	int32 AliveCount = 0;
	TArray<FOrionGlobalSocket> FreeSockets;
	TArray<FOrionGlobalSocket> OccupiedSockets;

	const TArray<FOrionGlobalSocket>& GetFreeSockets()
	{
		return FreeSockets;
	}

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
		AliveCount = 0;
	}

	virtual void Deinitialize() override
	{
		Super::Deinitialize();
	}

	template <typename TOrionStructure>
	void ConfirmPlaceStructure(TSubclassOf<TOrionStructure>& BPOrionStructure, TOrionStructure*& PreviewStructurePtr,
	                           bool& bStructureSnapped, FTransform& SpawnTransform)
	{
		if (PreviewStructurePtr->bForceSnapOnGrid && !bStructureSnapped)
		{
			UE_LOG(LogTemp, Warning, TEXT("Structure must snap to an unoccupied socket!"));
			return;
		}

		FActorSpawnParameters StructureSpawnParameter;
		StructureSpawnParameter.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (bStructureSnapped)
		{
			TOrionStructure* StructurePlaced = GetWorld()->SpawnActor<TOrionStructure>(
				BPOrionStructure, SpawnTransform, StructureSpawnParameter);
		}
		else if (!bStructureSnapped && PreviewStructurePtr && !PreviewStructurePtr->bForceSnapOnGrid)
		{
			TOrionStructure* StructurePlaced = GetWorld()->SpawnActor<TOrionStructure>(
				BPOrionStructure, PreviewStructurePtr->GetActorLocation(),
				PreviewStructurePtr->GetActorRotation(), StructureSpawnParameter);
		}

		bStructureSnapped = false;
	}

	template <typename TOrionStructure>
	void RemoveSocketRegistration(TOrionStructure& OrionStructureRef)
	{
		TArray<FOrionGlobalSocket> Freed;

		for (int32 i = OccupiedSockets.Num() - 1; i >= 0; --i)
		{
			if (OccupiedSockets[i].Owner == &OrionStructureRef)
			{
				// 转成 Free，稍后重新登记
				Freed.Add(OccupiedSockets[i]);
				OccupiedSockets.RemoveAt(i);
			}
		}

		for (int32 i = FreeSockets.Num() - 1; i >= 0; --i)
		{
			if (FreeSockets[i].Owner == &OrionStructureRef)
			{
				FreeSockets.RemoveAt(i);
			}
		}

		for (const auto& S : Freed)
		{
			RegisterSocket(S.Location, S.Rotation, S.Kind,
			               /*bOccupied=*/false, GetWorld(), nullptr);
		}
	}

	static constexpr float MergeTolSqr = 5.f * 5.f; // 5 cm²

	void RegisterSocket(const FVector& Loc,
	                    const FRotator& Rot,
	                    EOrionStructure Kind,
	                    const bool bOccupied,
	                    const UWorld* World,
	                    AActor* Owner)
	{
		if (!World)
		{
			return;
		}


		/* 1. 先检查是否被 Occupied 覆盖 */
		for (const auto& S : OccupiedSockets)
		{
			if (S.Kind == Kind &&
				FVector::DistSquared(S.Location, Loc) < MergeTolSqr)
			{
				if (bOccupied) // 已有占用，再来一个占用 = 逻辑错误
				{
					UE_LOG(LogTemp, Error, TEXT("[Socket] Two OCCUPIED sockets overlap! Kind=%d"), int32(Kind));
				}
				return; // 被占用覆盖，不登记
			}
		}

		if (bOccupied)
		{
			/* 2‑A  新占用：把所有重合的 Free 删除，再加入 Occupied */
			for (int32 i = FreeSockets.Num() - 1; i >= 0; --i)
			{
				if (FreeSockets[i].Kind == Kind &&
					FVector::DistSquared(FreeSockets[i].Location, Loc) < MergeTolSqr)
				{
					FreeSockets.RemoveAt(i); // 删掉所有重合条目
				}
			}
			OccupiedSockets.Emplace(Loc, Rot, Kind, Owner);
		}
		else
		{
			/* 2‑B  新 Free：**不再查重**，直接加入表 */
			FreeSockets.Emplace(Loc, Rot, Kind, Owner);
		}

		RefreshDebug(World);
	}

	void RefreshDebug(const UWorld* World)
	{
		// 先清空
		UKismetSystemLibrary::FlushPersistentDebugLines(World);

		constexpr float AxisLen = 40.f;
		constexpr bool bPersist = true;
		constexpr int32 Depth = 0;

		// 未占用：细轴
		for (const auto& S : FreeSockets)
		{
			DrawDebugCoordinateSystem(World, S.Location, S.Rotation,
			                          AxisLen, bPersist, -1.f, Depth,
			                          /*Thickness=*/2.f);
		}
		// 占用：粗轴
		for (const auto& S : OccupiedSockets)
		{
			DrawDebugCoordinateSystem(World, S.Location, S.Rotation,
			                          AxisLen, bPersist, -1.f, Depth,
			                          /*Thickness=*/5.f);
		}
	}

	bool IsSocketFree(const FVector& Loc, EOrionStructure Kind)
	{
		for (const auto& S : FreeSockets)
		{
			if (S.Kind == Kind &&
				FVector::DistSquared(S.Location, Loc) < MergeTolSqr)
			{
				return true;
			}
		}
		return false;
	}
};
