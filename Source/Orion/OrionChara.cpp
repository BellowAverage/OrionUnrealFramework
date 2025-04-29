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

#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

#include "DrawDebugHelpers.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include <limits>
#include <algorithm>
#include "TimerManager.h"
#include "EngineUtils.h"
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

	// UE_LOG(LogTemp, Log, TEXT("%s has been constructed."), *GetName());

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

	//UE_LOG(LogTemp, Log, TEXT("AOrionChara::Tick()"));

	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}

	/* Physics Handle - See Movement Module */
	RegisterCharaRagdoll(DeltaTime);

	ForceDetectionOnVelocityChange();

	/* AI Controlling */

	const FString PrevName = LastNonEmptyActionName;

	/*
	UE_LOG(LogTemp, Warning,
	       TEXT("[DEBUG] %s PrevAction='%s'"),
	       *GetName(),
	       PrevName.IsEmpty() ? TEXT("None") : *PrevName
	);
	*/

	DistributeCharaAction(DeltaTime);

	const FString CurrName = GetUnifiedActionName();

	/*
	UE_LOG(LogTemp, Warning,
	       TEXT("[DEBUG] %s CurrAction='%s'"),
	       *GetName(),
	       CurrName.IsEmpty() ? TEXT("None") : *CurrName
	);
	*/

	// if both non-empty and changed, invoke state transition
	if (!PrevName.IsEmpty() && !CurrName.IsEmpty() && PrevName != CurrName)
	{
		UE_LOG(LogTemp, Log,
		       TEXT("SwitchingStateHandle: %s -> %s"),
		       *PrevName, *CurrName
		);
		SwitchingStateHandle(PrevName, CurrName);
	}

	if (!CurrName.IsEmpty())
	{
		LastNonEmptyActionName = CurrName;
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

		UE_LOG(LogTemp, Log, TEXT("DistributeProceduralAction: bInterrupted = %s"),
		       bInterrupted ? TEXT("true") : TEXT("false"));

		if (!bInterrupted)
		{
			CurrentProcAction = &ProcAction;
			break;
		}
	}
}

void AOrionChara::SwitchingStateHandle(const FString& Prev, const FString& Curr)
{
	if (Prev.Contains(TEXT("InteractWithActor")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		InteractWithActorStop();
	}
	else if (Prev.Contains(TEXT("InteractWithProduction")) && Curr.Contains(TEXT("MoveToLocation")))
	{
		InteractWithProductionStop();
	}
}

bool AOrionChara::CollectingCargo(AOrionActorStorage* StorageActor)
{
	// 每帧进到这里先打个 tick 日志
	UE_LOG(LogTemp, Warning, TEXT("=== CollectingCargo Tick ==="));

	// 只有石料仓库才处理 ItemId=2
	if (!IsValid(StorageActor) ||
		StorageActor->StorageCategory != EStorageCategory::StoneStorage)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: invalid storage or not StoneStorage"));

		return true;
	}

	TMap<AActor*, TMap<int32, int32>> TradeRoute;

	// 初始化：只在第一次调用时构建路由
	if (!bIsCollectingCargo)
	{
		// 在场景中找第一个有货的 Ore
		TArray<AActor*> FoundActors;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AOrionActorOre::StaticClass(), FoundActors);

		AOrionActorOre* ChosenOre = nullptr;


		for (AActor* A : FoundActors)
		{
			auto* Ore = Cast<AOrionActorOre>(A);
			if (!Ore)
			{
				continue;
			}

			int32 Qty = Ore->InventoryComp->GetItemQuantity(2);
			UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: checking Ore %s, Qty=%d"), *Ore->GetName(), Qty);

			if (Qty > 0)
			{
				ChosenOre = Ore;
				break;
			}
		}

		if (!ChosenOre)
		{
			UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: no Ore has ItemId=2"));
			return true; // 没货就直接完成
		}

		// 构建单向运输路线：从 ChosenOre -> StorageActor

		TradeRoute.Empty();
		TradeRoute.Add(ChosenOre, {{2, ChosenOre->InventoryComp->GetItemQuantity(2)}});
		TradeRoute.Add(StorageActor, {}); // 目标仓库不送出任何东西

		UE_LOG(LogTemp, Warning, TEXT(
			       "CollectingCargo: route built: %s -> %s x%d"),
		       *ChosenOre->GetName(),
		       *StorageActor->GetName(),
		       TradeRoute[ChosenOre][2]
		);

		bIsCollectingCargo = true;
	}

	// 委托给你现成的 TradingCargo 去跑整个运输
	const bool bDone = TradingCargo(TradeRoute);

	if (bDone)
	{
		UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: TradingCargo completed, clearing state"));
		bIsCollectingCargo = false;
		TradeRoute.Empty();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("CollectingCargo: TradingCargo still in progress"));
	}

	return bDone;
}

void AOrionChara::ResetCollectingState()
{
	bIsCollectingCargo = false;
	CollectStorageTarget = nullptr;
	CollectItemId = 0;
	CollectRemaining = 0;
}

bool AOrionChara::MoveToLocation(FVector InTargetLocation)
{
	/*
	static auto DoOnce = []() -> bool
	    {
	        static bool bHasExecuted = false;
	        if (!bHasExecuted)
	        {
	            bHasExecuted = true;
	            return true;
	        }
	        return false;
	    };
	*/

	if (bIsPlayingMontage)
	{
		OnEquipWeaponMontageInterrupted();
		bIsPlayingMontage = false;
	}

	if (AIController)
	{
		FNavAgentProperties AgentProperties = AIController->GetNavAgentPropertiesRef();

		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		FNavLocation ProjectedLocation;
		if (NavSys && NavSys->ProjectPointToNavigation(InTargetLocation, ProjectedLocation))
		{
			// ProjectedLocation.Location 就是投影到可行走区域后的坐标，Z 值将是地面或 NavMesh 的真实高度
			InTargetLocation = ProjectedLocation.Location;
		}

		EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
			InTargetLocation, 5.0f, true, true, true, false, nullptr, true);

		/*
		    if (DoOnce())
		    {
		        // 根据请求结果处理逻辑
		        if (RequestResult == EPathFollowingRequestResult::RequestSuccessful)
		        {
		            UE_LOG(LogTemp, Log, TEXT("MoveToLocation called successfully with TargetLocation: %s"), *InTargetLocation.ToString());
		        }
		        else if (RequestResult == EPathFollowingRequestResult::Failed)
		        {
		            UE_LOG(LogTemp, Warning, TEXT("MoveToLocation failed for TargetLocation: %s"), *InTargetLocation.ToString());
		        }
		        else if (RequestResult == EPathFollowingRequestResult::AlreadyAtGoal)
		        {
		            UE_LOG(LogTemp, Log, TEXT("Already at TargetLocation: %s"), *InTargetLocation.ToString());
		        }
		    }
		*/
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AIController is not assigned!"));
	}

	FVector AjustedActorLocation = GetActorLocation();
	AjustedActorLocation.Z -= 85.f - 1.548;

	return FVector::Dist(AjustedActorLocation, InTargetLocation) < 60.0f;
}

void AOrionChara::MoveToLocationStop()
{
	AOrionAIController* OrionAC = Cast<AOrionAIController>(GetController());
	if (OrionAC)
	{
		OrionAC->StopMovement();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MoveToLocationStop: OrionAC is nullptr or not of type AOrionAIController."));
	}
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
		AOrionActorProduction* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();

		for (TActorIterator<AOrionActorProduction> It(GetWorld()); It; ++It)
		{
			AOrionActorProduction* Prod = *It;
			if (!IsValid(Prod))
			{
				continue;
			}

			if (auto* ProdInv = Prod->InventoryComp)
			{
				int32 Available = ProdInv->GetItemQuantity(BulletItemId);
				if (Available <= 0)
				{
					continue;
				}

				float d2 = FVector::DistSquared(Prod->GetActorLocation(), GetActorLocation());
				if (d2 < BestDistSq)
				{
					BestDistSq = d2;
					Best = Prod;
				}
			}
		}

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
	TradeMoveToLocation(BulletSource->GetActorLocation(), 50.f);
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
	if (CurrentInteractProduction.IsValid())
	{
		UE_LOG(LogTemp, Log,
		       TEXT("InteractWithProduction: stopped interacting with %s"),
		       *CurrentInteractProduction->GetName());
		CurrentInteractProduction->CurrWorkers--;
	}
	bPreparingInteractProd = false;
	CurrentInteractProduction = nullptr;
	// TODO: 清除手上工具、播放收工动画…
}

bool AOrionChara::InteractWithActor(float DeltaTime, AOrionActor* InTarget)
{
	// UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is being interacted."));

	if (!IsValid(InTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not valid."));

		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}

		IsInteractWithActor = false;
		return true;
	}

	if (InTarget->ActorStatus == EActorStatus::NotInteractable)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not Interactable."));

		if (IsInteractWithActor)
		{
			InteractWithActorStop();
		}

		IsInteractWithActor = false;
		return true;
	}

	USphereComponent* CollisionSphere = InTarget->FindComponentByClass<USphereComponent>();

	if (CollisionSphere && CollisionSphere->IsOverlappingActor(this))
	{
		// Determine the type of interaction

		FString TargetInteractType = InTarget->GetInteractType();
		if (TargetInteractType == "Mining")
		{
			InteractType = EInteractType::Mining;
		}
		else
		{
			InteractType = EInteractType::Unavailable;
			UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: Unavailable interact type."));

			if (IsInteractWithActor)
			{
				InteractWithActorStop();
			}

			IsInteractWithActor = false;
			return true;
		}

		if (!IsInteractWithActor)
		{
			SpawnAxeOnChara();
			IsInteractWithActor = true;
			CurrentInteractActor = InTarget;
			InTarget->CurrWorkers += 1;
			//InTarget->ArrInteractingCharas.Add(this);
		}

		if (AIController)
		{
			AIController->StopMovement();
		}

		FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			GetActorLocation(),
			InTarget->GetActorLocation()
		);

		LookAtRot.Pitch = 0.0f;
		LookAtRot.Roll = 0.0f;

		SetActorRotation(LookAtRot);

		return false;
	}

	if (AIController)
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation ProjectedLocation;

			FVector SearchExtent(500.0f, 500.0f, 500.0f);

			if (NavSys->ProjectPointToNavigation(
				InTarget->GetActorLocation(),
				ProjectedLocation,
				SearchExtent
			))
			{
				EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
					ProjectedLocation.Location,
					20.0f,
					true,
					true,
					true,
					false,
					nullptr,
					true
				);
			}
		}
	}

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

	RemoveAxeOnChara();
	IsInteractWithActor = false;
	DoOnceInteractWithActor = false;
	CurrentInteractActor->CurrWorkers -= 1;
	//CurrentInteractActor->ArrInteractingCharas.Remove(this);
	CurrentInteractActor = nullptr;
	InteractType = EInteractType::Unavailable;
}

bool AOrionChara::TradeMoveToLocation(const FVector& Dest, float AcceptanceRadius)
{
	if (!AIController)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] No AIController"));
		return true; // 没控制器就当到位了
	}

	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] No NavigationSystem"));
		return true;
	}

	// 1) Project to NavMesh
	FNavLocation Projected;
	// 尝试一个较大的搜索范围
	FVector SearchExtent(500.f, 500.f, 500.f);

	if (Nav->ProjectPointToNavigation(Dest, Projected, SearchExtent))
	{
		//UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] Projected to NavMesh at %s"),
		//*Projected.Location.ToString());

		// 发起寻路
		AIController->MoveToLocation(
			Projected.Location,
			AcceptanceRadius,
			/*StopOnOverlap=*/ true,
			/*UsePathfinding=*/ true,
			/*ProjectDestinationToNavigation=*/ false,
			/*bCanStrafe=*/ true,
			/*FilterClass=*/ nullptr,
			/*bAllowPartialPath=*/ true
		);

		return false; // 正在移动
	}

	//UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] ProjectPointToNavigation failed at %s"),
	//       *Destination.ToString());

	// 2) 退而求其次：只看水平距离
	float Dist2D = FVector::Dist2D(GetActorLocation(), Dest);
	if (Dist2D <= AcceptanceRadius)
	{
		//UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] Already within radius (%.1f <= %.1f)"),
		//       Dist2D, AcceptanceRadius);

		return true; // 已经到位
	}
	// 3) 尝试直接移动到原始点
	//UE_LOG(LogTemp, Warning, TEXT("[TradeMoveToLocation] Attempting raw MoveToLocation to %s"),
	//       *Destination.ToString());
	AIController->MoveToLocation(
		Dest,
		AcceptanceRadius,
		true, true, false, true, nullptr, true
	);
	return false;
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
			AActor* Src = Nodes[i];
			AActor* Dst = Nodes[(i + 1) % Nodes.Num()];
			for (auto& C : TradeRoute[Src])
			{
				FTradeSeg Seg;
				Seg.Source = Src;
				Seg.Destination = Dst;
				Seg.ItemId = C.Key;
				Seg.Quantity = C.Value;
				Seg.Moved = 0;
				TradeSegments.Add(Seg);
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

	UE_LOG(LogTemp, Log, TEXT("Current TradeStep: %s"), *UEnum::GetValueAsString(TradeStep));

	switch (TradeStep)
	{
	case ETradeStep::ToSource:
		if (bAtNode)
		{
			TradeStep = ETradeStep::Pickup;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			TradeMoveToLocation(TradeSegment.Source->GetActorLocation());
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
			TradeStep = ETradeStep::Dropoff;
			if (AIController)
			{
				AIController->StopMovement();
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("TradeSegment Destination: %s"), *TradeSegment.Destination->GetName());
			TradeMoveToLocation(TradeSegment.Destination->GetActorLocation());
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
	// 从自己身上扣除已搬运的物量
	FTradeSeg& Seg = TradeSegments[CurrentSegIndex];
	if (InventoryComp)
	{
		InventoryComp->ModifyItemQuantity(Seg.ItemId, -Seg.Moved);
	}
	// 进入下一段
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
			if (!Except.Contains("TempDoNotStopMovement"))
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
