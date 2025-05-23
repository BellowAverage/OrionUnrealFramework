// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EngineUtils.h"
#include "Orion/OrionGlobals/EOrionStructure.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"

class AOrionStructure;
class UOrionStructureComponent;

#include "OrionBuildingManager.generated.h"


USTRUCT()
struct FOrionGlobalSocket
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;
	UPROPERTY()
	FRotator Rotation;
	UPROPERTY()
	EOrionStructure Kind;
	UPROPERTY()
	bool bOccupied = false;
	UPROPERTY()
	TWeakObjectPtr<AActor> Owner;

	FOrionGlobalSocket() = default;

	FOrionGlobalSocket(const FVector& InLoc,
	                   const FRotator& InRot,
	                   const EOrionStructure InKind,
	                   const bool bInOccupied,
	                   AActor* InOwner)
		: Location(InLoc)
		  , Rotation(InRot)
		  , Kind(InKind)
		  , bOccupied(bInOccupied)
		  , Owner(InOwner)
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
	UPROPERTY()
	TArray<FOrionGlobalSocket> SocketsRaw;

	/* Reference pool: no duplicates allowed, used for snapping & Debug, rebuilt from Raw at any time */
	UPROPERTY()
	TArray<FOrionGlobalSocket> SocketsUnique;

	static constexpr float WALL_SPACING = 100.f;
	static constexpr float SPACING_TOLERANCE = 10.f;


	/* Save & Load */

	/* ========== ① Collect all buildings in the current world ========== */
	template <typename TStructureIterator = AOrionStructure>
	void CollectStructureRecords(TArray<FOrionStructureRecord>& Out) const
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		for (TActorIterator<TStructureIterator> It(World); It; ++It)
		{
			const AActor* Act = *It;
			FString Path = Act->GetClass()->GetPathName();
			Out.Emplace(Path, Act->GetActorTransform());
		}
	}

	/* ========== ② Completely reset Socket pool (called before loading) ========== */
	void ResetAllSockets(const UWorld* World)
	{
		SocketsRaw.Empty();
		SocketsUnique.Empty();
		SocketsByKind.Empty();
		UKismetSystemLibrary::FlushPersistentDebugLines(World);
	}

	static constexpr float MergeTolSqr = 5.f * 5.f;

	virtual void Initialize(FSubsystemCollectionBase&) override
	{
	}

	virtual void Deinitialize() override
	{
	}

	static bool ConfirmPlaceStructure(
		TSubclassOf<AActor> BPClass,
		AActor*& PreviewPtr,
		bool& bSnapped,
		const FTransform& SnapTransform);


	TMap<EOrionStructure, TArray<FOrionGlobalSocket>> SocketsByKind;

	const TArray<FOrionGlobalSocket>& GetSnapSocketsByKind(const EOrionStructure Kind) const
	{
		if (const TArray<FOrionGlobalSocket>* Bucket = SocketsByKind.Find(Kind))
		{
			return *Bucket;
		}
		static const TArray<FOrionGlobalSocket> Empty;
		return Empty;
	}

	/* Deprecated Due to Low Performance */
	//const TArray<FOrionGlobalSocket>& GetSnapSockets() const { return SocketsUnique; }

	bool IsSocketFree(const FVector& Loc, const EOrionStructure Kind) const
	{
		for (const FOrionGlobalSocket& S : GetSnapSocketsByKind(Kind))
		{
			if (!S.bOccupied && FVector::DistSquared(S.Location, Loc) < MergeTolSqr)
			{
				return true;
			}
		}
		return false;
	}

	void RegisterSocket(const FVector& Loc,
	                    const FRotator& Rot,
	                    const EOrionStructure Kind,
	                    bool bOccupied,
	                    const UWorld* World,
	                    AActor* Owner)
	{
		if (!World)
		{
			return;
		}

		if (Kind == EOrionStructure::Wall)
		{
			SocketsRaw.Emplace(Loc, Rot, EOrionStructure::DoubleWall, bOccupied, Owner);
			const FOrionGlobalSocket New(Loc, Rot, EOrionStructure::DoubleWall, bOccupied, Owner);

			SocketsRaw.Add(New);
			AddUniqueSocket(New);
			RefreshDebug(World);
		}

		const FOrionGlobalSocket New(Loc, Rot, Kind, bOccupied, Owner);

		SocketsRaw.Add(New);
		AddUniqueSocket(New);
		RefreshDebug(World);
	}


	void RebuildUniqueForKind(EOrionStructure Kind)
	{
		// 1) Remove this Kind from unique list and bucket
		for (int32 i = SocketsUnique.Num() - 1; i >= 0; --i)
		{
			if (SocketsUnique[i].Kind == Kind)
			{
				SocketsUnique.RemoveAt(i);
			}
		}
		SocketsByKind.Remove(Kind);

		// 2) Re-add sockets of the same type from raw to unique pool
		for (const FOrionGlobalSocket& Raw : SocketsRaw)
		{
			if (Raw.Kind == Kind)
			{
				AddUniqueSocket(Raw);
			}
		}
	}

	void RemoveSocketRegistration(const AActor& Ref)
	{
		const UWorld* World = Ref.GetWorld();
		TSet<EOrionStructure> KindsToRebuild;

		// 1) Remove this Owner from raw list and record all removed Kinds
		for (int32 i = SocketsRaw.Num() - 1; i >= 0; --i)
		{
			if (SocketsRaw[i].Owner.Get() == &Ref)
			{
				KindsToRebuild.Add(SocketsRaw[i].Kind);
				SocketsRaw.RemoveAt(i, 1, EAllowShrinking::No);
			}
		}

		// 2) For each removed Kind, rebuild unique sockets
		for (EOrionStructure Kind : KindsToRebuild)
		{
			RebuildUniqueForKind(Kind);
		}

		// 3) Redraw debug information
		if (World)
		{
			RefreshDebug(World);
		}
	}

	void AddUniqueSocket(const FOrionGlobalSocket& New)
	{
		// 先在 Unique 里找一下是不是"同位置+同Kind"
		for (int32 i = 0; i < SocketsUnique.Num(); ++i)
		{
			if (FOrionGlobalSocket& Exist = SocketsUnique[i]; Exist.Kind == New.Kind
				&& FVector::DistSquared(Exist.Location, New.Location) < MergeTolSqr)
			{
				// 如果已有占用就不换；如果已有空闲且新的是占用，就替换
				if (!Exist.bOccupied && New.bOccupied)
				{
					Exist = New; // ← 这里用 Exist 而不是 Exists

					// 桶里也要更新
					auto& Bucket = SocketsByKind.FindOrAdd(New.Kind);
					for (auto& B : Bucket)
					{
						if (FVector::DistSquared(B.Location, New.Location) < MergeTolSqr)
						{
							B = New;
							return;
						}
					}
				}
				return; // 已处理完毕
			}
		}

		// 没有找到就新增
		SocketsUnique.Add(New);
		SocketsByKind.FindOrAdd(New.Kind).Add(New);
	}

	void RemoveUniqueSocket(const FOrionGlobalSocket& Old)
	{
		// Unique 列表中删
		for (int32 i = SocketsUnique.Num() - 1; i >= 0; --i)
		{
			if (SocketsUnique[i].Kind == Old.Kind
				&& FVector::DistSquared(SocketsUnique[i].Location, Old.Location) < MergeTolSqr)
			{
				SocketsUnique.RemoveAt(i);
			}
		}
		// 桶里也删
		if (auto* Bucket = SocketsByKind.Find(Old.Kind))
		{
			for (int32 i = Bucket->Num() - 1; i >= 0; --i)
			{
				if (FVector::DistSquared((*Bucket)[i].Location, Old.Location) < MergeTolSqr)
				{
					Bucket->RemoveAt(i);
				}
			}
			if (Bucket->Num() == 0)
			{
				SocketsByKind.Remove(Old.Kind);
			}
		}
	}

	/* Deprecated Due to Low Performance */
	void RebuildUnique()
	{
		// 1) 把所有手动注册的插槽合并到 SocketsUnique
		SocketsUnique.Empty();
		for (const FOrionGlobalSocket& Src : SocketsRaw)
		{
			int32 FoundIdx = INDEX_NONE;
			for (int32 i = 0; i < SocketsUnique.Num(); ++i)
			{
				if (const auto& UniqueSocket = SocketsUnique[i]; UniqueSocket.Kind == Src.Kind &&
					FVector::DistSquared(UniqueSocket.Location, Src.Location) < MergeTolSqr)
				{
					FoundIdx = i;
					break;
				}
			}

			if (FoundIdx == INDEX_NONE)
			{
				SocketsUnique.Add(Src);
			}
			else if (!SocketsUnique[FoundIdx].bOccupied && Src.bOccupied)
			{
				// 如果已有空闲，但新的是占用，则替换
				SocketsUnique[FoundIdx] = Src;
			}
		}

		SocketsByKind.Empty();
		for (const FOrionGlobalSocket& S : SocketsUnique)
		{
			SocketsByKind.FindOrAdd(S.Kind).Add(S);
		}
	}

	bool BEnableDebugLine = false;

	void RefreshDebug(const UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (!BEnableDebugLine)
		{
			return;
		}

		// 清掉上一帧的线
		UKismetSystemLibrary::FlushPersistentDebugLines(World);

		// 调试线粗细
		constexpr float LineThickness = 2.f;

		for (const FOrionGlobalSocket& S : SocketsUnique)
		{
			const FColor Color = S.bOccupied ? FColor::Red : FColor::Green;

			switch (S.Kind)
			{
			//--------------------------------------------------
			// 1) 方形基座：画一个 100×100 的正方形
			//--------------------------------------------------
			//case EOrionStructure::BasicSquareFoundation:
			//	{
			//		// 半边长
			//		constexpr float HalfEdge = 50.f;
			//		DrawDebugBox(
			//			World,
			//			S.Location,
			//			FVector(HalfEdge, HalfEdge, 2.f),
			//			S.Rotation.Quaternion(),
			//			Color, /*bPersistent=*/true,
			//			/*LifeTime=*/-1.f,
			//			/*DepthPriority=*/0,
			//			LineThickness
			//		);
			//		break;
			//	}

			////--------------------------------------------------
			//// 2) 三角基座：画一个等边三角形轮廓
			////--------------------------------------------------
			//case EOrionStructure::BasicTriangleFoundation:
			//	{
			//		// 假设三角边长也是 100
			//		constexpr float EdgeLen = 100.f;
			//		// 等边三角高度
			//		const float TriHeight = EdgeLen * FMath::Sqrt(3.f) / 2.f;

			//		// 本地坐标系下的顶点（质心在原点）
			//		const FVector LocalVerts[3] = {
			//			FVector(-EdgeLen * 0.5f, -TriHeight / 3.f, 0.f),
			//			FVector(EdgeLen * 0.5f, -TriHeight / 3.f, 0.f),
			//			FVector(0.f, 2.f * TriHeight / 3.f, 0.f)
			//		};

			//		// 用插槽自己的旋转和位置来做变换
			//		const FTransform SockT(S.Rotation, S.Location);

			//		const FVector W0 = SockT.TransformPosition(LocalVerts[0]);
			//		const FVector W1 = SockT.TransformPosition(LocalVerts[1]);
			//		const FVector W2 = SockT.TransformPosition(LocalVerts[2]);

			//		DrawDebugLine(World, W0, W1, Color, true, -1.f, 0, LineThickness);
			//		DrawDebugLine(World, W1, W2, Color, true, -1.f, 0, LineThickness);
			//		DrawDebugLine(World, W2, W0, Color, true, -1.f, 0, LineThickness);
			//		break;
			//	}

			//--------------------------------------------------
			// 3) 墙：画一条垂直线
			//--------------------------------------------------
			case EOrionStructure::Wall:
				{
					DrawDebugPoint(GetWorld(), S.Location, 10.f, FColor::Blue, true, -1, 0);
					break;
				}
			case EOrionStructure::DoubleWall:
				{
					DrawDebugPoint(GetWorld(), S.Location, 10.f, FColor::Blue, true, -1, 0);
					break;
				}
			//--------------------------------------------------
			// 其它（fallback，用一个点标记）
			//--------------------------------------------------
			default:
				//DrawDebugPoint(World, S.Location, 8.f, Color, true, -1.f, 0);
				break;
			}
		}
	}
};
