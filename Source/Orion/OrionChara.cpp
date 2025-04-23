// OrionChara.cpp

#include "OrionChara.h"
#include "OrionAIController.h"

#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
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
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include <Components/SphereComponent.h>

/*
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "OrionGameMode.h"
#include "OrionWeapon.h"
#include "Animation/SkeletalMeshActor.h"
#include "OrionBPFunctionLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
*/


AOrionChara::AOrionChara()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AOrionAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;


	StimuliSourceComp = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("StimuliSourceComp"));
	StimuliSourceComp->RegisterForSense(TSubclassOf<UAISense_Sight>());
	StimuliSourceComp->bAutoRegister = true;


	SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
	CharacterActionQueue.Actions.reserve(100);

	/* AI Information */
	HostileGroupsIndex.Add(1);
	HostileGroupsIndex.Add(3);
	HostileGroupsIndex.Add(5);

	FriendlyGroupsIndex.Add(2);
	FriendlyGroupsIndex.Add(4);
	FriendlyGroupsIndex.Add(6);
}

void AOrionChara::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("%s has been constructed."), *GetName());

	if (GetMesh())
	{
		DefaultMeshRelativeLocation = GetMesh()->GetRelativeLocation();
		DefaultMeshRelativeRotation = GetMesh()->GetRelativeRotation();
		DefaultCapsuleMeshOffset = GetCapsuleComponent()->GetComponentLocation() - GetMesh()->GetComponentLocation();
	}

	// 1) 尝试获取当前的 AIController
	AIController = Cast<AOrionAIController>(GetController());
	if (AIController)
	{
		UE_LOG(LogTemp, Log, TEXT("OrionChara is now controlled by OrionAIController"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("OrionChara is not controlled by OrionAIController"));

		// 检查是否设置了 AIControllerClass
		if (AIControllerClass)
		{
			UE_LOG(LogTemp, Log, TEXT("AIControllerClass is set to: %s"), *AIControllerClass->GetName());

			// 获取世界上下文

			if (UWorld* World = GetWorld())
			{
				// 设置 Spawn 参数
				FActorSpawnParameters SpawnParams;
				SpawnParams.Instigator = GetInstigator();
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				UE_LOG(LogTemp, Log, TEXT("Attempting to spawn AIController of class: %s"),
				       *AIControllerClass->GetName());

				// 使用 SpawnActor 生成 AIController，不再使用 Cast<>()，因为 SpawnActor<AOrionAIController> 已返回正确类型
				AIController = World->SpawnActor<AOrionAIController>(AIControllerClass, SpawnParams);
				if (AIController)
				{
					// 让新的 AIController 控制这个角色
					AIController->Possess(this);
					UE_LOG(LogTemp, Log, TEXT("OrionAIController has been spawned and possessed the OrionChara."));
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to spawn OrionAIController. Skipping related logic."));
					// 转换失败，跳过相关逻辑
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("World context is invalid. Skipping related logic."));
				// 世界上下文无效，跳过相关逻辑
			}
		}
		else
		{
			UE_LOG(LogTemp, Error,
			       TEXT("AIControllerClass is not set. Please assign it in the constructor or editor."));
			// AIControllerClass 未设置，跳过相关逻辑
		}
	}

	// 2) 获取 CharacterMovementComponent
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent)
	{
		// Don't rotate character to camera direction
		bUseControllerRotationPitch = false;
		bUseControllerRotationYaw = false;
		bUseControllerRotationRoll = false;

		// Configure character movement
		MovementComponent->bOrientRotationToMovement = true; // Rotate character to moving direction
		MovementComponent->RotationRate = FRotator(0.f, 270.f, 0.f);
		MovementComponent->bConstrainToPlane = true;
		MovementComponent->bSnapToPlaneAtStart = true;

		MovementComponent->MaxWalkSpeed = OrionCharaSpeed;
	}

	// Initialize default health values
	CurrHealth = MaxHealth;

	// Enable the character to receive damage
	this->SetCanBeDamaged(true);
}

void AOrionChara::ChangeMaxWalkSpeed(float InValue)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement();)
	{
		MovementComponent->MaxWalkSpeed = InValue;
		OrionCharaSpeed = InValue;
	}
}

void AOrionChara::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CharaState == ECharaState::Dead || CharaState == ECharaState::Incapacitated)
	{
		return;
	}


	if (CharaState == ECharaState::Ragdoll)
	{
		FName RootBone = GetMesh()->GetBoneName(0);
		FVector MeshRootLocation = GetMesh()->GetBoneLocation(RootBone);

		UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
		if (CapsuleComp)
		{
			FVector NewCapsuleLocation = MeshRootLocation + DefaultCapsuleMeshOffset;
			CapsuleComp->SetWorldLocation(NewCapsuleLocation);
		}


		if (RagdollWakeupAccumulatedTime >= RagdollWakeupThreshold)
		{
			RagdollWakeupAccumulatedTime = 0;
			RagdollWakeup();
		}
		else
		{
			RagdollWakeupAccumulatedTime += DeltaTime;
		}
	}

	ForceDetectionOnVelocityChange();

	if (CurrentAction == nullptr && !CharacterActionQueue.IsEmpty())
	{
		CurrentAction = CharacterActionQueue.GetNextAction();
	}

	if (CurrentAction)
	{
		bool ActionComplete = CurrentAction->ExecuteFunction(DeltaTime);

		if (ActionComplete)
		{
			CharacterActionQueue.RemoveCurrentAction();
			CurrentAction = nullptr;
		}
	}

	if (IsAttackOnCharaLongRange != PreviousTickIsAttackOnCharaLongRange)
	{
		SpawnBulletActorAccumulatedTime = AttackFrequencyLongRange - AttackTriggerTimeLongRange;
	}

	PreviousTickIsAttackOnCharaLongRange = IsAttackOnCharaLongRange;
}

void AOrionChara::ForceDetectionOnVelocityChange()
{
	FVector CurrentVelocity = GetVelocity();
	float VelocityChange = (CurrentVelocity - PreviousVelocity).Size();

	if (VelocityChange > VelocityChangeThreshold)
	{
		// 认为受到了外力影响
		OnForceExceeded(CurrentVelocity - PreviousVelocity);
	}

	PreviousVelocity = CurrentVelocity;
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

bool AOrionChara::InteractWithActor(float DeltaTime, AOrionActor* InTarget)
{
	if (!IsValid(InTarget))
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not valid."));
		IsInteractWithActor = false;
		return true;
	}

	if (InTarget->ActorStatus == EActorStatus::NotInteractable)
	{
		UE_LOG(LogTemp, Warning, TEXT("InteractWithActor: InTarget is not Interactable."));
		IsInteractWithActor = false;
		return true;
	}

	USphereComponent* CollisionSphere = InTarget->FindComponentByClass<USphereComponent>();

	if (CollisionSphere && CollisionSphere->IsOverlappingActor(this))
	{
		if (InTarget->GetName().Contains("BP_OrionDynamicActor"))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Interacting with BP_OrionDynamicActor is now Discarded."),
			       *this->GetName());
			//return PickUpItem(DeltaTime, InTarget);
			return true;
		}

		IsInteractWithActor = true;

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
			IsInteractWithActor = false;
			return true;
		}

		if (!DoOnceInteractWithActor)
		{
			DoOnceInteractWithActor = true;
			CurrentInteractActor = InTarget;
			InTarget->CurrWorkers += 1;
		}

		if (AIController)
		{
			AIController->StopMovement();
			//UE_LOG(LogTemp, Log, TEXT("StopMovement Called. "));
		}

		// 计算朝向目标的旋转角度
		FRotator LookAtRot = UKismetMathLibrary::FindLookAtRotation(
			GetActorLocation(),
			InTarget->GetActorLocation()
		);

		// 忽略高度轴上的旋转
		LookAtRot.Pitch = 0.0f;
		LookAtRot.Roll = 0.0f;

		// 设置角色的旋转
		SetActorRotation(LookAtRot);

		return false;
	}
	if (AIController)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (NavSys)
		{
			FNavLocation ProjectedLocation; // 修改为 FNavLocation

			// 定义搜索范围
			FVector SearchExtent(500.0f, 500.0f, 500.0f);

			// 尝试将目标点投影到导航网格上
			if (NavSys->ProjectPointToNavigation(
				InTarget->GetActorLocation(), // 原始目标位置
				ProjectedLocation, // 输出投影后的可导航点
				SearchExtent // 搜索范围
			))
			{
				// 使用投影点作为目标位置
				EPathFollowingRequestResult::Type RequestResult = AIController->MoveToLocation(
					ProjectedLocation.Location, // 使用 ProjectedLocation.Location
					20.0f, // 容忍距离（单位：厘米）
					true, // 停止于重叠
					true, // 使用路径寻路
					true, // 投影到可导航区域
					false, // 不允许平移移动
					nullptr, // 无自定义路径过滤器
					true // 允许部分路径
				);

				if (RequestResult == EPathFollowingRequestResult::Failed)
				{
					//UE_LOG(LogTemp, Warning, TEXT("MoveToLocation failed even after projection!"));
				}
				else
				{
					//UE_LOG(LogTemp, Log, TEXT("MoveToLocation request successful."));
				}
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("Failed to project point to navigation!"));
			}
		}
		else
		{
			//UE_LOG(LogTemp, Error, TEXT("Navigation system is not available!"));
		}
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

	IsInteractWithActor = false;
	DoOnceInteractWithActor = false;
	CurrentInteractActor->CurrWorkers -= 1;
	CurrentInteractActor = nullptr;
	InteractType = EInteractType::Unavailable;
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

float AOrionChara::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                              AActor* DamageCauser)
{
	float DamageApplied = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	CurrHealth -= DamageApplied;

	//UE_LOG(LogTemp, Log, TEXT("AOrionChara took %f damage. Current Health: %f"), DamageApplied, CurrHealth);

	// 检查 DamageEvent 是否为径向伤害事件
	/*
	if (DamageEvent.GetTypeID() == FRadialDamageEvent::ClassID)
	{
		UE_LOG(LogTemp, Log, TEXT("DamageEvent is a radial damage event."));
	    // 将 DamageEvent 转换为 FRadialDamageEvent
	    const FRadialDamageEvent& RadialDamageEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);

	    // 计算角色与爆炸点的距离
	    float Distance = FVector::Dist(RadialDamageEvent.Origin, GetActorLocation());

	    // 计算力的大小
	    float MaxForce = DamageAmount; // 这里假设 DamageAmount 与最大力成正比
	    float ForceMagnitude = FMath::Max(0.f, MaxForce * (1 - Distance / RadialDamageEvent.Params.OuterRadius));

	    UE_LOG(LogTemp, Warning, TEXT("Calculated ForceMagnitude: %f at Distance: %f"), ForceMagnitude, Distance);

	    // 如果力超过阈值，调用 OnForceExceeded
	    if (ForceMagnitude > ForceThreshold)
	    {
	        // 计算力的方向
	        FVector ForceDirection = GetActorLocation() - RadialDamageEvent.Origin;
	        ForceDirection = ForceDirection.GetSafeNormal();

	        // 调用 OnForceExceeded，传递力向量
	        OnForceExceeded(ForceDirection * ForceMagnitude);
	    }
	}
	*/


	if (CurrHealth <= 0.0f)
	{
		Incapacitate();
	}

	return DamageApplied;
}

void AOrionChara::OnForceExceeded(const FVector& DeltaVelocity)
{
	float DeltaVSize = DeltaVelocity.Size();
	UE_LOG(LogTemp, Warning, TEXT("Force exceeded! Delta Velocity: %f"), DeltaVSize);

	float Mass = GetMesh()->GetMass();

	constexpr float DeltaT = 0.0166f;

	// 计算近似的平均受力 F = m * Δv / Δt
	float ApproximatedForce = Mass * DeltaVSize / DeltaT / 20.0f;
	UE_LOG(LogTemp, Warning, TEXT("Approximated Impulse Force: %f"), ApproximatedForce);

	FVector CurrVel = GetVelocity();

	Ragdoll();

	// 计算当前速度

	GetMesh()->SetPhysicsLinearVelocity(CurrVel);

	FVector ImpulseToAdd = DeltaVelocity.GetSafeNormal() * ApproximatedForce * DeltaT;
	GetMesh()->AddImpulse(ImpulseToAdd, NAME_None, true);
}

void AOrionChara::Die()
{
	CharaState = ECharaState::Dead;

	UE_LOG(LogTemp, Log, TEXT("AOrionChara::Die() called."));

	// 1. 停止所有动作
	RemoveAllActions();

	// 2. 停止AI感知
	if (StimuliSourceComp)
	{
		// Ensure that the sense class is valid
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

	// 1. 停止所有动作
	RemoveAllActions();
	// 2. 停止AI感知
	if (StimuliSourceComp)
	{
		// Ensure that the sense class is valid
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

	// Inside AOrionChara::BeginPlay() or wherever appropriate
	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(true);
	}

	// Disable capsule collision
	UCapsuleComponent* CharaCapsuleComponent = GetCapsuleComponent();
	if (CharaCapsuleComponent)
	{
		CharaCapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AOrionChara::Ragdoll()
{
	RemoveAllActions();

	//UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	//if (CapsuleComp)
	//{
	//    CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//    CapsuleComp->SetVisibility(false);
	//}

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(true);
	}

	CharaState = ECharaState::Ragdoll;
}

void AOrionChara::RagdollWakeup()
{
	CharaState = ECharaState::Alive;

	UE_LOG(LogTemp, Warning, TEXT("AOrionChara::RagdollWakeup() called."));

	UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(
		GetComponentByClass(UPrimitiveComponent::StaticClass()));
	if (PrimitiveComponent)
	{
		PrimitiveComponent->SetSimulatePhysics(false);
	}

	// 将角色旋转调整为正立（保留当前 Yaw，仅将 Pitch 与 Roll 设为 0）
	FRotator CurrentRot = GetActorRotation();
	FRotator UprightRot(0.0f, CurrentRot.Yaw, 0.0f);
	SetActorRotation(UprightRot);

	// 重置 Skeletal Mesh 的相对位置和旋转至初始状态
	if (GetMesh())
	{
		// 如果当前 Mesh 还处于物理模拟状态下，其骨骼可能被物理计算改变，停止物理模拟后重新设置初始状态
		GetMesh()->SetRelativeLocation(DefaultMeshRelativeLocation);
		GetMesh()->SetRelativeRotation(DefaultMeshRelativeRotation);

		// 刷新骨骼状态，使其回到参考姿势
		GetMesh()->RefreshBoneTransforms();

		// 如果需要，还可以重置动画实例（强制刷新到动画蓝图中的默认 Idle 或站立状态）
		if (UAnimInstance* AnimInst = GetMesh()->GetAnimInstance())
		{
			AnimInst->InitializeAnimation();
		}
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
	// 1) 拿到场景中所有 AOrionChara 实例
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), StaticClass(), AllActors);

	// 2) 过滤出“不是自己”且“阵营不同”的角色
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

	// 3) 按距离从近到远排序（用 DistSquared 更高效）
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
