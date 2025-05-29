// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"

void UOrionBuildingManager::ResetAllSockets(const UWorld* World)
{
	SocketsRaw.Empty();
	SocketsUnique.Empty();
	SocketsByKind.Empty();
	UKismetSystemLibrary::FlushPersistentDebugLines(World);
}

bool UOrionBuildingManager::ConfirmPlaceStructure(
	TSubclassOf<AActor> BPClass,
	AActor*& PreviewPtr,
	bool& bSnapped,
	const FTransform& SnapTransform)
{
	if (!PreviewPtr || !BPClass)
	{
		return false;
	}

	/* ① 预览体必须带结构组件 */
	if (UOrionStructureComponent* Comp =
			PreviewPtr->FindComponentByClass<UOrionStructureComponent>();
		!Comp)
	{
		return false;
	}

	if (const UOrionStructureComponent* Comp =
		PreviewPtr->FindComponentByClass<UOrionStructureComponent>())
	{
		if (Comp->bForceSnapOnGrid && !bSnapped)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("[Building] Structure must snap to socket before placement."));
			return false;
		}
	}

	UWorld* World = PreviewPtr->GetWorld();
	if (!World) { return false; }

	/* ② 计算最终放置的目标变换 */
	const FTransform TargetXform =
		bSnapped
			? SnapTransform
			: FTransform(PreviewPtr->GetActorRotation(),
			             PreviewPtr->GetActorLocation(),
			             PreviewPtr->GetActorScale3D());

	/* ③ 先做一次“占位体”碰撞检测 ------------------------------ */
	bool bBlocked = false;

	if (const UPrimitiveComponent* RootPrim =
		Cast<UPrimitiveComponent>(PreviewPtr->GetRootComponent()))
	{
		// 取预览体包围盒作为检测体积
		const FBoxSphereBounds Bounds = RootPrim->CalcBounds(TargetXform);
		const FVector Extent = Bounds.BoxExtent; // 半尺寸
		const FVector Center = Bounds.Origin;

		const FCollisionShape Shape = FCollisionShape::MakeBox(Extent);

		FCollisionQueryParams QP(SCENE_QUERY_STAT(PlacementTest), /*TraceComplex=*/false);
		QP.AddIgnoredActor(PreviewPtr); // 忽略自己（预览体关闭碰撞亦可再保险）

		bBlocked = World->OverlapBlockingTestByChannel(
			Center,
			TargetXform.GetRotation(),
			ECC_WorldStatic, // 也可换成 ECC_GameTraceChannel1 等
			Shape,
			QP);
	}

	if (bBlocked)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[Building] Placement blocked - cannot spawn structure here."));
		return false;
	}

	/* ④ 正式生成 Actor --------------------------------------- */
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewAct = World->SpawnActor<AActor>(BPClass, TargetXform, P);
	if (!NewAct)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("[Building] SpawnActor failed (class: %s)."), *BPClass->GetName());
		return false;
	}

	/* ⑤ 销毁预览体并复位状态 ---------------------------------- */
	PreviewPtr->Destroy();
	PreviewPtr = nullptr;
	bSnapped = false;

	return true;
}

const TArray<FOrionGlobalSocket>& UOrionBuildingManager::GetSnapSocketsByKind(const EOrionStructure Kind) const
{
	if (const TArray<FOrionGlobalSocket>* Bucket = SocketsByKind.Find(Kind))
	{
		return *Bucket;
	}
	static const TArray<FOrionGlobalSocket> Empty;
	return Empty;
}

bool UOrionBuildingManager::IsSocketFree(const FVector& Loc, const EOrionStructure Kind) const
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

void UOrionBuildingManager::RegisterSocket(const FVector& Loc, const FRotator& Rot, const EOrionStructure Kind,
                                           bool bOccupied, const UWorld* World, AActor* Owner)
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

	else if (Kind == EOrionStructure::BasicSquareFoundation)
	{
		SocketsRaw.Emplace(Loc, Rot, EOrionStructure::BasicRoof, bOccupied, Owner);
		const FOrionGlobalSocket New(Loc, Rot, EOrionStructure::BasicRoof, bOccupied, Owner);
		SocketsRaw.Add(New);
		AddUniqueSocket(New);
		RefreshDebug(World);
	}

	const FOrionGlobalSocket New(Loc, Rot, Kind, bOccupied, Owner);

	SocketsRaw.Add(New);
	AddUniqueSocket(New);
	RefreshDebug(World);
}

void UOrionBuildingManager::RebuildUniqueForKind(EOrionStructure Kind)
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

void UOrionBuildingManager::RemoveSocketRegistration(const AActor& Ref)
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

void UOrionBuildingManager::AddUniqueSocket(const FOrionGlobalSocket& New)
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

void UOrionBuildingManager::RemoveUniqueSocket(const FOrionGlobalSocket& Old)
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

void UOrionBuildingManager::RebuildUnique()
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

void UOrionBuildingManager::RefreshDebug(const UWorld* World)
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
		case EOrionStructure::BasicSquareFoundation:
			{
				// 半边长
				constexpr float HalfEdge = 50.f;
				DrawDebugBox(
					World,
					S.Location,
					FVector(HalfEdge, HalfEdge, 2.f),
					S.Rotation.Quaternion(),
					Color, /*bPersistent=*/true,
					/*LifeTime=*/-1.f,
					/*DepthPriority=*/0,
					LineThickness
				);
				break;
			}

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
		/*case EOrionStructure::Wall:
				{
					DrawDebugPoint(GetWorld(), S.Location, 10.f, FColor::Blue, true, -1, 0);
					break;
				}
			case EOrionStructure::DoubleWall:
				{
					DrawDebugPoint(GetWorld(), S.Location, 10.f, FColor::Blue, true, -1, 0);
					break;
				}*/
		//--------------------------------------------------
		// 其它（fallback，用一个点标记）
		//--------------------------------------------------
		default:
			//DrawDebugPoint(World, S.Location, 8.f, Color, true, -1.f, 0);
			break;
		}
	}
}
