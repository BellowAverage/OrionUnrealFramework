// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionAIController.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Orion/OrionComponents/OrionActionComponent.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"
#include "Orion/OrionGameInstance/OrionFactionManager.h"
#include "Orion/OrionChara/OrionChara.h"
#include "Orion/OrionActor/OrionActor.h"
#include "Orion/OrionStructure/OrionStructure.h"

AOrionAIController::AOrionAIController()
{
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));
	SetPerceptionComponent(*AIPerceptionComp);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	SightConfig->SightRadius = 1500.f;
	SightConfig->LoseSightRadius = 1600.f;
	SightConfig->PeripheralVisionAngleDegrees = 90.f;
	SightConfig->SetMaxAge(5.f);

	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	AIPerceptionComp->ConfigureSense(*SightConfig);
	AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AOrionAIController::BeginPlay()
{
	Super::BeginPlay();

	if (AIPerceptionComp)
	{
		AIPerceptionComp->OnTargetPerceptionUpdated.AddDynamic(this, &AOrionAIController::OnTargetPerceptionUpdated);
	}
}

void AOrionAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (ControlledPawn == nullptr)
	{
		//UE_LOG(LogTemp, Warning, TEXT("AOrionAIController::Tick: ControlledPawn is nullptr."));
		return;
	}

	if (CachedAIState == EAIState::Defensive && ControlledPawn->CharaAIState != EAIState::Defensive)
	{
		UE_LOG(LogTemp, Log, TEXT("AOrionAIController::Tick: Leaving Defensive state"));
		//ControlledPawn->RemoveWeaponActor();
		//ControlledPawn->bIsCharaArmed = false;
	}

	if (ControlledPawn->CharaAIState == EAIState::Defensive)
	{
		if (CachedAIState != EAIState::Defensive)
		{
			UE_LOG(LogTemp, Log, TEXT("AOrionAIController::Tick: Entering Defensive state"));
		}
		// [Fix] 通过 ActionComp 访问队列和当前动作
		// 检查是否空闲：ActionComp 存在 && RealTime 队列为空 && 当前没有正在运行的 RealTime 动作
		bool bIsIdle = false;
		if (ControlledPawn->ActionComp)
		{
			// [Fix] Use GetCurrentAction() instead of CurrentAction pointer
			bIsIdle = ControlledPawn->ActionComp->GetCurrentAction() == nullptr && 
					  ControlledPawn->ActionComp->RealTimeActionQueue.Actions.IsEmpty();
		}
		if (bIsIdle)
		{
			// 1) if we have ammo, enqueue an attack action
			if (ControlledPawn->InventoryComp->GetItemQuantity(3) > ControlledPawn->LowAmmoThreshold)
			{
				RegisterDefensiveAIActon();
			}
			// 2) if we're out of ammo, enqueue a fetch‐ammo action
			else
			{
				RegisterFetchingAmmoEvent();
			}
		}
	}

	CachedAIState = ControlledPawn->CharaAIState;
}

void AOrionAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledPawn = Cast<AOrionChara>(GetPawn());

	if (!ControlledPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("ControlledPawn is nullptr"));
		return;
	}


	if (ControlledPawn->ActionComp)
	{
		ActionComponent = ControlledPawn->ActionComp;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ControlledPawn has no Action Component. "));
	}
}

void AOrionAIController::RegisterFetchingAmmoEvent()
{
	// 确保只在 Defensive 状态下执行
	if (!ControlledPawn || ControlledPawn->CharaAIState != EAIState::Defensive)
	{
		return;
	}

	// [Fix] 通过 ActionComp 检查状态
	bool bIsIdle = false;
	if (ControlledPawn->ActionComp)
	{
		// [Fix] Use GetCurrentAction() instead of CurrentAction pointer
		bIsIdle = ControlledPawn->ActionComp->GetCurrentAction() == nullptr && 
				  ControlledPawn->ActionComp->RealTimeActionQueue.Actions.IsEmpty();
	}
	if (ControlledPawn->CharaState == ECharaState::Alive && bIsIdle)
	{
		UE_LOG(LogTemp, Log, TEXT("Enqueuing FetchAmmo action"));

		const FOrionAction AddingOrionAction = ControlledPawn->InitActionCollectBullets(TEXT("AC_CollectAmmo"));
		ControlledPawn->InsertOrionActionToQueue(AddingOrionAction, EActionExecution::RealTime, -1);
	}
}


void AOrionAIController::RegisterDefensiveAIActon()
{
	// 确保只在 Defensive 状态下执行
	if (!ControlledPawn || ControlledPawn->CharaAIState != EAIState::Defensive)
	{
		return;
	}

	// [Fix] 通过 ActionComp 检查状态
	bool bIsIdle = false;
	if (ControlledPawn->ActionComp)
	{
		// [Fix] Use GetCurrentAction() instead of CurrentAction pointer
		bIsIdle = ControlledPawn->ActionComp->GetCurrentAction() == nullptr && 
				  ControlledPawn->ActionComp->RealTimeActionQueue.Actions.IsEmpty();
	}
	if (ControlledPawn->CharaState == ECharaState::Alive && bIsIdle && ControlledPawn->InventoryComp->GetItemQuantity(3) > ControlledPawn->LowAmmoThreshold)
	{
		if (AActor* TargetActor = GetClosestHostileActor())
		{
			UE_LOG(LogTemp, Log, TEXT("AOrionAIController::RegisterDefensiveAIActon: Found target %s"), *TargetActor->GetName());
			const FString ActionName = FString::Printf(TEXT("AttackOnCharaLongRange|%s"), *TargetActor->GetName());

			const FOrionAction AddingOrionAction = ControlledPawn->InitActionAttackOnChara(ActionName, TargetActor, FVector());
			ControlledPawn->InsertOrionActionToQueue(AddingOrionAction, EActionExecution::RealTime, -1);
		}
		else
		{
			// UE_LOG(LogTemp, Log, TEXT("AOrionAIController::RegisterDefensiveAIActon: No available target found"));
		}
	}
}


void AOrionAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if ((ControlledPawn == nullptr) || (Actor == nullptr))
	{
		return;
	}

	// Stimulus.IsActive() == true 表示当前还能“看见”该Actor
	const FString DebugMessage = FString::Printf(TEXT("%s: %s sighted"), *ControlledPawn->GetName(), *Actor->GetName());
	if (Stimulus.IsActive())
	{
	}
	else
	{
		// 一旦看不见了，通常会收到 Stimulus.IsActive() == false
		//FString DebugMessage2 = FString::Printf(TEXT("%s: Lost sight of %s"), *GetControlledPawnName(), *Actor->GetName());
	}
}

AActor* AOrionAIController::GetClosestHostileActor() const
{
	AOrionChara* MyControlledPawn = Cast<AOrionChara>(GetPawn());
	if (!MyControlledPawn || !MyControlledPawn->AttributeComp)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UOrionFactionManager* FactionManager = nullptr;
	if (UGameInstance* GI = World->GetGameInstance())
	{
		FactionManager = GI->GetSubsystem<UOrionFactionManager>();
	}
	
	if (!FactionManager)
	{
		return nullptr;
	}

	const EFaction MyFaction = MyControlledPawn->AttributeComp->ActorFaction;
	const FVector MyLocation = MyControlledPawn->GetActorLocation();

	// 分别跟踪非 BaseStorage 和 BaseStorage 的最近目标
	AActor* ClosestNonBaseStorageActor = nullptr;
	float MinDistSquaredNonBaseStorage = FLT_MAX;
	AActor* ClosestBaseStorageActor = nullptr;
	float MinDistSquaredBaseStorage = FLT_MAX;

	// Helper lambda to process actor
	auto ProcessActor = [&](AActor* Actor)
	{
		if (!Actor || Actor == MyControlledPawn) return;

		// Check for AttributeComponent
		const UOrionAttributeComponent* TargetAttr = Actor->FindComponentByClass<UOrionAttributeComponent>();
		if (!TargetAttr || !TargetAttr->IsAlive()) return;

		// Check Faction
		if (!FactionManager->IsHostile(MyFaction, TargetAttr->ActorFaction)) return;

		// Check Distance
		float Dist2 = FVector::DistSquared(MyLocation, Actor->GetActorLocation());
		
		// 根据 IsBaseStorage 分类处理
		if (TargetAttr->IsBaseStorage)
		{
			// BaseStorage 目标：只有在没有非 BaseStorage 目标时才考虑
			if (Dist2 < MinDistSquaredBaseStorage)
			{
				MinDistSquaredBaseStorage = Dist2;
				ClosestBaseStorageActor = Actor;
			}
		}
		else
		{
			// 非 BaseStorage 目标：优先选择
			if (Dist2 < MinDistSquaredNonBaseStorage)
			{
				MinDistSquaredNonBaseStorage = Dist2;
				ClosestNonBaseStorageActor = Actor;
			}
		}
	};

	// 1. Iterate OrionCharas
	TArray<AActor*> AllCharas;
	UGameplayStatics::GetAllActorsOfClass(World, AOrionChara::StaticClass(), AllCharas);
	for (AActor* Actor : AllCharas)
	{
		// Skip dead/incapacitated logic is inside ProcessActor (via IsAlive)
		// But AOrionChara has specific state (Incapacitated) which might not set Health <= 0 yet?
		// Actually IsAlive() checks Health > 0.
		// However, OrionChara::Tick skips logic if Dead/Incapacitated.
		// Let's assume IsAlive is sufficient or add extra check if needed.
		// Previous code checked CharaState.
		if (AOrionChara* Chara = Cast<AOrionChara>(Actor))
		{
			if (Chara->CharaState == ECharaState::Dead || Chara->CharaState == ECharaState::Incapacitated) continue;
		}
		ProcessActor(Actor);
	}

	// 2. Iterate OrionActors (Buildings/Ore/etc that have AttributeComponent)
	// Note: AOrionActor is base for Buildings. Ore might also be AOrionActor.
	// If the user wants "any object with attribute component", iterating AOrionActor covers most.
	TArray<AActor*> AllOrionActors;
	UGameplayStatics::GetAllActorsOfClass(World, AOrionActor::StaticClass(), AllOrionActors);
	for (AActor* Actor : AllOrionActors)
	{
		ProcessActor(Actor);
	}

	// 3. Iterate standalone OrionStructure actors (not derived from AOrionActor)
	TArray<AActor*> AllStructures;
	UGameplayStatics::GetAllActorsOfClass(World, AOrionStructure::StaticClass(), AllStructures);
	for (AActor* Actor : AllStructures)
	{
		// Skip preview/ghost structures to avoid targeting placement previews
		if (const AOrionStructure* Structure = Cast<AOrionStructure>(Actor))
		{
			if (Structure->StructureComponent && Structure->StructureComponent->BIsPreviewStructure)
			{
				continue;
			}
		}
		ProcessActor(Actor);
	}

	// 优先返回非 BaseStorage 目标，只有在没有其他目标时才返回 BaseStorage 目标
	if (ClosestNonBaseStorageActor)
	{
		return ClosestNonBaseStorageActor;
	}
	
	// 如果没有非 BaseStorage 目标，返回 BaseStorage 目标（如果有的话）
	return ClosestBaseStorageActor;
}

/*
AOrionChara* AOrionAIController::GetClosestOrionCharaByRelation(
	ERelation Relation,
	const TArray<int32>& HostileGroups,
	const TArray<int32>& FriendlyGroups
) const
{
	// DEPRECATED IMPLEMENTATION REMOVED FOR BREvITY
	return nullptr;
}
*/
