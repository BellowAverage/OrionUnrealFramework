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
	if (!PreviewPtr || !BPClass) { return false; }

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
	const FTransform TargetTransform =
		bSnapped
			? SnapTransform
			: FTransform(
				PreviewPtr->GetActorRotation(),
				PreviewPtr->GetActorLocation(),
				PreviewPtr->GetActorScale3D());

	/*const FVector PreviewScale = PreviewPtr->GetActorScale3D();
	UE_LOG(LogTemp, Log, TEXT("[Building] PreviewPtr->GetActorScale3D() = (%.3f, %.3f, %.3f)"), PreviewScale.X,
	       PreviewScale.Y, PreviewScale.Z);
	const FVector SnapLoc = SnapTransform.GetLocation();
	const FRotator SnapRot = SnapTransform.GetRotation().Rotator();
	const FVector SnapScale = SnapTransform.GetScale3D();
	UE_LOG(LogTemp, Log,
	       TEXT("[Building] SnapTransform: Loc=(%.3f, %.3f, %.3f) Rot=(%.3f, %.3f, %.3f) Scale=(%.3f, %.3f, %.3f)"),
	       SnapLoc.X, SnapLoc.Y, SnapLoc.Z,
	       SnapRot.Pitch, SnapRot.Yaw, SnapRot.Roll,
	       SnapScale.X, SnapScale.Y, SnapScale.Z);*/


	/* ③ 占位体碰撞检测（含容忍度 + 调试绘制） ------------------ */
	bool bBlocked = false;

	if (const UPrimitiveComponent* RootPrim =
		Cast<UPrimitiveComponent>(PreviewPtr->GetRootComponent()))
	{
		const FBoxSphereBounds Bounds = RootPrim->CalcBounds(TargetTransform);
		const FVector ExtentFull = Bounds.BoxExtent;
		const FVector Center = Bounds.Origin;

		/* —— 3-A. 轻度容忍：给盒子收缩 2 cm ——————————— */
		constexpr float ToleranceCm = 100.f; // ← 容忍度
		const FVector ExtentLoose = (ExtentFull - FVector(ToleranceCm))
			.ComponentMax(FVector(1.f)); // 防负值

		/* —— 3-B. 先严格检测，再用容忍盒复检 ——————————— */
		FCollisionShape ShapeStrict = FCollisionShape::MakeBox(ExtentFull);
		FCollisionShape ShapeLoose = FCollisionShape::MakeBox(ExtentLoose);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlacementTest), /*TraceComplex=*/false);
		QueryParams.AddIgnoredActor(PreviewPtr);

		const bool bBlockedStrict = World->OverlapBlockingTestByChannel(
			Center, TargetTransform.GetRotation(), ECC_WorldStatic, ShapeStrict, QueryParams);

		const bool bBlockedLoose = World->OverlapBlockingTestByChannel(
			Center, TargetTransform.GetRotation(), ECC_WorldStatic, ShapeLoose, QueryParams);

		/* —— 3-C. 只有“严格阻挡”且“宽松也阻挡”才认定真阻挡 —— */
		bBlocked = bBlockedStrict && bBlockedLoose;

		/* —— 3-D. 调试输出 ———————————————————————————— */
		UE_LOG(LogTemp, Verbose,
		       TEXT(
			       "[Building][Debug] Center=(%.1f,%.1f,%.1f)  ExtentFull=(%.1f,%.1f,%.1f)  BlockedStrict=%d  BlockedLoose=%d"
		       ),
		       Center.X, Center.Y, Center.Z,
		       ExtentFull.X, ExtentFull.Y, ExtentFull.Z,
		       bBlockedStrict, bBlockedLoose);

		DrawDebugBox(World, Center, ExtentFull,
		             TargetTransform.GetRotation(),
		             bBlocked ? FColor::Red : FColor::Green,
		             /*PersistentLines=*/false, /*LifeTime=*/2.f);
	}

	if (bBlocked)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[Building] Placement blocked - cannot spawn structure here."));
		return false;
	}

	/* ⑤ 销毁预览体并复位状态 ------------------------------ */
	PreviewPtr->Destroy();
	PreviewPtr = nullptr;
	bSnapped = false;

	if (bool DelaySpawnNewStructureRes; DelaySpawnNewStructure(BPClass, World, TargetTransform,
	                                                           DelaySpawnNewStructureRes))
	{
		return DelaySpawnNewStructureRes;
	}

	return true;
}


bool UOrionBuildingManager::DelaySpawnNewStructure(TSubclassOf<AActor> BPClass, UWorld* World,
                                                   const FTransform TargetTransform, bool& bValue)
{
	/* ④ 正式生成 Actor --------------------------------------- */
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AActor* NewlySpawnedActor = World->SpawnActor<AActor>(BPClass, TargetTransform, P); !NewlySpawnedActor)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("[Building] SpawnActor failed (class: %s)."), *BPClass->GetName());
		bValue = false;
		return true;
	}
	else
	{
		/* Here the actor has done spawning, which means transforms made bellow won't affect socket registration */
		NewlySpawnedActor->SetActorScale3D(TargetTransform.GetScale3D());


		if (UOrionStructureComponent* SC = NewlySpawnedActor->FindComponentByClass<UOrionStructureComponent>())
		{
			SC->BIsPreviewStructure = false;

			/*if (SC->OrionStructureType == EOrionStructure::Wall)
			{
				const FVector WallBound = UOrionStructureComponent::GetStructureBounds(EOrionStructure::Wall);
				const float HalfLen = WallBound.Y;
				const float HalfThick = WallBound.X;

				FVector NewActorScale = NewlySpawnedActor->GetActorScale3D();

				// 1) 让长度缩短一个“厚度”：
				//    (2·HalfLen → 2·HalfLen − 2·HalfThick) ⇒ 比例 = (HalfLen − HalfThick)/HalfLen
				const float ShrinkRatio = (HalfLen - HalfThick) / HalfLen;
				NewActorScale.Y *= ShrinkRatio;
				NewlySpawnedActor->SetActorScale3D(NewActorScale);

				// 2) 沿本地 +Y 平移一个“厚度”，避免与邻墙重叠
				const float FullThick = HalfThick * 2.f;
				const FVector Offset = NewlySpawnedActor->GetActorRightVector() * FullThick;
				NewlySpawnedActor->AddActorWorldOffset(Offset);
			}*/
		}
	}
	return false;
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
                                           bool bOccupied, const UWorld* World, AActor* Owner, const FVector& Scale)
{
	if (!World)
	{
		return;
	}

	if (Kind == EOrionStructure::Wall)
	{
		SocketsRaw.Emplace(Loc, Rot, EOrionStructure::DoubleWall, bOccupied, Owner, Scale);
		const FOrionGlobalSocket New(Loc, Rot, EOrionStructure::DoubleWall, bOccupied, Owner, Scale);

		SocketsRaw.Add(New);
		AddUniqueSocket(New);
		RefreshDebug(World);
	}

	else if (Kind == EOrionStructure::BasicSquareFoundation)
	{
		SocketsRaw.Emplace(Loc, Rot, EOrionStructure::BasicRoof, bOccupied, Owner, Scale);
		const FOrionGlobalSocket New(Loc, Rot, EOrionStructure::BasicRoof, bOccupied, Owner, Scale);
		SocketsRaw.Add(New);
		AddUniqueSocket(New);
		RefreshDebug(World);
	}

	const FOrionGlobalSocket New(Loc, Rot, Kind, bOccupied, Owner, Scale);

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
