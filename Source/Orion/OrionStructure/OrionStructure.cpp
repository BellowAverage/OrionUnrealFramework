// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructure.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

AOrionStructure::AOrionStructure()
{
	PrimaryActorTick.bCanEverTick = false;
}


void AOrionStructure::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("AOrionStructure::AliveCount: %d"), AliveCount);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AOrionPlayerController* OrionPC = Cast<AOrionPlayerController>(PC);

	if (OrionPC->bIsSpawnPreviewStructure)
	{
		OrionPC->bIsSpawnPreviewStructure = false;
		return;
	}


	AliveCount++;

	TArray<UActorComponent*> TaggedComps =
		GetComponentsByTag(
			UStaticMeshComponent::StaticClass(),
			FName("StructureMesh")
		);

	if (TaggedComps.Num() > 0)
	{
		StructureMesh = Cast<UStaticMeshComponent>(TaggedComps[0]);
	}

	if (!StructureMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("No StructureMesh Found. "));
	}
}

void AOrionStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AOrionStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 临时收集：我要释放的 Occupied→Free
	TArray<FOrionGlobalSocket> Freed;

	/* —— 移除属于自己的记录 —— */
	for (int32 i = OccupiedSockets.Num() - 1; i >= 0; --i)
	{
		if (OccupiedSockets[i].Owner == this)
		{
			// 转成 Free，稍后重新登记
			Freed.Add(OccupiedSockets[i]);
			OccupiedSockets.RemoveAt(i);
		}
	}

	for (int32 i = FreeSockets.Num() - 1; i >= 0; --i)
	{
		if (FreeSockets[i].Owner == this)
		{
			FreeSockets.RemoveAt(i);
		}
	}

	/* —— 把释放出来的接口重新记为 Free —— */
	for (const auto& S : Freed)
	{
		RegisterSocket(S.Location, S.Rotation, S.Kind,
		               /*bOccupied=*/false, GetWorld(), nullptr);
	}

	/* —— 调试线已在 RegisterSocket 内刷新 —— */

	/* —— 计数归零时做最终清理 —— */
	if (--AliveCount == 0)
	{
		FreeSockets.Empty();
		OccupiedSockets.Empty();
		UKismetSystemLibrary::FlushPersistentDebugLines(GetWorld());
	}
}

void AOrionStructure::RegisterSocket(const FVector& Loc,
                                     const FRotator& Rot,
                                     EOrionStructure Kind,
                                     bool bOccupied,
                                     UWorld* World,
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

	/* 3. 刷新调试坐标系 */
	RefreshDebug(World);
}

/* ---------- 刷新所有调试线 ---------- */
void AOrionStructure::RefreshDebug(UWorld* World)
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

bool AOrionStructure::IsSocketFree(const FVector& Loc, EOrionStructure Kind)
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
