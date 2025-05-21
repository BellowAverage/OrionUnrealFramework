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

	/* 参考池：不允许重复，用于吸附 & Debug，随时由 Raw 重建 */
	UPROPERTY()
	TArray<FOrionGlobalSocket> SocketsUnique;


	/* Save & Load */

	/* ========== ① 收集当前世界里的所有建筑 ========== */
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

	/* ========== ② 彻底重置 Socket 池（读档前调用） ========== */
	void ResetAllSockets(const UWorld* World)
	{
		SocketsRaw.Empty();
		SocketsUnique.Empty();
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
		const FTransform& SnapXform);

	const TArray<FOrionGlobalSocket>& GetSnapSockets() const { return SocketsUnique; }

	bool IsSocketFree(const FVector& Loc, EOrionStructure Kind) const
	{
		for (const FOrionGlobalSocket& S : SocketsUnique)
		{
			if (S.Kind == Kind && !S.bOccupied &&
				FVector::DistSquared(S.Location, Loc) < MergeTolSqr)
			{
				return true;
			}
		}
		return false;
	}

	void RegisterSocket(const FVector& Loc,
	                    const FRotator& Rot,
	                    EOrionStructure Kind,
	                    bool bOccupied,
	                    const UWorld* World,
	                    AActor* Owner)
	{
		if (!World)
		{
			return;
		}

		SocketsRaw.Emplace(Loc, Rot, Kind, bOccupied, Owner);
		RebuildUnique();
		RefreshDebug(World);
	}

	void RemoveSocketRegistration(AActor& Ref)
	{
		const UWorld* World = Ref.GetWorld();
		for (int32 i = SocketsRaw.Num() - 1; i >= 0; --i)
		{
			if (SocketsRaw[i].Owner.Get() == &Ref)
			{
				SocketsRaw.RemoveAt(i, 1, EAllowShrinking::No);
			}
		}
		RebuildUnique();
		if (World) { RefreshDebug(World); }
	}

	void RebuildUnique()
	{
		SocketsUnique.Empty();

		for (const FOrionGlobalSocket& Src : SocketsRaw)
		{
			// 查是否已存在“同位置 + 同Kind”的条目
			int32 FoundIdx = INDEX_NONE;
			for (int32 i = 0; i < SocketsUnique.Num(); ++i)
			{
				if (SocketsUnique[i].Kind == Src.Kind &&
					FVector::DistSquared(SocketsUnique[i].Location, Src.Location) < MergeTolSqr)
				{
					FoundIdx = i;
					break;
				}
			}

			if (FoundIdx == INDEX_NONE)
			{
				SocketsUnique.Add(Src);
			}
			else
			{
				// 同一点已有条目：若现有为空闲而新条目为占用，则替换
				if (!SocketsUnique[FoundIdx].bOccupied && Src.bOccupied)
				{
					SocketsUnique[FoundIdx] = Src;
				}
			}
		}
	}

	bool BEnableDebugLine = false;

	//void RefreshDebug(const UWorld* World)
	//{
	//	UKismetSystemLibrary::FlushPersistentDebugLines(World);

	//	for (const FOrionGlobalSocket& S : SocketsUnique)
	//	{
	//		constexpr float AxisLen = 40.f;
	//		DrawDebugCoordinateSystem(
	//			World, S.Location, S.Rotation,
	//			AxisLen, /*bPersist=*/true, -1.f, 0,
	//			S.bOccupied ? 5.f : 2.f);
	//	}
	//}

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
					constexpr float WallHeight = 150.f;
					DrawDebugLine(
						World,
						S.Location,
						S.Location + FVector(0.f, 0.f, WallHeight),
						S.bOccupied ? FColor::Red : FColor::Blue,
						/*bPersistent=*/true,
						/*LifeTime=*/-1.f,
						/*DepthPriority=*/0,
						/*Thickness=*/3.f
					);
					break;
				}

			//--------------------------------------------------
			// 其它（fallback，用一个点标记）
			//--------------------------------------------------
			default:
				DrawDebugPoint(World, S.Location, 8.f, Color, true, -1.f, 0);
				break;
			}
		}
	}
};
