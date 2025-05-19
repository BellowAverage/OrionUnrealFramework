// OrionChara.cpp

#include "OrionChara.h"
#include "Orion/OrionAIController/OrionAIController.h"
#include "AIController.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Animation/AnimInstance.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "DrawDebugHelpers.h"
#include "Orion/OrionHUD/OrionUserWidgetCharaInfo.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
#include <algorithm>
#include "TimerManager.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "Components/CapsuleComponent.h"
#include <Components/SphereComponent.h>
#include "Orion/OrionActor/OrionActorOre.h"

/*
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "OrionGameMode.h"
#include "OrionWeapon.h"
#include "Animation/SkeletalMeshActor.h"
#include "OrionBPFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
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
	// RegisterCharaRagdoll(DeltaTime);

	// ForceDetectionOnVelocityChange();

	/* AI Controlling */

	const FString PrevName = LastActionName;

	DistributeCharaAction(DeltaTime);

	const FString CurrName = GetUnifiedActionName();

	//if (this->GetName().Contains("1")) UE_LOG(LogTemp, Log, TEXT("Previous: %s, Current: %s, bIsCached: %s"), *PrevName,
	//                                          *CurrName, bCachedEmptyAction ? TEXT("True") : TEXT("False"));

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

	if (!CharacterActionQueue.Actions.IsEmpty())
	{
		CurrentAction = CharacterActionQueue.GetFrontAction();
	}
	else
	{
		CurrentAction = nullptr;
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

	OnCharaActionChange.Broadcast(Prev, Curr);

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
	else if (Prev.Contains(TEXT("InteractWithActor")))
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
		UE_LOG(LogTemp, Log, TEXT("111111"));
		InteractWithProductionStop();
	}
	else if (Prev.Contains(TEXT("InteractWithProduction")) && Curr.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("222222"));
		InteractWithProductionStop();
	}
	else if (Prev.Contains(TEXT("InteractWithProduction")))
	{
		UE_LOG(LogTemp, Log, TEXT("33333"));
		InteractWithProductionStop();
	}

	else if (Prev.Contains(TEXT("CollectingCargo")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		CollectingCargoStop();
	}
	else if (Prev.Contains(TEXT("CollectingCargo")) && Curr.IsEmpty())
	{
		CollectingCargoStop();
	}
	else if (Prev.Contains(TEXT("CollectingCargo")))
	{
		CollectingCargoStop();
	}

	else if (Prev.Contains(TEXT("InteractWithInventory")))
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

	if (this->InventoryComp->GetItemQuantity(StoneItemId) > 0)
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
	// 1) 停止任何挂起的拾取/放下动画回调
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Pickup);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Dropoff);
	}

	// 2) 重置 TradingCargo 状态机
	bIsTrading = false;
	bPickupAnimPlaying = false;
	bDropoffAnimPlaying = false;
	TradeSegments.Empty();
	CurrentSegIndex = 0;
	TradeStep = ETradeStep::ToSource;

	// 3) 重置“先送自身库存”标记
	//bSelfDeliveryDone = false;

	// 4) 停止导航并清空路径
	MoveToLocationStop(); // 内部会清空 NavPathPoints 和 CurrentNavPointIndex

	// 5) 清理 CollectingCargo 自身状态
	bIsCollectingCargo = false;
	AvailableCargoSources.Empty();
	CurrentCargoIndex = 0;
	CollectStep = ECollectStep::ToSource;
	CollectSource.Reset();
}

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

bool AOrionChara::CollectBullets()
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

bool AOrionChara::InteractWithProduction(float DeltaTime,
                                         AOrionActorProduction* InTarget)
{
	// === 配置：true = 先查 Storage，再查 Ore；false = 先查 Ore，再查 Storage ===
	constexpr bool bPreferStorageFirst = true;

	bPreparingInteractProd = true;

	// ① 无效或不可交互时停止
	if (!IsValid(InTarget) ||
		InTarget->ActorStatus == EActorStatus::NotInteractable)
	{
		if (bPreparingInteractProd) { InteractWithProductionStop(); }
		if (bIsInteractProd)
		{
			InteractWithActorStop();
			bIsInteractProd = false;
		}
		return true;
	}

	// ② 检查生产点原料
	constexpr int32 RawItemId = 2;
	constexpr int32 NeedPerCycle = 2;

	UOrionInventoryComponent* ProdInv = InTarget->InventoryComp;
	int32 HaveProd = ProdInv ? ProdInv->GetItemQuantity(RawItemId) : 0;

	// ③ 原料不足时触发搬运
	if (HaveProd < NeedPerCycle)
	{
		// ③.1 优先用角色背包中的
		if (InventoryComp &&
			InventoryComp->GetItemQuantity(RawItemId) >= NeedPerCycle)
		{
			if (bIsInteractProd) { InteractWithActorStop(); }

			TMap<AActor*, TMap<int32, int32>> Route;
			Route.Add(this, {{RawItemId, NeedPerCycle}});
			Route.Add(InTarget, {{}});
			if (!TradingCargo(Route))
			{
				return false; // 运输未完成，继续本 Action
			}
		}

		// ③.2 现场容器搬运
		AOrionActorOre* BestOre = nullptr;
		float BestOreDist = TNumericLimits<float>::Max();
		AOrionActorStorage* BestStore = nullptr;
		float BestStoreDist = TNumericLimits<float>::Max();

		// 如果优先 Storage
		if (bPreferStorageFirst)
		{
			// 查 Storage
			for (TActorIterator<AOrionActorStorage> It(GetWorld()); It; ++It)
			{
				auto* S = *It;
				if (!IsValid(S))
				{
					continue;
				}
				auto* Inv = S->InventoryComp;
				if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
				{
					continue;
				}
				float d2 = FVector::DistSquared(S->GetActorLocation(),
				                                InTarget->GetActorLocation());
				if (d2 < BestStoreDist)
				{
					BestStoreDist = d2;
					BestStore = S;
				}
			}
			// 若无 Storage，再查 Ore
			if (!BestStore)
			{
				for (TActorIterator<AOrionActorOre> It(GetWorld()); It; ++It)
				{
					auto* O = *It;
					if (!IsValid(O))
					{
						continue;
					}
					auto* Inv = O->InventoryComp;
					if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
					{
						continue;
					}
					float d2 = FVector::DistSquared(O->GetActorLocation(),
					                                InTarget->GetActorLocation());
					if (d2 < BestOreDist)
					{
						BestOreDist = d2;
						BestOre = O;
					}
				}
			}
		}
		else // 优先 Ore
		{
			for (TActorIterator<AOrionActorOre> It(GetWorld()); It; ++It)
			{
				auto* O = *It;
				if (!IsValid(O))
				{
					continue;
				}
				auto* Inv = O->InventoryComp;
				if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
				{
					continue;
				}
				float d2 = FVector::DistSquared(O->GetActorLocation(),
				                                InTarget->GetActorLocation());
				if (d2 < BestOreDist)
				{
					BestOreDist = d2;
					BestOre = O;
				}
			}
			if (!BestOre)
			{
				for (TActorIterator<AOrionActorStorage> It(GetWorld()); It; ++It)
				{
					auto* S = *It;
					if (!IsValid(S))
					{
						continue;
					}
					auto* Inv = S->InventoryComp;
					if (!Inv || Inv->GetItemQuantity(RawItemId) < NeedPerCycle)
					{
						continue;
					}
					float d2 = FVector::DistSquared(S->GetActorLocation(),
					                                InTarget->GetActorLocation());
					if (d2 < BestStoreDist)
					{
						BestStoreDist = d2;
						BestStore = S;
					}
				}
			}
		}

		// 都没有来源时退出
		AActor* ChosenSource =
			BestStore
				? static_cast<AActor*>(BestStore)
				: static_cast<AActor*>(BestOre);

		if (!ChosenSource) { return true; }

		// 计算搬运量
		auto* SrcInv = ChosenSource->FindComponentByClass<UOrionInventoryComponent>();
		int32 SrcHave = SrcInv ? SrcInv->GetItemQuantity(RawItemId) : 0;
		int32 MaxCanPut = ProdInv && ProdInv->AvailableInventoryMap.Contains(RawItemId)
			                  ? ProdInv->AvailableInventoryMap[RawItemId]
			                  : TNumericLimits<int32>::Max();
		int32 ToMove = FMath::Min(SrcHave, MaxCanPut);
		if (ToMove < NeedPerCycle) { return true; }

		if (bIsInteractProd) { InteractWithActorStop(); }

		TMap<AActor*, TMap<int32, int32>> Route;
		Route.Add(ChosenSource, {{RawItemId, ToMove}});
		Route.Add(InTarget, {{}});
		if (!TradingCargo(Route))
		{
			return false;
		}
	}

	// ④ 原料准备完毕，开始生产交互
	bIsInteractProd = true;
	return InteractWithActor(DeltaTime, InTarget);
}

void AOrionChara::InteractWithProductionStop()
{
	// 如果已经进入“正式交互”阶段，需要减少生产点的工人数
	UE_LOG(LogTemp, Warning, TEXT("[IP] 231331"));
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
	CurrentInteractActor = nullptr;
	InteractType = EInteractCategory::Unavailable;
}


bool AOrionChara::TradingCargo(const TMap<AActor*, TMap<int32, int32>>& TradeRoute)
{
	/*--------------------------------------------------------------------
	 * 0) 进入状态机时的“一次性”初始化
	 *------------------------------------------------------------------*/
	if (!bIsTrading)
	{
		TradeSegments.Empty();
		CurrentSegIndex = 0;
		TradeStep = ETradeStep::ToSource;
		bIsTrading = true;
		bPickupAnimPlaying = false;
		bDropoffAnimPlaying = false;

		// 生成 TradeSegments
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
				FTradeSeg Seg;
				Seg.Source = Source;
				Seg.Destination = Destination;
				Seg.ItemId = CargoMap.Key;
				Seg.Quantity = CargoMap.Value;
				Seg.Moved = 0;
				TradeSegments.Add(Seg);
			}
		}
	}

	/*--------------------------------------------------------------------
	 * 1) 所有段都已完成？
	 *------------------------------------------------------------------*/
	if (CurrentSegIndex >= TradeSegments.Num())
	{
		bIsTrading = false;
		return true;
	}

	FTradeSeg& Seg = TradeSegments[CurrentSegIndex];
	if (!IsValid(Seg.Source) || !IsValid(Seg.Destination))
	{
		UE_LOG(LogTemp, Warning, TEXT("TradingCargo: invalid actor, abort"));
		bIsTrading = false;
		return true;
	}

	/*--------------------------------------------------------------------
	 * 2) 判断“是否抵达”当前节点
	 *    —— 角色本身作为 Source/Dest 时，天然视作“已抵达” ——
	 *------------------------------------------------------------------*/
	bool bSourceIsSelf = (Seg.Source == this);
	bool bDestIsSelf = (Seg.Destination == this);

	USphereComponent* Sphere = nullptr;
	if (TradeStep == ETradeStep::ToDest && !bDestIsSelf)
	{
		Sphere = Seg.Destination->FindComponentByClass<USphereComponent>();
	}
	else if (TradeStep == ETradeStep::ToSource && !bSourceIsSelf)
	{
		Sphere = Seg.Source->FindComponentByClass<USphereComponent>();
	}

	bool bAtNode = (Sphere && Sphere->IsOverlappingActor(this))
		|| (TradeStep == ETradeStep::ToSource && bSourceIsSelf)
		|| (TradeStep == ETradeStep::ToDest && bDestIsSelf);

	/*--------------------------------------------------------------------
	 * 3) 状态机
	 *------------------------------------------------------------------*/
	switch (TradeStep)
	{
	/*==============================================================
	 * → Source
	 *============================================================*/
	case ETradeStep::ToSource:
		if (bAtNode)
		{
			MoveToLocationStop(); // 已到达（或本就是自己），停止寻路
			TradeStep = ETradeStep::Pickup;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			MoveToLocation(Seg.Source->GetActorLocation());
		}
		return false;

	/*==============================================================
	 *  Pick‑up
	 *============================================================*/
	case ETradeStep::Pickup:
		{
			/* 如果 Source 就是自己：直接记数量，不放动画，马上进入 ToDest */
			if (bSourceIsSelf)
			{
				// 已经在自己背包里，只需记下实际要搬运的数量
				if (InventoryComp)
				{
					int32 Have = InventoryComp->GetItemQuantity(Seg.ItemId);
					int32 ToMove = FMath::Min(Have, Seg.Quantity);
					Seg.Moved = ToMove;
				}

				// 跳过动画，直接下一步
				TradeStep = ETradeStep::ToDest;
				bPickupAnimPlaying = false;
				return false;
			}

			// ---------- OLD 旧逻辑 ----------
			// if (!bPickupAnimPlaying) { … 播放动画并取货 … }
			// --------------------------------

			if (!bPickupAnimPlaying)
			{
				bPickupAnimPlaying = true;

				if (auto* SrcInv = Seg.Source->FindComponentByClass<UOrionInventoryComponent>())
				{
					int32 Available = SrcInv->GetItemQuantity(Seg.ItemId);
					int32 ToTake = FMath::Min(Available, Seg.Quantity);

					if (ToTake > 0 && InventoryComp)
					{
						SrcInv->ModifyItemQuantity(Seg.ItemId, -ToTake);
						InventoryComp->ModifyItemQuantity(Seg.ItemId, ToTake);
						Seg.Moved = ToTake;
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("TradingCargo: nothing to pickup"));
						bIsTrading = false;
						return true;
					}
				}

				if (PickupMontage)
				{
					float Rate = PickupMontage->GetPlayLength() / PickupDuration;
					GetMesh()->GetAnimInstance()->Montage_Play(PickupMontage, Rate);
				}
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Pickup,
					this, &AOrionChara::OnPickupAnimFinished,
					PickupDuration, false);
			}
			return false;
		}


	/*==============================================================
	 * → Destination
	 *============================================================*/
	case ETradeStep::ToDest:
		if (bAtNode)
		{
			MoveToLocationStop();
			TradeStep = ETradeStep::DropOff;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			MoveToLocation(Seg.Destination->GetActorLocation());
		}
		return false;

	/*==============================================================
	 *  Drop‑off
	 *============================================================*/
	case ETradeStep::DropOff:
		{
			/* 如果 Destination 是自己：直接扣背包，不放动画，马上下一段 */
			if (bDestIsSelf)
			{
				if (InventoryComp)
				{
					InventoryComp->ModifyItemQuantity(Seg.ItemId, -Seg.Moved);
				}

				CurrentSegIndex++;
				TradeStep = ETradeStep::ToSource;
				bDropoffAnimPlaying = false;
				return false;
			}

			if (!bDropoffAnimPlaying)
			{
				bDropoffAnimPlaying = true;

				if (auto* DstInv = Seg.Destination->FindComponentByClass<UOrionInventoryComponent>())
				{
					DstInv->ModifyItemQuantity(Seg.ItemId, Seg.Moved);
				}

				if (InventoryComp)
				{
					InventoryComp->ModifyItemQuantity(Seg.ItemId, -Seg.Moved);
				}

				if (DropoffMontage)
				{
					float Rate = DropoffMontage->GetPlayLength() / DropoffDuration;
					GetMesh()->GetAnimInstance()->Montage_Play(DropoffMontage, Rate);
				}
				GetWorld()->GetTimerManager().SetTimer(
					TimerHandle_Dropoff,
					this, &AOrionChara::OnDropoffAnimFinished,
					DropoffDuration, false);
			}
			return false;
		}
	}

	return true; // 不会走到这里
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

	CharacterActionQueue.Actions.Empty();
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
	auto& CharaProcQueueActionsRef = CharacterProcActionQueue.Actions;

	int32 Count = CharaProcQueueActionsRef.Num();
	if (DraggedIndex < 0 || DraggedIndex >= Count)
	{
		return;
	}

	OrionAction ActionRef = MoveTemp(CharaProcQueueActionsRef[DraggedIndex]);
	CharaProcQueueActionsRef.RemoveAt(DraggedIndex);

	int32 NewIndex = DropIndex;
	if (DropIndex > DraggedIndex)
	{
		NewIndex = DropIndex - 1;
	}

	NewIndex = FMath::Clamp(NewIndex, 0, CharaProcQueueActionsRef.Num());

	CharaProcQueueActionsRef.Insert(MoveTemp(ActionRef), NewIndex);
}

void AOrionChara::RemoveProceduralActionAt(int32 Index)
{
	auto& CharaProcQueueActionsRef = CharacterProcActionQueue.Actions;
	if (Index >= 0 && Index < CharaProcQueueActionsRef.Num())
	{
		CharaProcQueueActionsRef.RemoveAt(Index);
	}
}

bool AOrionChara::InteractWithInventory(AOrionActor* OrionActor)
{
	if (!IsValid(OrionActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithInventory: invalid actor"));

		return true;
	}

	if (!bIsOrionAIControlled)
	{
		UE_LOG(LogTemp, Warning, TEXT("[IP] InteractWithInventory: not controlled"));

		MoveToLocationStop();

		return true;
	}

	USphereComponent* CollisionSphere = OrionActor->CollisionSphere;
	const bool bOverlapping = CollisionSphere && CollisionSphere->IsOverlappingActor(this);

	if (bOverlapping)
	{
		MoveToLocationStop();

		bIsInteractWithInventory = true;

		OnInteractWithInventory.ExecuteIfBound(OrionActor);

		return true;
	}
	MoveToLocation(OrionActor->GetActorLocation());

	return false;
}
