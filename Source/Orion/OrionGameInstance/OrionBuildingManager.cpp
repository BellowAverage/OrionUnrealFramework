// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"

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
