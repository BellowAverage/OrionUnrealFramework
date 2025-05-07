// OrionChara.cpp

#include "OrionChara.h"
#include "OrionAIController.h"
#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

#include "DrawDebugHelpers.h"
#include "OrionHUD/OrionUserWidgetCharaInfo.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
#include <algorithm>
#include "TimerManager.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
//#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include <Components/SphereComponent.h>
#include "Kismet/GameplayStatics.h"
#include "OrionActor/OrionActorOre.h"

/*
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "OrionGameMode.h"
#include "OrionWeapon.h"
#include "Animation/SkeletalMeshActor.h"
#include "OrionBPFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
*/


AOrionChara::AOrionChara()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AOrionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;


	StimuliSourceComp = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComp"));
	StimuliSourceComp->RegisterForSense(TSubclassOf<UAISense_Sight>());
	StimuliSourceComp->bAutoRegister = true;

	InventoryComp = CreateDefaultSubobject<UOrionInventoryComponent>(TEXT("InventoryComp"));

	SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;

	//CharacterActionQueue.Actions.reserve(100);
}

void AOrionChara::BeginPlay()
{
	Super::BeginPlay();

	AIController = Cast<AOrionAIController>(GetController());

	/*
	if (AIController)
	{
		UE_LOG(LogTemp, Log, TEXT("OrionChara is now controlled by OrionAIController"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionChara is not controlled by OrionAIController"));

		if (AIControllerClass)
		{
			UE_LOG(LogTemp, Log, TEXT("AIControllerClass is set to: %s"), *AIControllerClass->GetName());

			if (UWorld* World = GetWorld())
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Instigator = GetInstigator();
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				UE_LOG(LogTemp, Log, TEXT("Attempting to spawn AIController of class: %s"),
				       *AIControllerClass->GetName());

				AIController = World->SpawnActor<AOrionAIController>(AIControllerClass, SpawnParams);
				if (AIController)
				{
					AIController->Possess(this);
					UE_LOG(LogTemp, Log, TEXT("OrionAIController has been spawned and possessed the OrionChara."));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to spawn OrionAIController. Skipping related logic."));
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("World context is invalid. Skipping related logic."));
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT("AIControllerClass is not set. Please assign it in the constructor or editor."));
		}
	}
	*/

	InitOrionCharaMovement();

	CurrHealth = MaxHealth;
	this->SetCanBeDamaged(true);
}

void AOrionChara::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//UE_LOG(LogTemp, Log, TEXT("Current Action: %s"), *GetUnifiedActionName());

	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}

	/* Physics Handle - See Movement Module */
	RegisterCharaRagdoll(DeltaTime);

	ForceDetectionOnVelocityChange();

	/* AI Controlling */

	const FString PrevName = LastActionName;

	DistributeCharaAction(DeltaTime);

	const FString CurrName = GetUnifiedActionName();

	if (this->GetName().Contains("1")) UE_LOG(LogTemp, Log, TEXT("Previous: %s, Current: %s, bIsCached: %s"), *PrevName,
	                                          *CurrName, bCachedEmptyAction ? TEXT("True") : TEXT("False"));

	if (!bCachedEmptyAction && CurrName.IsEmpty())
	{
		bCachedEmptyAction = true;
	}

	else if (bCachedEmptyAction)
	{
		if (PrevName != CurrName)
		{
			SwitchingStateHandle(PrevName, CurrName);
		}

		bCachedEmptyAction = false;
		LastActionName = CurrName;
	}

	else
	{
		if (PrevName != CurrName)
		{
			SwitchingStateHandle(PrevName, CurrName);
		}

		LastActionName = CurrName;
	}


	/* Refresh Attack Frequency */

	RefreshAttackFrequency();
}

FString AOrionChara::GetUnifiedActionName() const
{
	// choose between procedural vs real‐time
	if (bIsCharaProcedural)
	{
		return CurrentProcAction ? CurrentProcAction->Name : TEXT("");
	}
	return CurrentAction ? CurrentAction->Name : TEXT("");
}

void AOrionChara::DistributeCharaAction(float DeltaTime)
{
	if (bIsCharaProcedural)
	{
		DistributeProceduralAction(DeltaTime);
	}
	else
	{
		DistributeRealTimeAction(DeltaTime);
	}
}

void AOrionChara::DistributeRealTimeAction(float DeltaTime)
{
	if (CurrentAction)
	{
		PreviousAction = CurrentAction;
	}

	if (CurrentAction == nullptr && !CharacterActionQueue.IsEmpty())
	{
		CurrentAction = CharacterActionQueue.GetFrontAction();
	}

	if (CurrentAction && CurrentAction->ExecuteFunction(DeltaTime))
	{
		CharacterActionQueue.PopFrontAction();
		CurrentAction = nullptr;
	}
}

void AOrionChara::DistributeProceduralAction(float DeltaTime)
{
	if (CurrentProcAction)
	{
		PreviousProcAction = CurrentProcAction;
	}

	for (auto& ProcAction : CharacterProcActionQueue.Actions)
	{
		bool bInterrupted = ProcAction.ExecuteFunction(DeltaTime);

		if (!bInterrupted)
		{
			CurrentProcAction = &ProcAction;
			break;
		}
	}
}

void AOrionChara::SwitchingStateHandle(const FString& Prev, const FString& Curr)
{
	UE_LOG(LogTemp, Log,
	       TEXT("SwitchingStateHandle: %s -> %s"),
	       *Prev, *Curr
	);

	if (Prev.Contains(TEXT("InteractWithActor")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}
		else if (bIsMovingToInteraction)
		{
			MoveToLocationStop();
		}
	}
	else if (Prev.Contains(TEXT("InteractWithActor")) && Curr.IsEmpty())
	{
		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}
		else if (bIsMovingToInteraction)
		{
			MoveToLocationStop();
		}
	}

	else if (Prev.Contains(TEXT("InteractWithProduction")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		InteractWithProductionStop();
	}
	else if (Prev.Contains(TEXT("InteractWithProduction")) && Curr.IsEmpty())
	{
		InteractWithProductionStop();
	}

	else if (Prev.Contains(TEXT("CollectingCargo")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		MoveToLocationStop();
	}
	else if (Prev.Contains(TEXT("CollectingCargo")) && Curr.IsEmpty())
	{
		MoveToLocationStop();
	}
}


AOrionActor* AOrionChara::FindClosetAvailableCargoContainer(int32 ItemId) const
{
	UWorld* World = GetWorld();

	AOrionActor* ClosestAvailableActor = nullptr;

	for (TActorIterator<AOrionActor> It(World); It; ++It)
	{
		AOrionActor* OrionActor = *It;
		if (!OrionActor)
		{
			continue;
		}

		int32 Quantity = OrionActor->InventoryComp->GetItemQuantity(ItemId);

		if (Quantity > 0)
		{
			float Distance = FVector::Dist(GetActorLocation(), OrionActor->GetActorLocation());
			if (!ClosestAvailableActor || Distance < FVector::Dist(GetActorLocation(),
			                                                       ClosestAvailableActor->GetActorLocation()))
			{
				ClosestAvailableActor = OrionActor;
			}
		}
	}

	if (!ClosestAvailableActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindClosetAvailableCargoContainer: No available cargo container found."));
		return nullptr;
	}

	return ClosestAvailableActor;
}

TArray<AOrionActor*> AOrionChara::FindAvailableCargoContainersByDistance(int32 ItemId) const
{
	TArray<AOrionActor*> AvailableContainers;
	UWorld* World = GetWorld();
	if (!World)
	{
		return AvailableContainers;
	}

	for (TActorIterator<AOrionActor> It(World); It; ++It)
	{
		AOrionActor* OrionActor = *It;
		if (!OrionActor || !OrionActor->InventoryComp)
		{
			continue;
		}

		// ❌ 排除 Storage 或 Production
		if (OrionActor->IsA<AOrionActorStorage>() ||
			OrionActor->IsA<AOrionActorProduction>())
		{
			continue;
		}

		if (OrionActor->InventoryComp->GetItemQuantity(ItemId) > 0)
		{
			AvailableContainers.Add(OrionActor);
		}
	}

	if (AvailableContainers.Num() > 1)
	{
		AOrionActor** DataPtr = AvailableContainers.GetData();
		int32 Count = AvailableContainers.Num();

		std::sort(
			DataPtr,
			DataPtr + Count,
			[this](AOrionActor* A, AOrionActor* B)
			{
				const float DistA = FVector::DistSquared(GetActorLocation(), A->GetActorLocation());
				const float DistB = FVector::DistSquared(GetActorLocation(), B->GetActorLocation());
				return DistA < DistB;
			}
		);
	}

	return AvailableContainers;
}

bool AOrionChara::CollectingCargo(AOrionActorStorage* StorageActor)
{
	constexpr int32 StoneItemId = 2;

	// 1) 验证目标仓库
	if (!IsValid(StorageActor) ||
		StorageActor->StorageCategory != EStorageCategory::StoneStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: invalid storage or not StoneStorage"));
		return true;
	}

	// 2) 先把自身已有的石料送一趟（只做一次）
	if (!bSelfDeliveryDone)
	{
		int32 Have = InventoryComp
			             ? InventoryComp->GetItemQuantity(StoneItemId)
			             : 0;

		if (Have > 0)
		{
			// 构建 this -> Storage 的路由（自身取货、放到仓库）
			TMap<AActor*, TMap<int32, int32>> SelfRoute;
			SelfRoute.Add(this, {{StoneItemId, Have}});
			SelfRoute.Add(StorageActor, {});

			// 交给 TradingCargo 去跑这一段
			bool bDone = TradingCargo(SelfRoute);
			if (!bDone)
			{
				// 还在送自身库存，下一帧继续
				return false;
			}
			// 送完了就标记，下面才进入场上容器逻辑
		}
		bSelfDeliveryDone = true;
	}

	// 3) 每次 bIsTrading==false，都重新选下一个源头 并发起新一段运输
	if (!bIsTrading)
	{
		// 3.1) 现场所有可用矿 + （不含自身，这里自身库存已送完）
		TArray<AOrionActor*> Sources =
			FindAvailableCargoContainersByDistance(
				StoneItemId
			);

		if (Sources.Num() == 0)
		{
			// 连一个都没—结束
			UE_LOG(LogTemp, Log, TEXT("CollectingCargo: no more sources, done"));
			// 重置，以便下次再调用能重跑自身送货
			bSelfDeliveryDone = false;
			return true;
		}

		// 3.2) 选最近的那个，构建 一段 Source -> Storage
		AOrionActor* NextSource = Sources[0];
		int32 Qty = NextSource->InventoryComp->GetItemQuantity(StoneItemId);

		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(NextSource, {{StoneItemId, Qty}});
		Route.Add(StorageActor, {});

		// 发起这段运输
		TradingCargo(Route);

		// 始终 return false，等待 TradingCargo 状态机自行推进
		return false;
	}

	// 4) bIsTrading 为 true，说明上一段正在执行 → 继续推进
	TradingCargo(TMap<AActor*, TMap<int32, int32>>());
	return false;
}

void AOrionChara::CollectingCargoStop()
{
	if (bIsTrading)
	{
		// 停止任何挂起的拾取/放下动画回调
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Pickup);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Dropoff);
		// 重置TradingCargo状态机
		bIsTrading = false;
		bPickupAnimPlaying = false;
		bDropoffAnimPlaying = false;
		TradeSegments.Empty();
		CurrentSegIndex = 0;
		TradeStep = ETradeStep::ToSource;
	}

	// 2) 停止导航
	MoveToLocationStop();

	// 3) 清理 CollectingCargo 自身状态
	bSelfDeliveryDone = false;
	bIsCollectingCargo = false;
	AvailableCargoSources.Empty();
	CurrentCargoIndex = 0;
	CollectStep = ECollectStep::ToSource;
	CollectSource.Reset();
}


/* Deprecated MoveToLocation */
//bool AOrionChara::MoveToLocation(const FVector& InTargetLocation)
//{
//	constexpr float AcceptanceRadius = 50.f;
//	const FVector SearchExtent(500.f, 500.f, 500.f);
//
//	// 0) 检查 AIController
//	if (!AIController)
//	{
//		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No AIController - treating as arrived"));
//		return true;
//	}
//
//	// 1) 拿导航系统
//	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
//	if (!NavSys)
//	{
//		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No NavigationSystem - treating as arrived"));
//		return true;
//	}
//
//	// 2) 投影目标到 NavMesh
//	FNavLocation ProjectedNav;
//	FVector DestLocation = InTargetLocation;
//	if (NavSys->ProjectPointToNavigation(InTargetLocation, ProjectedNav, SearchExtent))
//	{
//		DestLocation = ProjectedNav.Location;
//		UE_LOG(LogTemp, Log,
//		       TEXT("[MoveToLocation] Projected to NavMesh: %s"),
//		       *DestLocation.ToString());
//	}
//	else
//	{
//		UE_LOG(LogTemp, Warning,
//		       TEXT("[MoveToLocation] ProjectPointToNavigation failed at %s, using raw target"),
//		       *InTargetLocation.ToString());
//	}
//
//	// 3) 水平距离检查
//	float Dist2D = FVector::Dist2D(GetActorLocation(), DestLocation);
//	UE_LOG(LogTemp, Log,
//	       TEXT("[MoveToLocation] Curr=%s, Dest=%s, Dist2D=%.1f, Radius=%.1f"),
//	       *GetActorLocation().ToString(),
//	       *DestLocation.ToString(),
//	       Dist2D,
//	       AcceptanceRadius);
//
//	if (Dist2D <= AcceptanceRadius)
//	{
//		UE_LOG(LogTemp, Log, TEXT("[MoveToLocation] Within acceptance radius → Arrived"));
//		AIController->StopMovement();
//		return true;
//	}
//
//	// 4) 发起寻路请求
//	UE_LOG(LogTemp, Log,
//	       TEXT("[MoveToLocation] Moving to %s (radius=%.1f)"),
//	       *DestLocation.ToString(),
//	       AcceptanceRadius);
//
//	EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
//		DestLocation,
//		AcceptanceRadius,
//		/*StopOnOverlap=*/ true,
//		/*UsePathfinding=*/ true,
//		/*ProjectDestinationToNavigation=*/ false,
//		/*bCanStrafe=*/ true,
//		/*FilterClass=*/ nullptr,
//		/*bAllowPartialPath=*/ true
//	);
//
//	if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
//	{
//		UE_LOG(LogTemp, Log, TEXT("[MoveToLocation] AlreadyAtGoal"));
//		return true;
//	}
//	if (RequestResult == EPathFollowingRequestResult::Failed)
//	{
//		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] MoveToLocation request FAILED → treat as arrived"));
//		return true;
//	}
//	// RequestSuccessful
//	UE_LOG(LogTemp, Verbose, TEXT("[MoveToLocation] MoveToLocation request accepted"));
//
//	// 正在移动
//	return false;
//}

// In AOrionChara.cpp, replace MoveToLocation 实现为：

bool AOrionChara::MoveToLocation(const FVector& InTargetLocation)
{
	// 记录目标以备“重新规划”时使用
	LastMoveDestination = InTargetLocation;
	bHasMoveDestination = true;

	constexpr float AcceptanceRadius = 50.f;
	const FVector SearchExtent(500.f, 500.f, 500.f);

	if (!AIController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No AIController → treat as arrived"));
		return true;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] No NavigationSystem → treat as arrived"));
		return true;
	}

	// 如果还没有生成路径，则同步计算一次
	if (NavPathPoints.Num() == 0)
	{
		FNavLocation ProjectedNav;
		FVector DestLocation = InTargetLocation;
		if (NavSys->ProjectPointToNavigation(InTargetLocation, ProjectedNav, SearchExtent))
		{
			DestLocation = ProjectedNav.Location;
		}
		// 计算路径
		FPathFindingQuery Query;
		UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), DestLocation,
		                                                                this);
		if (!Path || Path->PathPoints.Num() <= 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MoveToLocation] Failed to build path or already at goal"));
			return true;
		}
		NavPathPoints = Path->PathPoints;
		CurrentNavPointIndex = 1; // 跳过起点

		for (int32 i = 0; i < NavPathPoints.Num(); ++i)
		{
			// 绘制路径点球体
			DrawDebugSphere(
				GetWorld(),
				NavPathPoints[i],
				20.f, // 半径
				12, // 分段数
				FColor::Green,
				false, // 不持续
				5.f // 显示时长
			);
			// 绘制点与点之间的连线
			if (i > 0)
			{
				DrawDebugLine(
					GetWorld(),
					NavPathPoints[i - 1],
					NavPathPoints[i],
					FColor::Green,
					false, // 不持续
					5.f, // 显示时长
					0,
					2.f // 线宽
				);
			}
		}
	}

	// 当前目标点
	FVector TargetPoint = NavPathPoints[CurrentNavPointIndex];
	float Dist2D = FVector::Dist2D(GetActorLocation(), TargetPoint);
	UE_LOG(LogTemp, Verbose, TEXT("[MoveToLocation] NextPt=%s, Dist2D=%.1f"), *TargetPoint.ToString(), Dist2D);

	// 到达当前路径点后推进到下一个
	if (Dist2D <= AcceptanceRadius)
	{
		CurrentNavPointIndex++;
		if (CurrentNavPointIndex >= NavPathPoints.Num())
		{
			// 全部点到齐，停止移动
			NavPathPoints.Empty();
			CurrentNavPointIndex = 0;
			UE_LOG(LogTemp, Log, TEXT("[MoveToLocation] Arrived at final destination"));
			return true;
		}
	}

	// 通过添加输入来移动：朝向下一个路径点
	FVector Dir = (TargetPoint - GetActorLocation()).GetSafeNormal2D();
	AddMovementInput(Dir, 1.0f, true);

	return false; // 仍在移动
}

//void AOrionChara::MoveToLocationStop()
//{
//	AOrionAIController* OrionAC = Cast<AOrionAIController>(GetController());
//	if (OrionAC)
//	{
//		OrionAC->StopMovement();
//	}
//	else
//	{
//		UE_LOG(LogTemp, Warning, TEXT("MoveToLocationStop: OrionAC is nullptr or not of type AOrionAIController."));
//	}
//}

void AOrionChara::MoveToLocationStop()
{
	if (AIController)
	{
		AIController->StopMovement();
	}
	bHasMoveDestination = false;
	NavPathPoints.Empty();
	CurrentNavPointIndex = 0;
}

void AOrionChara::ReRouteMoveToLocation()
{
	if (!bHasMoveDestination || !AIController)
	{
		return;
	}

	// 1) 清空旧导航点
	NavPathPoints.Empty();
	CurrentNavPointIndex = 0;

	// 2) 立刻向 AIController 发一个新的 MoveToLocation 请求
	constexpr float AcceptanceRadius = 50.f;
	const FVector SearchExtent(500.f, 500.f, 500.f);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RefreshMoveToLocation] No NavSys"));
		return;
	}

	// 投影到 NavMesh
	FNavLocation ProjectedNav;
	FVector Dest = LastMoveDestination;
	if (NavSys->ProjectPointToNavigation(LastMoveDestination, ProjectedNav, SearchExtent))
	{
		Dest = ProjectedNav.Location;
	}

	// 发起寻路
	AIController->MoveToLocation(
		Dest,
		AcceptanceRadius,
		/*StopOnOverlap=*/ true,
		/*UsePathfinding=*/ true,
		/*ProjectDestinationToNavigation=*/ false,
		/*bCanStrafe=*/ true,
		/*FilterClass=*/ nullptr,
		/*bAllowPartialPath=*/ true
	);

	UE_LOG(LogTemp, Log, TEXT("[RefreshMoveToLocation] Re-routed to %s"), *Dest.ToString());
}

bool AOrionChara::CollectBullets(float DeltaTime)
{
	constexpr int32 BulletItemId = 3;
	constexpr int32 MaxCarry = 300;

	// 0) Ensure inventory comp
	if (!InventoryComp)
	{
		InventoryComp = FindComponentByClass<UOrionInventoryComponent>();
		if (!InventoryComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("CollectBullets: no InventoryComp on chara"));
			return true;
		}
	}

	// 1) Initialization: pick nearest production actor that has bullets
	if (!bIsCollectingBullets)
	{
		AOrionActor* Best = FindClosetAvailableCargoContainer(3);

		if (!Best)
		{
			return true;
		}

		BulletSource = Best;
		bIsCollectingBullets = true;
	}

	// 2) Abort if source destroyed
	if (!IsValid(BulletSource))
	{
		bIsCollectingBullets = false;
		return true;
	}

	// 3) Already full?
	{
		int32 Have = InventoryComp->GetItemQuantity(BulletItemId);
		if (Have >= MaxCarry)
		{
			bIsCollectingBullets = false;
			return true;
		}
	}

	// 4) Are we at the source?
	bool bAtSource = false;
	if (auto* Sphere = BulletSource->FindComponentByClass<USphereComponent>())
	{
		bAtSource = Sphere->IsOverlappingActor(this);
	}

	if (bAtSource)
	{
		MoveToLocationStop();

		// if montage not yet playing, start it
		if (!bBulletPickupAnimPlaying && BulletPickupMontage)
		{
			bBulletPickupAnimPlaying = true;

			// play at a rate so it finishes in BulletPickupDuration
			float RawLength = BulletPickupMontage->GetPlayLength();
			float PlayRate = RawLength > 0.f
				                 ? RawLength / BulletPickupDuration
				                 : 1.f;

			if (auto* AnimInst = GetMesh()->GetAnimInstance())
			{
				AnimInst->Montage_Play(BulletPickupMontage, PlayRate);
			}

			// schedule the actual ammo transfer for when the montage ends
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle_BulletPickup,
				this, &AOrionChara::OnBulletPickupFinished,
				BulletPickupDuration,
				false
			);
		}

		// while montage is playing, stay in this action
		return false;
	}

	// 5) Not at source yet → move toward it
	MoveToLocation(BulletSource->GetActorLocation());
	return false;
}

void AOrionChara::OnBulletPickupFinished()
{
	constexpr int32 BulletItemId = 3;
	constexpr int32 MaxCarry = 300;

	bBulletPickupAnimPlaying = false;

	// actually transfer as many as we can
	if (!IsValid(BulletSource) || !InventoryComp)
	{
		bIsCollectingBullets = false;
		return;
	}

	auto* SrcInv = BulletSource->InventoryComp;
	int32 Available = SrcInv->GetItemQuantity(BulletItemId);
	int32 Have = InventoryComp->GetItemQuantity(BulletItemId);
	int32 SpaceLeft = MaxCarry - Have;
	int32 ToTake = FMath::Min(Available, SpaceLeft);

	if (ToTake > 0)
	{
		SrcInv->ModifyItemQuantity(BulletItemId, -ToTake);
		InventoryComp->ModifyItemQuantity(BulletItemId, ToTake);
	}

	// done!
	bIsCollectingBullets = false;
}

bool AOrionChara::InteractWithProduction(float DeltaTime, AOrionActorProduction* InTarget)
{
	UE_LOG(LogTemp, Warning, TEXT("[IP] Tick -- InTarget=%p"), InTarget);

	bPreparingInteractProd = true;

	// ① 有效性检查
	if (!IsValid(InTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] target invalid"));
		if (bPreparingInteractProd)
		{
			UE_LOG(LogTemp, Warning, TEXT("[IP] stopping because invalid"));
			InteractWithProductionStop();
		}

		if (bIsInteractProd)
		{
			InteractWithActorStop();
			bIsInteractProd = false;
		}

		UE_LOG(LogTemp, Warning, TEXT("[IP] 111"));

		return true;
	}

	// ② 检查生产点可交互状态
	UE_LOG(LogTemp, Warning, TEXT("[IP] target ActorStatus=%d"), int32(InTarget->ActorStatus));
	if (InTarget->ActorStatus == EActorStatus::NotInteractable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] target not interactable"));
		if (bPreparingInteractProd)
		{
			UE_LOG(LogTemp, Warning, TEXT("[IP] stopping because not interactable"));
			InteractWithProductionStop();
		}

		if (bIsInteractProd)
		{
			InteractWithActorStop();
			bIsInteractProd = false;
		}

		UE_LOG(LogTemp, Warning, TEXT("[IP] 222"));
		return true;
	}

	constexpr int32 NeedPerCycle = 2;

	// ③ 如果生产点库存中 2 号物品不够，就先触发搬运
	UOrionInventoryComponent* ProdInv = InTarget->InventoryComp;
	int32 HaveProd = ProdInv ? ProdInv->GetItemQuantity(2) : 0;
	if (HaveProd < NeedPerCycle)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] need to trade in raw materials first"));

		// ----------------- 新增：优先用自己身上的原料 -----------------
		if (InventoryComp && InventoryComp->GetItemQuantity(2) >= NeedPerCycle)
		{
			UE_LOG(LogTemp, Warning, TEXT("[IP] supplying from self-inventory"));
			if (bIsInteractProd)
			{
				InteractWithActorStop();
			}

			TMap<AActor*, TMap<int32, int32>> Route;
			Route.Add(this, {{2, NeedPerCycle}});
			Route.Add(InTarget, {{}});
			bool bDone = TradingCargo(Route);
			UE_LOG(LogTemp, Warning,
			       TEXT("[IP] TradingCargo(self->prod) returned %s"),
			       bDone ? TEXT("true") : TEXT("false"));

			// 如果搬运还没完成，就返回 false 继续本 Action
			if (!bDone)
			{
				UE_LOG(LogTemp, Warning, TEXT("[IP] self->prod transport ongoing -> return false"));
				return false;
			}
			// 搬运完成，继续下面的生产逻辑
		}
		// --------------------------------------------------------------


		// 1) 找到最近的 Ore 源
		AOrionActorOre* BestSourceOfRawMaterial = nullptr;
		float BestDist2 = TNumericLimits<float>::Max();

		for (TActorIterator<AOrionActorOre> It(GetWorld()); It; ++It)
		{
			AOrionActorOre* Ore = *It;
			if (!IsValid(Ore))
			{
				continue;
			}
			if (UOrionInventoryComponent* SourceInventory = Ore->InventoryComp)
			{
				int32 AvailableRawMaterialNumber = SourceInventory->GetItemQuantity(2);
				if (AvailableRawMaterialNumber < 2)
				{
					continue;
				}
				float d2 = FVector::DistSquared(
					Ore->GetActorLocation(),
					InTarget->GetActorLocation()
				);
				if (d2 < BestDist2)
				{
					BestDist2 = d2;
					BestSourceOfRawMaterial = Ore;
				}
			}
		}

		if (!BestSourceOfRawMaterial)
		{
			UE_LOG(LogTemp, Warning, TEXT("[IP] no source ore found, abort entire action"));

			if (bIsInteractProd)
			{
				InteractWithActorStop();
				bIsInteractProd = false;
			}

			UE_LOG(LogTemp, Warning, TEXT("[IP] 444"));
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[IP] chosen source ore: %s"), *BestSourceOfRawMaterial->GetName());

		// 2) 计算最大可放量
		int32 MaxCanTake = TNumericLimits<int32>::Max();
		if (ProdInv)
		{
			if (ProdInv->AvailableInventoryMap.Contains(2))
			{
				MaxCanTake = ProdInv->AvailableInventoryMap[2];
			}
			else
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("[IP] ProdInv.AvailableInventoryMap has no entry for Item2, default MaxCanTake=%d"),
				       MaxCanTake);
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[IP] ProdInv is null!"));
		}

		// 3) 计算实际搬运量
		UOrionInventoryComponent* SrcInv = BestSourceOfRawMaterial->InventoryComp;
		int32 SrcHave = SrcInv ? SrcInv->GetItemQuantity(2) : 0;
		int32 ToTake = FMath::Min(MaxCanTake, SrcHave);
		UE_LOG(LogTemp, Warning,
		       TEXT("[IP] SrcHave=%d, MaxCanTake=%d -> ToTake=%d"),
		       SrcHave, MaxCanTake, ToTake);

		if (ToTake <= 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[IP] ToTake<=0, abort entire action"));

			if (bIsInteractProd)
			{
				InteractWithActorStop();
				bIsInteractProd = false;
			}

			UE_LOG(LogTemp, Warning, TEXT("[IP] 555"));
			return true;
		}

		// 4) 构造搬运路线：BestSrc -> InTarget
		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(BestSourceOfRawMaterial, {{2, ToTake}});
		Route.Add(InTarget, {});
		UE_LOG(LogTemp, Warning,
		       TEXT("[IP] Route built: from %s take %d of Item2 to %s"),
		       *BestSourceOfRawMaterial->GetName(), ToTake, *InTarget->GetName());

		// 5) 调用 TradingCargo

		if (bIsInteractProd)
		{
			InteractWithActorStop();
		}

		bool bTradeDone = TradingCargo(Route);
		UE_LOG(LogTemp, Warning,
		       TEXT("[IP] TradingCargo returned %s"),
		       bTradeDone ? TEXT("true") : TEXT("false"));
		if (!bTradeDone)
		{
			UE_LOG(LogTemp, Warning, TEXT("[IP] still trading, wait next tick"));
			return false;
		}
		UE_LOG(LogTemp, Warning, TEXT("[IP] trading complete, resume production interact"));
	}

	// ④ “采矿式”互动：走到生产点范围内
	bIsInteractProd = true;

	bool InteractWithActorResult = InteractWithActor(DeltaTime, InTarget);

	UE_LOG(LogTemp, Warning, TEXT("[IP] 666"));

	return InteractWithActorResult;
}

void AOrionChara::InteractWithProductionStop()
{
	// 如果已经进入“正式交互”阶段，需要减少生产点的工人数
	if (IsInteractWithActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithProductionStop: bIsInteractProd = true"));
		InteractWithActorStop();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithProductionStop: bIsInteractProd = false"));
		MoveToLocationStop();
	}

	// 停止“正式交互”状态
	IsInteractWithActor = false;


	// 重置“准备交互”标记
	bPreparingInteractProd = false;

	// 清空当前生产目标
	//CurrentInteractProduction = nullptr;
}

bool AOrionChara::InteractWithActor(float DeltaTime, AOrionActor* InTarget)
{
	// 1) 无效目标立即终止
	if (!IsValid(InTarget))
	{
		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}
		IsInteractWithActor = false;
		return true;
	}

	// 2) 不可交互立即终止
	if (InTarget->ActorStatus == EActorStatus::NotInteractable)
	{
		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}
		IsInteractWithActor = false;
		return true;
	}

	// 3) 只查一次 SphereComponent
	USphereComponent* CollisionSphere = InTarget->CollisionSphere;
	const bool bOverlapping = CollisionSphere && CollisionSphere->IsOverlappingActor(this);

	if (bOverlapping)
	{
		MoveToLocationStop();

		// 4) 缓存一次字符串引用，避免拷贝
		const FString& TargetType = InTarget->GetInteractType();

		// 5) 根据类型设置交互类别
		if (TargetType == TEXT("Mining"))
		{
			InteractType = EInteractCategory::Mining;
		}
		else if (TargetType == TEXT("CraftingBullets"))
		{
			InteractType = EInteractCategory::CraftingBullets;
		}
		else
		{
			// 不支持的类型
			InteractType = EInteractCategory::Unavailable;
			UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: Unavailable interact type."));

			if (IsInteractWithActor)
			{
				InteractWithActorStop();
			}
			IsInteractWithActor = false;
			return true;
		}

		// 6) 第一次到达，触发开始交互
		if (!IsInteractWithActor)
		{
			if (InteractType == EInteractCategory::Mining)
			{
				SpawnAxeOnChara();
			}
			else // CraftingBullets
			{
				SpawnHammerOnChara();
			}

			IsInteractWithActor = true;

			MoveToLocationStop();

			bIsMovingToInteraction = false;
			CurrentInteractActor = InTarget;
			InTarget->CurrWorkers += 1;
		}

		// 7) 停止移动并面向目标
		if (AIController)
		{
			AIController->StopMovement();
		}
		FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			GetActorLocation(),
			InTarget->GetActorLocation()
		);
		LookAtRot.Pitch = 0;
		LookAtRot.Roll = 0;
		SetActorRotation(LookAtRot);

		return false;
	}

	// —— 此处进入「移动到目标」分支 ——

	bIsMovingToInteraction = true;
	MoveToLocation(InTarget->GetActorLocation());

	// 8) 如果我们正处于「已经开始交互」状态，但又脱离了范围，取消之前的交互
	if (IsInteractWithActor)
	{
		InteractWithActorStop();
	}
	IsInteractWithActor = false;

	return false;
}

void AOrionChara::InteractWithActorStop()
{
	if (!CurrentInteractActor)
	{
		return;
	}

	if (InteractType == EInteractCategory::Mining)
	{
		RemoveAxeOnChara();
	}
	else if (InteractType == EInteractCategory::CraftingBullets)
	{
		RemoveHammerOnChara();
	}

	bIsMovingToInteraction = false;

	IsInteractWithActor = false;
	DoOnceInteractWithActor = false;
	CurrentInteractActor->CurrWorkers -= 1;
	//CurrentInteractActor->ArrInteractingCharas.Remove(this);
	CurrentInteractActor = nullptr;
	InteractType = EInteractCategory::Unavailable;
}

bool AOrionChara::TradingCargo(const TMap<AActor*, TMap<int32, int32>>& TradeRoute)
{
	// 1) 初始化
	if (!bIsTrading)
	{
		TradeSegments.Empty();
		CurrentSegIndex = 0;
		TradeStep = ETradeStep::ToSource;
		bIsTrading = true;
		bPickupAnimPlaying = false;
		bDropoffAnimPlaying = false;

		// 拆环：直接取所有 keys
		TArray<AActor*> Nodes;
		TradeRoute.GetKeys(Nodes);
		if (Nodes.Num() < 2)
		{
			bIsTrading = false;
			return true;
		}

		for (int32 i = 0; i < Nodes.Num(); ++i)
		{
			AActor* Source = Nodes[i];
			AActor* Destination = Nodes[(i + 1) % Nodes.Num()];
			for (auto& CargoMap : TradeRoute[Source])
			{
				FTradeSeg TradeSegment;
				TradeSegment.Source = Source;
				TradeSegment.Destination = Destination;
				TradeSegment.ItemId = CargoMap.Key;
				TradeSegment.Quantity = CargoMap.Value;
				TradeSegment.Moved = 0;
				TradeSegments.Add(TradeSegment);
			}
		}
	}

	// 2) 全部段跑完?
	if (CurrentSegIndex >= TradeSegments.Num())
	{
		bIsTrading = false;
		return true;
	}

	FTradeSeg& TradeSegment = TradeSegments[CurrentSegIndex];
	if (!IsValid(TradeSegment.Source) || !IsValid(TradeSegment.Destination))
	{
		UE_LOG(LogTemp, Warning, TEXT("TradingCargo: invalid actor, abort"));
		bIsTrading = false;
		return true;
	}

	// 3) 判定是否到达当前节点
	USphereComponent* Sphere = (TradeStep == ETradeStep::ToDest)
		                           ? TradeSegment.Destination->FindComponentByClass<USphereComponent>()
		                           : TradeSegment.Source->FindComponentByClass<USphereComponent>();

	bool bAtNode = Sphere && Sphere->IsOverlappingActor(this);

	// UE_LOG(LogTemp, Log, TEXT("Current TradeStep: %s"), *UEnum::GetValueAsString(TradeStep));

	switch (TradeStep)
	{
	case ETradeStep::ToSource:
		if (bAtNode)
		{
			MoveToLocationStop();

			TradeStep = ETradeStep::Pickup;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			MoveToLocation(TradeSegment.Source->GetActorLocation());
		}
		return false;

	case ETradeStep::Pickup:
		{
			if (!bPickupAnimPlaying)
			{
				bPickupAnimPlaying = true;

				// —— 把物品从 Source 拿到角色自己的 InventoryComp —— 
				if (auto* SourceInventory = TradeSegment.Source->FindComponentByClass<UOrionInventoryComponent>())
				{
					int32 Available = SourceInventory->GetItemQuantity(TradeSegment.ItemId);
					int32 ToTake = FMath::Min(Available, TradeSegment.Quantity);

					if (ToTake > 0 && InventoryComp)
					{
						SourceInventory->ModifyItemQuantity(TradeSegment.ItemId, -ToTake);
						InventoryComp->ModifyItemQuantity(TradeSegment.ItemId, +ToTake);
						TradeSegment.Moved = ToTake;
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("TradingCargo: nothing to pickup"));
						bIsTrading = false;
						return true;
					}
				}

				// 播放拾取蒙太奇
				if (PickupMontage)
				{
					float Rate = PickupMontage->GetPlayLength() / PickupDuration;
					GetMesh()->GetAnimInstance()->Montage_Play(PickupMontage, Rate);
				}
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Pickup,
					this, &AOrionChara::OnPickupAnimFinished,
					PickupDuration, false
				);
			}
			return false;
		}

	case ETradeStep::ToDest:
		if (bAtNode)
		{
			MoveToLocationStop();

			TradeStep = ETradeStep::Dropoff;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("TradeSegment Destination: %s"), *TradeSegment.Destination->GetName());
			MoveToLocation(TradeSegment.Destination->GetActorLocation());
		}
		return false;

	case ETradeStep::Dropoff:
		{
			if (!bDropoffAnimPlaying)
			{
				bDropoffAnimPlaying = true;

				// —— 把物品从角色背包放到 Destination —— 
				if (auto* InvDst = TradeSegment.Destination->FindComponentByClass<UOrionInventoryComponent>())
				{
					InvDst->ModifyItemQuantity(TradeSegment.ItemId, TradeSegment.Moved);
				}

				// 播放放下蒙太奇
				if (DropoffMontage)
				{
					float Rate = DropoffMontage->GetPlayLength() / DropoffDuration;
					GetMesh()->GetAnimInstance()->Montage_Play(DropoffMontage, Rate);
				}
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Dropoff,
					this, &AOrionChara::OnDropoffAnimFinished,
					DropoffDuration, false
				);
			}
			return false;
		}
	}

	return true;
}

void AOrionChara::OnPickupAnimFinished()
{
	TradeStep = ETradeStep::ToDest;
	bPickupAnimPlaying = false;
}

void AOrionChara::OnDropoffAnimFinished()
{
	FTradeSeg& TradeSegment = TradeSegments[CurrentSegIndex];
	if (InventoryComp)
	{
		InventoryComp->ModifyItemQuantity(TradeSegment.ItemId, -TradeSegment.Moved);
	}

	CurrentSegIndex++;
	TradeStep = ETradeStep::ToSource;
	bDropoffAnimPlaying = false;
}

void AOrionChara::RemoveAllActions(const FString& Except)
{
	if (CurrentAction)
	{
		FString OngoingActionName = CurrentAction->Name;

		if (OngoingActionName.Contains("ForceMoveToLocation") || OngoingActionName.Contains("MoveToLocation"))
		{
			if (!Except.Contains(TEXT("TempDoNotStopMovement")))
			{
				MoveToLocationStop();
			}
			else
			{
				// 不停止，而是重新规划当前移动目标
				//ReRouteMoveToLocation();
				MoveToLocationStop();
			}
		}

		if (OngoingActionName.Contains("ForceAttackOnCharaLongRange") || OngoingActionName.Contains(
			"AttackOnCharaLongRange"))
		{
			if (Except.Contains("SwitchAttackingTarget"))
			{
				IsAttackOnCharaLongRange = false;
				SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
			}
			else
			{
				AttackOnCharaLongRangeStop();
			}
		}

		if (OngoingActionName.Contains("ForceInteractWithActor") || OngoingActionName.Contains("InteractWithActor"))
		{
			InteractWithActorStop();
		}

		CurrentAction = nullptr;
	}

	CharacterActionQueue.Actions.clear();
}

void AOrionChara::Die()
{
	CharaState = ECharaState::Dead;

	UE_LOG(LogTemp, Log, TEXT("AOrionChara::Die() called."));

	RemoveAllActions();

	if (StimuliSourceComp)
	{
		if (UAISense_Sight::StaticClass())
		{
			StimuliSourceComp->UnregisterFromSense(UAISense_Sight::StaticClass());
			UE_LOG(LogTemp, Log, TEXT("Unregistered from UAISense_Sight."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAISense_Sight::StaticClass() returned nullptr."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("StimuliSourceComp is null in Die()."));
	}

	// 3. 停止AI控制
	if (AIController)
	{
		AIController->StopMovement();
		AIController->UnPossess();
		UE_LOG(LogTemp, Log, TEXT("AIController stopped and unpossessed."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIController is null in Die()."));
	}

	if (AttachedArrowComponents.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("No attached arrow components to destroy."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Cleaning up %d attached arrow components."), AttachedArrowComponents.Num());

	for (auto& ArrowComp : AttachedArrowComponents)
	{
		if (ArrowComp && ArrowComp->IsValidLowLevel())
		{
			ArrowComp->DestroyComponent();
			UE_LOG(LogTemp, Log, TEXT("Destroyed an attached arrow component."));
		}
	}

	// Clear the array after destruction
	AttachedArrowComponents.Empty();

	Destroy();
	UE_LOG(LogTemp, Log, TEXT("AOrionChara destroyed."));
}

void AOrionChara::Incapacitate()
{
	CharaState = ECharaState::Incapacitated;

	RemoveAllActions();

	if (StimuliSourceComp)
	{
		if (UAISense_Sight::StaticClass())
		{
			StimuliSourceComp->UnregisterFromSense(UAISense_Sight::StaticClass());
			UE_LOG(LogTemp, Log, TEXT("Unregistered from UAISense_Sight."));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("UAISense_Sight::StaticClass() returned nullptr."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("StimuliSourceComp is null in Die()."));
	}

	if (AIController)
	{
		AIController->StopMovement();
		AIController->UnPossess();
		UE_LOG(LogTemp, Log, TEXT("AIController stopped and unpossessed."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIController is null in Die()."));
	}

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(true);
	}

	if (UCapsuleComponent* CharaCapsuleComponent = GetCapsuleComponent())
	{
		CharaCapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AOrionChara::AddItemToInventory(int ItemID, int Quantity)
{
	if (CharaInventoryMap.contains(ItemID))
	{
		CharaInventoryMap[ItemID] += Quantity;
	}
	else
	{
		CharaInventoryMap[ItemID] = Quantity;
	}
}

std::vector<AOrionChara*> AOrionChara::GetOtherCharasByProximity() const
{
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), StaticClass(), AllActors);

	std::vector<AOrionChara*> Enemies;
	Enemies.reserve(AllActors.Num());
	for (AActor* Actor : AllActors)
	{
		if (Actor == this)
		{
			continue;
		}

		if (AOrionChara* Other = Cast<AOrionChara>(Actor))
		{
			if (Other->CharaSide != CharaSide)
			{
				Enemies.push_back(Other);
			}
		}
	}

	const FVector MyLocation = GetActorLocation();
	std::sort(Enemies.begin(), Enemies.end(),
	          [MyLocation](AOrionChara* A, AOrionChara* B)
	          {
		          const float DA = FVector::DistSquared(MyLocation, A->GetActorLocation());
		          const float DB = FVector::DistSquared(MyLocation, B->GetActorLocation());
		          return DA < DB;
	          });

	return Enemies;
}

bool AOrionChara::bIsLineOfSightBlocked(AActor* InTargetActor) const
{
	FHitResult Hit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(AttackTrace), true);
	TraceParams.AddIgnoredActor(this);
	TraceParams.AddIgnoredActor(InTargetActor);

	/*
	for (TActorIterator<AOrionProjectile> It(GetWorld()); It; ++It)
	{
		TraceParams.AddIgnoredActor(*It);
	}
	*/

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit,
		GetActorLocation(),
		InTargetActor->GetActorLocation(),
		ECC_Visibility,
		TraceParams
	);

	if (bHit)
	{
		if (Hit.GetActor())
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by %s."),
			       *Hit.GetActor()->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AttackOnCharaLongRange: Line of sight blocked by an unknown object."));
		}
	}
	else
	{
		return true;
	}

	return false;
}

void AOrionChara::ReorderProceduralAction(int32 DraggedIndex, int32 DropIndex)
{
	auto& Q = CharacterProcActionQueue.Actions;

	int32 Count = static_cast<int32>(Q.size());
	if (DraggedIndex < 0 || DraggedIndex >= Count)
	{
		return;
	}

	Action Temp = MoveTemp(Q[DraggedIndex]);
	Q.erase(Q.begin() + DraggedIndex);

	int32 NewIndex = DropIndex;
	if (DropIndex > DraggedIndex)
	{
		NewIndex = DropIndex - 1;
	}

	NewIndex = FMath::Clamp(NewIndex, 0, static_cast<int32>(Q.size()));

	Q.insert(Q.begin() + NewIndex, MoveTemp(Temp));
}

void AOrionChara::RemoveProceduralActionAt(int32 Index)
{
	auto& Q = CharacterProcActionQueue.Actions;
	if (Index >= 0 && Index < Q.size())
	{
		Q.erase(Q.begin() + Index);
	}
}
